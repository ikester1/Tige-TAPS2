#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/critical_section.h"

#include "taps.hpp"

static CMessage *freeList = nullptr;
static CMessage *queueHead = nullptr, *queueTail = nullptr;
static bool stopped = false;

static class CriticalSection {
	critical_section_t		m_critSec;
public:
	CriticalSection() {
		critical_section_init( &m_critSec );
	}
	~CriticalSection() {
		critical_section_deinit( &m_critSec );
	}

	operator critical_section_t *()	{ return &m_critSec; }

} critSec;

class LOCK {
public:
	LOCK()	{ critical_section_enter_blocking( critSec ); }
	~LOCK()	{ critical_section_exit( critSec ); }
};

CMessage *CMessage::alloc( Type t, int optionalData ) {
	static bool initialized = false;

	LOCK l;

	if( !initialized ) {
		static CMessage messages[20];

		for( auto i = 0; i < int( sizeof(messages)/sizeof(messages[0]) ) ; ++i )
			messages[i].freeWithoutLock();

		initialized = true;
	}

	if( stopped )
		return nullptr;

	if( CMessage *instance = freeList ) {
		freeList = instance->m_next;
		instance->m_next = nullptr;
		instance->m_type = t;
		instance->m_data = optionalData;
		return instance;
	}

	return nullptr;
}

void CMessage::start() {
	stopped = false;
}

void CMessage::stop() {
	stopped = true;
	flush();
}

void CMessage::freeWithoutLock() {
	m_type = Type::FREE;
	m_next = freeList;
	freeList = this;
}

void CMessage::free() {
	LOCK l;
	freeWithoutLock();
}

void CMessage::push() {
	if( !stopped ) {
		LOCK l;
		if( queueTail ) {
			queueTail->m_next = this;
			queueTail = this;
		} else {
			queueHead = queueTail = this;
		}
		this->m_next = nullptr;
		return;
	}

	//
	// Can't queue the message.... don't lose it!
	//
	free();
}

CMessage *CMessage::pop() {
	LOCK l;
	if( CMessage *instance = queueHead ) {
		queueHead = instance->m_next;
		if( queueTail == instance )
			queueTail = nullptr;

		instance->m_next = nullptr;
		return instance;
	}
	return nullptr;
}

void CMessage::flush() {
	while( auto msg = pop() )
		msg->free();
}

void CMessage::print() const {
	char const * const text[] = {
		"FREE",
		"TRIM_TOP_ON",
		"TRIM_BOTTOM_ON",
		"TRIM_OFF",
		"GAUGE_UPDATE",
		"FULL_RETRACT",
		"HEARTBEAT",
		"CONFIG_BUTTON_ON",
		"POWER_FAILED",
		"POWER_RESTORED",
		"USER_COMMAND"
	};

	if( unsigned(m_type) < sizeof(text)/sizeof(text[0]) )
		printf("%s\n", text[ unsigned(m_type) ] );
	else
		printf( "INVALID MESSAGE TYPE %d\n", int( m_type ) );
}
