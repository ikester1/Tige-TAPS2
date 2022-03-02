#include <stdio.h>
#include <memory.h>
#include "pico/stdlib.h"

#include <cmath>

#include "config.h"
#include "util.hpp"
#include "CGauge.hpp"
#include "CNVFlash.hpp"
#include "CNVFRAM.hpp"
#include "taps.hpp"
#include "CActuator.hpp"
#include "CPowerFail.hpp"

void configure( CNVState&, CLED& statusLED, CButton& button, CSPDT& trimSwitch, CActuator&, CGauge& );
void demoMode( CGauge&, CButton& stopButton );
void doCommand( int ch, CNVState& );

static CNVState& findNVResource() {
    static CNVFRAM nvFRAM( I2C_ADDR::FRAM, BoardPin::FRAM_SDA );
    if( nvFRAM.size() > 0 )
        return nvFRAM;

    static CNVFlash nvFlash;
    return nvFlash;
}

int main()
{
    stdio_init_all();

    CNVState&   nvState = findNVResource();
    CLED        picoLED( BoardPin::STANDARD_LED );
    CLED        statusLED( BoardPin::STATUS_LED );
    CPowerFail  powerFail;
    CActuator   actuator( ACTUATOR_FULL_TRANSIT_MS );
    CGauge      gauge( nvState.gaugeCal().get(), GAUGE_PWM_FREQ );

    bool        movedTrimSinceLastSave = false;
    int         numberOfTrimMovements  = 0;

    //
    // This object posts "TRIM_SWITCH_xxx" messages as the trim switch is manipulated
    //
    class trimSwitch : public CSPDT {
        typedef CSPDT   super;
        CGPIO_OUT       m_switchEnable;     //  Must be set to output high on boards with optical switch isolators
    public:
        trimSwitch() :
        super( BoardPin::TRIM_SWITCH_EXTEND, BoardPin::TRIM_SWITCH_RETRACT ),
        m_switchEnable( BoardPin::TRIM_SWITCH_ENABLE )
        {
            m_switchEnable.setOn();
        }

        void onTop(bool pressed) {
            if( auto msg = CMessage::alloc( pressed ? CMessage::Type::TRIM_TOP_ON : CMessage::Type::TRIM_OFF ) )
                msg->push();
        }
        void onBottom(bool pressed) {
            if( auto msg = CMessage::alloc( pressed ? CMessage::Type::TRIM_BOTTOM_ON : CMessage::Type::TRIM_OFF ) )
                msg->push();
        }
    } spdt;

    //
    // This object posts a "CONFIG_BUTTON_ON" message when the configure pushbutton is pressed
    //
    class ConfigButton : public CButton {
        typedef     CButton super;
        bool        m_messages;
    public:
        ConfigButton() : super( BoardPin::CONFIG_PUSHBUTTON ), m_messages( false )  {}
        void onChange( bool b ) override {
            if( b && m_messages )
                if( auto msg = CMessage::alloc( CMessage::Type::CONFIG_BUTTON_ON ) )
                    msg->push();
        }
        void enableMessages()       { super::enable(); m_messages = true; }
        void disableMessages()      { m_messages = false; }
    } configButton;
    configButton.enable();          // no messages yet

    //
    // This object posts a "HEARTBEAT" message once per second, and blinks the pico led
    //
    class heartBeat {
        struct myTick : public CGlobalTimer::COnTick {
            heartBeat &m_me;
            void onTick() override                      { m_me.onTick(); }
            myTick( heartBeat &m ) : m_me( m )          {}
        } m_myTick;

        const int   m_interval;
        int         m_count;
        CLED        &m_led;                 // LED to toggle every m_interval
        bool        m_msg;                  // should we send message at m_interval?

    public:
        heartBeat( int interval, CLED& led ) : m_myTick(*this), m_interval( interval ), m_count(0), m_led(led), m_msg(false) {}
        void onTick()           {   if( --m_count <= 0 ) {
                                        m_led.toggle();
                                        m_count = m_interval;
                                        if( m_msg )
                                            if( auto msg = CMessage::alloc( CMessage::Type::HEARTBEAT ) )
                                                msg->push();
                                    }
        
                                };

        bool enableMessages()           { auto oval = m_msg; m_msg = true; m_myTick.start(); return oval; }
        bool disableMessages()          { auto oval = m_msg; m_msg = false; return oval; }

    } heartBeat( 1000/CGlobalTimer::msPerTick(), picoLED );

    gauge.enable();

    if( configButton.strobe() == true ) {
        //
        // Config button is pressed during power up....enter demo mode which sweeps the gauge dial
        //
        configButton.waitForPressed();
        demoMode( gauge, configButton );
    }

    float recoveredActuatorPercent = NAN;
    if( nvState.reason().wasValid() && nvState.actuatorPercent().wasValid() ) {
        if( nvState.reason().get() == CNVState::PowerDownSave || nvState.reason().get() == CNVState::SavedPositionImmediate ) {
            //
            // We were able to save and recover the last actuator postion, so we assume it is still at that position.
            //   All we need to do is let the actuator know where it is
            //
            recoveredActuatorPercent = nvState.actuatorPercent().get();
            actuator.setAlreadyAtPercent( recoveredActuatorPercent );
            spdt.enable();
            configButton.enableMessages();
            gauge.setSlow( 100 );
        }
    }

    if( isnan( recoveredActuatorPercent ) ) {
        //
        // Don't know where the actuator is/was, so best we can do is reset it
        //
	    actuator.startFullRetract();
    }

    heartBeat.enableMessages();
    gauge.setSlow( actuator.percent() );

    while (true) {
        auto msg = CMessage::pop();
        if( msg == nullptr ) {
            if( int ch = getchar_timeout_us(0); ch != PICO_ERROR_TIMEOUT ) {
                if( actuator.active() == false ) {
                    if( msg = CMessage::alloc( CMessage::Type::USER_COMMAND, ch ); msg != nullptr )
                        msg->push();
                }
                continue;
            }
            __wfi();
            continue;
        }

        switch( msg->type() ) {
        case CMessage::Type::TRIM_TOP_ON:
            movedTrimSinceLastSave = true;
            ++numberOfTrimMovements;
            statusLED = true;
            actuator.retract();
            configButton.disableMessages();
            break;

        case CMessage::Type::TRIM_BOTTOM_ON:
            movedTrimSinceLastSave = true;
            ++numberOfTrimMovements;
            statusLED = true;
            actuator.extend();
            configButton.disableMessages();
            break;

        case CMessage::Type::TRIM_OFF:
            statusLED = false;
            actuator.stop();
            gauge.set( actuator.percent() );

            if( nvState.unlimitedUpdates() ) {
                nvState.setActuatorPercent( actuator.percent(), CNVState::SavedPositionImmediate ).commit();
                movedTrimSinceLastSave = false;
            } else if( numberOfTrimMovements == 1 ) {
                nvState.setActuatorPercent( actuator.percent(), CNVState::InitialMovement ).commit();
            }

            configButton.enableMessages();
            break;

        case CMessage::Type::USER_COMMAND:
        case CMessage::Type::CONFIG_BUTTON_ON:
            configButton.disableMessages();
            heartBeat.disableMessages();
            CMessage::flush();
            statusLED = true;

            if( msg->type() == CMessage::Type::CONFIG_BUTTON_ON )
                configure( nvState, statusLED, configButton, spdt, actuator, gauge );
            else
                doCommand( msg->data(), nvState );

            statusLED = false;
            CMessage::flush();
            configButton.enableMessages();
            heartBeat.enableMessages();
            break;

        case CMessage::Type::GAUGE_UPDATE:
            { const auto percentUnbounded = actuator.percentUnbounded();
                if( percentUnbounded < -50 || percentUnbounded > 150 ) {
                    statusLED = false;
                    actuator.stop();
                }
            }
            gauge.set( actuator.percent() );
            break;

        case CMessage::Type::FULL_RETRACT:
            statusLED = true;
            if( actuator.activeTime() >= actuator.targetRullRetractRunTime() ) {
                actuator.stop();
                sleep_ms( 100 );
                if( nvState.reason().get() == CNVState::SavedPositionLazy && !nvState.closeEnough( actuator.percent() ) ) {
                        gauge.setSlow( actuator.percent() );
                        if( nvState.actuatorPercent().get() >= 0 && nvState.actuatorPercent().get() <= 100 ) {
                            actuator.extend( false );
                            for( auto pct = actuator.percent(); !nvState.closeEnough(pct) && pct < nvState.actuatorPercent().get(); pct = actuator.percent() )
                                gauge.set( pct );
                            actuator.stop();
                        }
                }
                gauge.setSlow( actuator.percent() );
                statusLED = false;
                spdt.enable();
                msg->flush();
                configButton.enableMessages();
            } else {
                //
                // We want a sweep of the TAPS gauge while the actuator is doing its initial retraction
                //
                const static auto ticksForRetract = float( actuator.targetRullRetractRunTime() ) / actuator.msPerGaugeTick();
                const static auto deltaPercentPerTick = 2 * (100 / ticksForRetract);
                static bool gaugeSweepIncreasing = true;

                auto currentGauge = gauge.get();
                if( currentGauge >= 99 )
                    gaugeSweepIncreasing = false;
                else if( currentGauge <= 1 )
                    gaugeSweepIncreasing = true;

                gauge.set( currentGauge + (gaugeSweepIncreasing ? deltaPercentPerTick : -deltaPercentPerTick) );
            }
            break;

        case CMessage::Type::HEARTBEAT:
            if( nvState.unlimitedUpdates() == false && !powerFail.available() && spdt == false ) {
                if( movedTrimSinceLastSave && actuator.secondsSinceLastStop() >= ACTUATOR_POSITION_SAVE_DELAY_SEC ) {
                    movedTrimSinceLastSave = false;
                    if( !nvState.closeEnough( actuator.percent() ) ) {
                        statusLED.toggle();
                        nvState.setActuatorPercent( actuator.percent(), CNVState::SavedPositionLazy ).commit();
                        sleep_ms(100);
                        statusLED.toggle();
                    }
                }
            }
            break;

        case CMessage::Type::POWER_FAILED:
            actuator.stop();
            gauge.disable();
            printf( "*** POWER FAILURE ***\n");
            configButton.disableMessages();
            heartBeat.disableMessages();
            CMessage::flush();
            picoLED = true;
            statusLED = true;
            if( movedTrimSinceLastSave ) {
                nvState.setActuatorPercent( actuator.percent(), CNVState::PowerDownSave ).commit();
                movedTrimSinceLastSave = false;
                numberOfTrimMovements = 0;
            }
            break;

        case CMessage::Type::POWER_RESTORED:
            printf( "*** POWER RESTORED ***\n" );
            statusLED = false;
            picoLED = false;
            {
                auto currentGauge = gauge.get();
                gauge.set(0).enable().setSlow( currentGauge );
            }
            configButton.enableMessages();
            heartBeat.enableMessages();
            break;

        default:
            msg->print();
            break;
        }
        msg->free();
    }
    return 0;       // never gets here, but...
}

//
// We use the trim switch to set the 0, 25, 50, 75, and 100% points on the gauge.  We use
// the button presses to move to the next percentile.
//
// If the user presses and holds the config button, we abort
//
// If the user doesn't do anything for CONFIG_INACTIVITY_ABORT_SEC seconds, we abort
//
void calibrateGauge( CNVState& nvState, CButton& button, CSPDT& trimSwitch, CGauge& gauge ) {
    CGauge::calType_t gaugeCal( nvState.gaugeCal().get() );

    nvState.print();
    gauge.setCurrentDutyCycleSlow( gaugeCal[0] );
    button.waitForReleased();

    uint slotNumber = 0;
    auto lastActivityTime = get_absolute_time();

    while( absolute_time_diff_us( lastActivityTime, get_absolute_time() ) / 1000000 < CONFIG_INACTIVITY_ABORT_SEC ) {

        while( trimSwitch.bottom() ) {
            gauge.setCurrentDutyCycle( gauge.currentDutyCycle() - 0.25f );
            sleep_ms( 25 );
            lastActivityTime = get_absolute_time();
        }
        while( trimSwitch.top() ) {
            gauge.setCurrentDutyCycle( gauge.currentDutyCycle() + 0.25f );
            sleep_ms( 25 );
            lastActivityTime = get_absolute_time();
        }
        if( button.pressed() ) {
            if( button.waitForReleased( CONFIG_ABORT_MS ) == false ) {
                //
                // Button is being held down.....abort!
                //
                return;
            }

            gaugeCal[ slotNumber ] = gauge.currentDutyCycle();
            printf("  PWM[%d] = %.2f%%\n", slotNumber, gaugeCal[ slotNumber ] );
            if( ++slotNumber >= gaugeCal.size() )
                break;

            lastActivityTime = get_absolute_time();
        }
        __wfi();
    }

    if( !gauge.isValidCalibration( gaugeCal ) ) {
        printf( "\n** REJECTED!\n");
        return;
    }

    nvState.setGaugeCal( gaugeCal ).commit();
    gauge.calibrate( gaugeCal );
}

//
// Make the status LED pulsate and let the user calibrate the trim gauge.  Retract the actuator along
//   the way to allow the user to verify the actuator leads are correct (i.e. they may be reversed)
//
void configure( CNVState& nvState, CLED& statusLED, CButton& button, CSPDT& trimSwitch, CActuator& actuator, CGauge& gauge ) {
    const auto initialActuatorPercent = actuator.percent();
    {
        //
        // Fully retract the actuator so the owner can know the motor is hooked up right
        //
        actuator.startFullRetract(false);
        while( actuator.activeTime() < actuator.targetRullRetractRunTime() )
            ;
        actuator.stop();

        //
        // Let the owner adjust the gauge settings
        //
        CPWM statusPWM( statusLED, 1000 );
        CPWMCycler ledCycler( statusPWM, 2 );
        ledCycler.setPercents( 5, 100 ).enable();
        calibrateGauge( nvState, button, trimSwitch, gauge );
    }
    statusLED = false;

    //
    // Restore the actuator to its initial position
    //
    if( actuator.percent() != initialActuatorPercent ) {
        actuator.extend( false );
        while( actuator.percent() < initialActuatorPercent )
            ;
        actuator.stop();
    }

    //
    // Set the gauge to the actuator position
    //
    gauge.setSlow( actuator.percent() );

    button.waitForReleased();
}

//
// Enter "demo mode".  Demo mode continuously sweeps the gauge dial until the stop button is pressed
//
void demoMode( CGauge& gauge, CButton& stopButton ) {
    auto origPercent = gauge.enable().get();
    {
        CPWMCycler cycler( gauge.pwm(), 1.5*ACTUATOR_FULL_TRANSIT_MS/1000.0 );
        cycler.setPercents( gauge.getLowPercent(), gauge.getHighPercent() ).enable();

        while( !stopButton.released() )
            __wfi();
        while( !stopButton.pressed() )
            __wfi();
    }
    gauge.setSlow( origPercent );
    stopButton.waitForReleased();
}

void doCommand( int cmd, CNVState& nvState ) {
    if( cmd != '\r' ) {
        switch( cmd = tolower(cmd) ) {
        case 'z':               // zap the NV state
            if( areYouSure( "\nErase all remembered settings...") ) {
                nvState.zap();
                printf( "\nzapped!\n");
            }
            break;

        case 'p':               // print the non-volatile storage state
            nvState.print();
            break;

        default:              // fool with nonvolatile's i2c
            if( !nvState.doCommand( cmd ) )
                putchar('?');
            break;
        }
    }
    printf("\n> ");
}