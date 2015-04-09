//microstepping mode, used as index in the array for lenght moved by the carriage when a microstep is done
enum microstepping_t {SINGLE=0, HALF=1, QUARTER=2, EIGHTH=3, SIXTEENTH=4};

//in which state the hardware-controlling state machine can be in
enum hardware_action_t {NONE, MOVE, STEP_AND_SHOOT_FASE_1, STEP_AND_SHOOT_FASE_2, STEP_AND_SHOOT_FASE_3A, STEP_AND_SHOOT_FASE_3B};

enum display_menu_t {MOVE_SCREEN, MM_CAPTURE, NUM_SHOOTS, ESPOSITION_TIME, DELAY_SHOT, START_CAPTURE, CAPTURING};



struct param_display{
	char label[11] PROGMEM;
	char short_label[9] PROGMEM;
	unsigned int val;
	unsigned int inc;
	display_menu_t prev;
	display_menu_t next;
};

struct param_display mm_param{"mm Capture", "mm capt ", 0, 1, MOVE_SCREEN, NUM_SHOOTS}, 
shoots_param {"Shoots", "#shoots ", 0, 1, MM_CAPTURE, ESPOSITION_TIME}, 
exposition_param {"ms ExpTime", "ms exp  ", 0, 10, NUM_SHOOTS, DELAY_SHOT}, 
delay_param{"ms Delay", "ms delay", 0, 10, ESPOSITION_TIME, START_CAPTURE};



const microstepping_t microstep_mode= QUARTER;