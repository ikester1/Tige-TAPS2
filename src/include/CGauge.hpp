#include <array>

class CGauge {
    CGPIO_OUT                   m_gpio;
	CGPIO_OUT					m_gaugeEnablePin;
	CGPIO_OUT					m_notGaugeEnablePin;
    CPWM                        m_gaugePWM;
	std::array< float, 401 >	m_dutyCycleMap;
	float						m_percent;

	void fillBetween( int low, float dutyLow, int high, float dutyHigh);
    void smooth();
    float mapGaugeToDutyCycle( float gaugePercent ) const;

public:
	typedef std::array< float, 5 >		calType_t;

	CGauge( const calType_t &cal, uint hz = 10000 );
	~CGauge() {}
	CGauge&	set( float percent );
	CGauge&	setSlow( float percent );
	float	get() const						{ return m_percent; }

	CGauge&	setCurrentDutyCycle( float dutyCycle );
	CGauge&	setCurrentDutyCycleSlow( float dutyCycle );
	float	currentDutyCycle() const;

	static bool isValidCalibration( const calType_t &settings );

	CGauge& enable();			// enable power to the gauge
	CGauge& disable();			// disable power to the gauge
	
	//
	// Pass pwm duty cycle setpoints for 0%, 25%, 50%, 75%, and 100% gauge readings readings
	//
	void calibrate( const calType_t &settings );

//	CGauge& disable()						{ m_gaugePWM.disable(); return *this; }		// no coming back from this!

	auto getLowPercent() const				{ return m_dutyCycleMap[0]; }
	auto getHighPercent() const				{ return m_dutyCycleMap[ m_dutyCycleMap.size() - 1 ]; }

	CPWM&	pwm()							{ return m_gaugePWM; }
};