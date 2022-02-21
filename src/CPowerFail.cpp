#include <stdio.h>
#include "pico/stdlib.h"

#include "util.hpp"
#include "taps.hpp"
#include "CPowerFail.hpp"

int CPowerFail::m_powerFailCount;

CPowerFail::CPowerFail() : m_gpio( BoardPin::POWER_FAIL, true ) {
    //
    // Can we do power fail detection?
    //
    if( m_gpio.available() == false )
        return;

    //
    // Enable interrupts for edge low and edge high
    //
    gpio_set_irq_enabled_with_callback( m_gpio.pin(), GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, onInterrupt );
}

void CPowerFail::onInterrupt( uint gpio, uint32_t events ) {
    if( events & GPIO_IRQ_EDGE_FALL ) {
        //
        // GPIO just went from high to low
        //
        if( ++m_powerFailCount == 1 )
            if( auto msg = CMessage::alloc( CMessage::Type::POWER_FAILED ) )
                    msg->push();

    } else if( events & GPIO_IRQ_EDGE_RISE ) {
        //
        // GPIO just went from low to high
        //
        if( m_powerFailCount && --m_powerFailCount == 0 )
            if( auto msg = CMessage::alloc( CMessage::Type::POWER_RESTORED ) )
                    msg->push();
    }
}