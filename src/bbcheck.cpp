//  This file is distributed as part of the bit-babbler package.
//  Copyright 2014 - 2018,  Ron <ron@debian.org>

#include "private_setup.h"

#include <bit-babbler/secret-source.h>
#include <bit-babbler/term_escape.h>

#include <bit-babbler/impl/health-monitor.h>
#include <bit-babbler/impl/log.h>

#include <getopt.h>

using BitB::USBContext;
using BitB::BitBabbler;
using BitB::QA::Ent8;
using BitB::QA::BitRuns;
using BitB::StrToU;
using BitB::StrToScaledU;
using BitB::StrToScaledUL;
using BitB::StrToScaledD;
using BitB::Log;
using BitB::stringprintf;
using std::string;

#define CYAN_IF(s)  COLOUR_STR_IF(m_options.colour,CYAN,s)


class Test : public BitB::RefCounted
{ //{{{
public:

    typedef BitB::RefPtr< Test >    Handle;
    typedef std::list< Handle >     List;


    struct Options
    { //{{{

        size_t                  test_len;
        unsigned                block_size;
        unsigned                bitrate_max;
        unsigned                bitrate_min;
        bool                    show_all;
        bool                    colour;
        BitBabbler::Options     bboptions;

        Options()
            : test_len( 1024 * 1024 )
            , block_size( 65536 )
            , bitrate_max( 5000000 )
            , bitrate_min( 3000000 )
            , show_all( false )
            , colour( true )
        {}

    }; //}}}


    struct Result : public BitB::RefCounted
    { //{{{

        enum TestID
        { //{{{

            BITRUN_BIAS,
            BITRUN_CHISQ,

            ENT_ENTROPY,
            ENT_CHISQ,
            ENT_MEAN,
            ENT_PI,
            ENT_CORR,
            ENT_MINENTROPY,

            VALUE_MAX

        }; //}}}

        struct Rank
        { //{{{

            typedef std::vector< Rank >     Vector;

            enum Order
            {
                ASCENDING,      // Smallest values are best
                DESCENDING      // Largest values are best
            };

            size_t  index;
            double  value;
            Order   order;

            Rank( size_t i, double v, Order o = ASCENDING )
                : index( i )
                , value( v )
                , order( o )
            {}

            bool operator<( const Rank &r ) const
            {
                if( order == ASCENDING )
                    return value < r.value;

                return r.value < value;
            }

        }; //}}}

        struct Ranking
        { //{{{

            Rank::Vector    results[ VALUE_MAX ];


            void AddResult( TestID t, const Rank &r )
            {
                results[t].push_back( r );
            }

            void SortResults()
            {
                for( size_t i = 0; i < VALUE_MAX; ++i )
                    std::stable_sort( results[i].begin(), results[i].end() );
            }


            const char *ResultColour( TestID t, size_t test_index ) const
            { //{{{

                // We're not terribly precious about colour palettes here, but
                // normal bold yellow is insanely bright and as the 3rd best option
                // we don't want it screaming for the most attention, so make the
                // 2 - 4th place options about the same brightness, and darker than
                // the best one, and make the worst one even darker still, so the
                // red doesn't stand out too intensely either.

                size_t  n = results[t].size();

                if( n > 0 && results[t][0].index == test_index )
                    return BOLD_GREEN;

                if( n > 1 && results[t][1].index == test_index )
                    return MID_GREEN;

                if( n > 2 && results[t][2].index == test_index )
                    return MID_YELLOW;

                if( n > 3 && results[t][3].index == test_index )
                    return MID_ORANGE;


                if( n > 4 && results[t][n-1].index == test_index )
                    return DARK_RED;

                return "";

            } //}}}

        }; //}}}


        typedef BitB::RefPtr< Result >      Handle;
        typedef std::vector< Handle >       Vector;

        unsigned            bitrate;
        unsigned            enable_mask;
        Ent8::Data          ent8;
        BitRuns::Result     bitruns;


        Result( unsigned                bitrate,
                unsigned                enable_mask,
                const Ent8::Data       &ent8,
                const BitRuns::Result  &bitruns )
            : bitrate( bitrate )
            , enable_mask( enable_mask )
            , ent8( ent8 )
            , bitruns( bitruns )
        {}


        void RankResults( size_t i, Ranking &r )
        { //{{{

            const Ent8::Result  &e8 = ent8.result[Ent8::CURRENT];

            r.AddResult( BITRUN_BIAS,    Rank( i, fabs(1.0 - bitruns.GetBias()) ) );
            r.AddResult( BITRUN_CHISQ,   Rank( i, bitruns.GetChisq() ) );
            r.AddResult( ENT_ENTROPY,    Rank( i, e8.entropy, Rank::DESCENDING ) );
            r.AddResult( ENT_CHISQ,      Rank( i, e8.chisq ) );
            r.AddResult( ENT_MEAN,       Rank( i, fabs(127.5 - e8.mean) ) );
            r.AddResult( ENT_PI,         Rank( i, fabs(e8.PiError()) ) );
            r.AddResult( ENT_CORR,       Rank( i, fabs(e8.corr) ) );
            r.AddResult( ENT_MINENTROPY, Rank( i, e8.minentropy, Rank::DESCENDING ) );

        } //}}}

        void Report( size_t n, Ranking &r )
        { //{{{

            if( enable_mask == 0x0f )
                printf("%u Hz\n", bitrate );
            else if( BitB::popcount( enable_mask ) == 1 )
                printf("%u Hz, generator %u\n", bitrate, BitB::fls(enable_mask) - 1 );
            else
                printf("%u Hz, generator mask 0x%02x\n", bitrate, enable_mask );

            double  chisqp;
            double  chisq = bitruns.GetChisq( &chisqp );

            printf( "Max run of %3zu (expected %3zu), bias %s%.9f%s, χ² %s%.3f%s (p = %f)\n",
                    bitruns.maxrun, bitruns.GetExpectedMax(),
                    r.ResultColour(BITRUN_BIAS, n),  bitruns.GetBias(), END_COLOUR,
                    r.ResultColour(BITRUN_CHISQ, n), chisq,             END_COLOUR,
                    chisqp );

            const Ent8::Result  &e8 = ent8.result[Ent8::CURRENT];

            printf( "Ent8: Hs %s%f%s, Hm %s%f%s, Mean %s%f%s, Corr %s% .8f%s,"
                                   " π %s%.8f%s (% .5f), χ² %s%f%s (%.2f)\n",
                    r.ResultColour(ENT_ENTROPY, n),    e8.entropy,    END_COLOUR,
                    r.ResultColour(ENT_MINENTROPY, n), e8.minentropy, END_COLOUR,
                    r.ResultColour(ENT_MEAN, n),       e8.mean,       END_COLOUR,
                    r.ResultColour(ENT_CORR, n),       e8.corr,       END_COLOUR,
                    r.ResultColour(ENT_PI, n),         e8.pi,         END_COLOUR,
                    e8.PiError(),
                    r.ResultColour(ENT_CHISQ, n),      e8.chisq,      END_COLOUR,
                    e8.ChisqProb() );
        } //}}}

        void Report()
        { //{{{

            if( enable_mask == 0x0f )
                printf("%u Hz\n", bitrate );
            else if( BitB::popcount( enable_mask ) == 1 )
                printf("%u Hz, generator %u\n", bitrate, BitB::fls(enable_mask) - 1 );
            else
                printf("%u Hz, generator mask 0x%02x\n", bitrate, enable_mask );

            double  chisqp;
            double  chisq = bitruns.GetChisq( &chisqp );

            printf( "Max run of %3zu (expected %3zu), bias %.9f, χ² %.3f (p = %f)\n",
                                    bitruns.maxrun, bitruns.GetExpectedMax(),
                                    bitruns.GetBias(), chisq, chisqp );

            printf( "Ent8: %s\n", ent8.result[Ent8::CURRENT].Report().c_str() );

        } //}}}

    }; //}}}


private:

    USBContext::Device::Handle  m_dev;
    Options                     m_options;
    string                      m_id;
    pthread_t                   m_threadid;
    uint8_t                    *m_buf;

    Result::Vector              m_results;


    static unsigned DecrementBitrate( unsigned rate )
    {
        return 30000000 / (30000000 / rate + 1);
    }


    void run_test( const BitBabbler::Options &bbo )
    { //{{{

        static unsigned nchunks[] = { 16, 10, 8, 5, 2 };

        BitBabbler  b( m_dev, bbo );

        unsigned    fold        = b.GetFolding();
        size_t      bs          = m_options.block_size;
        size_t      len         = m_options.test_len * (1u << fold);
        unsigned    sec         = unsigned(len * 8 / bbo.bitrate);
        unsigned    min         = sec / 60;
        size_t      e8short_len = m_options.test_len;

        for( size_t i = 0; i < sizeof(nchunks) / sizeof(*nchunks); ++i )
        {
            if( m_options.test_len % nchunks[i] == 0 )
            {
                e8short_len = m_options.test_len / nchunks[i];
                break;
            }
        }

        if( min )
            Log<1>( _("Test %s reading %zu samples at %u Hz (will take ~%u:%02u min)\n"),
                                m_id.c_str(), len, bbo.bitrate, min, sec - 60 * min );
        else
            Log<1>( _("Test %s reading %zu samples at %u Hz (will take ~%u sec)\n"),
                                            m_id.c_str(), len, bbo.bitrate, sec );

        BitRuns     bitruns;
        Ent8        ent8( e8short_len );

        for( size_t n = 0; n < len; )
        {
            size_t  rs = std::min( bs, len );

            for( size_t r = 0; r < rs; )
                r += b.read( m_buf + r, std::min( size_t(65536), rs - r ) );

            len -= rs;

            size_t  flen = BitB::FoldBytes( m_buf, rs, fold );

            ent8.Analyse( m_buf, flen );
            bitruns.AddBits( m_buf, flen );
        }

        bitruns.flush();

        if( m_options.show_all )
        {
            // We need to globally mutex this block so that tests which complete
            // at the same time don't intermingle the output of their results.
            static pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER;

            BitB::ScopedMutex   lock( &mutex );

            if( bbo.enable_mask == 0x0f )
                printf( CYAN_IF("\n%s %u Hz\n"), m_id.c_str(), bbo.bitrate );

            else if( BitB::popcount( bbo.enable_mask ) == 1 )
                printf( CYAN_IF("\n%s %u Hz, generator %u\n"), m_id.c_str(),
                                bbo.bitrate, BitB::fls(bbo.enable_mask) - 1 );
            else
                printf( CYAN_IF("\n%s %u Hz, generator mask 0x%02x\n"),
                                m_id.c_str(), bbo.bitrate, bbo.enable_mask );

            printf( "%s\n", bitruns.GetResult().Report().c_str() );
            printf( "\n Ent8 short, %s\n", ent8.ShortTermData().ReportResults().c_str() );
            printf( "\n Ent8 long, %s\n", ent8.LongTermData().ReportResults().c_str() );
        }

        m_results.push_back( new Result( bbo.bitrate,
                                         bbo.enable_mask,
                                         ent8.LongTermData(),
                                         bitruns.GetResult() ) );
    } //}}}

    void run_test_thread()
    { //{{{

        BitBabbler::Options     bbo = m_options.bboptions;

        m_buf = new uint8_t[ m_options.block_size ];

        for( bbo.bitrate = m_options.bitrate_max;
             bbo.bitrate >= m_options.bitrate_min;
             bbo.bitrate = DecrementBitrate( bbo.bitrate ) )
        {

            if( (m_options.bboptions.enable_mask & 0xf) == 0 )
            {
                bbo.enable_mask = 1;
                for( size_t i = 0; i < 4; ++i )
                {
                    run_test( bbo );
                    bbo.enable_mask <<= 1;
                }

                if( (m_options.bboptions.enable_mask & 0x10) == 0 )
                    continue;

                bbo.enable_mask = 0x0f;
            }
            run_test( bbo );
        }

    } //}}}

    static void *test_thread( void *p )
    { //{{{

        Test::Handle    h = static_cast<Test*>( p );

        // Drop the 'virtual handle' from the ctor, we have a real one now.
        h->Unref();

        try {
            h->run_test_thread();
        }
        catch( const abi::__forced_unwind& )
        {
            Log<3>( _("Test %s thread cancelled\n"), h->m_id.c_str() );
            throw;
        }
        BB_CATCH_STD( 0, stringprintf( _("uncaught exception in Test %s thread"),
                                                    h->m_id.c_str() ).c_str() )
        return NULL;

    } //}}}

    void begin_tests()
    { //{{{

        using BitB::SystemError;

        // Bump the refcount until the thread is started, otherwise we
        // may lose a race with this Test being released by the caller
        // before the thread can take its handle from the raw pointer.
        // Think of it as a virtual Handle passed with pthread_create.
        //
        // In practice, this isn't actually a problem in the current code
        // because the only time the Test might be destroyed before the
        // thread has run its course is if we're crash diving our way out
        // after getting an early termination signal, when no unwinding
        // will be done anyway.  But conceptually the problem is real in
        // this class, so handle it correctly in case future use changes.
        Ref();

        // We don't need to Unref() if this fails, because we'll throw
        // and it will never have been constructed to be destroyed ...
        // That assumes this method is only ever called from the ctor,
        // which currently is true.
        int ret = pthread_create( &m_threadid, BitB::GetDefaultThreadAttr(),
                                  test_thread, this );
        if( ret )
            throw SystemError( ret, _("Test %s failed to create thread"), m_id.c_str() );

    } //}}}


public:

    Test( const USBContext::Device::Handle &dev, const Options &options )
        : m_dev( dev )
        , m_options( options )
        , m_id( dev->GetSerial() )
        , m_buf( NULL )
    {
        begin_tests();
    }

    ~Test()
    {
        delete [] m_buf;
    }


    void WaitForCompletion() const
    {
        pthread_join( m_threadid, NULL );
        Log<1>( _("Test %s completed\n"), m_id.c_str() );
    }

    void ReportResults() const
    { //{{{

        unsigned bitrate = 0;

        printf( "\n%s:\n", m_id.c_str() );

        if( ! m_options.colour )
        {
            // We could just disable colouring in Result::Ranking, but we don't
            // actually need to do the collation and sorting if we aren't going
            // to indicate the relative ranking of the results anyway.

            for( size_t i = 0, n = m_results.size(); i < n; ++i )
            {
                if( bitrate && m_results[i]->bitrate != bitrate )
                    putchar('\n');

                bitrate = m_results[i]->bitrate;
                m_results[i]->Report();
            }

            return;
        }


        Result::Ranking     r;

        for( size_t i = 0, n = m_results.size(); i < n; ++i )
            m_results[i]->RankResults( i, r );

        r.SortResults();

        for( size_t i = 0, n = m_results.size(); i < n; ++i )
        {
            if( bitrate && m_results[i]->bitrate != bitrate )
                putchar('\n');

            bitrate = m_results[i]->bitrate;
            m_results[i]->Report( i, r );
        }

    } //}}}

}; //}}}



static void usage()
{
    printf("Usage: bbcheck [OPTION...]\n");
    printf("\n");
    printf("Run automated tests on BitBabbler hardware RNG devices\n");
    printf("\n");
    printf("Options:\n");
    printf("  -s, --scan                Scan for available devices\n");
    printf("  -i, --device-id=id        Read from only the selected device(s)\n");
    printf("  -r, --bitrate=Hz[:Hz max] Set the bitrate range to scan\n");
    printf("  -b, --bytes=n             The number of bytes to test\n");
    printf("  -B, --block-size=bytes    Set the folding block size\n");
    printf("  -A, --all-results         Show all results, not just the summary\n");
    printf("  -v, --verbose             Enable verbose output\n");
    printf("      --no-colour           Don't colourise final results\n");
    printf("  -?, --help                Show this help message\n");
    printf("      --version             Print the program version\n");
    printf("\n");
    printf("Per device options:\n");
    printf("      --latency=ms          Override the USB latency timer\n");
    printf("  -f, --fold=n              Set the amount of entropy folding\n");
    printf("      --enable-mask=mask    Select a subset of the generators\n");
    printf("      --limit-max-xfer      Limit the transfer chunk size to 16kB\n");
    printf("\n");
    printf("Report bugs to support@bitbabbler.org\n");
    printf("\n");
}

int main( int argc, char *argv[] )
{
  try {

    unsigned                    opt_scan        = 0;
    Test::Options               opt_testoptions;

    BitBabbler::Options         default_options;
    BitBabbler::Options::List   device_options;

    enum
    {
        LATENCY_OPT,
        ENABLEMASK_OPT,
        LIMIT_MAX_XFER,
        NOCOLOUR_OPT,
        VERSION_OPT
    };

    struct option long_options[] =
    {
        { "scan",           no_argument,        NULL,      's' },
        { "device-id",      required_argument,  NULL,      'i' },
        { "bitrate",        required_argument,  NULL,      'r' },
        { "bytes",          required_argument,  NULL,      'b' },
        { "block-size",     required_argument,  NULL,      'B' },
        { "latency",        required_argument,  NULL,      LATENCY_OPT },
        { "fold",           required_argument,  NULL,      'f' },
        { "enable-mask",    required_argument,  NULL,      ENABLEMASK_OPT },
        { "limit-max-xfer", no_argument,        NULL,      LIMIT_MAX_XFER },
        { "no-colour",      no_argument,        NULL,      NOCOLOUR_OPT },
        { "all-results",    no_argument,        NULL,      'A' },
        { "verbose",        no_argument,        NULL,      'v' },
        { "help",           no_argument,        NULL,      '?' },
        { "version",        no_argument,        NULL,      VERSION_OPT },
        { 0, 0, 0, 0 }
    };

    int opt_index = 0;

    for(;;)
    { //{{{

        int c = getopt_long( argc, argv, ":si:r:b:B:f:Av?",
                                long_options, &opt_index );
        if( c == -1 )
            break;

        switch(c)
        {
            case 's':
                opt_scan = 1;
                break;

            case 'i':
            {
                BitBabbler::Options     bbo = default_options;

                try {
                    bbo.id = optarg;
                }
                catch( const std::exception &e )
                {
                    fprintf( stderr, "%s: error, %s\n", argv[0], e.what() );
                    return EXIT_FAILURE;
                }

                device_options.push_back( bbo );
                break;
            }

            case 'r':
            {
                string  r( optarg );
                size_t  n = r.find(':');

                if( n == string::npos )
                {
                    opt_testoptions.bitrate_min =
                    opt_testoptions.bitrate_max =
                        BitBabbler::RealBitrate( unsigned(StrToScaledD(optarg)) );

                } else {

                    opt_testoptions.bitrate_min =
                        BitBabbler::RealBitrate( unsigned(StrToScaledD(r.substr(0,n))) );
                    opt_testoptions.bitrate_max =
                        BitBabbler::RealBitrate( unsigned(StrToScaledD(r.substr(n+1))) );
                }
                break;
            }

            case 'b':
                opt_testoptions.test_len = StrToScaledUL( optarg, 1024 );
                break;

            case 'B':
                opt_testoptions.block_size = StrToScaledU( optarg, 1024 );
                break;

            case LATENCY_OPT:
            {
                unsigned latency = StrToU( optarg, 10 );

                if( device_options.empty() )
                    default_options.latency = latency;
                else
                    device_options.back().latency = latency;

                break;
            }

            case 'f':
            {
                unsigned fold = StrToU( optarg, 10 );

                if( device_options.empty() )
                    default_options.fold = fold;
                else
                    device_options.back().fold = fold;

                break;
            }

            case ENABLEMASK_OPT:
            {
                unsigned mask = StrToU( optarg );

                if( device_options.empty() )
                    default_options.enable_mask = mask;
                else
                    device_options.back().enable_mask = mask;

                break;
            }

            case LIMIT_MAX_XFER:
                if( device_options.empty() )
                    default_options.chunksize = 16384;
                else
                    device_options.back().chunksize = 16384;

                break;

            case NOCOLOUR_OPT:
                opt_testoptions.colour = false;
                break;

            case 'A':
                opt_testoptions.show_all = true;
                break;

            case 'v':
                ++BitB::opt_verbose;
                break;

            case '?':
                if( optopt != '?' && optopt != 0 )
                {
                    fprintf(stderr, "%s: invalid option -- '%c', try --help\n",
                                                            argv[0], optopt);
                    return EXIT_FAILURE;
                }
                usage();
                return EXIT_SUCCESS;

            case ':':
                fprintf(stderr, "%s: missing argument for '%s', try --help\n",
                                                    argv[0], argv[optind - 1] );
                return EXIT_FAILURE;

            case VERSION_OPT:
                printf("bbcheck " PACKAGE_VERSION "\n");
                return EXIT_SUCCESS;
        }

    } //}}}


    BitB::Devices   d;

    if( opt_scan )
    {
        d.ListDevices();
        return EXIT_SUCCESS;
    }
    else if( d.GetNumDevices() == 0 )
    {
        fprintf( stderr, _("bbcheck: No devices found, aborting.\n") );
        return EXIT_FAILURE;
    }


    Test::List      tests;

    if( device_options.empty() )
    {
        // We don't want devices that were hotplugged after this was started to
        // be considered here. This isn't a daemon, it just runs a one-shot set
        // of tests, so start them up for just the currently available set.
        USBContext::Device::List    devices = d.GetDevices();

        opt_testoptions.bboptions = default_options;

        for( USBContext::Device::List::iterator i = devices.begin(),
                                                e = devices.end(); i != e; ++i )
            tests.push_back( new Test( *i, opt_testoptions ) );

    } else {

        for( BitBabbler::Options::List::const_iterator i = device_options.begin(),
                                                       e = device_options.end();
                                                       i != e; ++i )
        {
            opt_testoptions.bboptions = *i;
            tests.push_back( new Test( d.GetDevice( i->id ), opt_testoptions ) );
        }
    }

    for( Test::List::iterator i = tests.begin(), e = tests.end(); i != e; ++i )
        (*i)->WaitForCompletion();

    for( Test::List::iterator i = tests.begin(), e = tests.end(); i != e; ++i )
        (*i)->ReportResults();


    return EXIT_SUCCESS;
  }
  BB_CATCH_ALL( 0, _("bbcheck fatal exception") )

  return EXIT_FAILURE;
}

// vi:sts=4:sw=4:et:foldmethod=marker
