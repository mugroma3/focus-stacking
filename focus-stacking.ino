
#include <math.h>

//in which state the hardware-controlling state machine can be in
enum hardware_action_t {NONE, MOVE, STEP_AND_SHOOT_FASE_1, STEP_AND_SHOOT_FASE_2};

//in which state the hardware-controlling state machine is in now
hardware_action_t current_h_action = NONE;

void setup() {

	setupScreen();
	setupDriver();
	setupCamera();
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

}


/*
 * functions to interface with the state machine that controls the hardware
 */

//microstepping mode, used as index in the array for lenght moved by the carriage when a microstep is done
enum microstepping_t {SINGLE=0, HALF=1, QUARTER=2, EIGHTH=3, SIXTEENTH=4};

//linear move in the home direction
void move_home(){
	move(-1f, SINGLE);
	current_h_action = MOVE;
}

//linear move in the end direction
void move_end(){
	move(1f, SINGLE);
	current_h_action = MOVE;
}

//setup the main use case and initiate it
void step_and_shoot(unsigned int shoot_period_ms, unsigned int steps_each_picture, microstepping_t mode, unsigned int repetitions){
	move(1f, mode);
	shoot(shoot_period_ms);
	shoot_repetitions=repetitions;
	steps_for_picture=steps_each_picture;

	current_h_action = STEP_AND_SHOOT_FASE_1;
}

//stop every action
void stop(){
	digitalWrite(pin_enable, HIGH); //disable motor
	current_h_action = NONE;
}


/*
 * utility functions for the hardware state machine
 */

//for each microstepping mode, a step will move for this mm. TODO: fill
const float[] step_length = {1}
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
	stepping_period_ms = (unsigned long) floor(fabs(vel_mm_sec)*1000f*step_length[stepping_mode]);
}

//setup the shoot hardware
void shoot(unsigned int shoot_period_ms){
	/*set pins*/

	shooting_period_ms=shoot_period_ms;
}

//return the time elapsed between two timestamps
unsigned long count_elapsed(unsigned long previous_time, unsigned long successive_time){
	const unsigned long ulong_max = (unsigned long)~(0ul);	
	return (previous_time =< successive_time) successive_time - previous_time :	/*rollback of millis*/ ulong_max - previous_time + successive_time;
}

//exec a step if is time
unsigned long last_activation_ms=0;
bool step_if_due(unsigned long period_ms){
	unsigned long elapsed = count_elapsed(last_activation_ms, milli());
	if(elapsed>=period_ms){
		/**step **/
		digitalWrite(pin_step, LOW);
		digitalWrite(pin_step, HIGH);
		delayMicroseconds(1);
		digitalWrite(pin_step, LOW);

		last_activation_ms=millis();
		return true;
	}
	return false;
}

//exec a shoot. return true if shooting_period_ms is elapsed from the first activation 
bool counting_time=false
bool keep_shooting_if_due(unsigned long shooting_period_ms){
	if(!counting_time){
		counting_time=true;
		pinMode(pin_shoot, HIGH);			//trigger (and keep triggering) camera
		last_activation_ms=millis();
	}

	unsigned long elapsed = count_elapsed(last_activation_ms, millis());
	if(elapsed>shooting_period_ms){
		pinMode(pin_shoot, LOW);			//stop triggering camera
		counting_time=false;
	}
	
	return counting_time;
}

