
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

// #define TIGE2004_20V 1
#define TIGE2006_22VE 1


#if TIGE2003_22V
 
    // 
    // From Tige, for 2003 22V:
    //  Actuator is Lenco 20740-002
    //  Stroke 1.25 inches
    //
    constexpr float ACTUATOR_STROKE_INCHES = 1.25;
    

    //
    // From Lenco:
    //  "The actuators are reverse polarity and all of them move 1 inch every 2 seconds"
    //
    //  However, i have observed it runs at 1.6 seconds per inch
    //
    constexpr float ACTUATOR_SECONDS_PER_INCH = 1.6; //2.0;

#elif TIGE2004_20V

    // 
    // From a fellow owner, these seem to be the parameters for a 2004 20V
    //  Actuator stroke is 2.25 inches
    //
    constexpr float ACTUATOR_STROKE_INCHES = 2.25;

    //
    // From Lenco:
    //  "The actuators are reverse polarity and all of them move 1 inch every 2 seconds"
    //
    //  However, i have observed it runs at 1.6 seconds per inch
    //
    constexpr float ACTUATOR_SECONDS_PER_INCH = 2.2;

#elif TIGE2006_22VE

    // 
    // From a fellow owner, these seem to be the parameters for a 2006 22Ve
    //  Actuator is a Bennett TIGSA4015C
    //
    constexpr float ACTUATOR_STROKE_INCHES = 2.625;

    //
    // From Bennett:
    //
    //  Actuator is a Bennett TIGSA4015C stroke is 2.625 inches, which
    //  "should be able to run frmo full up to full down in about 5 seconds with a 10' or less
    //    hydraulic line"
    //
    constexpr float ACTUATOR_SECONDS_PER_INCH = 5 / 2.625 ;

#else

    #pragma GCC error "Must define type of boat!"

#endif

const int ACTUATOR_FULL_TRANSIT_MS = int( ACTUATOR_STROKE_INCHES * ACTUATOR_SECONDS_PER_INCH * 1000 );
