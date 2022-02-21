#include <stdio.h>
#include "pico/stdlib.h"

#include "config.h"
#include "util.hpp"
#include "taps.hpp"
#include "CActuator.hpp"

static constexpr int ENABLE_DELAY_MS = 20;

CActuator::CActuator( int fullTransitMs ) :
	m_MOTOR_ENABLE( BoardPin::MOTOR_ENABLE ),
	m_LPWM( BoardPin::MOTOR_LPWM ),
	m_RPWM( BoardPin::MOTOR_RPWM ),

	m_fullTransitMs( fullTransitMs ),
	m_fullTransitMsToBeSure( (11*fullTransitMs)/10 ),
    m_moved( false ),
	m_currentDirection( 0 )

{
	stopMotion();
}

void CActuator::setMoved( bool moved ) {
    m_moved = moved;
}

//
// Low level start extending the actuator
//
void CActuator::startExtendMotion() {
	m_MOTOR_ENABLE = false;
	if( ENABLE_DELAY_MS )
		sleep_ms( ENABLE_DELAY_MS );
	m_LPWM = false;
	m_RPWM = true;
	m_MOTOR_ENABLE = true;
	m_currentDirection = 1;
    setMoved( true );
}

//
// Low level start retracting the actuator
//
void CActuator::startRetractMotion() {
	m_MOTOR_ENABLE = false;
	if( ENABLE_DELAY_MS )
		sleep_ms( ENABLE_DELAY_MS );
	m_RPWM = false;
	m_LPWM = true;
	m_MOTOR_ENABLE = true;
	m_currentDirection = -1;
    setMoved( true );
}

//
// Low level stop moving the actuator
//
void CActuator::stopMotion() {
	m_MOTOR_ENABLE = false;
	m_LPWM = m_RPWM = false;
	m_MOTOR_ENABLE = true;			// brake
	m_currentDirection = 0;
}

void CActuator::extend( bool updateGauge ) {
	m_lastStopTime.clear();
	startExtendMotion();
	m_startTime.setNow();
	if( updateGauge )
		m_gaugeUpdater.start();
}

void CActuator::retract( bool updateGauge ) {
	m_lastStopTime.clear();
	startRetractMotion();
	m_startTime.setNow();
	if( updateGauge )
		m_gaugeUpdater.start();
}

void CActuator::startFullRetract( bool updateGauge ) {
	m_lastStopTime.clear();
	m_currentPositionMs = 0;
	startRetractMotion();
	m_startTime.setNow();

	if( updateGauge )
		m_gaugeUpdater.start( CMessage::Type::FULL_RETRACT );
}

void CActuator::stop() {
	if( !active() )
		return;

	const bool retracting = (m_currentDirection < 0);

	stopMotion();
	auto runTime = m_startTime.ms();

	m_lastStopTime.setNow();

	m_currentPositionMs += retracting ? -runTime : runTime;
	m_currentPositionMs = MAX( m_currentPositionMs, 0 );
	m_currentPositionMs = MIN( m_currentPositionMs, m_fullTransitMs );

	m_startTime.clear();
	m_gaugeUpdater.stop();

	printf("Actuator Stop: %d mS, %.2f%%\n", m_currentPositionMs, percent() );
}

int CActuator::secondsSinceLastStop() const {
	return m_lastStopTime.ms() / 1000;
}

float CActuator::percentUnbounded() const {
	int position = m_currentPositionMs;
	if( active() )
		position += (m_currentDirection < 0) ? -m_startTime.ms() : m_startTime.ms();
	float unboundedPercent =  (position*100.0f) / m_fullTransitMs;
	return unboundedPercent;
}

float CActuator::percent() const {
	auto val = percentUnbounded();
	val = MAX( val, 0 );
	val = MIN( val, 100 );
	return val;
}

void CActuator::setAlreadyAtPercent( float percent ) {
	m_currentPositionMs = int( m_fullTransitMs * (percent/100) );
}