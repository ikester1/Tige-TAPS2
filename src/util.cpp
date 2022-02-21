
#include <stdio.h>
#include "pico/stdlib.h"

#include    "util.hpp"

CGlobalTimer& CGlobalTimer::instance() {
    static CGlobalTimer globalTimer;

    return globalTimer;
}


CGlobalTimer::CGlobalTimer() : super( msPerTick() ), m_head( nullptr ) {
}

void CGlobalTimer::add( COnTick& callback ) {
    CINTERRUPTS_OFF intsOff;
    callback.m_next = m_head;
    m_head = &callback;
    super::start();
}

bool CGlobalTimer::remove( COnTick& callback ) {
    CINTERRUPTS_OFF intsOff;

    for( COnTick *ptr = m_head, *prev = nullptr; ptr; prev = ptr, ptr = ptr->m_next ) {
        if( ptr == &callback ) {
            if( prev != nullptr )
                prev->m_next = ptr->m_next;
            else {
                m_head = ptr->m_next;
                if( m_head == nullptr )
                    super::stop();
            }

            ptr->m_next = nullptr;
            return true;
        }
    }

    return false;
}

//
// Called at interrupt time
//
bool CGlobalTimer::onTimer() {
    for( auto ptr = m_head; ptr != nullptr; ptr = ptr->m_next )
        ptr->onTick();
    return true;
}
