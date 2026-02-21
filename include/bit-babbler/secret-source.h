//  This file is distributed as part of the bit-babbler package.
//  Copyright 2010 - 2021,  Ron <ron@debian.org>

#ifndef _BB_SECRET_SOURCE_H
#define _BB_SECRET_SOURCE_H

#include <bit-babbler/health-monitor.h>
#include <bit-babbler/ftdi-device.h>

#if EM_PLATFORM_LINUX
 #include <linux/random.h>
 #include <poll.h>
#endif

#if defined(__FreeBSD__) && __FreeBSD__ < 13
 #include <pthread_np.h>
#endif

#if EM_PLATFORM_MAC
 #include <fcntl.h>
#endif

#if HAVE_LIBUDEV
// Prior to udev-150 their headers don't wrap this for C++, so do it here until
// the buggy distro releases catch up.  FC12 and CentOS 6, I'm looking at you.
extern "C" {
 #include <libudev.h>
}
#endif


#define BB_VENDOR_ID        FTDI_VENDOR_ID
#define BB_PRODUCT_ID       0x7840

#define BB_WHITE_PRODUCTSTR "White RNG"
#define BB_BLACK_PRODUCTSTR "Black RNG"

// We sample on the rising edge and clock bits out MSB first by default
//#define SAMPLE_FALLING_EDGE
//#define LSB_FIRST

// Sanity check that we only get exactly as much data as we requested.
//{{{
// This guards against a class of kernel/USB controller/libusb bugs which could
// corrupt correct transfer of the data we request from the hardware - such as
// the issues that were seen with how early XHCI drivers handled short packets
// differently to previous generations of USB controllers.  We do already deal
// with the known problem versions - since some of them may still be used by
// people, but we can't test it with every possible system combination nor can
// we predict every issue that may occur in future versions.  But we can make a
// good effort to detect any such problems and shut down the device until they
// are properly understood and fixes or mitigations for them are in place too.
//
// You could somewhat safely disable this test if you've satisified yourself
// that it really won't fire on the combination of hardware and software that
// you are using - but it's really only worth even thinking about that if you
// need to squeeze every last bit per second out of the hardware, which most
// people simply won't ever really need to do.  And the current test is quite
// cheap to run.
//}}}
#define CHECK_EXCESS_BYTES


namespace BitB
{

    class BitBabbler : public FTDI
    { //{{{
    private:

        static const unsigned   FTDI_INIT_RETRIES = 20;

        unsigned        m_enable_mask;
        unsigned        m_disable_pol;
        unsigned        m_bitrate;
        unsigned        m_fold;
        unsigned        m_sleep_init;   // in milliseconds
        unsigned        m_sleep_max;
        unsigned        m_suspend_after;
        bool            m_no_qa;


        void init_device()
        { //{{{

            for( unsigned retries = FTDI_INIT_RETRIES; retries; --retries )
            {
                if( retries < FTDI_INIT_RETRIES )
                    LogMsg<2>("BitBabbler::init_device: retrying");

                if( ! InitMPSSE() )
                    continue;

                unsigned        clk_div = 30000000 / m_bitrate - 1;
                const uint8_t   cmd[] =
                {
                    MPSSE_NO_CLK_DIV5,
                    MPSSE_NO_ADAPTIVE_CLK,
                    MPSSE_NO_3PHASE_CLK,

                    MPSSE_SET_DATABITS_LOW,
               #ifdef SAMPLE_FALLING_EDGE
                    uint8_t( 0x01 | m_disable_pol ),    //  all outputs low, clk high.
               #else
                    uint8_t( 0x00 | m_disable_pol ),    //  CLK, DO, CS, outputs low
               #endif                                   //  masking polarity as per config
                    uint8_t( 0x0B | m_enable_mask ),    //  Set CLK, DO, CS, as outputs,
                                                        //  generator mask as per config
                    MPSSE_SET_DATABITS_HIGH,
                    0x00,                               //  all outputs low
                    0x00,                               //  set all pins as inputs

                    MPSSE_SET_CLK_DIVISOR,
                    uint8_t( clk_div & 0xFF ),          // divisor low byte
                    uint8_t( clk_div >> 8 ),            // divisor high byte

                    MPSSE_NO_LOOPBACK,
                };

                try {
                    WriteCommand( cmd, sizeof(cmd) );
                }
                BB_CATCH_ALL( 0, _("BitBabbler::init_device: set clock failed"), continue; )

                // Wait another 30ms for all of this to settle too
                usleep(30000);

                // Clear the (empty) return from the WriteCommand()
                purge_read();

                return;
            }

            ThrowError( _("BitBabbler::init_device failed") );

        } //}}}


    public:

        typedef RefPtr< BitBabbler >    Handle;


        struct Options
        { //{{{

            typedef std::list< Options >    List;

            USBContext::Device::ID  id;
            unsigned                enable_mask;
            unsigned                disable_polarity;
            unsigned                bitrate;
            unsigned                chunksize;
            unsigned                latency;
            unsigned                fold;
            unsigned                group;
            unsigned                sleep_init;     // in milliseconds
            unsigned                sleep_max;
            unsigned                suspend_after;
            bool                    no_qa;


            Options()
                : enable_mask( 0x0f )
                , disable_polarity( 0x00 )
                , bitrate( 0 )
                , chunksize( 0 )
                , latency( unsigned(-1) )
                , fold( unsigned(-1) )
                , group( 0 )
                , sleep_init( 100 )
                , sleep_max( 60000 )
                , suspend_after( 0 )
                , no_qa( false )
            {}


            void SetIdleSleep( const std::string &arg )
            { //{{{

                size_t  n = arg.find(':');

                if( n == std::string::npos )
                    throw Error( _("BitBabbler::Options: invalid idle-sleep argument '%s'"),
                                                                            arg.c_str() );

                if( n != 0 )
                {
                    try {
                        sleep_init = StrToScaledU( arg.substr(0, n) );
                    }
                    catch( const std::exception &e )
                    {
                        throw Error( _("BitBabbler::Options: invalid idle-sleep init '%s': %s"),
                                                                        arg.c_str(), e.what() );
                    }
                }

                if( n + 1 < arg.size() )
                {
                    try {
                        sleep_max = StrToScaledU( arg.substr(n + 1) );
                    }
                    catch( const std::exception &e )
                    {
                        throw Error( _("BitBabbler::Options: invalid idle-sleep max '%s': %s"),
                                                                        arg.c_str(), e.what() );
                    }
                }

                if( sleep_max && sleep_init > sleep_max )
                    throw Error( _("BitBabbler::Options: invalid idle-sleep, init %u > max %u"),
                                                                        sleep_init, sleep_max );
            } //}}}

        }; //}}}


        static unsigned RealBitrate( unsigned bitrate )
        { //{{{

            if( bitrate >= 30000000 )
                return 30000000;

            if( bitrate <= 458 )
                return 458;

            return 30000000 / (30000000 / bitrate);

        } //}}}


    private:

        unsigned choose_bitrate( const Options &opt )
        { //{{{

            if( opt.bitrate )
                return RealBitrate( opt.bitrate );

            return 2500000;

        } //}}}

        unsigned choose_folding( const Options &opt )
        { //{{{

            if( opt.fold != unsigned(-1) )
                return opt.fold;

            if( GetProduct() == BB_WHITE_PRODUCTSTR )
                return 1;

            if( GetProduct() == BB_BLACK_PRODUCTSTR )
                return 3;

            ThrowError( _("BitBabbler: unknown product '%s', and folding not set"),
                                                            GetProduct().c_str() );
           #if EM_PLATFORM_MSW
            // Really we'll never get here, but mingw-w64 4.9.2-21+15.4
            // appears to be too stupid to figure that out ...
            return 0;
           #endif

        } //}}}


    public:

        BitBabbler( const USBContext::Device::Handle   &dev,
                    const Options                      &options   = Options(),
                    bool                                claim_now = true )
            : FTDI( dev, false )
            , m_enable_mask( ~options.enable_mask << 4 & 0xf0 )
            , m_disable_pol( options.disable_polarity << 4 & 0xf0 )
            , m_bitrate( choose_bitrate(options) )
            , m_fold( choose_folding(options) )
            , m_sleep_init( options.sleep_init )
            , m_sleep_max( options.sleep_max )
            , m_suspend_after( options.suspend_after )
            , m_no_qa( options.no_qa )
        { //{{{

            if( options.bitrate == m_bitrate )
                LogMsg<2>( "+ BitBabbler( bitrate %u, fold %u, mask 0x%02x [%02x] )",
                                m_bitrate, m_fold, options.enable_mask, m_enable_mask );
            else
                LogMsg<2>( "+ BitBabbler( bitrate %u (%u), fold %u, mask 0x%02x [%02x] )",
                    options.bitrate, m_bitrate, m_fold, options.enable_mask, m_enable_mask );


            unsigned    maxpacket = GetMaxPacketSize();

            // Select the chunk size to be the largest power of 2 between the
            // maximum packet size and 64kB that will take less than 250ms to
            // transfer (which is then the maximum time we'll block waiting to
            // perform an orderly exit).
            size_t      chunksize = std::max( maxpacket,
                                    std::min( options.chunksize ? options.chunksize
                                                                : 65536u,
                                              powof2_down(m_bitrate / 32
                                                                    / maxpacket
                                                                    * maxpacket) ) );

            // Select the latency to avoid timing out and returning a short packet
            // before the full chunksize can be transferred.  This isn't actually
            // the fastest way to get lots of data out of the device, but it does
            // significantly minimise the CPU load because we don't spin hard doing
            // many small transactions to get a whole chunk out.
            unsigned    latency = std::max( 1u,
                                  std::min( 255u,
                                            maxpacket * 8000 / m_bitrate + 2 ) );

            // The above computes what should be our theoretical optimum latency,
            // ie. the amount of time it would take to completely fill a packet
            // of the maximum allowable size.  That in theory, should give us the
            // best throughput, since it requires the least number of transactions
            // to complete the transfer.  In practice however, it appears that we
            // can wring a bit more speed out with a lower latency than that, even
            // when it means every packet ends up 'short' - though it comes at the
            // cost of significantly increasing CPU usage as the number of packets
            // required can increase drastically.  This appears to be true even if
            // the 'performance' CPU governor is used, though it still may be a
            // function of just not letting the CPU throttle down as much by simply
            // keeping it busy more of the time.
            //
            // For a BitBabbler White in the default configuration, the latency
            // required to fill a 64kB request is 3ms.  Decreasing that to 1ms is
            // worth about 2MB/hr in the rate of data which we can read from one.
            // Which is less than half a percent improvement, and the extra CPU
            // time cost is disproportionately larger than that - but it may still
            // be a useful speed up to want for some uses cases.  (By comparison,
            // on the system I tested this on, using the performance governor was
            // worth about an extra 2% in output rate, or about 10MB/hr over what
            // was seen with the default 'powersave' option.)
            //
            // So we default to making efficient use of the CPU, but allow people
            // to override the latency if they do want speed over everything else.
            if( options.latency != unsigned(-1) )
                latency = options.latency;


            chunksize = SetChunkSize( chunksize );
            SetLatency( latency );

            LogMsg<3>( "Chunk size %zu, %zu ms/per chunk (latency %u ms, max packet %u)",
                        chunksize, chunksize * 8000 / m_bitrate, latency, maxpacket );

            if( claim_now )
                Claim();

        } //}}}

        ~BitBabbler()
        {
            LogMsg<2>( "- BitBabbler" );
            Release();
        }


        virtual bool Claim()
        { //{{{

            if( ! FTDI::Claim() )
                return false;

            init_device();
            return true;

        } //}}}

        virtual void Release()
        { //{{{

            ResetBitmode();
            FTDI::Release();

        } //}}}


        unsigned GetBitrate() const
        {
            return m_bitrate;
        }

        unsigned GetFolding() const
        {
            return m_fold;
        }

        unsigned GetIdleSleepInit() const
        {
            return m_sleep_init;
        }

        unsigned GetIdleSleepMax() const
        {
            return m_sleep_max;
        }

        unsigned GetSuspendAfter() const
        {
            return m_suspend_after;
        }

        bool NoQA() const
        {
            return m_no_qa;
        }


        size_t read( uint8_t *buf, size_t len )
        { //{{{

            if( __builtin_expect( len < 1 || len > 65536, 0 ) )
                throw Error( _("BitBabbler::read( %zu ): invalid length"), len );

            const uint8_t   cmd[] =
            {
              #ifdef LSB_FIRST

               #ifdef SAMPLE_FALLING_EDGE
                MPSSE_DATA_BYTE_IN_NEG_LSB,
               #else
                MPSSE_DATA_BYTE_IN_POS_LSB,
               #endif

              #else   // MSB first

               #ifdef SAMPLE_FALLING_EDGE
                MPSSE_DATA_BYTE_IN_NEG_MSB,
               #else
                MPSSE_DATA_BYTE_IN_POS_MSB,
               #endif

              #endif

                uint8_t((len - 1) & 0xFF),
                uint8_t((len - 1) >> 8),

                MPSSE_SEND_IMMEDIATE
            };

            unsigned    reset_attempts = 0;

            try {
                WriteCommand( cmd, sizeof(cmd) );
                goto ok;
            }
            catch( const abi::__forced_unwind& ) { throw; }
            BB_CATCH_STD( 0, stringprintf("BitBabbler::read( %zu ) exception", len).c_str() )

            while( ++reset_attempts < FTDI_INIT_RETRIES ) {
                // We shouldn't ever get here in normal operation, but if for some
                // reason things back up and the USB write request fails, try once
                // to reset the device and reissue the request before bailing out.
                //
                // We could also be here if the read repeatedly returns no data,
                // which could happen if the command written was somehow corrupted
                // in which case resetting the device is also the only way we can
                // be sure of what state it is in.  We bound the maximum number of
                // times that may be attempted while no error is being reported by
                // any of these operations, but any real or permanent error should
                // normally result in bailing out before that limit is reached.
                LogMsg<1>( "BitBabbler::read( %zu ): attempting to reset device", len );
                FTDI::Claim();
                init_device();
                WriteCommand( cmd, sizeof(cmd) );

            ok:
                LogMsg<6>( "BitBabbler::read( %zu ): wrote request", len );

                size_t  count = 0;
                size_t  n = 0;

                do {
                    size_t  ret = ftdi_read( buf + count, len - count );

                    if( __builtin_expect( ret > 0, 1 ) )
                    {
                        LogMsg<6>( "BitBabbler::read( %zu ): read %zu (n = %zu)",
                                                                    len, ret, n );
                        count += ret;

                        if( __builtin_expect( count == len, 1 ) )
                        {
                            // This is just to create buffer bloat errors,
                            // mostly for testing the purge recovery code.
                            //WriteCommand( cmd, sizeof(cmd) );

                           #ifdef CHECK_EXCESS_BYTES

                            if( __builtin_expect(GetReadAhead() != 0 ||
                                                 GetLineStatus() != (FTDI_THRE | FTDI_TEMT), 0) )
                            {
                                size_t      ra = GetReadAhead();
                                unsigned    ls = GetLineStatus();

                                ret = ftdi_read( buf, len );

                                throw Error( _("BitBabbler::read( %zu ): Uh Oh excess data. "
                                                "Buffered %zu, line status 0x%02x [%s ]"),
                                                len, ra, ls,
                                                OctetsToHex( OctetString( buf,
                                                                          std::min(ret, size_t(8)) )
                                                           ).c_str() );
                            }

                           #endif

                            return len;
                        }

                        n = 0;
                    }

                } while( ++n < FTDI_READ_RETRIES );

                LogMsg<1>( _("BitBabbler::read( %zu ) failed (n = %zu)"), len, n );
            }

            throw Error( _("BitBabbler::read( %zu ) failed after %u reset attempts"),
                                                               len, reset_attempts );
        } //}}}

    }; //}}}


    class Pool : public RefCounted
    { //{{{
    public:

        struct Options
        { //{{{

            size_t          pool_size;
            std::string     kernel_device;
            unsigned        kernel_refill_time;     // in seconds


            Options()
                : pool_size( 65536 )
                , kernel_device( "/dev/random" )
                , kernel_refill_time( 60 )
            {}


            std::string Str() const
            {
                return stringprintf( "Size %zu, Kernel dev '%s', refill time %us",
                                     pool_size, kernel_device.c_str(), kernel_refill_time );
            }

        }; //}}}


        class Group : public RefCounted
        { //{{{
        public:

            typedef unsigned                                ID;
            typedef uint32_t                                Mask;
            typedef RefPtr< Group >                         Handle;
            typedef was_tr1::unordered_map< ID, Handle >    Map;


            struct Options
            { //{{{

                typedef std::list< Options >    List;

                Group::ID   groupid;
                size_t      size;


                Options( const char *arg )
                {
                    char           *e;
                    unsigned long   v = strtoul( arg, &e, 10 );

                    if( e == arg || *e != ':' || v > unsigned(-1) )
                        throw Error( _("Invalid --group-size option '%s'"), arg );

                    groupid = Group::ID(v);
                    size    = StrToScaledUL( e + 1, 1024 );
                }

            }; //}}}


        private:

            Pool               *m_pool;
            ID                  m_id;
            size_t              m_size;
            uint8_t            *m_buf;
            Mask                m_filled;
            Mask                m_mask;
            unsigned            m_members;
            pthread_mutex_t     m_mutex;


        public:

            Group( Pool *p, ID group_id, size_t size )
                : m_pool( p )
                , m_id( group_id )
                , m_size( powof2_up(size) )
                , m_buf( new uint8_t[m_size] )
                , m_filled( 0 )
                , m_mask( 0 )
                , m_members( 0 )
            {
                Log<2>( "+ Pool::Group( %u, %zu )\n", m_id, m_size );

                pthread_mutex_init( &m_mutex, NULL );
            }

            ~Group()
            {
                Log<2>( "- Pool::Group( %u, %zu )\n", m_id, m_size );

                pthread_mutex_destroy( &m_mutex );
                delete [] m_buf;
            }


            ID GetID() const
            {
                return m_id;
            }

            size_t GetSize() const
            {
                return m_size;
            }


            Mask GetNextMask()
            { //{{{

                ScopedMutex     lock( &m_mutex );

                if( m_id == 0 )
                    return 0;

                for( Mask i = 1; i; i <<= 1 )
                {
                    if( (m_mask & i) == 0 )
                    {
                        m_mask |= i;
                        m_members++;
                        return i;
                    }
                }

                throw Error( _("Pool::Group %u is full"), m_id );

            } //}}}

            void ReleaseMask( Mask i )
            { //{{{

                ScopedMutex     lock( &m_mutex );

                if( m_id == 0 )
                    return;

                if( (m_mask & i) == 0 )
                {
                    // This shouldn't ever happen, but we don't want to
                    // corrupt the m_members count if it somehow does.
                    // We don't want to throw, since we're probably in
                    // a destructor (or similar unwinding) somewhere.
                    Log<0>( _("Pool::Group(%u)::ReleaseMask( %x ): "
                              "mask was not allocated (%x)\n"), m_id, i, m_mask );
                    return;
                }

                m_mask &= ~i;
                m_filled &= ~i;
                m_members--;

            } //}}}


            void AddEntropy( Mask m, uint8_t *b, size_t len )
            { //{{{

                if( len == 0 )
                    return;

                // This should never happen
                if( len != m_size )
                    throw Error( _("Pool::Group(%u:%x)::AddEntropy: len %zu != group size %zu"),
                                                                        m_id, m, len, m_size );

                ScopedMutex     lock( &m_mutex );

                if( m_id == 0 || m_members == 1 )
                {
                    // short-circuit directly to the main pool if there is
                    // only one source in this group (or if this is group 0).
                    m_filled = 0;

                    lock.Unlock();
                    m_pool->AddEntropy( b, len );
                    return;
                }

                if( ! m_filled )
                {
                    memcpy( m_buf, b, len );
                    m_filled = m;

                } else {

                    for( size_t i = 0; i < len; ++i )
                        m_buf[i] ^= b[i];

                    m_filled |= m;
                }

                Log<5>("Group %u:%x: filled %x\n", m_id, m, m_filled);

                if( m_filled == m_mask )
                {
                    uint8_t buf[m_size];

                    memcpy( buf, m_buf, m_size );
                    m_filled = 0;

                    lock.Unlock();
                    m_pool->AddEntropy( buf, m_size );
                }

            } //}}}

        }; //}}}


    private:

        struct Source : public RefCounted
        { //{{{

            typedef RefPtr< Source >        Handle;
            typedef std::list< Handle >     List;


            Pool                   *pool;
            uint8_t                *buf;
            size_t                  size;
            Group::Handle           group;
            Group::Mask             groupmask;
            BitBabbler::Handle      babbler;
            pthread_t               thread;


            Source( Pool                       *p,
                    const Group::Handle        &g,
                    const BitBabbler::Handle   &b )
                : pool( p )
                , size( g->GetSize() * (1u << b->GetFolding()) )
                , group( g )
                , groupmask( g->GetNextMask() )
                , babbler( b )
            {
                Log<2>( "+ Pool::Source( %u:%u, %zu, %s )\n", group->GetID(), groupmask,
                                                    size, babbler->GetSerial().c_str() );
                if( size < babbler->GetChunkSize() )
                    throw Error( _("Pool::Source( %u:%u, %s ): size %zu < chunksize %zu"),
                                   group->GetID(), groupmask, babbler->GetSerial().c_str(),
                                                        size, babbler->GetChunkSize() );

                buf = new uint8_t[size];

                // Bump the refcount until the thread is started, otherwise we
                // may lose a race with this Source being released by the caller
                // before the thread can take its handle from the raw pointer.
                // Think of it as a virtual Handle passed with pthread_create.
                Ref();

                // We don't need to Unref() if this fails, because we'll throw
                // and it will never have been constructed to be destroyed ...
                int ret = pthread_create( &thread, GetDefaultThreadAttr(),
                                          Pool::source_thread, this );
                if( ret )
                {
                    group->ReleaseMask( groupmask );
                    delete [] buf;

                    throw SystemError( ret, _("Pool::Source: failed to create thread") );
                }
            }

            ~Source()
            {
                Log<2>( "- Pool::Source( %u:%u, %zu, %s )\n", group->GetID(), groupmask,
                                                    size, babbler->GetSerial().c_str() );
                group->ReleaseMask( groupmask );
                delete [] buf;
            }

        }; //}}}


        struct WriteFD : public RefCounted
        {  //{{{

            typedef RefPtr< WriteFD >   Handle;
            typedef void (*Completion)(void *user_data);

            Pool           *pool;
            int             fd;
            size_t          len;
            Completion      completion_handler;
            void           *user_data;

            WriteFD( Pool *p, int fd, size_t len, Completion handler = NULL,
                                                     void *user_data = NULL )
                : pool( p )
                , fd( fd )
                , len( len )
                , completion_handler( handler )
                , user_data( user_data )
            {}

        }; //}}}


        typedef std::list< pthread_t >      ThreadList;


        const Options       m_opt;

        uint8_t            *m_buf;
        size_t              m_fill;
        size_t              m_next;

        Group::Map          m_groups;
        Source::List        m_sources;
        ThreadList          m_threads;

        pthread_mutex_t     m_mutex;
        pthread_cond_t      m_sourcecond;
        pthread_cond_t      m_sinkcond;


        // You must hold m_mutex to call this
        bool PoolIsFull_()
        {
            return m_fill == m_opt.pool_size;
        }

        bool PoolIsFull()
        {
            ScopedMutex     lock( &m_mutex );
            return PoolIsFull_();
        }

        void AddEntropy( uint8_t *buf, size_t len )
        { //{{{

            if( len == 0 )
                return;

            ScopedMutex     lock( &m_mutex );
            size_t          n = 0;

            if( m_fill < m_opt.pool_size )
            {
                size_t  b = std::min( m_opt.pool_size - m_fill, len );

                Log<5>( "Pool::AddEntropy: add %zu / %zu octets at %zu / %zu\n",
                                                b, len, m_fill, m_opt.pool_size );
                memcpy( m_buf + m_fill, buf, b );

                n = b;
                m_fill += b;

                pthread_cond_broadcast( &m_sinkcond );
            }

            while( n < len )
            {
                size_t  b = std::min( m_opt.pool_size - m_next, len - n );

                Log<5>( "Pool::AddEntropy: mix %zu / %zu octets at %zu / %zu\n",
                                                b, len, m_next, m_opt.pool_size );
                for( size_t i = 0; i < b; ++i )
                    m_buf[m_next + i] ^= buf[n + i];

                n += b;
                m_next += b;

                if( m_next >= m_opt.pool_size )
                    m_next = 0;
            }

        } //}}}


        void detach_source( const Source::Handle &s )
        { //{{{

            ScopedMutex     lock( &m_mutex );

            for( Source::List::iterator i = m_sources.begin(),
                                        e = m_sources.end(); i != e; ++ i )
            {
                if( *i == s )
                {
                    m_sources.erase( i );
                    pthread_detach( s->thread );
                    return;
                }
            }

        } //}}}

        BB_NORETURN
        void do_source_thread( const Source::Handle &s )
        { //{{{

            // All times here are in milliseconds.
            // - MIN_SLEEP is the minimum timeout before we will actually sleep.
            // - MAX_SLEEP is the longest duration we will actually sleep for,
            //   with 0 meaning sleep indefinitely once MIN_SLEEP is exceeded.
            // - INITIAL_SLEEP is the duration we start doubling from once the
            //   Pool is full, with 0 meaning sleep indefinitely immediately.
            static const unsigned   MIN_SLEEP       = 512;
            static const unsigned   MAX_SLEEP       = s->babbler->GetIdleSleepMax();
            static const unsigned   INITIAL_SLEEP   = s->babbler->GetIdleSleepInit();
            static const unsigned   SUSPEND_AFTER   = s->babbler->GetSuspendAfter();

            SetThreadName( s->babbler->GetSerial().substr(0,15) );

            s->babbler->LogMsg<3>( "Pool: begin source_thread (idle sleep %u:%u, suspend %u)",
                                                    INITIAL_SLEEP, MAX_SLEEP, SUSPEND_AFTER );

            // At rates of 5Mbps or greater, wait for the first Ent8 test results
            // before declaring the source is generating an acceptable quality of
            // entropy.  Below that let it come online if the FIPS tests aren't
            // rejecting it, with at least 20 consecutive blocks having passed.
            HealthMonitor   qa( s->babbler->GetSerial(),
                                s->babbler->GetBitrate() < 5000000 );

            size_t          read_size   = s->babbler->GetChunkSize();
            unsigned        fold        = s->babbler->GetFolding();
            bool            no_qa       = s->babbler->NoQA();
            unsigned        sleep_for   = 0;


            for(;;) try {

                s->babbler->Claim();

                for(;;)
                {
                    if( __builtin_expect( sleep_for == unsigned(-1), 0 ) )
                    {
                        // Sleep until we're explicitly woken by the pool being read from.
                        ScopedMutex     lock( &m_mutex );

                        if( __builtin_expect( PoolIsFull_(), 1 ) )
                        {
                            s->babbler->LogMsg<6>( "Pool: source_thread waiting for wakeup" );

                            if( SUSPEND_AFTER )
                                s->babbler->Release();

                            int ret = pthread_cond_wait( &m_sourcecond, &m_mutex );

                            if( ret )
                                throw SystemError( ret, "pthread_cond_wait failed: %s",
                                                                       strerror(ret) );
                            if( SUSPEND_AFTER )
                            {
                                lock.Unlock();
                                s->babbler->Claim();
                            }
                        }
                    }
                    else if( __builtin_expect( sleep_for >= MIN_SLEEP, 0 ) )
                    {
                        // Sleep until explicitly woken or the timeout expires.
                        timespec        wait_until;

                        GetFutureTimespec( wait_until, sleep_for );

                        ScopedMutex     lock( &m_mutex );

                        if( __builtin_expect( PoolIsFull_(), 1 ) )
                        {
                            s->babbler->LogMsg<6>( "Pool: source_thread sleeping for %ums",
                                                                                sleep_for );
                            if( SUSPEND_AFTER && sleep_for >= SUSPEND_AFTER )
                                s->babbler->Release();

                            int ret = pthread_cond_timedwait( &m_sourcecond, &m_mutex, &wait_until );

                            if( ret && ret != ETIMEDOUT )
                                throw SystemError( ret, "pthread_cond_timedwait failed: %s",
                                                                            strerror(ret) );
                            if( SUSPEND_AFTER && sleep_for >= SUSPEND_AFTER )
                            {
                                lock.Unlock();
                                s->babbler->Claim();
                            }
                        }
                    }


                    for( size_t p = 0, n = 0; p <= s->size - read_size; p += n )
                        n = s->babbler->read( s->buf + p, read_size );

                    size_t n = FoldBytes( s->buf, s->size, fold );


                    if( __builtin_expect( PoolIsFull(), 0 ) )
                    {
                        // If the pool is already (still) full, start to throttle back
                        // on how often we keep mixing more new entropy into it.  It's
                        // not that it hurts to do that, but there's probably not much
                        // point burning CPU cycles to do so as fast as possible while
                        // nobody is actually consuming what we alredy have.
                        if( sleep_for == 0 )
                        {
                            sleep_for = INITIAL_SLEEP ? INITIAL_SLEEP : unsigned(-1);
                        }
                        else if( sleep_for < MIN_SLEEP || sleep_for < MAX_SLEEP )
                        {
                            sleep_for *= 2;

                            if( MAX_SLEEP && sleep_for > MAX_SLEEP )
                                sleep_for = MAX_SLEEP;
                        }
                        else if( MAX_SLEEP == 0 )
                        {
                            sleep_for = unsigned(-1);
                        }
                    }
                    else
                        sleep_for = 0;

                    // Don't idle if the QA check failed.  We want to find out as quickly
                    // as possible if that was just a transient spike outside the limits
                    // (which being random is always possible, however rare it may be),
                    // and bring the device back on line if that's what it appears to be.
                    // If it stays bad, then the sysadmin is going to need to take some
                    // action of their own in response to the alert, and this likewise
                    // will ensure they have as much data as possible, as quickly as
                    // possible to base that decision on.
                    if( __builtin_expect( qa.Check( s->buf, n ) || no_qa, 1 ) )
                        s->group->AddEntropy( s->groupmask, s->buf, n );
                    else
                        sleep_for = 0;
                }
            }
            catch( const USBError &e )
            {
                // Don't warn about enum values not being explicitly handled here,
                // we don't want to have to chase every new error code added to
                // libusb that we don't explicitly care about handling here.
                EM_PUSH_DIAGNOSTIC_IGNORE("-Wswitch-enum")

                switch( e.GetErrorCode() )
                {
                    case LIBUSB_ERROR_PIPE:
                        s->babbler->LogMsg<1>( "Pool source_thread caught (device %sclaimed): %s",
                                                s->babbler->IsClaimed() ? "": "un", e.what() );
                        s->babbler->Release();
                        break;

                    case LIBUSB_ERROR_TIMEOUT:
                    case LIBUSB_ERROR_OTHER:
                        s->babbler->LogMsg<1>( "Pool source_thread caught: %s", e.what() );

                        s->babbler->SoftReset();
                        s->babbler->FTDI::Release();
                        break;

                    default:
                        throw;
                }
                EM_POP_DIAGNOSTIC
            }

        } //}}}

        static void *source_thread( void *p )
        { //{{{

            Source::Handle  s = static_cast<Source*>( p );

            // Drop the 'virtual handle' from the ctor, we have a real one now.
            s->Unref();

            try {
                s->pool->do_source_thread( s );
            }
            catch( const abi::__forced_unwind& )
            {
                s->babbler->LogMsg<3>( "Pool: source_thread cancelled" );
                throw;
            }
            BB_CATCH_STD( 0, s->babbler->MsgStr( _("uncaught source_thread exception") ).c_str() )

            s->pool->detach_source( s );
            return NULL;

        } //}}}


        void detach_thread( pthread_t p )
        { //{{{

            ScopedMutex     lock( &m_mutex );

            for( ThreadList::iterator i = m_threads.begin(),
                                      e = m_threads.end(); i != e; ++i )
            {
                if( *i == p )
                {
                    m_threads.erase( i );
                    pthread_detach( p );
                    return;
                }
            }

        } //}}}

        static void *writefd_thread( void *p )
        { //{{{

            WriteFD::Handle     w = static_cast<WriteFD*>( p );

            SetThreadName( "write fd" );

            try {
                try {
                    w->pool->WriteToFD( w->fd, w->len );

                    Log<3>( "Pool: writefd_thread completed\n" );
                }
                catch( const abi::__forced_unwind& )
                {
                    Log<3>( "Pool: writefd_thread cancelled\n" );
                    throw;
                }
                BB_CATCH_STD( 0, _("uncaught writefd_thread exception") )

                if( w->completion_handler )
                    w->completion_handler( w->user_data );
            }
            catch( const abi::__forced_unwind& )
            {
                Log<3>( "Pool: writefd_thread cancelled\n" );
                throw;
            }
            BB_CATCH_STD( 0, _("uncaught writefd_thread completion exception") )

            w->pool->detach_thread( pthread_self() );
            return NULL;

        } //}}}

        static void *feedkernel_thread( void *p )
        { //{{{

            Pool    *pool = static_cast<Pool*>( p );

            SetThreadName( "kernel pool" );

            try {
                Log<3>( "Pool: begin feedkernel_thread\n" );
                pool->FeedKernelEntropy();
            }
            catch( const abi::__forced_unwind& )
            {
                Log<3>( "Pool: feedkernel_thread cancelled\n" );
                throw;
            }
            BB_CATCH_STD( 0, _("uncaught feedkernel_thread exception") )

            pool->detach_thread( pthread_self() );
            return NULL;

        } //}}}


    public:

        typedef RefPtr< Pool >      Handle;


        Pool( const Options &options = Options() )
            : m_opt( options )
            , m_fill( 0 )
            , m_next( 0 )
        { //{{{

            Log<2>( "+ Pool( %s )\n", m_opt.Str().c_str() );

            m_buf = new uint8_t[m_opt.pool_size];

            pthread_mutex_init( &m_mutex, NULL );
            pthread_cond_init( &m_sourcecond, NULL );
            pthread_cond_init( &m_sinkcond, NULL );

        } //}}}

        ~Pool()
        { //{{{

            pthread_mutex_lock( &m_mutex );

            Log<3>( "Pool: terminating threads\n" );
            for( ThreadList::iterator i = m_threads.begin(),
                                      e = m_threads.end(); i != e; ++i )
                pthread_cancel( *i );

            Log<3>( "Pool: terminating sources\n" );
            for( Source::List::iterator i = m_sources.begin(),
                                        e = m_sources.end(); i != e; ++ i )
                pthread_cancel( (*i)->thread );


           #if HAVE_ABI_FORCED_UNWIND

            Log<3>( "Pool: waiting for thread termination\n" );
            while( ! m_threads.empty() )
            {
                pthread_t   p = m_threads.back();

                m_threads.pop_back();

                pthread_mutex_unlock( &m_mutex );
                pthread_join( p, NULL );
                pthread_mutex_lock( &m_mutex );
            }

            Log<3>( "Pool: waiting for source termination\n" );
            while( ! m_sources.empty() )
            {
                pthread_t   p = m_sources.back()->thread;

                m_sources.pop_back();

                pthread_mutex_unlock( &m_mutex );
                pthread_join( p, NULL );
                pthread_mutex_lock( &m_mutex );
            }

            pthread_mutex_unlock( &m_mutex );

           #else

            // This isn't entirely "safe", new threads could be created or new
            // sources added while we are tearing down the existing set, but in
            // the case where we don't have full stack unwinding when a thread
            // is cancelled, there are other things which are much more likely
            // to be in an awkward state than this.  We shouldn't be here in
            // "normal" operation anyway, only if we are in the process of a
            // controlled termination, so we can get away with crash diving our
            // way out of things to some extent.
            //
            // The main thing here is not to try to take the mutex again once
            // we unlock it, because a rudely cancelled thread might still be
            // holding it if the ScopedMutex wasn't properly unwound.

            ThreadList      t;
            Source::List    s;

            t.swap( m_threads );
            s.swap( m_sources );

            pthread_mutex_unlock( &m_mutex );

            Log<3>( "Pool: waiting for thread termination\n" );
            for( ThreadList::iterator i = t.begin(), e = t.end(); i != e; ++ i )
                pthread_join( *i, NULL );

            Log<3>( "Pool: waiting for source termination\n" );
            for( Source::List::iterator i = s.begin(), e = s.end(); i != e; ++ i )
                pthread_join( (*i)->thread, NULL );

           #endif

            pthread_cond_destroy( &m_sinkcond );
            pthread_cond_destroy( &m_sourcecond );
            pthread_mutex_destroy( &m_mutex );

            delete [] m_buf;

            Log<2>( "- Pool( %s )\n", m_opt.Str().c_str() );

        } //}}}


        // Group size will be rounded up to a power of 2
        void AddGroup( Group::ID group_id, size_t size )
        { //{{{

            Log<2>( "Pool::AddGroup( %u, %zu )\n", group_id, size );

            ScopedMutex             lock( &m_mutex );
            Group::Map::iterator    i = m_groups.find( group_id );

            if( i != m_groups.end() )
                throw Error( _("Pool::AddGroup( %u, %zu ): group already exists"),
                                                                group_id, size );

            m_groups[group_id] = new Group( this, group_id, size );

        } //}}}

        void AddSource( Group::ID group_id, const BitBabbler::Handle &babbler )
        { //{{{

            babbler->LogMsg<2>( "Pool::AddSource: adding to group %u", group_id );

            ScopedMutex             lock( &m_mutex );
            Group::Map::iterator    gi = m_groups.find( group_id );
            Group::Handle           g;

            if( gi == m_groups.end() )
            {
                g = new Group( this, group_id, m_opt.pool_size );
                m_groups[group_id] = g;

            } else {

                g = gi->second;
            }

            m_sources.push_back( new Source( this, g, babbler ) );

        } //}}}

        void RemoveSource( const USBContext::Device::Handle &d )
        { //{{{

            ScopedMutex     lock( &m_mutex );

            for( Source::List::iterator i = m_sources.begin(),
                                        e = m_sources.end(); i != e; ++ i )
            {
                if( (*i)->babbler->IsDevice( d ) )
                {
                    pthread_t   p = (*i)->thread;
                    m_sources.erase( i );

                    lock.Unlock();
                    pthread_cancel( p );

                   #if defined(__FreeBSD__) && __FreeBSD__ < 13
                    //{{{
                    // On FreeBSD 11 (where hotplug support was first added)
                    // if the device was removed while we were in an active
                    // call to libusb_bulk_transfer(), then that call may
                    // deadlock (typically somewhere in libusb_handle_events)
                    // and we will never be able to join the cancelled thread.
                    //
                    // So to avoid it also deadlocking us, we limit the amount
                    // of time we'll wait for the join to succeed, and if that
                    // elapses, we'll bark about it, then just leak the thread
                    // and its associated resources, and move on.
                    //
                    // It's not ideal, but it's about the best we can do until
                    // the FreeBSD side of things is fixed.
                    //
                    // Since it looks like FreeBSD 13 doesn't have this problem
                    // anymore, it can use the same code as everyone else again.

                    Log<4>( "Pool::RemoveSource: cancelling thread for %s\n",
                                                    d->VerboseStr().c_str() );

                    timespec           wait_until;
                    GetFutureTimespec( wait_until, 2000 );

                    int ret = pthread_timedjoin_np( p, NULL, &wait_until );

                    if( ret )
                        LogErr<0>( ret,
                                   "Pool::RemoveSource: failed to join thread for removed device %s",
                                   d->VerboseStr().c_str() );
                    else
                        Log<4>( "Pool::RemoveSource: joined thread for %s\n",
                                                    d->VerboseStr().c_str() );
                    //}}}
                   #else

                    pthread_join( p, NULL );

                   #endif

                    return;
                }
            }

        } //}}}

        void RemoveAllSources()
        { //{{{

            Source::List    s;
            {
                ScopedMutex     lock( &m_mutex );

                for( Source::List::iterator i = m_sources.begin(),
                                            e = m_sources.end(); i != e; ++ i )
                    pthread_cancel( (*i)->thread );

                s.swap( m_sources );
            }

            // Block until they are actually released, so that the caller has some
            // guarantee that it's safe to use removed sources for something else,
            // without racing against the removal still being completed here.
            for( Source::List::iterator i = s.begin(), e = s.end(); i != e; ++ i )
                pthread_join( (*i)->thread, NULL );

        } //}}}


        // Will block until it can return min(len,poolsize) octets
        size_t read( uint8_t *buf, size_t len )
        { //{{{

            Log<5>( "Pool::read( %zu )\n", len );

            ScopedMutex     lock( &m_mutex );

            while( m_fill < m_opt.pool_size && m_fill < len )
                pthread_cond_wait( &m_sinkcond, &m_mutex );

            size_t  n = std::min( m_fill, len );

            memcpy( buf, m_buf + (m_fill - n), n );
            m_fill -= n;

            pthread_cond_broadcast( &m_sourcecond );

            Log<5>( "Pool::read( %zu ) returning %zu (%zu remain)\n", len, n, m_fill );
            return n;

        } //}}}


        void WriteToFD( int fd, size_t len = 0 )
        { //{{{

            uint8_t     buf[65536];

            for(;;)
            {
                size_t      b = len ? std::min( len, sizeof(buf) ) : sizeof(buf);
                size_t      n = read( buf, b );

                for( size_t c = n; c; )
                {
                    ssize_t w = write( fd, buf + n - c, c );

                    if( w < 0 )
                        throw SystemError( _("Pool::WriteToFD( %d ) failed"), fd );

                    if( w == 0 )
                        throw Error( _("Pool::WriteToFD( %d ) EOF"), fd );

                    c -= size_t(w);
                }

                if( len && (len -= n) == 0 )
                    return;
            }

        } //}}}

        void WriteToFDAsync( int fd, size_t len = 0, WriteFD::Completion handler = NULL,
                                                                 void *user_data = NULL )
        { //{{{

            ScopedMutex     lock( &m_mutex );
            WriteFD        *w = new WriteFD( this, fd, len, handler, user_data );
            pthread_t       p;

            int ret = pthread_create( &p, GetDefaultThreadAttr(), writefd_thread, w );
            if( ret )
            {
                delete w;
                throw SystemError( ret, _("Pool::WriteToFDAsync( %d, %zu ): "
                                          "failed to create thread"), fd, len );
            }

            m_threads.push_back( p );

        } //}}}


        BB_NORETURN
        void FeedKernelEntropy( const std::string &dev = std::string() )
        { //{{{

           #if !EM_PLATFORM_LINUX

            // This is normally defined in linux/random.h, but we'll reuse it
            // here as a generic struct to share as much of the code below as
            // we reasonably can between all platforms.
            struct rand_pool_info {
                    int         entropy_count;
                    int         buf_size;
                    uint32_t    buf[];
            };

           #endif


           #if EM_PLATFORM_LINUX || EM_PLATFORM_MAC

            // Each time we wake up, either because the kernel pool has fallen
            // below its watermark or due to the timer expiring, we read a full
            // FIPS 140-2 analysis block (20k bits) from our own pool, and if
            // it passes the QA checks we then fold it twice to give us 5k bits
            // to pass to the kernel.  We then pass it through the QA tests yet
            // again, partly to be completely paranoid, and partly so that we
            // can query the QA statistics at that stage too, and if it still
            // passes, we give it to the kernel.
            //
            // If all is working well, the QA testing feeding our pool should
            // ensure that it has very near to 8 bits of entropy per byte, so
            // for each iteration here we take ~20k bits of entropy, fold it
            // to 5k bits for the kernel to process, of which with its default
            // pool size it will credit us for at most 4096 bits of entropy.
            // That should mean the estimate of 8 bits of entropy per byte that
            // we tell it we are providing should be reasonable even if we are
            // a hair below that in the worst case.

            int fd = open( dev.empty() ? m_opt.kernel_device.c_str() : dev.c_str(), O_RDWR );

            if( fd < 0 )
                throw SystemError( _("Pool::FeedKernelEntropy: failed to open %s"),
                                     dev.empty() ? m_opt.kernel_device.c_str() : dev.c_str() );

            const unsigned  N       = QA::FIPS::BUFFER_SIZE; // 20kbits for FIPS test.
            const unsigned  folds   = 2;
            const int       timeout = int(m_opt.kernel_refill_time)
                                    ? int(m_opt.kernel_refill_time * 1000) : -1;

            union {
                uint8_t                 b[N + sizeof(struct rand_pool_info)];
                struct rand_pool_info   rpi;
            };

            uint8_t        *buf = reinterpret_cast<uint8_t*>( rpi.buf );
            uint8_t         b2[N];
            size_t          b2_fill = 0;

            HealthMonitor   qa( "Pool" );
            HealthMonitor   qa2( "Kernel" );

            bool            source_ok;
            bool            folded_ok = false;

            for(;;)
            {
                size_t      n;

                do {
                    n = read( buf, N );

                    if( ! (source_ok = qa.Check( buf, n )) )
                    {
                        b2_fill = 0;
                        continue;
                    }

                    n = FoldBytes( buf, n, folds );

                    memcpy( b2 + b2_fill, buf, n );
                    b2_fill += n;

                    if( b2_fill >= N )
                    {
                        b2_fill   = 0;
                        folded_ok = qa2.Check( b2, N );
                    }

                } while( ! source_ok || ! folded_ok );


                rpi.entropy_count = int(n * 8);
                rpi.buf_size      = int(n);


               #if EM_PLATFORM_LINUX

                if( ioctl( fd, RNDADDENTROPY, &rpi ) )
                    throw SystemError( _("Pool::FeedKernelEntropy: ioctl failed") );

                EM_TRY_PUSH_DIAGNOSTIC_IGNORE("-Wgnu-designator")

                struct pollfd   p = { fd: fd, events: POLLOUT, revents: 0 };
                int             r = poll( &p, 1, timeout );

                if( r < 0 )
                    throw SystemError( _("Pool::FeedKernelEntropy: poll failed") );

                EM_POP_DIAGNOSTIC

               #elif EM_PLATFORM_MAC

                // MacOS has no method of signalling to us when its kernel might
                // actually want more entropy, so we'll just feed it a new block
                // each time the kernel_refill_time expires.
                ssize_t r = write( fd, buf, n );

                if( r < 0 )
                    throw SystemError( _("Pool::FeedKernelEntropy: write to kernel device failed") );

                usleep( timeout * 1000 );

               #else
                // We should only be here if we added extra platforms to the
                // outer condition and didn't actually add them here too ...
                #error "You forgot to implement this didn't you ..."
               #endif
            }

           #else
            (void)dev;
            throw Error("Kernel entropy not supported on this platform");
           #endif

        } //}}}

        void FeedKernelEntropyAsync()
        { //{{{

            ScopedMutex     lock( &m_mutex );
            pthread_t       p;

            int ret = pthread_create( &p, GetDefaultThreadAttr(), feedkernel_thread, this );
            if( ret )
                throw SystemError( ret, _("Pool::FeedKernelEntropyAsync: "
                                          "failed to create thread") );

            m_threads.push_back( p );

        } //}}}

    }; //}}}


    class DevList : public USBContext
    { //{{{
    private:

        unsigned                    m_vendorid;
        unsigned                    m_productid;

        pthread_mutex_t             m_pool_mutex;
        Pool::Handle                m_pool;
        BitBabbler::Options         m_default_options;
        BitBabbler::Options::List   m_device_options;


    protected:

        virtual void DeviceAdded( const Device::Handle &d )
        { //{{{

            ScopedMutex     lock( &m_pool_mutex );

            if( ! m_pool )
                return;

            if( d->GetSerial().empty() )
            {
                // We shouldn't be here in normal use, the user would need to
                // explicitly assign a pool to a monitor that enumerated bare
                // (or broken) devices ...  so, bark and then ignore them.
                Log<0>( _("DevList::DeviceAdded: "
                          "not adding device with no serial number to the pool: %s\n"),
                                                            d->VerboseStr().c_str() );
                return;
            }

            if( m_device_options.empty() )
            {
                m_pool->AddSource( m_default_options.group,
                                   new BitBabbler( d, m_default_options, false ) );
                return;
            }

            for( BitBabbler::Options::List::iterator i = m_device_options.begin(),
                                                     e = m_device_options.end();
                                                     i != e; ++i )
            {
                if( i->id.Matches( d ) )
                {
                    m_pool->AddSource( i->group, new BitBabbler( d, *i, false ) );
                    return;
                }
            }

        } //}}}

        virtual void DeviceRemoved( const Device::Handle &d )
        { //{{{

            ScopedMutex     lock( &m_pool_mutex );

            if( m_pool != NULL )
                m_pool->RemoveSource( d );

        } //}}}


    public:

        DevList( unsigned vendorid, unsigned productid )
            : m_vendorid( vendorid )
            , m_productid( productid )
        {
            pthread_mutex_init( &m_pool_mutex, NULL );
        }

        virtual ~DevList()
        {
            pthread_mutex_destroy( &m_pool_mutex );
        }


        void AddDevicesToPool( const Pool::Handle               &pool,
                               const BitBabbler::Options        &default_options,
                               const BitBabbler::Options::List  &device_options )
        { //{{{

            {
                ScopedMutex     lock( &m_pool_mutex );

                if( m_pool != NULL )
                    m_pool->RemoveAllSources();

                m_pool            = pool;
                m_default_options = default_options;
                m_device_options  = device_options;
            }

            WarmplugAllDevices();

        } //}}}


        unsigned GetVendorID() const        { return m_vendorid; }
        unsigned GetProductID() const       { return m_productid; }

        std::string GetVendorIDStr() const  { return stringprintf("%04x", m_vendorid); }
        std::string GetProductIDStr() const { return stringprintf("%04x", m_productid); }

    }; //}}}


   #if HAVE_LIBUDEV

    class UDEVMonitor : public DevList
    { //{{{
    private:

        static const unsigned       m_qsize = 4096;

        struct udev                *m_udev;
        struct udev_monitor        *m_mon;

        pthread_t                   m_actor_thread;
        pthread_t                   m_monitor_thread;

        pthread_mutex_t             m_action_mutex;
        pthread_cond_t              m_action_cond;
        struct udev_device         *m_deviceq[ m_qsize ];
        unsigned                    m_deviceq_rd;
        unsigned                    m_deviceq_wr;


        void dump_udev_data( struct udev_device *d )
        { //{{{

            const char *action      = udev_device_get_action( d );
            const char *sysname     = udev_device_get_sysname( d );
            const char *driver      = udev_device_get_driver( d );
            const char *subsys      = udev_device_get_subsystem( d );
            const char *devtype     = udev_device_get_devtype( d );
            const char *devpath     = udev_device_get_devpath( d );
            const char *devnode     = udev_device_get_devnode( d );
            const char *syspath     = udev_device_get_syspath( d );
            const char *vendorid    = udev_device_get_sysattr_value( d, "idVendor" );
            const char *productid   = udev_device_get_sysattr_value( d, "idProduct" );
            const char *serial      = udev_device_get_sysattr_value( d, "serial" );
         // const char *vendorid    = udev_device_get_property_value( d, "ID_VENDOR_ID" );
         // const char *productid   = udev_device_get_property_value( d, "ID_MODEL_ID" );
         // const char *serial      = udev_device_get_property_value( d, "ID_SERIAL_SHORT" );

            Log<0>( "action: %s, sysname: %s, driver: %s\n", action, sysname, driver );
            Log<0>( "subsys: %s, devtype: %s, devpath: %s\n", subsys, devtype, devpath );
            Log<0>( "devnode: %s, syspath: %s\n", devnode, syspath );
            Log<0>( "vendorid: %s, productid: %s, serial: %s\n", vendorid, productid, serial );


            // This function was added in udev release 167.
           #if HAVE_UDEV_DEVICE_GET_SYSATTR_LIST_ENTRY

            struct udev_list_entry  *attrlist, *a;

            attrlist = udev_device_get_sysattr_list_entry( d );

            udev_list_entry_foreach( a, attrlist)
            {
                const char *key = udev_list_entry_get_name( a );

                if( ! key )
                    continue;

                const char *val = udev_device_get_sysattr_value( d, key );

                Log<0>( "  attribute %s = '%s'\n", key, val );
            }

           #endif


            struct udev_list_entry  *proplist, *p;

            proplist = udev_device_get_properties_list_entry( d );

            udev_list_entry_foreach( p, proplist )
            {
                const char *key = udev_list_entry_get_name( p );

                if( ! key )
                    continue;

                const char *val = udev_list_entry_get_value( p );

                Log<0>( "  property  %s = '%s'\n", key, val );
            }


            // This function was added in udev release 154.
           #if HAVE_UDEV_DEVICE_GET_TAGS_LIST_ENTRY

            struct udev_list_entry  *taglist, *t;

            taglist = udev_device_get_tags_list_entry( d );

            udev_list_entry_foreach( t, taglist )
            {
                const char *key = udev_list_entry_get_name( t );

                if( ! key )
                    continue;

                const char *val = udev_list_entry_get_value( t );

                Log<0>( "  tag       %s = '%s'\n", key, val );
            }

           #endif

        } //}}}


    private:

        Device::Handle new_device( unsigned             vendorid,
                                   unsigned             productid,
                                   unsigned             busnum,
                                   unsigned             devnum,
                                   const std::string   &mfg,
                                   const std::string   &product,
                                   const std::string   &serial,
                                   const std::string   &devport,
                                   const std::string   &devpath )
        { //{{{

            Device::Handle  d = find_device( busnum, devnum );

            if( ! d )
            {
                // This should only happen if we lose some race.
                // Either the device was unplugged again before we got here,
                // or libusb somehow doesn't know that it exists yet ...
                Log<0>( _("UDEVMonitor: failed to find device %03u:%03u\n"), busnum, devnum );
                return NULL;
            }

            if( d->GetVendorID() != vendorid || d->GetProductID() != productid )
            {
                // This really shouldn't ever happen ...
                Log<0>( _("UDEVMonitor: device matched devnum %03u:%03u "
                          "but mismatched vendor:product, %04x:%04x != %04x:%04x\n"),
                                            busnum, devnum, vendorid, productid,
                                            d->GetVendorID(), d->GetProductID() );
                return NULL;
            }

            // This shouldn't normally ever happen, but it can if the EEPROM was
            // rewritten or corrupted in some way.  Warn because possibly the
            // device just needs to be re-enumerated to correct the kernel's idea
            // of what is really there.  It can also happen if the user calling
            // this does not have permission to actually read from the device.
            if( mfg != d->GetManufacturer()
             || product != d->GetProduct()
             || serial != d->GetSerial() )
            {
                Log<1>( _("UDEVMonitor: expecting mfg '%s', product '%s', serial '%s', "
                          "but device %03u:%03u returned '%s', '%s', '%s'\n"),
                          mfg.c_str(), product.c_str(), serial.c_str(), busnum, devnum,
                          d->GetManufacturer().c_str(), d->GetProduct().c_str(),
                          d->GetSerial().c_str() );

                // On the assumption that most of the time this will actually be
                // just a permission problem, if the attempt to read the strings
                // from the device returned an empty value, set them to the values
                // we read from udev because they are probably correct, and this
                // is a lot let confusing to an unprivileged user who just wants
                // to see what devices are available.
                //
                // If they try to do something more than that it will fail anyway
                // because they really won't have permission for it.
                if( d->GetManufacturer().empty() )
                    d->SetManufacturer( mfg );

                if( d->GetProduct().empty() )
                    d->SetProduct( product );

                if( d->GetSerial().empty() )
                    d->SetSerial( serial );
            }

            if( d->GetDevicePort().empty() )
                d->SetDevicePort( devport );

            else if( devport != d->GetDevicePort() )
                Log<0>( _("UDEVMonitor: udev says device has port '%s', but libusb reported '%s'\n"),
                                                    devport.c_str(), d->GetDevicePort().c_str() );

            d->SetDevpath( devpath );

            return d;

        } //}}}

        Device::Handle new_device( unsigned     vendorid,
                                   unsigned     productid,
                                   const char  *busnum,
                                   const char  *devnum,
                                   const char  *mfg,
                                   const char  *product,
                                   const char  *serial,
                                   const char  *devport,
                                   const char  *devpath )
        { //{{{

            return new_device( vendorid, productid,
                               StrToU( busnum ? busnum : "", 10 ),
                               StrToU( devnum ? devnum : "", 10 ),
                               mfg     ? mfg : "",
                               product ? product : "",
                               serial  ? serial  : "",
                               devport ? devport : "",
                               devpath ? devpath : "" );
        } //}}}


        // Run the queue of device notifications.
        BB_NORETURN
        void __actor_thread()
        { //{{{

            SetThreadName( "hotplug event" );

            std::string     vid = GetVendorIDStr();
            std::string     pid = GetProductIDStr();

            for(;;)
            {
                while( m_deviceq_wr - m_deviceq_rd > 0 )
                {
                    // There is a window here, where if we get cancelled, a few
                    // new device notifications may have already been queued.
                    // We could install a fancier cleanup handler, to catch any
                    // that we might otherwise leak - but since right now, this
                    // thread should only be exiting if the whole app is being
                    // torn down, we'll just let the Big Reaper get them for us.

                    pthread_testcancel();
                    ScopedCancelState   cancelstate;

                    struct udev_device *d = m_deviceq[m_deviceq_rd++ % m_qsize];

                    if( opt_verbose > 3 )
                        dump_udev_data( d );


                    const char *action  = udev_device_get_action( d );
                    const char *devpath = udev_device_get_devpath( d );

                    if( ! action || ! devpath )
                    {
                        // Ensure strcmp won't explode if udev is braindead
                        Log<0>( _("UDEVMonitor: event with no action or devpath\n") );
                    }
                    else if( strcmp(action, "remove") == 0 )
                    {
                        RemoveDeviceByDevpath( devpath );
                    }
                    else if( strcmp(action, "add") == 0 )
                    {
                      // Alternatively, but less reliably:
                      //const char *vendorid    = udev_device_get_property_value( d, "ID_VENDOR_ID" );
                      //const char *productid   = udev_device_get_property_value( d, "ID_MODEL_ID" );
                        const char *vendorid    = udev_device_get_sysattr_value( d, "idVendor" );
                        const char *productid   = udev_device_get_sysattr_value( d, "idProduct" );

                        if( vendorid && productid && vendorid == vid && productid == pid )
                        {
                          //const char *serial  = udev_device_get_property_value( d, "ID_SERIAL_SHORT" );
                            const char *serial  = udev_device_get_sysattr_value( d, "serial" );

                            try {
                              //const char *mfg     = udev_device_get_property_value( d, "ID_VENDOR" );
                              //const char *product = udev_device_get_property_value( d, "ID_MODEL" );
                                const char *mfg     = udev_device_get_sysattr_value( d, "manufacturer" );
                                const char *product = udev_device_get_sysattr_value( d, "product" );
                                const char *busnum  = udev_device_get_sysattr_value( d, "busnum" );
                                const char *devnum  = udev_device_get_sysattr_value( d, "devnum" );
                                const char *devport = udev_device_get_sysattr_value( d, "devpath" );

                                Device::Handle h = new_device( GetVendorID(), GetProductID(),
                                                               busnum, devnum,
                                                               mfg, product, serial,
                                                               devport, devpath );
                                if( h != NULL )
                                    AddDevice( h );
                            }
                            BB_CATCH_ALL( 0, _("UDEVMonitor: add event exception") )
                        }
                    }

                    udev_device_unref( d );
                }


                ScopedMutex     lock( &m_action_mutex );

                pthread_cond_wait( &m_action_cond, &m_action_mutex );
            }

        } //}}}

        // Wait for udev to chirp at us.
        BB_NORETURN
        void __monitor_thread()
        { //{{{

            SetThreadName( "udev monitor" );

            for(;;)
            {
                struct udev_device *d = udev_monitor_receive_device( m_mon );

                if( ! d )
                    continue;

                ScopedCancelState   cancelstate;

                if( m_deviceq_wr - m_deviceq_rd < m_qsize )
                {
                    m_deviceq[m_deviceq_wr % m_qsize] = d;
                    m_deviceq_wr++;

                    ScopedMutex     lock( &m_action_mutex );

                    pthread_cond_broadcast( &m_action_cond );

                } else {

                    Log<0>( _("UDEVMonitor: *** queue full, packet dropped ***\n") );
                    udev_device_unref( d );
                }
            }

        } //}}}

        static void *actor_thread( void *p )
        { //{{{

            try {
                static_cast<UDEVMonitor*>(p)->__actor_thread();
            }
            catch( const abi::__forced_unwind& )
            {
                Log<3>( "UDEVMonitor: actor_thread cancelled\n" );
                throw;
            }
            BB_CATCH_STD( 0, _("uncaught actor_thread exception") )

            return NULL;

        } //}}}

        static void *monitor_thread( void *p )
        { //{{{

            try {
                static_cast<UDEVMonitor*>(p)->__monitor_thread();
            }
            catch( const abi::__forced_unwind& )
            {
                Log<3>( "UDEVMonitor: monitor_thread cancelled\n" );
                throw;
            }
            BB_CATCH_STD( 0, _("uncaught monitor_thread exception") )

            return NULL;

        } //}}}


    public:

        UDEVMonitor( unsigned vendorid = BB_VENDOR_ID, unsigned productid = BB_PRODUCT_ID )
            : DevList( vendorid, productid )
            , m_udev( udev_new() )
            , m_mon( NULL )
            , m_deviceq_rd( 0 )
            , m_deviceq_wr( 0 )
        { //{{{

            Log<2>( "+ UDEVMonitor( %04x:%04x )\n", vendorid, productid );

            if( ! m_udev )
                throw Error( _("UDEVMonitor: failed to create udev context") );

            try {
                m_mon = udev_monitor_new_from_netlink( m_udev, "udev" );
                if( ! m_mon )
                    throw Error( _("UDEVMonitor: failed to create udev monitor") );


                // Let the monitor socket block, it has its own thread here
                int     monfd       = udev_monitor_get_fd( m_mon );
                int     socketflags = fcntl( monfd, F_GETFL );

                if( socketflags == -1 )
                    throw SystemError( _("UDEVMonitor: failed to get monitor fd flags") );

                if( fcntl( monfd, F_SETFL, socketflags & ~O_NONBLOCK ) == -1 )
                    throw SystemError( _("UDEVMonitor: failed to clear monitor fd O_NONBLOCK") );


                udev_monitor_filter_add_match_subsystem_devtype( m_mon, "usb", "usb_device" );

                if( udev_monitor_enable_receiving( m_mon ) )
                    throw Error( _("UDEVMonitor: failed to bind udev monitor") );


                struct udev_enumerate   *e = udev_enumerate_new( m_udev );
                struct udev_list_entry  *devlist, *d;

                if( ! e )
                    throw Error( _("UDEVMonitor: failed to create enum context") );

                udev_enumerate_add_match_sysattr( e, "idVendor", GetVendorIDStr().c_str() );
                udev_enumerate_add_match_sysattr( e, "idProduct", GetProductIDStr().c_str() );
                udev_enumerate_scan_devices( e );
                devlist = udev_enumerate_get_list_entry( e );

                udev_list_entry_foreach( d, devlist )
                {
                    const char         *syspath = udev_list_entry_get_name( d );
                    struct udev_device *ud = udev_device_new_from_syspath( m_udev, syspath );

                    if( ! ud )
                    {
                        Log<0>( _("UDEVMonitor: failed to get device from syspath '%s'\n"),
                                                                                syspath );
                        continue;
                    }

                    if( opt_verbose > 3 )
                        dump_udev_data( ud );

                  // Alternatively, but less reliably:
                  //const char *serial  = udev_device_get_property_value( ud, "ID_SERIAL_SHORT" );
                  //const char *mfg     = udev_device_get_property_value( ud, "ID_VENDOR" );
                  //const char *product = udev_device_get_property_value( ud, "ID_MODEL" );
                    const char *serial  = udev_device_get_sysattr_value( ud, "serial" );
                    const char *mfg     = udev_device_get_sysattr_value( ud, "manufacturer" );
                    const char *product = udev_device_get_sysattr_value( ud, "product" );
                    const char *busnum  = udev_device_get_sysattr_value( ud, "busnum" );
                    const char *devnum  = udev_device_get_sysattr_value( ud, "devnum" );
                    const char *devport = udev_device_get_sysattr_value( ud, "devpath" );
                    const char *devpath = udev_device_get_devpath( ud );

                    try {
                        if( devpath )
                        {
                            Device::Handle h = new_device( GetVendorID(), GetProductID(),
                                                           busnum, devnum,
                                                           mfg, product, serial,
                                                           devport, devpath );
                            if( h != NULL )
                                AddDevice( h );

                        } else {
                            // Guard against udev being braindead
                            Log<0>( _("UDEVMonitor: device '%s' with no devpath\n"), syspath );
                        }
                    }
                    catch( ... )
                    {
                        udev_device_unref( ud );
                        udev_enumerate_unref( e );
                        throw;
                    }

                    udev_device_unref( ud );
                }

                udev_enumerate_unref( e );


                // Now wait in the background for things to change
                pthread_mutex_init( &m_action_mutex, NULL );
                pthread_cond_init( &m_action_cond, NULL );

                const pthread_attr_t    *attr = GetDefaultThreadAttr();

                int ret = pthread_create( &m_actor_thread, attr, actor_thread, this );
                if( ret )
                {
                    pthread_cond_destroy( &m_action_cond );
                    pthread_mutex_destroy( &m_action_mutex );

                    throw SystemError( ret, _("UDEVMonitor: failed to create actor thread") );
                }

                ret = pthread_create( &m_monitor_thread, attr, monitor_thread, this );
                if( ret )
                {
                    pthread_cancel( m_actor_thread );
                    pthread_join( m_actor_thread, NULL );

                    pthread_cond_destroy( &m_action_cond );
                    pthread_mutex_destroy( &m_action_mutex );

                    throw SystemError( ret, _("UDEVMonitor: failed to create monitor thread") );
                }
            }
            catch( ... )
            {
                if( m_mon )
                    udev_monitor_unref( m_mon );

                if( m_udev )
                    udev_unref( m_udev );

                throw;
            }

        } //}}}

        virtual ~UDEVMonitor()
        { //{{{

            if( m_mon )
            {
                Log<3>( "UDEVMonitor( %04x:%04x ): halting monitor threads\n",
                                                GetVendorID(), GetProductID() );

                pthread_cancel( m_actor_thread );
                pthread_cancel( m_monitor_thread );
                pthread_join( m_actor_thread, NULL );
                pthread_join( m_monitor_thread, NULL );

                udev_monitor_unref( m_mon );
            }

            if( m_udev )
                udev_unref( m_udev );

            pthread_cond_destroy( &m_action_cond );
            pthread_mutex_destroy( &m_action_mutex );

            Log<2>( "- UDEVMonitor( %04x:%04x )\n", GetVendorID(), GetProductID() );

        } //}}}


        virtual bool HasHotplugSupport() const
        {
            return true;
        }

    }; //}}}

    typedef UDEVMonitor     Devices;

   #else   // ! HAVE_LIBUDEV

    class DeviceList : public DevList
    { //{{{
    private:

       #if LIBUSB_SINCE(0x01000102)
        // Only available since 1.0.16

        libusb_hotplug_callback_handle  m_callbackhandle;


        static int LIBUSB_CALL hotplug_callback( libusb_context        *ctx,
                                                 libusb_device         *dev,
                                                 libusb_hotplug_event   event,
                                                 void                  *user_data )
        { //{{{

            (void)ctx;
            DeviceList  *devlist = static_cast<DeviceList*>( user_data );

            switch( event )
            {
                case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
                {
                    Device::Handle  d = new Device( dev );
                    devlist->AddDevice( d );
                    break;
                }

                case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
                    devlist->RemoveDevice( dev );
                    break;

                default:
                    Log<0>( _("DeviceList( %04x:%04x ): unexpected hotplug event type %d\n"),
                                    devlist->GetVendorID(), devlist->GetProductID(), event );
            }

            return 0;

        } //}}}

       #endif


    public:

        DeviceList( unsigned vendorid = BB_VENDOR_ID, unsigned productid = BB_PRODUCT_ID )
            : DevList( vendorid, productid )
        { //{{{

            Log<2>( "+ DeviceList( %04x:%04x )\n", vendorid, productid );

           #if LIBUSB_SINCE(0x01000102)
            // Only available since 1.0.16

            // Ensure this isn't uninitialised if registering the callback fails,
            // since we'll access it in the destructor.  Unfortunately we have to
            // rely on an implementation detail of libusb to do this, since it
            // doesn't give us a publicly defined 'invalid' handle to use.
            //
            // The handle IDs are integers, and as of libusb-1.0.19 at least, the
            // first valid handle is 1, and subsequent handles simply increment
            // the ID allocated, so we should be ok until integer wraparound, or
            // until someone changes how the handles work in that code.
            m_callbackhandle = 0;

            // FreeBSD 11 bumped LIBUSB_API_VERSION to 0x01000102, but didn't add
            // the libusb_has_capability() function ...  so if we're here, but
            // don't have it, let's assume it succeeded until some other platform
            // explodes in flames to say otherwise.
           #if HAVE_LIBUSB_HAS_CAPABILITY
            if( libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) )
           #endif
            {
               #if defined(__FreeBSD__) && __FreeBSD__ < 13
                // On FreeBSD 11, we're seeing a 4 second delay between when we
                // register the hotplug callback, and when it actually gets called
                // to enumerate any already available devices.  In "normal" use,
                // that probably isn't a major problem, but in the case where we
                // are only requested to scan for available devices, that means
                // we won't have any to report before it returns (without also
                // doing a semi-random sleep to wait for these events).  So until
                // we know whatever is causing that is fixed, on FreeBSD let's
                // enumerate them explictly first, and let AddDevice() deal with
                // weeding out the duplicates if/when we get them.
                //
                // There does seem to be some internal issue there, since we are
                // also seeing libusb_exit() block for 4 seconds at shutdown too.
                //
                // With FreeBSD 13 there is still a 4 second delay in libusb_exit
                // but devices are enumerated immediately so we don't need to do
                // this manually anymore.
                EnumerateDevices( vendorid, productid );
               #endif

                ScopedCancelState   cancelstate;

                int ret = libusb_hotplug_register_callback( GetContext(),
                                                            libusb_hotplug_event(
                                                             LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT |
                                                             LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED
                                                            ),
                                                            LIBUSB_HOTPLUG_ENUMERATE,
                                                            vendorid, productid,
                                                            LIBUSB_HOTPLUG_MATCH_ANY, // device class
                                                            hotplug_callback,
                                                            this,
                                                            &m_callbackhandle
                                                          );
                if( ret )
                    LogUSBError<0>( ret,
                                    _("DeviceList( %04x:%04x ): failed to register hotplug callback"),
                                    vendorid, productid );
                else
                    return;
            }

           #endif

            Log<2>( _("DeviceList: hotplug support not available\n") );

            // Scan for devices manually if we don't have hotplug support,
            // or if trying to enable it failed.
            EnumerateDevices( vendorid, productid );

        } //}}}

        ~DeviceList()
        { //{{{

            Log<2>( "- DeviceList( %04x:%04x )\n", GetVendorID(), GetProductID() );

           #if LIBUSB_SINCE(0x01000102)
            // Only available since 1.0.16

            ScopedCancelState   cancelstate;

            libusb_hotplug_deregister_callback( GetContext(), m_callbackhandle );

           #endif

        } //}}}


        virtual bool HasHotplugSupport() const
        { //{{{

           #if LIBUSB_SINCE(0x01000102)
            // Only available since 1.0.16

            // FreeBSD 11 bumped LIBUSB_API_VERSION to 0x01000102, but didn't add
            // the libusb_has_capability() function ...  so if we're here, but
            // don't have it, let's assume it succeeded until some other platform
            // exploded in flames to say otherwise.
            #if HAVE_LIBUSB_HAS_CAPABILITY
            return libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) ? true : false;
            #else
            return true;
            #endif

           #else

            return false;

           #endif

        } //}}}

    }; //}}}

    typedef DeviceList      Devices;

   #endif  // HAVE_LIBUDEV


}   // BitB namespace


#endif  // _BB_SECRET_SOURCE_H

// vi:sts=4:sw=4:et:foldmethod=marker
