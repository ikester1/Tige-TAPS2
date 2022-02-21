#pragma once

namespace BoardPin {
    //
    // These are "virtual" pin assignments, and should be used in the general
    //   program.  HAL::pinNumber() maps the virtual assignment to the physical
    //   assignment, thereby letting one piece of code run on various versions
    //   of the board
    //
    typedef uint type_t;

    //
    // version bits .. must be hardware independent!
    //
    constexpr type_t VERSION_B0             = 2;
    constexpr type_t VERSION_B1             = 1;
    constexpr type_t VERSION_B2             = 0;

    //
    // The mapped value of these can move around on different boards
    //
    constexpr type_t STANDARD_LED           = 100;
    constexpr type_t STATUS_LED             = 101;
    constexpr type_t TRIM_SWITCH_ENABLE     = 102;
    constexpr type_t TRIM_SWITCH_EXTEND     = 103;
    constexpr type_t TRIM_SWITCH_RETRACT    = 104;
    constexpr type_t CONFIG_PUSHBUTTON      = 105;
    constexpr type_t MOTOR_RPWM             = 106;
    constexpr type_t MOTOR_LPWM             = 107;
    constexpr type_t MOTOR_ENABLE           = 108;
    constexpr type_t GAUGE_PWM              = 109;

    //
    // Either GAUGE_ENABLE or NOT_GAUGE_ENABLE are available, not both
    //
    constexpr type_t GAUGE_ENABLE           = 110;
    constexpr type_t NOT_GAUGE_ENABLE       = 111;

    constexpr type_t POWER_FAIL             = 112;

    constexpr type_t FRAM_SDA               = 113;
}

namespace I2C_ADDR {
    constexpr uint FRAM = 0x50;
}

namespace HAL {

//
// This returns the hardware revision level
//
uint boardVersion();

//
// This maps BoardPin to a physical pin number, likely with an assist from boardVersion()
//
// A pinNumber of -1 means "not available"
//
int pinNumber( BoardPin::type_t p );

inline bool hasPin( BoardPin::type_t p )       { return pinNumber( p ) != -1; }

}