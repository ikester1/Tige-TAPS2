class CMessage {
public:
    enum class Type { FREE,
                      TRIM_TOP_ON,
                      TRIM_BOTTOM_ON,
                      TRIM_OFF,
                      GAUGE_UPDATE,
                      FULL_RETRACT,
                      HEARTBEAT,
                      CONFIG_BUTTON_ON,
                      POWER_FAILED,
                      POWER_RESTORED,
                      USER_COMMAND
    };

    static CMessage     *alloc( Type, int optionalData = 0 );
    void                free();

    void                push();
    static CMessage     *pop();

    static void         flush();

    Type                type() const    { return m_type; }
    void                print() const;
    int                 data() const    { return m_data; }

    static void         stop();     // stop message queueing
    static void         start();    // resume message queueing

private:
    ~CMessage()         {}
    void                freeWithoutLock();

    CMessage    *m_next;
    Type        m_type;
    uint        m_data;
};

