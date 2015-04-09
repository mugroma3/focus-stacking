//#include <avr/pgmspace.h>

#include "LCD16x2.h"
#include <math.h>
#include <stdlib.h>
#include "Additional.h"
#include <Wire.h>

//in which state the hardware-controlling state machine is in now
hardware_action_t current_h_action = NONE;

display_menu_t current_menu = MOVE_SCREEN;

LCD16x2 lcd;

void setupScreen(){
	Wire.begin();
	delay(270);
	lcd.lcdClear();
}

//sleep & reset are wired HIGH
//const int pin_reset = 0;
//const int pin_sleep = 1;
const int pin_enable= 8;
const int pin_ms1   = 7;
const int pin_ms2   = 6;
const int pin_ms3   = 5;
const int pin_step  = 4;
const int pin_dir   = 3;

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

const int pin_shoot = 12;
void setupCamera(){
	pinMode(13, OUTPUT);
	digitalWrite(13, LOW);
	pinMode(pin_shoot, OUTPUT);
	pinMode(A7, INPUT);
	digitalWrite(pin_shoot, LOW);
}

//how many shoots are to be done
unsigned int shoot_to_be_done;
//how many microsteps must be done between shoots 
//unsigned int steps_for_picture;
//how many ms must be elapsed before issuing the next microstep
unsigned long stepping_period_ms;
//how long is each picture taken 
unsigned long shooting_period_ms;
//gratuitus delay time
unsigned long delay_period_ms;

//how many microsteps must be done between shoots, plus dithering
struct 
{
	float actual_value;
	float error;
} steps_for_picture;


/*
 * functions to interface with the state machine that controls the hardware
 */


//linear move in the home direction
void move_home(){
	move(-10, microstep_mode);
	current_h_action = MOVE;
}

//linear move in the end direction
void move_end(){
	move(10, microstep_mode);
	current_h_action = MOVE;
}

//for each microstepping mode, a step will move for this mm. TODO: fill
//const float step_length[] = {(3./20.), (3./20.)/(1<< HALF), (3./20.)/(1<< QUARTER), (3./20.)/(1<<EIGHTH), (3./20.)/(1<<SIXTEENTH)};
const float step_length[] = {0.18, 0.18/(1<< HALF), 0.18/(1<< QUARTER), 0.18/(1<<EIGHTH), 0.18/(1<<SIXTEENTH)};

/*
//setup the main use case and initiate it
void step_and_shoot(unsigned int shoots, unsigned int exp_ms, unsigned int mm, unsigned int delay_ms){
	move(5, microstep_mode);
	shoot(exp_ms);
	shoot_to_be_done=shoots;


	float mm_picture =(shoots>1)? ((float) mm )/(shoots -1): 0;
	steps_for_picture=(unsigned int)floor(mm_picture/step_length[QUARTER]);

	unsigned long total_steps= (long) mm / step_length[QUARTER];
	unsigned long min_steps_p = total_steps/(shoots - 1);
	unsigned long rem = total_step % (shoots - 1);


	delay_period_ms=delay_ms;
	current_h_action = STEP_AND_SHOOT_FASE_1;
}
*/

//setup the main use case and initiate it
void step_and_shoot(unsigned int shoots, unsigned int exp_ms, unsigned int mm, unsigned int delay_ms){
	move(5, microstep_mode);
	shoot(exp_ms);
	shoot_to_be_done=shoots;
	delay_period_ms=delay_ms;

	float total_steps= mm / step_length[QUARTER];

	steps_for_picture= {total_steps/(shoots-1), 0};
	
	current_h_action = STEP_AND_SHOOT_FASE_1;
}


unsigned int get_steps(){

	steps_for_picture;

	unsigned int s= (unsigned int) floor(steps_for_picture.actual_value + steps_for_picture.error);

	steps_for_picture.error = steps_for_picture.actual_value + steps_for_picture.error - s;
	return s; 
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
	if(vel_mm_sec==0) return;

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
	digitalWrite(13, LOW);
	digitalWrite(pin_shoot, LOW);
	shooting_period_ms=shoot_period_ms;
}

//return the time elapsed between two timestamps
inline unsigned long count_elapsed(unsigned long previous_time, unsigned long successive_time){
	const unsigned long ulong_max = (unsigned long)~(0ul);	
	return (previous_time <= successive_time)? successive_time - previous_time : ulong_max - previous_time + successive_time;
}

//exec a step if is time
bool step_if_due(unsigned long period_ms){
	static unsigned long last_activation_ms=0;
	static bool first_activation=true;
	if(first_activation){
		first_activation=false;
	}

	if(count_elapsed(last_activation_ms, millis()) >= period_ms){
		/**step **/
		digitalWrite(pin_step, LOW);
		digitalWrite(pin_step, HIGH);
		delayMicroseconds(360);
		digitalWrite(pin_step, LOW);

		last_activation_ms=millis();
		
		first_activation=true;
		return true;
	}else{
		return false;
	}
}

//exec a shoot. return true if shooting_period_ms is elapsedom the first activation 
bool keep_shooting_if_due(unsigned long shooting_period_ms){
	static bool counting_time=false;
	static unsigned long last_activation_ms;
	if(!counting_time){
		counting_time=true;
		digitalWrite(13, HIGH);			//trigger (and keep triggering) camera
		digitalWrite(pin_shoot, HIGH);			//trigger (and keep triggering) camera
		last_activation_ms=millis();
	}

	if(count_elapsed(last_activation_ms, millis()) > shooting_period_ms){
		digitalWrite(13, LOW);			//trigger (and keep triggering) camera
		digitalWrite(pin_shoot, LOW);			//stop triggering camera
		counting_time=false;
	}
	
	return counting_time;
}


bool delay_elapsed(unsigned long delay){
	static bool first_enter=true;
	static unsigned long first_activation; 
	if(first_enter){
		first_enter=false;
		first_activation=millis();
	}

	if(count_elapsed(first_activation, millis()) > delay_period_ms){
		first_enter=true;
		return true;
	}else{
		return false;
	}
}

const char leftArrow[] = "\x7f";
const char rightArrow[] = "\x7e";
const char asterisk[] = "\x2a";

#define BUTS(buttons) !(buttons)
#define BUT(buttons, num) !((buttons) & (0x01 << (num -1)))

#define BUT_1(buttons) BUT((buttons), 1)
#define BUT_2(buttons) BUT((buttons), 2)
#define BUT_3(buttons) BUT((buttons), 3)
#define BUT_4(buttons) BUT((buttons), 4)


const char move_string[] = "Move";
const char home_string[] = "Home";
const char end_string[] = "End";


void move_screen(){
	static bool enteringState=true;
	static int but_num=0;
	static unsigned int counter=0;

	if(enteringState){
		lcd.lcdClear();
		lcd.lcdGoToXY(1,1);
		lcd.lcdWrite(move_string);
		lcd.lcdGoToXY(1,2);
		lcd.lcdWrite(home_string);
		lcd.lcdGoToXY(6,2);
		lcd.lcdWrite(end_string);
		lcd.lcdGoToXY(11,2);
		lcd.lcdWrite(leftArrow);
		lcd.lcdGoToXY(16,2);
		lcd.lcdWrite(rightArrow);


		enteringState=false;
	}

	int buttons = lcd.readButtons();
	if(buttons==0xff) return;
	if(but_num && !BUT(buttons, but_num)){
			//on falling edge
			/*fire event*/

			//stop moving
			if(but_num==1||but_num==2){
				stop();
				//stop
			}

			//changeScreen shoots
			if(but_num==3){
				current_menu=START_CAPTURE;
				enteringState=true;
			}

			if(but_num==4){
				current_menu=NUM_SHOOTS;
				enteringState=true;
			}

			but_num=0;
	}else if(!but_num){
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


const char plus_string[]= "+";
const char minus_string[]= "-";


const char space_string[] = " ";
void sps_printval(unsigned int val){
	char val_representation [6]; //good for uint_16 (5char + null termintator)
	utoa(val, val_representation, 10); 
	int len = strlen(val_representation);
	for(int i=1; i<6-len; i++){
		lcd.lcdGoToXY(i, 1);
		lcd.lcdWrite(space_string);	
	}
	lcd.lcdGoToXY(6-len, 1);	lcd.lcdWrite(val_representation);
}

void single_param_screen(param_display* p){

	static bool enteringState=true;
	static int but_num=0;
	static unsigned int counter=0;

	if(enteringState){
		lcd.lcdClear();
		sps_printval(p->val);
		lcd.lcdGoToXY(7,1);
		lcd.lcdWrite(p->label);
		lcd.lcdGoToXY(1,2);
		lcd.lcdWrite(plus_string);
		lcd.lcdGoToXY(6,2);
		lcd.lcdWrite(minus_string);
		lcd.lcdGoToXY(11,2);
		lcd.lcdWrite(leftArrow);
		lcd.lcdGoToXY(16,2);
		lcd.lcdWrite(rightArrow);

		enteringState=0;
	}

	counter++;

	int buttons = lcd.readButtons();
	if(buttons==0xff) return;

	if(but_num && !BUT(buttons, but_num)){
			//on falling edge
			//fire event

			//changeScreen shoots
			if(but_num==3){
				current_menu=p->prev;
				enteringState=true;
			}

			if(but_num==4){
				current_menu=p->next;
				enteringState=true;
			}

			but_num=0;
	}else if(!but_num){ 
		//capture a button
		if(BUT_1(buttons)){
			but_num=1;

			(p->val)+=p->inc;
			sps_printval(p->val);
		}else if(BUT_2(buttons)){
			but_num=2;

			(p->val)-=p->inc;
			sps_printval(p->val);
		}else if(BUT_3(buttons)){
			but_num=3;
			//donothing
		}else if(BUT_4(buttons)){
			but_num=4;
			//donothing
		}

		counter=0;

	}else{	//pressing state
		if(counter%10 == 0){
			if(BUT_1(buttons)){
				(p->val)+=(p->inc) * 10;
				sps_printval(p->val);
			}else if(BUT_2(buttons)){
				(p->val)-=(p->inc) * 10;
				sps_printval(p->val);
			}
		}
	}
	
}



const char start_string[] = "Start";
const char ellipses_string[] = "...";

void scs_printval(struct param_display* p){
	char val_representation [6]; //good for uint_16 (5char + null termintator)
	utoa(p->val, val_representation, 10); 
	int len = strlen(val_representation);
	for(int i=1; i<6-len; i++){
		lcd.lcdGoToXY(i, 1);
		lcd.lcdWrite(space_string);	
	}
	lcd.lcdGoToXY(6-len, 1);	lcd.lcdWrite(val_representation);
	lcd.lcdGoToXY(6, 1); lcd.lcdWrite(p->short_label);
}

void start_capture_screen(){

	static bool enteringState=true;
	static int but_num=0;
	static unsigned int counter=0;
	static display_menu_t start_capture_current_value=NUM_SHOOTS;

	if(enteringState){
		lcd.lcdClear();
		lcd.lcdGoToXY(14,1);
		lcd.lcdWrite(ellipses_string);

		lcd.lcdGoToXY(2,2);
		lcd.lcdWrite(start_string);
		lcd.lcdGoToXY(11,2);
		lcd.lcdWrite(leftArrow);
		lcd.lcdGoToXY(16,2);
		lcd.lcdWrite(rightArrow);
		enteringState=false;
		counter=0;
		start_capture_current_value=NUM_SHOOTS;

		scs_printval(&shoots_param);
	}
	++counter;

	if(counter%60 ==0){
		struct param_display* p;
		switch(start_capture_current_value){
			case NUM_SHOOTS:
				p=&shoots_param;
				start_capture_current_value=p->next;
			break;
			case ESPOSITION_TIME:
				p=&exposition_param;
				start_capture_current_value=p->next;
			break;
			case MM_CAPTURE:
				p=&mm_param;
				start_capture_current_value=p->next;
			break;
			case DELAY_SHOT:
				p=&delay_param;
				start_capture_current_value=NUM_SHOOTS;
			break;
			default:
			break;
		}
		scs_printval(p);
	}

	int buttons = lcd.readButtons();
	if(buttons==0xff) return;

	if(but_num && !BUT(buttons, but_num)){
			//on falling edge
			//fire event

			//start capturing
			if(but_num==1|| but_num==2){
				step_and_shoot(shoots_param.val, exposition_param.val, mm_param.val, delay_param.val);
				current_menu=CAPTURING;
				enteringState=true;
			}

			//changeScreen
			if(but_num==3){
				current_menu=DELAY_SHOT;
				enteringState=true;
			}

			if(but_num==4){
				current_menu=MOVE_SCREEN;
				enteringState=true;
			}

			but_num=0;
	}else if(!but_num){
		//capture a button
		if(BUT_1(buttons)){
			but_num=1;
			//do no
		}else if(BUT_2(buttons)){
			but_num=2;
			//donothing
		}else if(BUT_3(buttons)){
			but_num=3;
			//donothing
		}else if(BUT_4(buttons)){
			but_num=4;
			//donothing
		}
	}

}


const char cancel_string[] = "abort";
const char of_string[] = "of";
const char return_string[] ="return";
const char done_string[] = "Done";

void cs_printval(unsigned int val, unsigned int pos){
	char val_representation [6]; //good for uint_16 (5char + null termintator)
	utoa(val, val_representation, 10); 
	int len =strlen(val_representation);
	for(int i=pos-5; i<pos-len; i++){
		lcd.lcdGoToXY(i, 1);
		lcd.lcdWrite(space_string);	
	}

	lcd.lcdGoToXY(pos-len, 1); 
	lcd.lcdWrite(val_representation);
}

void capturing_screen(){

	static bool enteringState=true;
	static int but_num=0;
	static unsigned int counter=0;
	static unsigned int done_capture_screen=0;

	if(enteringState){
		lcd.lcdClear();

		lcd.lcdGoToXY(1,2);
		lcd.lcdWrite(cancel_string);
		lcd.lcdGoToXY(8,1);
		lcd.lcdWrite(of_string);

		enteringState=0;
		done_capture_screen=0;
		cs_printval(shoots_param.val, 16);
		cs_printval(done_capture_screen, 7);

		//start capture
	}
	
//update num
	if(shoots_param.val-shoot_to_be_done != done_capture_screen){
	//update_val
		done_capture_screen=shoots_param.val-shoot_to_be_done;
		cs_printval(done_capture_screen, 7);

		if(shoot_to_be_done==0){
			lcd.lcdGoToXY(1,2);
			lcd.lcdWrite(return_string);
			stop();
		}
	}

	int buttons = lcd.readButtons();
	if(buttons==0xff) return;

	if(but_num && !BUT(buttons, but_num)){
			//on falling edge
			/*fire event*/

			if(but_num==1||but_num==2){
				current_menu=MOVE_SCREEN;
				enteringState=true;

				stop();
				//cancel capture
			}

			but_num=0;
	}else if(!but_num){
		//capture a button
		if(BUT_1(buttons)){
			but_num=1;
			//donothing
		}else if(BUT_2(buttons)){
			but_num=2;
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

	//how many steps in this train are done
	static unsigned int steps_to_be_done=0;

	// hardware actions
	switch(current_h_action){
		case MOVE:			//generic moving movement. never end alone
			step_if_due(stepping_period_ms);
			break;
		case STEP_AND_SHOOT_FASE_1:				//fase 1 of main use case. wait the period needed for a picture, and at the end of it decrement the number of shoot to be taken 
			if(!keep_shooting_if_due(shooting_period_ms)){
				shoot_to_be_done--;
				if(shoot_to_be_done>0){		//if there are more shoots, transition to fase2
					steps_to_be_done= get_steps();				
					current_h_action=STEP_AND_SHOOT_FASE_2;
				}else{							//otherwise, stop.
					current_h_action=NONE;
				}
			}
			break;
		case STEP_AND_SHOOT_FASE_2:				//fase 2 of main use case. try to issue a microstep (waiting the right amount of time). if successful, decrement the number of steps to be taken
			if(step_if_due(stepping_period_ms)){
				steps_to_be_done--;
				if(steps_to_be_done==0){				//and goto to fase 1 if no more steps are to be taken
					current_h_action=STEP_AND_SHOOT_FASE_3;
				}
			}
			break;

		case STEP_AND_SHOOT_FASE_3: 			//fase 3: wait delay ms to give the camera a chance to be ready
			if(delay_elapsed(delay_period_ms)){
				current_h_action=STEP_AND_SHOOT_FASE_1;
			}
			break;
		case NONE:
		default:
			break;
	}

	lcd.lcdUpdate();

	static unsigned long time_for_display=0;

	if(count_elapsed(time_for_display, millis())>20){
		switch(current_menu){
			case MOVE_SCREEN:
				move_screen();
			break;
			case NUM_SHOOTS:
				single_param_screen(&shoots_param);
			break;
			case ESPOSITION_TIME:
				single_param_screen(&exposition_param);
			break;
			case MM_CAPTURE:
				single_param_screen(&mm_param);
			break;
			case DELAY_SHOT:
				single_param_screen(&delay_param);
			break;
			case START_CAPTURE:
				start_capture_screen();
			break;
			case CAPTURING:
				capturing_screen();
			break;
			default:
			break;
		}
		time_for_display=millis();
	}

}
