#include <stdio.h>
#include <cstring>
#include "pico/stdlib.h"

#include "config.h"
#include "util.hpp"
#include "CGauge.hpp"
#include "CNVFRAM.hpp"
#include "CRC.hpp"

CNVFRAM::CNVFRAM( uint8_t sevenBitAddr, BoardPin::type_t sdaPin ) : CNVState( "FRAM" ), m_i2c( sevenBitAddr, sdaPin )
{
    super::reason_t r;
    super::actuatorPercent_t percent;
    CCRC16::type_t storedCRC;

    super::reason().setDefault();
    super::actuatorPercent().setDefault();

    if( get( ADDR_REASON, &r, sizeof(r) ) &&
        r >= super::First && r <= super::Last &&
        get( ADDR_ACTUATOR_PERCENT, &percent, sizeof(percent) ) &&
        percent >= 0 && percent <= 100 &&
        get( ADDR_ACTUATOR_CRC, &storedCRC, sizeof(storedCRC) ) ) {

        auto computedCRC = CCRC16( &percent, sizeof(percent) ).add( &r, sizeof(r) ).crc();
        if( computedCRC == storedCRC ) {
            super::reason().init( r ).setWasValid(true);
            super::actuatorPercent().init( percent ).setWasValid(true);
        }
    }

    bool success = false;
    if( super::gaugeCal_t cal; get( ADDR_GAUGE_CAL, cal.data(), cal.size() * sizeof(*cal.data()) ) ) {
        if( CGauge::isValidCalibration( cal ) ) {
            get( ADDR_GAUGE_CAL_CRC, &storedCRC, sizeof(storedCRC) );
            const auto computedCRC = CCRC16( cal.data(), cal.size() * sizeof(*cal.data()) ).crc();

            if( computedCRC == storedCRC ) {
                super::gaugeCal().init( cal ).setWasValid(true);
                success = true;
            }
        }
    }
    if( !success )
        super::gaugeCal().setDefault();
}

uint CNVFRAM::size() const {
    if( m_i2c.available() == false )
        return 0;

    return 8 * 1024;
}

bool CNVFRAM::commit() {
    if( super::gaugeCal().changed() ) {
        const super::gaugeCal_t cal = super::gaugeCal().get();
        const auto computedCRC = CCRC16(cal.data(), cal.size() * sizeof(*cal.data()) ).crc();

        set( ADDR_GAUGE_CAL, cal.data(), cal.size() * sizeof(*cal.data()) );
        set( ADDR_GAUGE_CAL_CRC, &computedCRC, sizeof(computedCRC) );
        super::gaugeCal().clearChanged();
    }

    if( super::actuatorPercent().changed() || super::reason().changed() ) {
        const auto percent = super::actuatorPercent().get();
        if( super::actuatorPercent().changed() )
            set( ADDR_ACTUATOR_PERCENT, &percent, sizeof(percent) );

        const auto r = super::reason().get();
        if( super::reason().changed() )
            set( ADDR_REASON, &r, sizeof(r) );

        const auto computedCRC = CCRC16( &percent, sizeof(percent) ).add( &r, sizeof(r) ).crc();
        if( set( ADDR_ACTUATOR_CRC, &computedCRC, sizeof( computedCRC ) ) ) {
            super::actuatorPercent().clearChanged();
            super::reason().clearChanged();
        }
    }

    return true;
}

//
// Clear out all the FRAM
//
void CNVFRAM::zap() {
    auto bytes = size();
    uint addr = 0;

    uint8_t buf[100];
    memset( buf, 0, sizeof(buf) );

    while( bytes ) {
        auto n = MIN( bytes, sizeof(buf) );
        if( this->set( addr, buf, n ) == false )
            break;
        addr += n;
        bytes -= n;
    }
}

//
// Fetch 'numberOfBytes' into 'buf' from FRAM address 'eepromAddress'
//
bool CNVFRAM::get( const uint eepromAddress, void * const buf, const uint numberOfBytes ) const {
    const uint8_t framAddr[2] = { uint8_t(eepromAddress >> 8), uint8_t(eepromAddress) };
    return m_i2c.writeRead( framAddr, sizeof(framAddr), buf, numberOfBytes );
}

//
// Put 'numberOfBytes' from 'buf' to FRAM address 'eepromAddress'
//
bool CNVFRAM::set( const uint eepromAddress, const void * const buf, const uint numberOfBytes ) {
    const uint8_t framAddr[2] = { uint8_t(eepromAddress >> 8), uint8_t(eepromAddress) };
    return m_i2c.writeWrite( framAddr, sizeof( framAddr ), buf, numberOfBytes, true );
}

bool CNVFRAM::doCommand( int cmd ) {
    
    if( cmd == 'i' ) {
        m_i2c.scan();
        return true;
    }

    if( cmd == 'r' || cmd == 'w' ) {
        bool success;
        if( auto address = getDecimal( success, "\nAddr" ); success ) {
            uint8_t buffer[ 200 ];
            uint bytes = getDecimal(success, "# of bytes" );
            if( success && bytes > 0 ) {
                bytes = MIN( bytes, sizeof(buffer) );
                if( cmd == 'r' ) {
                    if( get( address, buffer, bytes ) )
                        hexdump( buffer, bytes );
                } else {
                    for( uint i = 0; success && i < bytes; ++i )
                        buffer[i] = (uint8_t)getDecimal( success, "Byte[%u/%u]", i+1, bytes );
                    if( success ) {
                        hexdump( buffer, bytes, "Bytes to write:\n" );
                        set( address, buffer, bytes );
                    }
                }
            }
        }
        return true;
    }
    return false;
}