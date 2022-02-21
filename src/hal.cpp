#include <stdio.h>
#include "pico/stdlib.h"

#include    "util.hpp"


namespace HAL {

//
// VERSION_B0, VERSION_B1, and VERSION_B2 are selectively grounded to indicate the board version
//
uint boardVersion() {
    static uint version = uint(-1);

    if( version == uint(-1) ) {
        CGPIO_IN b0( BoardPin::VERSION_B0, true ), b1( BoardPin::VERSION_B1, true ), b2( BoardPin::VERSION_B2, true );
        
        version &= ~0x07;
        version |= b0.read();
        version |= b1.read() << 1;
        version |= b2.read() << 2;
        version = ~version;

        //
        // Reduce current draw
        //
        b0.setPullDown();
        b1.setPullDown();
        b2.setPullDown();
    }

    return version;
}

//
// This maps BoardPin to a physical pin number, likely using a version number on the physical board
//
int pinNumber( BoardPin::type_t p ) {

    switch( p ) {
        case BoardPin::VERSION_B0:
            return 2;
        case BoardPin::VERSION_B1:
            return 1;
        case BoardPin::VERSION_B2:
            return 0;
        case BoardPin::STANDARD_LED:
            return PICO_DEFAULT_LED_PIN;
    }

    if( auto version = boardVersion(); version == 0 ) {
        switch( p ) {
            case BoardPin::STATUS_LED:          return 15;
            case BoardPin::TRIM_SWITCH_EXTEND:  return 11;      // switch top
            case BoardPin::TRIM_SWITCH_RETRACT: return 12;      // switch bottom
            case BoardPin::CONFIG_PUSHBUTTON:   return 14;
            case BoardPin::MOTOR_RPWM:          return 16;
            case BoardPin::MOTOR_LPWM:          return 17;
            case BoardPin::MOTOR_ENABLE:        return 18;
            case BoardPin::GAUGE_PWM:           return 9;
            case BoardPin::GAUGE_ENABLE:        return 5;
        }

    } else if( version == 1) {
        switch( p ) {
            case BoardPin::STATUS_LED:          return 13;
            case BoardPin::TRIM_SWITCH_EXTEND:  return 11;      // switch top
            case BoardPin::TRIM_SWITCH_RETRACT: return 12;      // switch bottom
            case BoardPin::CONFIG_PUSHBUTTON:   return 14;
            case BoardPin::MOTOR_RPWM:          return 16;
            case BoardPin::MOTOR_LPWM:          return 17;
            case BoardPin::MOTOR_ENABLE:        return 18;
            case BoardPin::GAUGE_PWM:           return 9;
            case BoardPin::GAUGE_ENABLE:        return 5;
        }

    } else if( version == 2 ) {
        switch( p ) {
            case BoardPin::STATUS_LED:          return 12;
            case BoardPin::TRIM_SWITCH_ENABLE:  return 15;
            case BoardPin::TRIM_SWITCH_EXTEND:  return 13;      // switch top
            case BoardPin::TRIM_SWITCH_RETRACT: return 14;      // switch bottom
            case BoardPin::CONFIG_PUSHBUTTON:   return 11;
            case BoardPin::MOTOR_RPWM:          return 16;
            case BoardPin::MOTOR_LPWM:          return 18;
            case BoardPin::MOTOR_ENABLE:        return 17;
            case BoardPin::GAUGE_PWM:           return 8;
            case BoardPin::GAUGE_ENABLE:        return 3;
            case BoardPin::POWER_FAIL:          return 21;
        }

    } else if( version == 4 ) {
            switch( p ) {
            case BoardPin::STATUS_LED:          return 12;
            case BoardPin::TRIM_SWITCH_ENABLE:  return 15;
            case BoardPin::TRIM_SWITCH_EXTEND:  return 13;      // switch top
            case BoardPin::TRIM_SWITCH_RETRACT: return 14;      // switch bottom
            case BoardPin::CONFIG_PUSHBUTTON:   return 11;
            case BoardPin::MOTOR_RPWM:          return 17;
            case BoardPin::MOTOR_LPWM:          return 18;
            case BoardPin::MOTOR_ENABLE:        return 16;
            case BoardPin::GAUGE_PWM:           return 10;
            case BoardPin::GAUGE_ENABLE:        return 7;
            case BoardPin::FRAM_SDA:            return 26;
        }

    } else if( version == 5 ) {
            switch( p ) {
            case BoardPin::STATUS_LED:          return 12;
            case BoardPin::TRIM_SWITCH_ENABLE:  return 15;
            case BoardPin::TRIM_SWITCH_EXTEND:  return 13;      // switch top
            case BoardPin::TRIM_SWITCH_RETRACT: return 14;      // switch bottom
            case BoardPin::CONFIG_PUSHBUTTON:   return 11;
            case BoardPin::MOTOR_RPWM:          return 18;
            case BoardPin::MOTOR_LPWM:          return 17;
            case BoardPin::MOTOR_ENABLE:        return 16;
            case BoardPin::GAUGE_PWM:           return 10;
            case BoardPin::NOT_GAUGE_ENABLE:    return 19;
            case BoardPin::FRAM_SDA:            return 26;
        }
    }

    return -1;
}

}