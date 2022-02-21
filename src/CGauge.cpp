#include <stdio.h>
#include "pico/stdlib.h"

#include "util.hpp"
#include "CGauge.hpp"

CGauge::CGauge( const calType_t &cal, uint hz ) :
m_gpio( BoardPin::GAUGE_PWM ),
m_gaugeEnablePin( BoardPin::GAUGE_ENABLE, false ),
m_notGaugeEnablePin( BoardPin::NOT_GAUGE_ENABLE, true ),
m_gaugePWM( m_gpio, hz )
{
	calibrate( cal );
	set(0);
}

CGauge& CGauge::enable() {
	m_gaugeEnablePin = true;
	m_notGaugeEnablePin = false;
	return *this;
}

CGauge& CGauge::disable() {
	m_gaugeEnablePin = false;
	m_notGaugeEnablePin = true;
	return *this;
}

float CGauge::mapGaugeToDutyCycle( float gaugePercent ) const {
    int index = int( (gaugePercent / 100) * m_dutyCycleMap.size() + 0.5f );
    index = MAX( index, 0 );
    index = MIN( index, int(m_dutyCycleMap.size()-1) );
    return m_dutyCycleMap[ index ];
}

float CGauge::currentDutyCycle() const {
    return m_gaugePWM.getPercent();
}

CGauge&	CGauge::setCurrentDutyCycle( float dutyCycle ) {
    m_gaugePWM.setPercent( dutyCycle );
    return *this;
}

CGauge& CGauge::setCurrentDutyCycleSlow( const float desiredPWM ) {
	float current;
	while( (current = currentDutyCycle()) != desiredPWM ) {
		sleep_ms(20);
		float delta = current > desiredPWM ? -1.0f : 1.0f;
		if( abs(current + delta - desiredPWM) <= 1.0f )
			delta = desiredPWM - current;
		setCurrentDutyCycle( current + delta );
	}
	return *this;
}

CGauge& CGauge::set( float percent ) {
	setCurrentDutyCycle( mapGaugeToDutyCycle( percent ) );
	m_percent = percent;
	return *this;
}

CGauge& CGauge::setSlow( float percent ) {
	setCurrentDutyCycleSlow( mapGaugeToDutyCycle( percent ) );
	m_percent = percent;
	return *this;
}

//
// Pass pwm duty cycle setpoints for 0%, 25%, 50%, 75%, and 100% gauge readings
//
void CGauge::calibrate( const calType_t &settings ) {
	const auto rangeDelta = (m_dutyCycleMap.size()-1) / (settings.size()-1);
	for( uint i = 0; i < settings.size()-1; ++i )
		fillBetween( i*rangeDelta, settings[i], (i+1)*rangeDelta, settings[i+1] );

    smooth();
#if 0
	int i = 0;
	for( auto val : m_dutyCycleMap ) {
		printf("%.2f ", val );
		if( ++i % 10 == 0 )
			printf("\n");
	}
	printf("\n");
#endif
}

void CGauge::fillBetween( int low, float dutyLow, int high, float dutyHigh) {
	const auto deltaY = dutyHigh - dutyLow;
	const float deltaX = float(high - low);

	for( auto i = low; i <= high; ++i )
		m_dutyCycleMap[i] = dutyLow + ((i - low)/deltaX) * deltaY;
}

//
// Make sure the gauge map is reasonably smooth.  The only "abrupt" places might
//  be at the 0, 25, 50, and 75% "knees".  But whatever..
//
void CGauge::smooth() {
    /*
     * maybe later
     */
}

//
// Make sure the configuration is sensible.  That is, ensure the values are
//   legitimate percentage values and monitonically increasing or decreasing
//
bool CGauge::isValidCalibration( const calType_t &gaugeCal ) {

	if( gaugeCal[0] < 0 || gaugeCal[0] > 100 )
		return false;

    const bool increasing = gaugeCal[1] > gaugeCal[0];
    for( uint i = 1; i < gaugeCal.size()-1; ++i ) {
		if( gaugeCal[i] < 0 || gaugeCal[i] > 100 )
			return false;
        if( increasing ) {
            if( gaugeCal[i+1] <= gaugeCal[i] )
                return false;
        } else {
            if( gaugeCal[i+1] >= gaugeCal[i] )
                return false;
        }
    }

	return true;
}
