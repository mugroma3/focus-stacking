//#include <avr/pgmspace.h>

#include <LCD16x2.h>
#include <math.h>
#include <stdlib.h>
#include "Additional.h"
#include <Wire.h>

#define runEvery(t) for(static uint16_t _lasttime; (uint16_t)((uint16_t)millis() - _lasttime) >= (t); _lasttime += (t))


#define refreshPeriod 20
#define blinkfrequency 25


//in which state the hardware-controlling state machine is in now
hardware_action_t current_h_action = NONE;

display_menu_t current_menu = MAIN_SCREEN;

LCD16x2 lcd;

void setupScreen(){
	Wire.begin();
	delay(270);
	lcd.lcdClear();
}

//sleep & reset are wired HIGH
//const int pin_reset = 0;
//const int pin_sleep = 1;
const int pin_ms1   = 3;
const int pin_ms2   = 4;
const int pin_ms3   = 5;
const int pin_enable= 6;

const int pin_step  = 8;
const int pin_dir   = 9;
void setupDriver(){
	pinMode(pin_ms1, OUTPUT);
	pinMode(pin_ms2, OUTPUT);
	pinMode(pin_ms3, OUTPUT);
	pinMode(pin_enable, OUTPUT);
	digitalWrite(pin_enable, HIGH);		//disabled at startup
	pinMode(pin_dir, OUTPUT);
	pinMode(pin_step, OUTPUT);
	digitalWrite(pin_dir, LOW);
	digitalWrite(pin_step, LOW);
}

const int pin_shoot = 10;
void setupCamera(){
	pinMode(pin_shoot, OUTPUT);
	digitalWrite(pin_shoot, LOW);
}

//how many shoots are to be done
unsigned int shoot_to_be_done;
//how many microsteps must be done between shoots 
unsigned int steps_for_picture;
//how many ms must be elapsed before issuing the next microstep
unsigned long stepping_period_ms;
//how long is each picture taken 
unsigned long shooting_period_ms;


//how many steps in this train are done
unsigned int steps_done;



/*
 * functions to interface with the state machine that controls the hardware
 */


//linear move in the home direction
void move_home(){
	move(-30, QUARTER);
	current_h_action = MOVE;
}

//linear move in the end direction
void move_end(){
	move(30, SIXTEENTH);
	current_h_action = MOVE;
}

//for each microstepping mode, a step will move for this mm. TODO: fill
const float step_length[] = {(3./20.), (3./20.)/(1<< HALF), (3./20.)/(1<< QUARTER), (3./20.)/(1<<EIGHTH), (3./20.)/(1<<SIXTEENTH)};

//setup the main use case and initiate it
void step_and_shoot(unsigned int shoot_idle_period_ms, unsigned int shoot_period_ms, unsigned int steps_each_picture, microstepping_t mode, unsigned int repetitions){
	float shoot_vel=step_length[mode] / shoot_period_ms;
	move(shoot_vel, mode);
	shoot(shoot_period_ms);
	shoot_to_be_done=repetitions;
	steps_for_picture=steps_each_picture;

	current_h_action = STEP_AND_SHOOT_FASE_1;
}

//stop every action
void stop(){
	digitalWrite(pin_enable, HIGH); //disable motor
	digitalWrite(pin_shoot, LOW);	//close trigger
	current_h_action = NONE;
}


/*
 * utility functions for the hardware state machine
 */

//setup the stepping_period_ms to create a motion with the supplied velocity and microstepping mode
void move(float vel_mm_sec, microstepping_t stepping_mode){
	/* set direction */
	if(vel_mm_sec<0){
		digitalWrite(pin_dir, HIGH);		//TODO
	}else{
		digitalWrite(pin_dir, LOW);
	}
	/* set microstepping pin */
	switch(stepping_mode){
		case SINGLE:
			digitalWrite(pin_ms1, LOW);
			digitalWrite(pin_ms2, LOW);
			digitalWrite(pin_ms3, LOW);
			break;
		case HALF:
			digitalWrite(pin_ms1, HIGH);
			digitalWrite(pin_ms2, LOW);
			digitalWrite(pin_ms3, LOW);
			break;
		case QUARTER:
			digitalWrite(pin_ms1, LOW);
			digitalWrite(pin_ms2, HIGH);
			digitalWrite(pin_ms3, LOW);
			break;
		case EIGHTH:
			digitalWrite(pin_ms1, HIGH);
			digitalWrite(pin_ms2, HIGH);
			digitalWrite(pin_ms3, LOW);
			break;
		case SIXTEENTH:
			digitalWrite(pin_ms1, HIGH);
			digitalWrite(pin_ms2, HIGH);
			digitalWrite(pin_ms3, HIGH);
			break;
		default:
			break;
	}

	digitalWrite(pin_enable, LOW); //enable module
	stepping_period_ms = (unsigned long) floor((1000*step_length[stepping_mode])/fabs(vel_mm_sec));
}

//setup the shoot hardware
void shoot(unsigned int shoot_period_ms){
	/*set pins*/
	digitalWrite(pin_shoot, LOW);
	shooting_period_ms=shoot_period_ms;
}

//return the time elapsed between two timestamps
inline unsigned long count_elapsed(unsigned long previous_time, unsigned long successive_time){
	const unsigned long ulong_max = (unsigned long)~(0ul);	
	return (previous_time <= successive_time)? successive_time - previous_time :	ulong_max - previous_time + successive_time;
}

//exec a step if is time
unsigned long last_activation_ms=0;
bool step_if_due(unsigned long period_ms){
	unsigned long elapsed = count_elapsed(last_activation_ms, millis());
	if(elapsed>=period_ms){
		/**step **/
		digitalWrite(pin_step, LOW);
		digitalWrite(pin_step, HIGH);
		delayMicroseconds(360);
		digitalWrite(pin_step, LOW);

		last_activation_ms=millis();
		return true;
	}
	return 0;
}

//exec a shoot. return true if shooting_period_ms is elapsed from the first activation 
bool counting_time=0;
bool keep_shooting_if_due(unsigned long shooting_period_ms){
	if(!counting_time){
		counting_time=true;
		pinMode(pin_shoot, HIGH);			//trigger (and keep triggering) camera
		last_activation_ms=millis();
	}

	unsigned long elapsed = count_elapsed(last_activation_ms, millis());
	if(elapsed>shooting_period_ms){
		pinMode(pin_shoot, LOW);			//stop triggering camera
		counting_time=0;
	}
	
	return counting_time;
}

#define BUTS(buttons) !(buttons)
#define BUT(buttons, num) !((buttons) & (0x01 << (num -1)))

#define BUT_1(buttons) BUT((buttons), 1)
#define BUT_2(buttons) BUT((buttons), 2)
#define BUT_3(buttons) BUT((buttons), 3)
#define BUT_4(buttons) BUT((buttons), 4)



bool enteringState=true;
int but_num=0;

void main_diplay(int buttons){
	if(enteringState){
		lcd.lcdClear();
		lcd.lcdGoToXY(1,2);
		lcd.lcdWrite("<");
		lcd.lcdGoToXY(6,2);
		lcd.lcdWrite(">");

		lcd.lcdGoToXY(12,2);
		lcd.lcdWrite("Start");


		enteringState=false;
	}

	if(but_num && !BUT(buttons, but_num)){
			//on falling edge
			/*fire event*/

			//stop moving
			if(but_num==1||but_num==2){
				stop();
				//stop
			}

			if(but_num==4){
				current_menu=START_CAPTURE;
				enteringState=true;
			}

			but_num=0;
	}else if(!but_num){ //free state
		//capture a button
		if(BUT_1(buttons)){
			but_num=1;
			move_home();
			//home
		}else if(BUT_2(buttons)){
			but_num=2;
			move_end();
			//end
		}else if(BUT_3(buttons)){
			but_num=3;
			//donothing
		}else if(BUT_4(buttons)){
			but_num=4;
			//donothing
		}
	}

}


void setup() {

	setupScreen();
	setupDriver();
	setupCamera();
	Serial.begin(115200);

}

void loop() {
	// hardware actions
	switch(current_h_action){
		case MOVE:			//generic moving movement. never end alone
			step_if_due(stepping_period_ms);
			break;
		case STEP_AND_SHOOT_FASE_1:				//fase 1 of main use case. wait the period needed for a picture, and at the end of it decrement the number of shoot to be taken 
			if(!keep_shooting_if_due(shooting_period_ms)){
				shoot_to_be_done--;
				if(shoot_to_be_done>0){		//if there are more shoots, transition to fase2
					steps_done=steps_for_picture;				
					current_h_action=STEP_AND_SHOOT_FASE_2;
				}else{							//otherwise, stop.
					current_h_action=NONE;
				}
			}
			break;
		case STEP_AND_SHOOT_FASE_2:				//fase 2 of main use case. try to issue a microstep (waiting the right amount of time). if successful, decrement the number of steps to be taken
			if(step_if_due(stepping_period_ms)){
				steps_done--;
				if(steps_done==0){				//and goto to fase 1 if no more steps are to be taken
					current_h_action=STEP_AND_SHOOT_FASE_1;
				}
			}
			break;
		case NONE:
		default:
			break;
	}

	int buttons;
	runEvery(200){
		Serial.print(F("Buttons: "));
		Serial.println(buttons=lcd.readButtons());

		main_diplay(buttons);
	}

}
