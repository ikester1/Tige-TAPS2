
#pragma once

#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "hardware/i2c.h"
#include "math.h"
#include "hal.hpp"
#include <stdarg.h>

//
// Read a character from stdin, set 'success' accordingly
//
inline int getcharTimeout( bool& success, uint32_t timeout_us = 60*1000000 ) {
	auto ch = getchar_timeout_us( timeout_us );
    success = (ch != PICO_ERROR_TIMEOUT);
    if( success )
        putchar_raw( ch );
    return ch;
}

//
// Make sure user is sure, return true if so
//
inline bool areYouSure( const char * const prompt = nullptr... ) {
    if( prompt != nullptr ) {
        va_list args;
        va_start( args, prompt );
        vprintf( prompt, args );
        va_end(args);
        putchar(' ');
    }
    printf( "really (y/n)? " );
    bool success;
    auto ch = getcharTimeout( success );
    putchar('\n');
    return success && (ch == 'y' || ch == 'Y' );
}

//
// Get a decimal value from stdin, set 'success' accordingly
//
inline int getDecimal( bool& success, const char * const prompt = nullptr... ) {
	int value = 0, sign = 1;
	success = false;

    if( prompt != nullptr ) {
        va_list args;
        va_start( args, prompt );
        vprintf( prompt, args );
        va_end(args);
        printf(": ");
    }

	auto ch = getcharTimeout(success);

	//
	// Interpret an optional sign character
	//
	if( ch == '-' || ch == '+' ) {
		sign = (ch == '-') ? -1 : +1;
		ch = getcharTimeout(success);
	}
	
	while( success && (ch >= '0' && ch <= '9') ) {
		value *= 10;
		value += ch - '0';
		ch = getcharTimeout( success );
	}
	
	if( success && (ch == '\r') )
        putchar_raw('\n');

	return value * sign;
}

inline void hexdump( const void * ptr, int buflen, const char *fmt = nullptr... ) {
    if( fmt != nullptr ) {
        va_list args;
        va_start( args, fmt );
        vprintf( fmt, args );
        va_end(args);
    }

    auto *buf = (const uint8_t *)ptr;
    int i, j;
    for (i=0; i<buflen; i+=16) {
        printf("%06x: ", i);
        for (j=0; j<16; j++) 
            if (i+j < buflen)
                printf( i+j < buflen ? "%02x " : "    ", buf[i+j]);
        printf(" ");
        for (j=0; j<16; j++) 
            if (i+j < buflen)
                printf("%c", (buf[i+j] >= ' ' && buf[i+j] <= '~') ? buf[i+j] : '.');
        printf("\n");
    }
}

//
// This can be added to class definitions to ensure instances aren't accidentally
//  copied around
//
class NonCopyable {
    NonCopyable(const NonCopyable &);
    NonCopyable& operator=(const NonCopyable &);
protected:
    NonCopyable() {}
};

class CINTERRUPTS_OFF {
    const uint32_t m_oldState;
public:
    __force_inline CINTERRUPTS_OFF() : m_oldState( save_and_disable_interrupts() ) {}
    __force_inline ~CINTERRUPTS_OFF() { restore_interrupts( m_oldState ); }
};

//
// This is a basic callback timer.  Each one of these makes a new hardware timer, so interrupts
//   can really start flying around!  Use CGlobalTimer whenever possible
//
class COnTimer {
    repeating_timer_t   m_timer;
    const int           m_interval;
    bool                m_running = false;
public:
    COnTimer( int interval_ms ) : m_interval( (interval_ms < 0) ? -interval_ms : interval_ms ) {}
    ~COnTimer() {
        stop();
    }
    bool start() {
        if( !m_running )
            m_running = add_repeating_timer_ms( -m_interval, m_callback, this, &m_timer );
        return m_running;
    }
    void stop() {
        if( m_running ) {
            cancel_repeating_timer( &m_timer );
            m_running = false;
        }
    }
    bool enabled() const {
        return m_running;
    }
    int inteval() const {
        return m_interval;
    }
    bool operator==( bool b ) const { return m_running == b; }
    bool operator=( bool b )        { if( b != m_running ){ if(b) start(); else stop(); } return b; }
protected:
    //
    // Called periodically at interrupt time, return true to keep going and false to stop
    //
    virtual bool onTimer() = 0;

private:
    static bool m_callback( repeating_timer_t *t ) {
        auto instance = static_cast< COnTimer * >( t->user_data );
        return instance->onTimer();
    }

    COnTimer( const COnTimer& other ) = delete;
};


//
// This makes one singular timer that can handle multiple callbacks.  All instances of this use the
//  same hardware timer
//
// Users should derive a private class from CGlobalTimer::COnTick and override the onTick() member.  Then
//  call start(), stop(), etc..  in this derived private class to manage the callback.
//
class CGlobalTimer : private COnTimer {
    typedef COnTimer super;

public:
    static CGlobalTimer& instance();

    class COnTick {
        friend class CGlobalTimer;
        COnTick     *m_next;
        bool        m_inList;
    public:
        COnTick() : m_next( nullptr ), m_inList(false)      {}
        ~COnTick()                                          { stop(); }

        virtual void onTick()               = 0;
        void    start()                     { CINTERRUPTS_OFF intsOff; if( !m_inList) { CGlobalTimer::instance().add( *this ); m_inList = true; } }
        void    stop()                      { CINTERRUPTS_OFF intsOff; if( m_inList ) { CGlobalTimer::instance().remove( *this ); m_inList = false; } }
        bool    enabled() const             { return m_inList; }
        int     msPerTick() const           { return CGlobalTimer::msPerTick(); }
    };

    static int  msPerTick()                 { return 20; }      // how often is each callback called?
    void        add( COnTick& callback );
    bool        remove( COnTick& callback );
private:
    COnTick *m_head;
    int     m_msPerTick;

    CGlobalTimer();
    bool onTimer() override;
};

//
// Base class for operating on physical GPIO pins
//
class CGPIOBase : private NonCopyable {
    const int           m_gpio;         // GPIO pin number
    const bool          m_available;    // Is there really a configured pin?  Or is this fake
    gpio_drive_strength m_strength;     // only valid if output port
    
    CGPIOBase( BoardPin::type_t p ) : m_gpio( HAL::pinNumber(p) ), m_available(HAL::hasPin(p)) {
        if( m_available )
            gpio_init( m_gpio );
    }   
public:
    enum DIR { IN = false, OUT = true };

    inline int pin() const          { return m_gpio; }
    inline bool available() const   { return m_available; }

    CGPIOBase( BoardPin::type_t p, DIR dir ) : CGPIOBase( p ) {
        if( available() )
            gpio_set_dir( m_gpio, bool(dir) );
    }
    CGPIOBase( BoardPin::type_t p, gpio_drive_strength s ) : CGPIOBase(p) {
        if( available() ) {
            m_strength = s;
            gpio_set_drive_strength( m_gpio, m_strength );
            gpio_set_dir( m_gpio, OUT );
        }
    }

    bool write( bool val )      { if( available() )gpio_put( m_gpio, val ); return val; }
    bool setHigh()              { return write( true ); }
    bool setOn()                { return write( true ); }
    bool setLow()               { return write( false ); }
    bool setOff()               { return write( false ); }
    bool toggle()               { return write( !read() ); }

    bool read() const           { return available() ? gpio_get( m_gpio ) : false; }
    void setPullUp()            { if( available() )gpio_pull_up( m_gpio ); }
    void setPullDown()          { if( available() )gpio_pull_down( m_gpio ); }
    void setFloat()             { if( available() )gpio_disable_pulls( m_gpio); }

    gpio_drive_strength getStrength() const     { return m_strength; }
    void setStrength()                          { setStrength( m_strength ); }
    void setStrength( gpio_drive_strength s )   { m_strength = s; if( available() )gpio_set_drive_strength( m_gpio, s ); }

    operator bool() const       { return read(); }
};

//
// An Input GPIO pin
//
class CGPIO_IN : public CGPIOBase {
    typedef CGPIOBase super;
public:
    CGPIO_IN( BoardPin::type_t p, bool enablePullUp = false ) : super( p, super::IN ) { if( enablePullUp ) super::setPullUp(); }
};

//
// An Output GPIO pin
//
class CGPIO_OUT : public CGPIOBase {
    typedef CGPIOBase super;
public:
    CGPIO_OUT( BoardPin::type_t p, bool initialState = false, gpio_drive_strength s = GPIO_DRIVE_STRENGTH_2MA ) : super( p, s ) {
        super::write(initialState);
    }

    virtual bool operator=( bool b )    { return super::write(b); }
};

//
// An LED connected to a GPIO pin
//
class CLED : public CGPIO_OUT {
    typedef CGPIO_OUT super;
public:
    CLED( BoardPin::type_t p, bool initialState = false ) : super( p, initialState, GPIO_DRIVE_STRENGTH_4MA ) {}
    bool operator=(bool b )             { return super::operator=(b); }
};

//
// A GPIO pin emitting a Pulse Width Modulated signal
//
class CPWM : private NonCopyable {
    CGPIO_OUT   &m_gpio;
    uint8_t     m_slice, m_channel;
    uint8_t     m_divider, m_frac;
    uint16_t    m_top;
    uint16_t    m_level;
    float       m_percent = 50;

public:
    CPWM( CGPIO_OUT& g, int hz, bool doEnable = false ) : m_gpio( g )
    {
        //
        // https://www.i-programmer.info/programming/hardware/14849-the-pico-in-c-basic-pwm.html
        //
        const uint32_t sysClk = clock_get_hz( clk_sys );
        uint32_t divider16 = sysClk / hz / 4096 + ((sysClk % ( hz * 4096)) != 0);
        if( divider16/16 == 0 )
            divider16 = 16;

        m_top = uint16_t( (sysClk * 16) / divider16 / hz );

        if( m_gpio.available() ) {
            const auto pin = m_gpio.pin();
            gpio_set_function( pin, GPIO_FUNC_PWM );
            m_slice = pwm_gpio_to_slice_num( pin );
            m_channel = pwm_gpio_to_channel( pin );
            pwm_set_clkdiv_mode( m_slice, PWM_DIV_FREE_RUNNING );

            m_divider = divider16/16;
            m_frac = divider16 & 0x0F;

            pwm_set_clkdiv_int_frac( m_slice, m_divider, m_frac );
            pwm_set_wrap( m_slice, m_top - 1 );
            pwm_set_enabled( m_slice, doEnable );
        } else {
            m_slice = m_channel = m_divider = m_frac = 0;
        }
    }

    ~CPWM()
    {   disable();
        if( m_gpio.available() ) {
            gpio_init( m_gpio.pin() );
            m_gpio.setStrength();
            gpio_set_dir( m_gpio, m_gpio.OUT );
        }
    }

    CPWM& setPercent( const float percent ) {
        m_percent = MIN( percent, 100 );
        m_percent = MAX( m_percent, 0 );

        if( m_percent <= 0 )
            m_level = 0;
        else if( m_percent >= 100 )
            m_level = m_top-1;
        else
            m_level = uint16_t( (m_percent * m_top / 100) + 0.5f) - 1;

        if( m_gpio.available() ) {
            pwm_set_chan_level( m_slice, m_channel, m_level );
            enable();
        }
        return *this;
    }
    float getPercent() const            { return m_percent; }

    virtual void enable()               { if( m_gpio.available() ) { m_gpio.setOn(); pwm_set_enabled( m_slice, true ); } }
    virtual void disable()              { if( m_gpio.available() ) { m_gpio.setOff(); pwm_set_enabled( m_slice, false ); } }

    void print() const {
        printf( "PWM pin %d: slice %u, divider %u.%u, top %u, percent %.2f%%, level %u\n",
            m_gpio.pin(), m_slice, m_divider, m_frac, m_top, getPercent(), m_level );
    }
};

//
// This class makes the GPIO output port's PWM cycle between 'low Percent' duty and 'high Percent' duty cycle
//   every secPerCycle seconds
//
class CPWMCycler : private NonCopyable {
    CPWM            &m_pwm;
    const float     m_secPerCycle;
    float           m_lowPercent = 0;
    float           m_highPercent = 100;
    int             m_ticksPerPhase;
    float           m_deltaPerTick;
    bool            m_increasing = true;

    struct myTick : public CGlobalTimer::COnTick {
        CPWMCycler&     m_cycler;
        void onTick() override                  { m_cycler.onTick(); }
        myTick( CPWMCycler& p ) : m_cycler(p)   {}
    } m_myTick;

    CPWMCycler& setDeltaPerTick() {
        m_deltaPerTick = (m_highPercent - m_lowPercent) / m_ticksPerPhase;
        return *this;
    }

public:
    CPWMCycler( CPWM& pwm, float secPerCycle = 2 ) :
        m_pwm(pwm), m_secPerCycle( secPerCycle ), m_myTick( *this ) {

        m_ticksPerPhase = int( ((1000 / m_myTick.msPerTick()) * secPerCycle) / 2 );
        setDeltaPerTick();
        }

    void enable()       { m_pwm.enable(); m_myTick.start(); }
    void disable()      { m_pwm.disable(); m_myTick.stop(); }

    CPWMCycler& setPercents( float lowPercent, float highPercent ) {
            if( lowPercent < highPercent ) {
                m_lowPercent = lowPercent;
                m_highPercent = highPercent;
            } else if( highPercent < lowPercent ) {
                m_lowPercent = highPercent;
                m_highPercent = lowPercent;
            }
            setDeltaPerTick();
            return *this;
    }

    void onTick() {
        auto percent = m_pwm.getPercent();
        if( m_increasing ) {
            if( (percent += m_deltaPerTick) >= m_highPercent ) {
                percent = m_highPercent;
                m_increasing = false;
            }
        } else {
            if( (percent -= m_deltaPerTick) <= m_lowPercent ) {
                percent = m_lowPercent;
                m_increasing = true;
            }
        }
        m_pwm.setPercent( percent );
    }
};


//
// A debounced pushbutton.  GPIO is set to input w/pullup.  When reads 0, the switch is closed
//
// NOTE: call enable() to get the switch going as it is initialized **disabled**
//
class CButton : private NonCopyable {
    CGPIO_IN    m_gpio;
    uint8_t     m_switchState;

    absolute_time_t		m_whenPressed;      // timestamp when button first pressed
    int                 m_ms;               // how long was the switch pressed?

    struct myTick : public CGlobalTimer::COnTick {
        CButton &m_btn;
        void onTick() override                      { m_btn.onTick(); }
        myTick( CButton &btn ) : m_btn( btn )       {}
    } m_myTick;

public:
    CButton( BoardPin::type_t p, bool enablePullup = true ) : m_gpio( p ), m_ms(0), m_myTick(*this) { if( enablePullup ) m_gpio.setPullUp(); }

    void enable()           { m_switchState = 0xF; m_myTick.start(); }
    void disable()          { m_switchState = 0xF; m_myTick.stop(); }

    //
    // Return the present debounced button state
    //
    bool pressed() const    { return m_switchState == 0; }
    bool released() const   { return m_switchState == 0x0F; }

    void waitForPressed() const         { while( !pressed() ) __wfi(); }

    void waitForReleased() const        { while( !released() ) __wfi(); }
    bool waitForReleased( int maxMS )   { while( !released() && ms() < maxMS ){ __wfi(); } return released(); }

    bool strobe() const     { return m_gpio == false; }     // true if currently depressed, false otherwise

    //
    // Return the length of time the button is or was pressed
    //
    int ms() const {
        if( pressed() )
            return int( absolute_time_diff_us( m_whenPressed, get_absolute_time() ) / 1000 );
        else
            return m_ms;
    }

    //
    // Derive from this class and override this function to get callbacks when debounced button state changes
    //  WARNING:  called at interrupt time!
    //
    virtual void onChange( bool b ) { (void)b; }

    CGPIO_IN& gpio()    { return m_gpio; }

private:
    void onTick() {
        const uint8_t newState = ((m_switchState << 1) | m_gpio.read()) & 0x0F;
        if( newState != m_switchState ) {
            m_switchState = newState;
            if( released() ) {
                m_ms = m_whenPressed != nil_time ? int( absolute_time_diff_us( m_whenPressed, get_absolute_time() ) / 1000 ) : 0;
                onChange( false );
            } else if( pressed() ) {
                m_whenPressed = get_absolute_time();
                onChange( true );
            }
        }
    }
};

//
// A rocker switch.  Switch is initialized as input w/ pullup.  Reading of 0 means
//   switch is closed.
//
// NOTE: call enable() to get the switch going as it is initialized **disabled**
//
class CSPDT : private NonCopyable {
    CGPIO_IN    m_top, m_bottom;

    union _switchState {
        _switchState( uint8_t val = 0xFF ) : all(val) {}
        struct {
            uint8_t topBits : 4;
            uint8_t botBits : 4;
        };
        uint8_t     all;
    } m_switchState;

    bool        m_wasTop = false, m_wasBottom = false;

    struct myTick : public CGlobalTimer::COnTick {
        CSPDT &m_switch;
        void onTick() override                      { m_switch.onTimer(); }
        myTick( CSPDT &s ) : m_switch( s )          {}
    } m_myTick;

public:
    CSPDT( BoardPin::type_t t, BoardPin::type_t b) :
        m_top( t ),
        m_bottom( b ),
        m_myTick( *this )
    {
        m_top.setPullUp();
        m_bottom.setPullUp();
    }

    void enable() {
        m_switchState.all = 0xFF;
        m_myTick.start();
    }
    void disable() {
        m_myTick.stop();
    }

    //
    // Return true if the top or bottom portions of the switch are pressed
    //
    bool top() const        { return m_switchState.topBits == 0; }
    bool bottom() const     { return m_switchState.botBits == 0; }

    bool operator==( bool b ) const { return (top() || bottom()) == b; }

    //
    // Derive from this class and override one or both of these to detect switch changes.
    //   WARNING:  they are called at interrupt time
    //
    virtual void onTop( bool pressed )      { (void)pressed; }
    virtual void onBottom( bool pressed )   { (void)pressed; }
private:
    void onTimer() {
        const auto oldState = m_switchState;

        _switchState newState;
        newState.topBits = (oldState.topBits << 1) | m_top.read();
        newState.botBits = (oldState.botBits << 1) | m_bottom.read();

        if( oldState.all != newState.all ) {
            m_switchState = newState;

            if( newState.topBits != oldState.topBits ) {
                if( newState.topBits == 0 || newState.topBits == 0x0F ) {
                    const bool isTop = (newState.topBits == 0);
                    if( isTop != m_wasTop ) {
                        if( m_wasBottom )
                            onBottom( m_wasBottom = false );
                        onTop( m_wasTop = isTop );
                    }
                }

            } else if( newState.botBits != oldState.botBits ) {
                if( newState.botBits == 0 || newState.botBits == 0x0F ) {
                    const bool isBottom = (newState.botBits == 0);
                    if( isBottom != m_wasBottom ) {
                        if( m_wasTop )
                            onTop( m_wasTop = false );
                        onBottom( m_wasBottom = isBottom );
                    }
                }
            }
        }
    }
};

class CI2C : private NonCopyable {
    const uint8_t       m_7BitAddr;
    const bool          m_available;
    const uint          m_sdaPin;
    const uint          m_sclPin;
    i2c_inst_t * const  m_i2c;

    static constexpr uint timeout_us = 1000000;

    void showError( const char *format... );
    bool reservedAddr( uint8_t addr ) const;

public:
    CI2C( uint8_t sevenBitAddr, BoardPin::type_t sdaPin, uint hz = 100 * 1000 );
    ~CI2C();

    inline bool available() const   { return m_available; }

    //
    // Sweep through all 7-bit addresses to see if any slaves are present.  Print
    //  a table of located devices
    //
    void scan();

    //
    // Write 'bytesToWrite' to the slave, then turn around and read 'bytesToRead'.
    //  No bytes are written if 'bytesToWrite' is zero
    //  No bytes are read if 'bytesToRead' is zero
    //  STOP is issued after the operation
    //
	bool writeRead( const void *writeData, uint8_t bytesToWrite, void *readData, uint16_t bytesToRead );

    //
    // Write 'writeData1' of 'bytesToWrite1' to the slave, followed by 'writeData2' of 'bytesToWrite2'.
    //   If 'sendStop' then issue a stop when the writes are complete.
    //
    // Either 'bytesToWrite1' or 'bytesToWrite2' can be zero
    //
	bool writeWrite( const void *writeData1, uint8_t bytesToWrite1, const void *writeData2, uint16_t bytesToWrite2, bool sendStop );

	//
	// These utility routines help read/write values to/from I2C slave registers
	//
	bool readRegister( uint8_t regNo, void *outputBuffer, uint16_t bytes ) {
		return writeRead( &regNo, 1, outputBuffer, bytes );
	}
	bool writeRegister( uint8_t regNo, const void *buffer, uint8_t bytes ) {
		return writeWrite( &regNo, 1, buffer, bytes, true );
	}
};