#include "hardware/flash.h"
#include "hardware/sync.h"

#include <stdio.h>
#include <memory.h>
#include "pico/stdlib.h"

#include "config.h"
#include "util.hpp"
#include "CGauge.hpp"
#include "CNVFlash.hpp"
#include "CRC.hpp"

//
// This file implements the storage of persistent data into the pico's flash.  The data is separated
//   into infrequently changing elements and frequently changing elements.  We make a reasonable attempt
//   to avoid wearing out the flash, and by my calculations this should work for decades of use.
//
// We CRC the data to make sure corruption is detected (but it is not corrected)
//

template < class t, uint32_t top >
class configBase {
    struct tWithCRC {
        t               m_data;
        CCRC16::type_t  m_crc;

        tWithCRC()  { memset( (void *)(this), 0xFF, sizeof(*this)); }

        bool empty() const {
            for( uint8_t const *p = (uint8_t const *)(this); p < (uint8_t const *)(this+1); ++p )
                if( *p != 0xFF )
                    return false;
            return true;
        }
    };

    const char * const m_name;
public:
    static const int    pagesInFlash =      (sizeof(tWithCRC) + FLASH_PAGE_SIZE-1)/FLASH_PAGE_SIZE;
    static const int    sectorsInFlash =    (pagesInFlash*FLASH_PAGE_SIZE + FLASH_SECTOR_SIZE - 1)/FLASH_SECTOR_SIZE;
    static const auto   offsetInFlash =     top - (sectorsInFlash*FLASH_SECTOR_SIZE);
    static const int    numConfigInFlash =  FLASH_SECTOR_SIZE / sizeof(tWithCRC);

    static auto         baseAddress()                       { return (tWithCRC const *)((char *)(XIP_BASE) + offsetInFlash); }
    auto                calcCRC( const t * const dp ) const { return CCRC16( dp, dp+1 ).crc(); }

    inline static tWithCRC const *  m_flashPtr    = nullptr;          // Points to area in flash holding array of tWithCRC
    inline static bool              m_valid       = false;
 
    configBase( const char *name );
    void        save( t const * );
    inline bool valid() const           { return m_valid == true; }
    const char * name() const           { return m_name; }
};

template< class t, uint32_t top >
configBase<t,top>::configBase( const char *name ) : m_name( name ) {
    if( m_flashPtr == nullptr ) {
        for( m_flashPtr = baseAddress() + numConfigInFlash - 1;  m_flashPtr >= baseAddress(); --m_flashPtr ) {
            if( calcCRC( &m_flashPtr->m_data ) == m_flashPtr->m_crc ) {
                m_valid = true;
                break;
            }
        }
        if( !m_valid )
            m_flashPtr = baseAddress();
    }
}

template< class t, uint32_t top >
void configBase<t,top>::save( t const * const tp ) {
    const auto newCRC = calcCRC(tp);

    if( newCRC == calcCRC( &m_flashPtr->m_data ) )
        return;

    printf("*******\nCURRENT %s AT SLOT %u of %u at %p: ", name(), m_flashPtr - baseAddress(), numConfigInFlash, m_flashPtr );
    m_flashPtr->m_data.print();
    printf("\n");

    tWithCRC emptyData;
    bool foundSlot = false;

    for( m_flashPtr = baseAddress(); m_flashPtr < baseAddress() + numConfigInFlash; ++m_flashPtr )
        if( m_flashPtr->empty() ) {
            foundSlot = true;
            break;
        }
    uint8_t newImage[ sectorsInFlash * FLASH_SECTOR_SIZE ];

    if( !foundSlot ) {
        m_flashPtr = baseAddress();
        memset( newImage, 0xFF, sizeof(newImage) );
    } else {
        memcpy( newImage, baseAddress(), sizeof( newImage ) );
    }

    printf("%u bytes reserved at %p in flash for %s, each entry is %u bytes\n", sizeof(newImage), baseAddress(), name(), sizeof(emptyData) );

    memcpy( &emptyData.m_data, tp, sizeof( emptyData.m_data ) );
    emptyData.m_crc = newCRC;
    memcpy( &newImage[ (m_flashPtr - baseAddress()) * sizeof(emptyData) ], &emptyData, sizeof(emptyData) );

    {
        CINTERRUPTS_OFF intsOff;
        if( !foundSlot )
            flash_range_erase( offsetInFlash, sizeof(newImage) );
        flash_range_program( offsetInFlash, newImage, sizeof(newImage) );
    }

    printf("NEW %s AT SLOT %u at %p: ", name(), m_flashPtr - baseAddress(), m_flashPtr ); m_flashPtr->m_data.print(); printf("\n\n");
}


//
// Data in flash is split between two regions...one that changes often and one that doesn't
//
struct _constantData {
    struct _d {
        CNVState::gaugeCal_t    gaugeCal;       // gauge 0%, 25%, 50%, 75%, and 100% duty cycle times
        _d()                                    { memset( this, 0xFF, sizeof(*this) ); }
        void                    print() const   { printf("PWM:"); for( auto val : gaugeCal ) printf(" %.2f%%", val ); }
    };

    configBase< _d, PICO_FLASH_SIZE_BYTES > m_base;
    _constantData() : m_base( "CONSTANT_DATA" ) {}
};


struct _changingData {
    struct _d {
        uint16_t        reason:2;               // CNVState::reason_t
        uint16_t        percentTimes100:14;     // actuator percent (x100)

        _d()                                        { memset( this, 0xFF, sizeof(*this) ); }
        void            print() const               { printf("reason %s, percent: %.2f%%", CNVState::string(CNVState::reason_t(reason)), percent() ); }
        CNVState::actuatorPercent_t percent() const { return percentTimes100 / 100.0f; }
        void            setPercent( CNVState::reason_t r, CNVState::actuatorPercent_t p )
                                                    { reason = uint16_t(r); percentTimes100 = uint16_t(p * 100); }

    };

    configBase< _d, PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE > m_base;
    _changingData() : m_base( "CHANGING_DATA" ) {}
};

static _constantData* constData() {
    static _constantData d;
    return &d;
}

static _changingData * changingData() {
    static _changingData d;
    return &d;
}


CNVFlash::CNVFlash() : CNVState( "FLASH" ) {
    if( _constantData * const constd = constData(); constd->m_base.valid() == false ||
        CGauge::isValidCalibration( constd->m_base.m_flashPtr->m_data.gaugeCal ) == false  ) {
        super::gaugeCal().setDefault();
    } else {
        super::gaugeCal().init( constd->m_base.m_flashPtr->m_data.gaugeCal ).setWasValid(true);
    }

    if( _changingData * const changd = changingData(); changd->m_base.valid() == false ) {
        super::actuatorPercent().setDefault();
        super::reason().setDefault();
    } else {
        super::actuatorPercent().init( changd->m_base.m_flashPtr->m_data.percent()  ).setWasValid(true);
        super::reason().init( static_cast< super::reason_t >( changd->m_base.m_flashPtr->m_data.reason ) ).setWasValid(true);
    }
}

//
// Unsafe if other cores are executing!
//
bool CNVFlash::commit() {
    if( super::actuatorPercent().changed() || super::reason().changed() ) {
        _changingData::_d changd;
        changd.setPercent( super::reason().get(), super::actuatorPercent().get() );
        changingData()->m_base.save( &changd );

        super::actuatorPercent().clearChanged();
        super::reason().clearChanged();
    }

    if( super::gaugeCal().changed() ) {
        _constantData::_d constd;
        constd.gaugeCal = gaugeCal().get();
        constData()->m_base.save( &constd );

        super::gaugeCal().clearChanged();
    }
    return true;
}
