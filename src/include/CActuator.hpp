class CActuator {
	CGPIO_OUT	m_MOTOR_ENABLE;
	CGPIO_OUT	m_LPWM;
	CGPIO_OUT	m_RPWM;

	const int m_fullTransitMs;			// designed actuator full retract time
	const int m_fullTransitMsToBeSure;	// m_fullTransitMs plus some more time just to make sure

	class runTime_t {
		absolute_time_t		m_value;

	public:
		runTime_t()							{ clear(); }
		runTime_t( absolute_time_t now )	{ m_value = now; }

		bool operator==( const runTime_t& rhs ) const			{ return m_value == rhs.m_value; }
		bool operator==( bool b ) const							{ return (m_value != nil_time) == b; }

		runTime_t & operator=( const runTime_t& rhs )			{ m_value = rhs.m_value; return *this; }
		runTime_t &operator-=( const runTime_t& rhs ) 			{ m_value -= rhs.m_value; return *this; }
		runTime_t &operator+=( const runTime_t& rhs )			{ m_value += rhs.m_value; return *this; }

		void clear()		{ m_value = nil_time; }
		void setNow()		{ m_value = get_absolute_time(); }
		int  ms() const		{ return m_value != nil_time ? int(absolute_time_diff_us( m_value, get_absolute_time() ) / 1000) : 0; }
	} m_startTime, m_lastStopTime;

	bool		m_moved;
	int8_t		m_currentDirection;				// 0 -> not moving, 1 extending, -1 retracting
	int			m_currentPositionMs = 0;		// current position in ms running time from fully retracted

	void		startExtendMotion();
	void		startRetractMotion();
	void		stopMotion();
	
	class CGaugeUpdater {
		CMessage::Type		m_msgType = CMessage::Type::GAUGE_UPDATE;

		struct myTick : public CGlobalTimer::COnTick {
			CGaugeUpdater& m_updater;
			void onTick() override						{ m_updater.onTick(); }
			myTick( CGaugeUpdater &u ) : m_updater(u)	{}
		} m_myTick;

	public:
		CGaugeUpdater() : m_myTick(*this) {};

		void start( CMessage::Type m = CMessage::Type::GAUGE_UPDATE ) {
			m_msgType = m;
			m_myTick.start();
		}
		void stop() {
			m_myTick.stop();
		}
		void onTick() {
			if( auto msg = CMessage::alloc( m_msgType ) )
				msg->push();
		}
		int msPerTick() const							{ return m_myTick.msPerTick(); }
	} m_gaugeUpdater;

public:
	CActuator( int fullTransitMs );
	~CActuator()	{}
    bool            moved() const                   	{ return m_moved; }     // have we moved the actuator?
    void            setMoved( bool state );

	void			extend( bool updateGauge = true );
	void			retract( bool updateGauge = true );
	void			startFullRetract( bool updateGauge = true);
	int				targetRullRetractRunTime() const	{ return m_fullTransitMsToBeSure; }
	void			stop();
	bool			active() const						{ return m_currentDirection != 0; }
	int				activeTime() const					{ return m_startTime.ms(); }
	int				fullTransitMs() const				{ return m_fullTransitMs; }
	int				msPerGaugeTick() const				{ return m_gaugeUpdater.msPerTick(); }
	int				secondsSinceLastStop() const;
	float		    percent() const;
	void			setAlreadyAtPercent( float percent );		// the actuator is already at 'percent'.  Let it know
	float			percentUnbounded() const;
};