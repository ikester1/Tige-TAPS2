#include    "CNVState.hpp"
#include    "CRC.hpp"


//
// The configuration is stored in non volatile memory.  This class implements
//   the storage in FRAM
//
class CNVFRAM : public CNVState {
    typedef CNVState super;

    mutable CI2C    m_i2c;

    bool    get( uint eepromAddress, void *buf, uint numberOfBytes ) const;
    bool    set( uint eepromAddress, const void *buf, uint numberOfBytes );

public:
    CNVFRAM( uint8_t sevenBitAddr, BoardPin::type_t sdaPin );
    bool commit() override;
    void zap() override;
    bool unlimitedUpdates() const override  { return true; }

    uint size() const;

    //
    // Here are the addresses of the items we store in FRAM
    //
    static constexpr uint   ADDR_GAUGE_CAL          = 0;
    static constexpr uint   ADDR_GAUGE_CAL_CRC      = ADDR_GAUGE_CAL + sizeof( super::gaugeCal_t );

    static constexpr uint   ADDR_REASON             = ADDR_GAUGE_CAL_CRC + sizeof( CCRC16::type_t );
    static constexpr uint   ADDR_ACTUATOR_PERCENT   = ADDR_REASON + sizeof( super::reason_t );
    static constexpr uint   ADDR_ACTUATOR_CRC       = ADDR_ACTUATOR_PERCENT + sizeof( super::actuatorPercent_t );

    bool doCommand( int cmd ) override;
};