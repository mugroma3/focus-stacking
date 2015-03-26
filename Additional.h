//microstepping mode, used as index in the array for lenght moved by the carriage when a microstep is done
enum microstepping_t {SINGLE=0, HALF=1, QUARTER=2, EIGHTH=3, SIXTEENTH=4};

//in which state the hardware-controlling state machine can be in
enum hardware_action_t {NONE, MOVE, STEP_AND_SHOOT_FASE_1, STEP_AND_SHOOT_FASE_2};

enum display_menu_t {MAIN_SCREEN, NUM_SHOOTS, ESPOSITION_TIME, MM_CAPTURE, DELAY_SHOT, START_CAPTURE, CAPTURING};



struct param_display{
	char label[11] PROGMEM;
	char short_label[9] PROGMEM;
	unsigned int val;
	display_menu_t prev;
	display_menu_t next;
};

/* struct param_display shoots_param {"Shoots", "#shoots ", 0, MOVE_SCREEN, ESPOSITION_TIME}, 
exposition_param {"ms ExpTime", "ms exp  ", 0, NUM_SHOOTS, MM_CAPTURE}, 
mm_param{"mm Capture", "mm capt ", 0, ESPOSITION_TIME, DELAY_SHOT}, 
delay_param{"ms Delay", "ms delay", 0, MM_CAPTURE, START_CAPTURE};

*/