//  This file is distributed as part of the bit-babbler package.
//  Copyright 2014 - 2018,  Ron <ron@debian.org>

#include "private_setup.h"

#include <bit-babbler/client-socket.h>
#include <bit-babbler/qa.h>

#include <bit-babbler/impl/log.h>

#include <unistd.h>
#include <getopt.h>


using BitB::Json;
using BitB::ClientSock;
using BitB::QA::Ent8;
using BitB::QA::Ent16;
using BitB::StrToU;
using BitB::StrToScaledU;
using BitB::StrToScaledUL;
using BitB::Error;
using BitB::Log;
using BitB::stringprintf;
using std::string;


static void usage()
{
    printf("Usage: bbctl [OPTION...]\n");
    printf("\n");
    printf("Query and control tool for BitBabbler hardware RNG devices\n");
    printf("\n");
    printf("Options:\n");
    printf("  -s, --scan                Scan for active devices\n");
    printf("  -i, --device-id=id        Act on only a single device\n");
    printf("  -b, --bin-freq            Report the 8-bit symbols sorted by frequency\n");
    printf("  -B, --bin-freq16          Report the 16-bit symbols sorted by frequency\n");
    printf("      --bin-count           Report the 8-bit symbols in symbol order\n");
    printf("      --bin-count16         Report the 16-bit symbols in symbol order\n");
    printf("      --first=n             Show only the first n bins\n");
    printf("      --last=n              Show only the last n bins\n");
    printf("  -r, --bit-runs            Report on runs of consecutive bits\n");
    printf("  -S, --stats               Report general QA statistics\n");
    printf("  -c, --control-socket=path The service socket to query\n");
    printf("  -V, --log-verbosity=n     Change the logging verbosity\n");
    printf("      --waitfor=dev:n:r:max Wait for a device to pass some number of bytes\n");
    printf("  -v, --verbose             Enable verbose output\n");
    printf("  -?, --help                Show this help message\n");
    printf("      --version             Print the program version\n");
    printf("\n");
    printf("Report bugs to support@bitbabbler.org\n");
    printf("\n");
}


struct WaitFor
{ //{{{

    typedef std::list< WaitFor >    List;

    std::string     deviceid;
    size_t          bytes;
    size_t          retry_ms;
    size_t          timeout_ms;

    WaitFor( const std::string &arg )
        : retry_ms( 1000 )
        , timeout_ms( 0 )
    { //{{{

        // Parse the options from a string of the form:
        // device:bytes:retry_ms:timeout_ms
        // Where device and bytes are mandatory.

        size_t  n = arg.find( ':' );

        if( n == string::npos )
            throw Error( _("No byte count given in --waitfor=%s"), arg.c_str() );

        deviceid = arg.substr( 0, n );
        ++n;

        try {
            size_t  n2 = arg.find( ':', n );

            if( n2 == string::npos )
            {
                bytes = StrToScaledUL( arg.substr(n), 1024 );
                return;
            }

            bytes   = StrToScaledUL( arg.substr(n, n2 - n), 1024 );
            n       = n2 + 1;
            n2      = arg.find( ':', n );

            if( n2 == string::npos )
            {
                retry_ms = StrToScaledUL( arg.substr(n) );
                goto done;
            }

            retry_ms    = StrToScaledUL( arg.substr(n, n2 - n) );
            n           = n2 + 1;
            timeout_ms  = StrToScaledUL( arg.substr(n) );

        done:
            if( retry_ms < 1 )
                throw Error( _("Retry time must be >= 1ms in --waitfor=%s"),
                                                                arg.c_str() );
        }
        catch( const std::exception &e )
        {
            throw Error( _("Invalid --waitfor argument '%s': %s"),
                                            arg.c_str(), e.what() );
        }

    } //}}}

}; //}}}


int main( int argc, char *argv[] )
{
  try {

    unsigned        opt_scan        = 0;
    unsigned        opt_bin_count   = 0;
    unsigned        opt_bin_freq    = 0;
    unsigned        opt_bit_runs    = 0;
    unsigned        opt_stats       = 0;
    unsigned        opt_first       = 65536;
    unsigned        opt_last        = 65536;
    unsigned        opt_log_level   = unsigned(-1);
    string          opt_deviceid;
    string          opt_controlsock = SEEDD_CONTROL_SOCKET;
    WaitFor::List   opt_wait;

    enum
    {
        BINCOUNT_OPT,
        BINCOUNT16_OPT,
        FIRST_OPT,
        LAST_OPT,
        WAITFOR_OPT,
        VERSION_OPT
    };

    struct option long_options[] =
    {
        { "scan",           no_argument,        NULL,      's' },
        { "device-id",      required_argument,  NULL,      'i' },
        { "bin-freq",       no_argument,        NULL,      'b' },
        { "bin-freq16",     no_argument,        NULL,      'B' },
        { "bin-count",      no_argument,        NULL,      BINCOUNT_OPT },
        { "bin-count16",    no_argument,        NULL,      BINCOUNT16_OPT },
        { "first",          required_argument,  NULL,      FIRST_OPT },
        { "last",           required_argument,  NULL,      LAST_OPT },
        { "bit-runs",       no_argument,        NULL,      'r' },
        { "stats",          no_argument,        NULL,      'S' },
        { "control-socket", required_argument,  NULL,      'c' },
        { "log-verbosity",  required_argument,  NULL,      'V' },
        { "waitfor",        required_argument,  NULL,      WAITFOR_OPT },
        { "verbose",        no_argument,        NULL,      'v' },
        { "help",           no_argument,        NULL,      '?' },
        { "version",        no_argument,        NULL,      VERSION_OPT },
        { 0, 0, 0, 0 }
    };

    int opt_index = 0;

    for(;;)
    { //{{{

        int c = getopt_long( argc, argv, ":si:c:bBrSV:v?",
                             long_options, &opt_index );
        if( c == -1 )
            break;

        switch(c)
        {
            case 's':
                opt_scan = 1;
                break;

            case 'i':
                opt_deviceid = optarg;
                break;

            case 'b':
                opt_bin_freq = 1;
                break;

            case 'B':
                opt_bin_freq = 16;
                break;

            case BINCOUNT_OPT:
                opt_bin_freq = 1;
                opt_bin_count = 1;
                break;

            case BINCOUNT16_OPT:
                opt_bin_freq = 16;
                opt_bin_count = 1;
                break;

            case FIRST_OPT:
                opt_first = StrToScaledU( optarg );
                if( opt_last == 65536 )
                    opt_last = 0;

                break;

            case LAST_OPT:
                opt_last = StrToScaledU( optarg );
                if( opt_first == 65536 )
                    opt_first = 0;
                break;

            case 'r':
                opt_bit_runs = 1;
                break;

            case 'S':
                opt_stats = 1;
                break;

            case 'c':
                opt_controlsock = optarg;
                break;

            case 'V':
                opt_log_level = StrToU( optarg, 10 );
                break;

            case WAITFOR_OPT:
                opt_wait.push_back( WaitFor(optarg) );
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
                printf("bbctl " PACKAGE_VERSION "\n");
                return EXIT_SUCCESS;
        }

    } //}}}


    ClientSock      client( opt_controlsock );


    if( opt_log_level != unsigned(-1) )
    { //{{{

        client.SendRequest( stringprintf( "[\"SetLogVerbosity\",0,%u]",
                                                        opt_log_level ) );
        Json::Handle    json = client.Read();

        Log<4>("read reply: %s\n", json->JSONStr().c_str() );

        if( json[0]->String() == "SetLogVerbosity" )
        {
            printf( "Log verbosity is now %u\n", unsigned(json[2]) );

        } else {

            Log<0>( "unrecognised reply\n" );
        }

    } //}}}


    if( opt_scan )
    { //{{{

        client.SendRequest( "\"GetIDs\"" );

        Json::Handle    json = client.Read();

        Log<4>("read reply: %s\n", json->JSONStr().c_str() );

        if( json[0]->String() == "GetIDs" )
        {
            Json::Data::Handle  ids  = json[2];
            size_t              n    = ids->GetArraySize();

            printf( P_("Have %zu active device:\n",
                       "Have %zu active devices:\n", n), n );

            for( size_t i = 0; i < n; ++i )
                printf( _("  Device ID: %s\n"), ids[i]->String().c_str() );

        } else {

            Log<0>( "unrecognised reply\n" );
        }

    } //}}}


    while( ! opt_wait.empty() )
    { //{{{

        const WaitFor   &w = opt_wait.front();
        size_t          elapsed;

        if( w.timeout_ms )
            Log<1>( _("Waiting up to %zu ms for %zu good bytes from %s\n"),
                                w.timeout_ms, w.bytes, w.deviceid.c_str() );
        else
            Log<1>( _("Waiting for %zu good bytes from %s\n"),
                                              w.bytes, w.deviceid.c_str() );

        for( elapsed = 0; w.timeout_ms == 0 || elapsed < w.timeout_ms;
                                                        elapsed += w.retry_ms )
        {
            client.SendRequest( "[\"ReportStats\",1,\"" + w.deviceid + "\"]" );

            Json::Handle    json = client.Read();

            Log<4>("read reply: %s\n", json->JSONStr().c_str() );

            if( json[0]->String() == "ReportStats" )
            {
                Json::Data::Handle  stats = json[2]->Get( w.deviceid );

                if( ! stats )
                    throw Error( _("No statistics available for device '%s'"),
                                                         w.deviceid.c_str() );

                unsigned long long  passed = stats["QA"]["BytesPassed"]->
                                                    As<unsigned long long>();
                if( passed >= w.bytes )
                {
                    Log<1>( _("Have %llu good bytes from %s in %zums\n"),
                                    passed, w.deviceid.c_str(), elapsed );
                    goto done;
                }

                Log<3>( _("Have %llu good bytes from %s in %zums (waiting for %zu)\n"),
                                        passed, w.deviceid.c_str(), elapsed, w.bytes );
            } else {

                // Possibly we should throw here too, but since this should
                // never happen, assume it's a glitch and just try again.
                Log<0>( "Unrecognised reply to ReportStats request\n" );
            }

            usleep( useconds_t( w.retry_ms * 1000 ) );
        }

        throw Error( _("Timeout after %zums waiting for %zu bytes from %s\n"),
                                    elapsed, w.bytes, w.deviceid.c_str() );

    done:
        opt_wait.pop_front();

    } //}}}


    if( opt_bin_freq )
    { //{{{

        if( opt_deviceid.empty() )
            client.SendRequest( "\"GetRawData\"" );
        else
            client.SendRequest( "[\"GetRawData\",1,\"" + opt_deviceid + "\"]" );

        Json::Handle    json = client.Read();

        Log<4>("read reply: %s\n", json->JSONStr().c_str() );

        if( json[0]->String() == "GetRawData" )
        {
            Json::Data::Handle  data = json[2];
            Json::MemberList    sources;

            data->GetMembers( sources );

            for( Json::MemberList::iterator si = sources.begin(),
                                            se = sources.end(); si != se; ++si )
            {
                Json::Data::Handle  ent8  = data[*si]->Get("Ent8");
                Json::Data::Handle  ent16 = data[*si]->Get("Ent16");

                if( ! ent8 )
                {
                    printf( "\nsource: %s has no 8-bit data (yet)\n", si->c_str() );

                } else {

                    Ent8::Data  e8_short( ent8["Short"] );
                    Ent8::Data  e8_long( ent8["Long"] );

                    if( opt_bin_count )
                    {
                        printf( "\nsource: %s\n%s\n",
                                si->c_str(), e8_short.ReportBins( opt_first, opt_last ).c_str() );
                        printf( "\nsource: %s\n%s\n",
                                si->c_str(), e8_long.ReportBins( opt_first, opt_last ).c_str() );

                    } else {

                        printf( "\nsource: %s\n%s\n",
                                si->c_str(), e8_short.ReportBinsByFreq( opt_first, opt_last ).c_str() );
                        printf( "\nsource: %s\n%s\n",
                                si->c_str(), e8_long.ReportBinsByFreq( opt_first, opt_last ).c_str() );
                    }
                }

                if( opt_bin_freq == 16 )
                {
                    if( ! ent16 )
                    {
                        printf( "\nsource: %s has no 16-bit data (yet)\n", si->c_str() );

                    } else {

                        Ent16::Data  e16_short( ent16["Short"] );
                        Ent16::Data  e16_long( ent16["Long"] );

                        if( opt_bin_count )
                        {
                            printf( "\nsource: %s\n%s\n",
                                    si->c_str(), e16_short.ReportBins( opt_first, opt_last ).c_str() );
                            printf( "\nsource: %s\n%s\n",
                                    si->c_str(), e16_long.ReportBins( opt_first, opt_last ).c_str() );

                        } else {

                            printf( "\nsource: %s\n%s\n",
                                    si->c_str(), e16_short.ReportBinsByFreq( opt_first, opt_last ).c_str() );
                            printf( "\nsource: %s\n%s\n",
                                    si->c_str(), e16_long.ReportBinsByFreq( opt_first, opt_last ).c_str() );
                        }
                    }
                }
            }

        } else {

            Log<0>( "unrecognised reply\n" );
        }

    } //}}}


    if( opt_bit_runs )
    { //{{{

        using BitB::QA::BitRuns;

        if( opt_deviceid.empty() )
            client.SendRequest( "\"ReportStats\"" );
        else
            client.SendRequest( "[\"ReportStats\",1,\"" + opt_deviceid + "\"]" );

        Json::Handle    json = client.Read();

        Log<4>("read reply: %s\n", json->JSONStr().c_str() );

        if( json[0]->String() == "ReportStats" )
        {
            Json::Data::Handle  stats = json[2];
            Json::MemberList    sources;

            stats->GetMembers( sources );

            for( Json::MemberList::iterator si = sources.begin(),
                                            se = sources.end(); si != se; ++si )
            {
                BitRuns::Result     bitruns( stats[*si]["BitRuns"] );

                printf( "\nsource: %s\n%s\n", si->c_str(), bitruns.Report().c_str() );
            }

        } else {

            Log<0>( "unrecognised reply\n" );
        }

    } //}}}


    if( opt_stats )
    { //{{{

        if( opt_deviceid.empty() )
            client.SendRequest( "\"ReportStats\"" );
        else
            client.SendRequest( "[\"ReportStats\",1,\"" + opt_deviceid + "\"]" );

        Json::Handle    json = client.Read();

        Log<4>("read reply: %s\n", json->JSONStr().c_str() );

        if( json[0]->String() == "ReportStats" )
        {
            Json::Data::Handle  stats = json[2];
            Json::MemberList    sources;

            stats->GetMembers( sources );

            for( Json::MemberList::iterator si = sources.begin(),
                                            se = sources.end(); si != se; ++si )
            {
                unsigned long long  analysed = stats[*si]["QA"]["BytesAnalysed"]->
                                                            As<unsigned long long>();
                unsigned long long  passed   = stats[*si]["QA"]["BytesPassed"]->
                                                            As<unsigned long long>();
                BitB::QA::FIPS      fips( stats[*si]["FIPS"] );

                printf( "\nsource: %s\n", si->c_str() );

                printf( "Octets analysed %llu, passed %llu, (not passed %llu)\n",
                                            analysed, passed, analysed - passed );

                printf( "FIPS %s\n", fips.ReportFailRates().c_str() );
                printf( "FIPS %s\n", fips.ReportPassRuns().c_str() );

                Json::Data::Handle  ent8  = stats[*si]->Get("Ent8");
                Json::Data::Handle  ent16 = stats[*si]->Get("Ent16");

                if( ! ent8 )
                {
                    printf( "Ent8: no results (yet)\n" );

                } else {

                    Ent8::Data  e8_short( Ent8::Results_Only, ent8["Short"] );
                    Ent8::Data  e8_long( Ent8::Results_Only, ent8["Long"] );

                    printf( "Ent8 short %s\n", e8_short.ReportResults().c_str() );
                    printf( "Ent8 long %s\n", e8_long.ReportResults().c_str() );
                }

                if( ! ent16 )
                {
                    printf( "Ent16: no results (yet)\n" );

                } else {

                    Ent16::Data  e16_short( Ent16::Results_Only, ent16["Short"] );
                    Ent16::Data  e16_long( Ent16::Results_Only, ent16["Long"] );

                    printf( "Ent16 short %s\n", e16_short.ReportResults().c_str() );
                    printf( "Ent16 long %s\n", e16_long.ReportResults().c_str() );
                }
            }

        } else {

            Log<0>( "unrecognised reply\n" );
        }

    } //}}}


    return EXIT_SUCCESS;
  }
  BB_CATCH_ALL( 0, _("bbctl fatal exception") )

  return EXIT_FAILURE;
}

// vi:sts=4:sw=4:et:foldmethod=marker
