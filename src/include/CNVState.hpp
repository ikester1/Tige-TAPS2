#pragma once

//
// This encapsulates our non-volatile storage for items such as actuator position
//   and gauge calibration.  It might be implemented with different backing store
//
class CNVState {
    const char *const   m_name;

public:
    //
    // Possible reasons the actuator position was saved
    //
    enum reason_t: uint8_t {
        InitialMovement         = 0,
        SavedPositionLazy       = 1,
        SavedPositionImmediate  = 2,
        PowerDownSave           = 3,

        First           = InitialMovement,
        Last            = PowerDownSave
    };

    inline static const char *string( reason_t r ) {
        const char * s[ reason_t::Last + 1 ] = {
            "InitialMovement",
            "SavedPositionLazy",
            "SavedPositionImmediate",
            "PowerDownSave"
        };
        return s[r];
    }
    typedef CGauge::calType_t   gaugeCal_t;
    typedef float               actuatorPercent_t;

private:
    //
    // Encapsulates our individual NV values to help us keep track if one has changed
    //
    template <class T>
    class CValue {
        char const *m_name;
        T           m_value;
        bool        m_changed;          // have we changed m_value?
        bool        m_wasValid;         // was m_value, as fetched from nv storage, valid?
    public:
        CValue( const char *name ) : m_name(name), m_changed(false), m_wasValid(false) {}
        CValue&         init( const T& value )      { m_value = value; m_changed = false; return *this; }
        CValue&         set( const T& newValue )    { if( m_value != newValue ) { m_value = newValue; m_changed = true; } return *this; }
        const T&        get() const                 { return m_value; }
        bool            changed() const             { return m_changed; }
        void            clearChanged()              { m_changed = false; }
        bool            wasValid() const            { return m_wasValid; }
        CValue&         setWasValid( bool b )       { m_wasValid = b; return *this; }
        void            print() const               { printf("%s (changed %d, wasValid %d): ", m_name, m_changed, m_wasValid ); }
        virtual CValue& setDefault()                = 0;
    };

    struct __1__: public CValue< gaugeCal_t > {
        __1__() : CValue("Gauge Duty Cycle") {}

        CValue& setDefault() override {
            CGauge::calType_t d = { 86.75, 71.25, 61.25, 51.75, 35.25 };
            return init( d );
        }
    } m_gaugeCal;                   // gauge 0%, 25%, 50%, 75%, and 100% duty cycle times

    struct __2__ : public CValue< actuatorPercent_t > {
        __2__() : CValue("Actuator") {}
        CValue& setDefault() override {
            return init(0);
        }
    } m_actuatorPercent;            // actuator position
    
    struct __3__: public CValue< reason_t > {
        __3__() : CValue("Reason") {}
        CValue& setDefault() override {
            return init( InitialMovement );
        }
    } m_reason;                     // why did we save this actuator position?

protected:
    //
    // Load our members from the stored state
    //
    CNVState( const char * const name ) : m_name( name ) {}

public:
    //
    // Initialize with reasonable defaults
    //
    CNVState&       setDefaults();

    //
    // Zap (clear out) the nvstate
    //
    virtual void    zap() = 0;

    //
    // Save our members to nonvolatile storage
    //
    virtual bool    commit() = 0;

    //
    // Can we update NV storage without any wear-out issues?
    //
    virtual bool    unlimitedUpdates() const        { return false; }

    //
    // print our members to the console
    //
    void        print() const;

    auto&       gaugeCal()                          { return m_gaugeCal; }
    const auto& gaugeCal() const                    { return m_gaugeCal; }

    auto&       actuatorPercent()                   { return m_actuatorPercent; }
    const auto& actuatorPercent() const             { return m_actuatorPercent; }

    auto&       reason()                            { return m_reason; }
    const auto& reason() const                      { return m_reason; }

    CNVState&   setGaugeCal( const gaugeCal_t& c )  { m_gaugeCal.set(c); return *this; }
    CNVState&   setActuatorPercent( actuatorPercent_t p, reason_t r )   { m_actuatorPercent.set(p); m_reason.set(r); return *this; }

    const char  *name() const                       { return m_name; }
    virtual bool doCommand( int cmd )               { (void)cmd; return false; }

    bool    closeEnough( float percent ) const      { return abs( m_actuatorPercent.get() - percent ) <= 0.1f; }
};

inline CNVState& CNVState::setDefaults() {
    m_gaugeCal.setDefault();
    m_actuatorPercent.setDefault();
    m_reason.setDefault();
    return *this;
}

inline void CNVState::print() const {
    printf("\n%s\n", m_name );

    printf("PWM freq: %d HZ\n", GAUGE_PWM_FREQ );
    printf("Actuator stroke: %.2f inches, rate: %.2f sec per inch\n", ACTUATOR_STROKE_INCHES, ACTUATOR_SECONDS_PER_INCH );

    auto cal = gaugeCal();
    cal.print();
    for( auto val : cal.get() ) printf(" %.2f%%", val );
    printf("\n");

    actuatorPercent().print();
    printf("%.2f%%\n", actuatorPercent().get() );

    reason().print();
    printf( "%s\n", string(reason().get()) );
}