#include <stdio.h>
#include <stdarg.h>
#include <cstring>
#include "pico/stdlib.h"

#include    "util.hpp"

CI2C::CI2C( uint8_t sevenBitAddr, BoardPin::type_t sdaPin, uint hz ) :
 m_7BitAddr( sevenBitAddr ),
 m_available( HAL::hasPin( sdaPin ) ),
 m_sdaPin( HAL::pinNumber(sdaPin) ),
 m_sclPin( HAL::pinNumber(sdaPin) + 1 ),
 m_i2c( (HAL::pinNumber(sdaPin)&02) ? i2c1 : i2c0)
{
    if( available() ) {
        i2c_init( m_i2c, hz );
        gpio_set_function( m_sdaPin, GPIO_FUNC_I2C );
        gpio_set_function( m_sclPin, GPIO_FUNC_I2C );
        gpio_pull_up( m_sdaPin );
        gpio_pull_up( m_sclPin );
    }
}

CI2C::~CI2C() {
    if( available() ) {
        i2c_deinit( m_i2c );
        gpio_set_function( m_sdaPin, GPIO_FUNC_NULL );
        gpio_set_function( m_sclPin, GPIO_FUNC_NULL );
        gpio_disable_pulls( m_sdaPin );
        gpio_disable_pulls( m_sclPin );
    }
}

//
// Write 'writeData1' of 'bytesToWrite1' to the slave, followed by 'writeData2' of 'bytesToWrite2'.
//   If 'sendStop' then issue a stop when the writes are complete.
//
// Either 'bytesToWrite1' or 'bytesToWrite2' can be zero
//
bool
CI2C::writeWrite( const void *writeData1, uint8_t bytesToWrite1, const void *writeData2, uint16_t bytesToWrite2, bool sendStop ) {
    if( !available() ) {
        showError( "marked unavailable\n" );
        return false;
    }
    const uint totalBytes = bytesToWrite1 + bytesToWrite2;
    if( totalBytes == 0 )
        return false;

    if( bytesToWrite1 && bytesToWrite2 ) {
        uint8_t buffer[ 1000 ];
        if( totalBytes > sizeof(buffer) )
            return false;

        memcpy( buffer, writeData1, bytesToWrite1 );
        memcpy( buffer + bytesToWrite1, writeData2, bytesToWrite2 );
        if( i2c_write_timeout_us( m_i2c, m_7BitAddr, buffer, totalBytes, !sendStop, CI2C::timeout_us ) < 0 ) {
            showError( "writeWrite i2c_write_timeout_us %u bytes failed\n", totalBytes );
            return false;
        }
        return true;
    }

    if( bytesToWrite1 )
        if( i2c_write_timeout_us( m_i2c, m_7BitAddr, static_cast<const uint8_t *>(writeData1), bytesToWrite1, !sendStop, CI2C::timeout_us ) < 0 ) {
            showError( "writeWrite i2c_write_timeout_us failed; %u bytes to write for 'bytesToWrite1'\n", bytesToWrite1 );
            return false;
        }

    if( bytesToWrite2 )
        if( i2c_write_timeout_us( m_i2c, m_7BitAddr, static_cast<const uint8_t *>(writeData2), bytesToWrite2, !sendStop, CI2C::timeout_us ) < 0 ) {
            showError( "writeWrite i2c_write_timeout_us failed; %u bytes to write for 'bytesToWrite2'\n", bytesToWrite2 );
            return false;
        }

    return true;
}

//
// Write 'bytesToWrite' to the slave, then turn around and read 'bytesToRead'.
//  No bytes are written if 'bytesToWrite' is zero
//  No bytes are read if 'bytesToRead' is zero
//  STOP is issued after the operation
//
bool
CI2C::writeRead( const void *writeData, uint8_t bytesToWrite, void *readData, uint16_t bytesToRead ) {
    if( !available() ) {
        showError( "marked unavailable\n" );
        return false;
    }

    if( bytesToWrite )
        if( !writeWrite( writeData, bytesToWrite, nullptr, 0, bytesToRead ? false : true ) )
            return false;

    if( bytesToRead )
        if( i2c_read_timeout_us( m_i2c, m_7BitAddr, static_cast<uint8_t *>(readData), bytesToRead, false, CI2C::timeout_us ) != bytesToRead ) {
            showError("writeRead i2c_read_timeout_us failed; %u bytes to read\n", bytesToRead );
            return false;
        }

    return true;
}

void
CI2C::showError( const char *format... ) {
    va_list args;
    va_start( args, format );
    printf("CI2C addr x%x: ", m_7BitAddr );
    vprintf( format, args );
    va_end(args);
}

bool CI2C::reservedAddr( uint8_t addr ) const {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void CI2C::scan() {
    printf("\nScan of i2c%d:\n", m_i2c == i2c0 ? 0 : 1 );
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if( bool success; ( getcharTimeout(success,0), success ) )
            break;

        if (addr % 16 == 0)
            printf("%02x ", addr);

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (reservedAddr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_timeout_us(m_i2c, addr, &rxdata, 1, false, CI2C::timeout_us );

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
}
