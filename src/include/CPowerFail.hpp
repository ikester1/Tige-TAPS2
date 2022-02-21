class CPowerFail {
    CGPIO_IN    m_gpio;
    static int m_powerFailCount;       // increments on fail, decrements on not fail

    static void onInterrupt( uint gpio, uint32_t events );
public:
    CPowerFail();

    //
    // Can we do power fail detection?
    //
    bool available() const  { return m_gpio.available(); }

    //
    // TRUE on power failure, FALSE otherwise
    //
    bool operator==( bool b ) const     { return ((m_powerFailCount > 0) == b) ? true : false; }
};