//
// How long must the actuator remain in a changed position until we save the setting?
//    Too short, and we might wear out the flash
//    Too long, and it becomes useless
//
const int ACTUATOR_POSITION_SAVE_DELAY_SEC = 15;

//
// How long do you need to press CONFIG_PUSHBUTTON_PIN to abort the configuration?
//
const int CONFIG_ABORT_MS = 2000;

//
// How many seconds of inactivity during configuration mode we we allow before
//   aborting?
//
const int CONFIG_INACTIVITY_ABORT_SEC = 60;

//
// What PWM frequency do we use to run the actuator gauge?
//
const int GAUGE_PWM_FREQ = 30000;

// 
// From Tige:
//  Actuator is Lenco 20740-002
//  Stroke 1.25 inches
//
const float ACTUATOR_STROKE_INCHES = 1.25;

//
// From Lenco:
//  "The actuators are reverse polarity and all of them move 1 inch every 2 seconds"
//
//  However, i have observed it runs at 1.6 seconds per inch
//
constexpr float ACTUATOR_SECONDS_PER_INCH = 1.6; //2.0;

const int ACTUATOR_FULL_TRANSIT_MS = int( ACTUATOR_STROKE_INCHES * ACTUATOR_SECONDS_PER_INCH * 1000 );