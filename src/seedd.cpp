//  This file is distributed as part of the bit-babbler package.
//  Copyright 2012 - 2018,  Ron <ron@debian.org>

#ifndef _REENTRANT
#error "seedd requires pthread support"
#endif

#include "private_setup.h"

#include <bit-babbler/iniparser.h>
#include <bit-babbler/socket-source.h>
#include <bit-babbler/secret-sink.h>
#include <bit-babbler/control-socket.h>
#include <bit-babbler/signals.h>

#include <bit-babbler/impl/health-monitor.h>
#include <bit-babbler/impl/log.h>

#include <getopt.h>


using BitB::BitBabbler;
using BitB::Pool;
using BitB::SocketSource;
using BitB::ControlSock;
using BitB::CreateControlSocket;
using BitB::SecretSink;
using BitB::StrToU;
using BitB::StrToScaledU;
using BitB::StrToScaledUL;
using BitB::StrToScaledD;
using BitB::afterfirst;
using BitB::beforefirst;
using BitB::stringprintf;
using BitB::Error;
using BitB::SystemError;
using BitB::Log;


static void usage()
{
    printf("Usage: seedd [OPTION...]\n");
    printf("\n");
    printf("Read entropy from BitBabbler hardware RNG devices\n");
    printf("\n");
    printf("Options:\n");
    printf("  -s, --scan                Scan for available devices\n");
    printf("      --shell-mr            Output a machine readable list of devices\n");
    printf("  -C, --config=file         Read configuration options from a file\n");
    printf("  -i, --device-id=id        Read from only the selected device(s)\n");
    printf("  -b, --bytes=n             Send n bytes to stdout\n");
    printf("  -o, --stdout              Send entropy to stdout\n");
    printf("  -d, --daemon              Run as a background daemon\n");
    printf("  -k, --kernel              Feed entropy to the kernel\n");
    printf("  -u, --udp-out=host:port   Provide a UDP socket for entropy output\n");
    printf("  -c, --control-socket=path Where to create the control socket\n");
    printf("      --socket-group=grp    Grant group access to the control socket\n");
    printf("      --ip-freebind         Allow sockets to be bound to dynamic interfaces\n");
    printf("  -P, --pool-size=n         Size of the entropy pool\n");
    printf("      --kernel-device=path  Where to feed entropy to the OS kernel\n");
    printf("      --kernel-refill=sec   Max time in seconds before OS pool refresh\n");
    printf("  -G, --group-size=g:n      Size of a single pool group\n");
    printf("      --watch=path:ms:bs:n  Monitor an external device\n");
    printf("      --gen-conf            Output a config file using the options passed\n");
    printf("  -v, --verbose             Enable verbose output\n");
    printf("  -?, --help                Show this help message\n");
    printf("      --version             Print the program version\n");
    printf("\n");
    printf("Per device options:\n");
    printf("  -r, --bitrate=Hz          Set the bitrate (in bits per second)\n");
    printf("      --latency=ms          Override the USB latency timer\n");
    printf("  -f, --fold=n              Set the amount of entropy folding\n");
    printf("  -g, --group=n             The pool group to add the device to\n");
    printf("      --enable-mask=mask    Select a subset of the generators\n");
    printf("      --idle-sleep=init:max Tune the rate of pool refresh when idle\n");
    printf("      --suspend-after=ms    Set the threshold for USB autosuspend\n");
    printf("      --low-power           Convenience preset for idle and suspend\n");
    printf("      --limit-max-xfer      Limit the transfer chunk size to 16kB\n");
    printf("      --no-qa               Don't drop blocks that fail QA checking\n");
    printf("\n");
    printf("Report bugs to support@bitbabbler.org\n");
    printf("\n");
}


#if EM_PLATFORM_POSIX

static void WriteCompletion( void *p )
{
    pthread_t   *t = static_cast<pthread_t*>( p );
    pthread_kill( *t, SIGRTMIN );
}

#else

static pthread_mutex_t  wait_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   wait_cond    = PTHREAD_COND_INITIALIZER;
static int              done_waiting = 0;

static void WriteCompletion( void *p )
{
    (void)p;
    BitB::ScopedMutex   lock( &wait_mutex );
    done_waiting = 1;
    pthread_cond_broadcast( &wait_cond );
}

#endif


// Configuration options, imported from file(s) and/or the command line.
class Config : public BitB::IniData
{ //{{{
private:

    // The last --device-id passed on the command line, which any
    // subsequent per-device options there should be applied to.
    std::string         m_curdev;
    Validator::Handle   m_validator;


    // Option validator for unsigned number values in any base.
    static void UnsignedValue( const std::string &option, const std::string &value )
    { //{{{

        try {
            StrToU( value );
        }
        catch( const std::exception &e )
        {
            throw Error( _("Option '%s' expected integer: %s"),
                                    option.c_str(), e.what() );
        }

    } //}}}

    // Option validator for base-10 unsigned integer values.
    static void UnsignedBase10Value( const std::string &option, const std::string &value )
    { //{{{

        try {
            StrToU( value, 10 );
        }
        catch( const std::exception &e )
        {
            throw Error( _("Option '%s' expected decimal integer: %s"),
                                            option.c_str(), e.what() );
        }

    } //}}}

    // Option validator for base-10 unsigned integer values, optionally scaled by a suffix.
    static void ScaledUnsignedValue( const std::string &option, const std::string &value )
    { //{{{

        try {
            StrToScaledUL( value );
        }
        catch( const std::exception &e )
        {
            throw Error( _("Option '%s' expected decimal integer: %s"),
                                            option.c_str(), e.what() );
        }

    } //}}}

    // Option validator for decimal fraction values, optionally scaled by a suffix.
    static void ScaledFloatValue( const std::string &option, const std::string &value )
    { //{{{

        try {
            StrToScaledD( value );
        }
        catch( const std::exception &e )
        {
            throw Error( _("Option '%s' expected decimal value: %s"),
                                            option.c_str(), e.what() );
        }

    } //}}}

    // Validate Sections and Options.
    // We don't exhaustively validate all the option values here, mostly we just
    // want to catch invalid section and option names, but there's no reason not
    // to do basic sanity checking of the easy ones at this stage too.
    void validate()
    { //{{{

        if( ! m_validator )
        {
            // We may have multiple (or no) config files to validate,
            // so create this once the first time that it's needed.
            m_validator = new Validator;


            // [Service] section options
            Validator::OptionList::Handle   service_opts = new Validator::OptionList;

            service_opts->AddTest( "daemon",            Validator::OptionWithoutValue )
                        ->AddTest( "kernel",            Validator::OptionWithoutValue )
                        ->AddTest( "udp-out",           Validator::OptionWithValue )
                        ->AddTest( "control-socket",    Validator::OptionWithValue )
                        ->AddTest( "socket-group",      Validator::OptionWithValue )
                        ->AddTest( "ip-freebind",       Validator::OptionWithoutValue )
                        ->AddTest( "verbose",           UnsignedValue );

            m_validator->Section( "Service", Validator::SectionNameEquals, service_opts );


            // [Pool] section options
            Validator::OptionList::Handle   pool_opts = new Validator::OptionList;

            pool_opts->AddTest( "size",             ScaledUnsignedValue )
                     ->AddTest( "kernel-device",    Validator::OptionWithValue )
                     ->AddTest( "kernel-refill",    UnsignedBase10Value );

            m_validator->Section( "Pool", Validator::SectionNameEquals, pool_opts );


            // [PoolGroup:] section options
            Validator::OptionList::Handle   poolgroup_opts = new Validator::OptionList;

            poolgroup_opts->AddTest( "size",        ScaledUnsignedValue );

            m_validator->Section( "PoolGroup:", Validator::SectionNamePrefix, poolgroup_opts );


            // [Devices] and [Device:] section options
            Validator::OptionList::Handle   device_opts = new Validator::OptionList;

            device_opts->AddTest( "bitrate",        ScaledFloatValue )
                       ->AddTest( "latency",        UnsignedBase10Value )
                       ->AddTest( "fold",           UnsignedBase10Value )
                       ->AddTest( "group",          UnsignedBase10Value )
                       ->AddTest( "enable-mask",    UnsignedValue )
                       ->AddTest( "idle-sleep",     Validator::OptionWithValue )
                       ->AddTest( "suspend-after",  ScaledUnsignedValue )
                       ->AddTest( "low-power",      Validator::OptionWithoutValue )
                       ->AddTest( "limit-max-xfer", Validator::OptionWithoutValue )
                       ->AddTest( "no-qa",          Validator::OptionWithoutValue );

            m_validator->Section( "Devices", Validator::SectionNameEquals, device_opts );
            m_validator->Section( "Device:", Validator::SectionNamePrefix, device_opts );


            // [Watch:] section options
            Validator::OptionList::Handle   watch_opts = new Validator::OptionList;

            watch_opts->AddTest( "path",        Validator::OptionWithValue )
                      ->AddTest( "delay",       ScaledUnsignedValue )
                      ->AddTest( "block-size",  ScaledUnsignedValue )
                      ->AddTest( "max-bytes",   ScaledUnsignedValue );

            m_validator->Section( "Watch:", Validator::SectionNamePrefix, watch_opts );
        }

        m_validator->Validate( *this );

    } //}}}


    // Implementation detail to handle the 'low-power' option, which is set as
    // a (per)Device option, implicitly setting the global Pool kernel-refill
    // time too.  We need to check this when exporting Pool options, even if
    // there was no explict [Pool] section otherwise defined.
    void check_pool_low_power_option( Pool::Options &p ) const
    { //{{{

        if( HasOption("Devices", "low-power") )
        {
            p.kernel_refill_time = 3600;
        }
        else
        {
            const Sections  &s = GetSections("Device:");

            for( Sections::const_iterator i = s.begin(), e = s.end(); i != e; ++i )
            {
                if( i->second->HasOption("low-power") )
                {
                    p.kernel_refill_time = 3600;
                    break;
                }
            }
        }

    } //}}}

    // Implementation detail for extracting the per-device options from either
    // the global [Devices] section or an individual [Device:] definition (with
    // the global [Devices] options used as defaults for it unless overridden).
    BitBabbler::Options
    get_device_options( const std::string         &section,
                        const std::string         &device_id = std::string(),
                        const BitBabbler::Options &defaults  = BitBabbler::Options() ) const
    { //{{{

        BitBabbler::Options     bbo = defaults;
        Section::Handle         s = GetSection( section );
        std::string             opt;

        if( ! device_id.empty() )
            bbo.id = device_id;

        try {
            opt = "bitrate";
            if( s->HasOption( opt ) )
                bbo.bitrate = unsigned(StrToScaledD( s->GetOption(opt) ));

            opt = "latency";
            if( s->HasOption( opt ) )
                bbo.latency = StrToU( s->GetOption(opt), 10 );

            opt = "fold";
            if( s->HasOption( opt ) )
                bbo.fold = StrToU( s->GetOption(opt), 10 );

            opt = "group";
            if( s->HasOption( opt ) )
                bbo.group = StrToU( s->GetOption(opt), 10 );

            opt = "enable-mask";
            if( s->HasOption( opt ) )
                bbo.enable_mask = StrToU( s->GetOption(opt) );

            opt = "low-power";
            if( s->HasOption( opt ) )
            {
                bbo.SetIdleSleep( "100:0" );
                bbo.suspend_after = 10000;
            }

            opt = "suspend-after";
            if( s->HasOption( opt ) )
                bbo.suspend_after = StrToScaledU( s->GetOption(opt) );

            opt = "no-qa";
            if( s->HasOption( opt ) )
                bbo.no_qa = true;

            opt = "limit-max-xfer";
            if( s->HasOption( opt ) )
                bbo.chunksize = 16384;

            opt = "idle-sleep";
            if( s->HasOption( opt ) )
                bbo.SetIdleSleep( s->GetOption(opt) );
        }
        catch( const std::exception &e )
        {
            throw Error( _("Failed to apply [%s] option '%s': %s"),
                            section.c_str(), opt.c_str(), e.what() );
        }

        return bbo;

    } //}}}


public:

    // We only need a trivial default constructor at present.
    Config() {}


    // Update the current state with (additional) options from an INI file.
    // As with command line options, where some option setting is duplicated,
    // the last one applied will override any seen previously.
    void ImportFile( const char *path )
    { //{{{

        char            buf[65536];
        std::string     data;
        size_t          n;
        FILE           *f = fopen( path, "r" );

        if( ! f )
            throw SystemError( _("Failed to open config file '%s'"), path );

        while(( n = fread( buf, 1, sizeof(buf), f ) ))
            data.append( buf, n );

        fclose( f );

        try {
            UpdateWith( data );
            validate();
        }
        catch( const std::exception &e )
        {
            throw Error( _("Failed to import config from '%s': %s"),
                                                    path, e.what() );
        }

    } //}}}


    // Export entropy Pool configuration options.
    Pool::Options GetPoolOptions() const
    { //{{{

        Pool::Options   p;
        std::string     opt;

        try {
            if( HasSection("Pool") )
            {
                Section::Handle     s = GetSection("Pool");

                opt = "size";
                if( s->HasOption( opt ) )
                    p.pool_size = StrToScaledUL( s->GetOption(opt), 1024 );

                opt = "kernel-device";
                if( s->HasOption( opt ) )
                    p.kernel_device = s->GetOption(opt);

                // This one is set implicitly to 3600 if any device uses the
                // low-power option, unless it is explicitly set directly.
                opt = "kernel-refill";
                if( s->HasOption( opt ) )
                    p.kernel_refill_time = StrToU( s->GetOption(opt), 10 );
                else
                    check_pool_low_power_option( p );
            }
            else
            {
                opt = "kernel-refill (low-power)";
                check_pool_low_power_option( p );
            }
        }
        catch( const std::exception &e )
        {
            throw Error( _("Failed to apply [Pool] option '%s': %s"),
                                                opt.c_str(), e.what() );
        }

        return p;

    } //}}}

    // Export a list of defined entropy Pool groups.
    Pool::Group::Options::List GetPoolGroupOptions() const
    { //{{{

        const Sections             &s = GetSections("PoolGroup:");
        Pool::Group::Options::List  g;

        for( Sections::const_iterator i = s.begin(),
                                      e = s.end(); i != e; ++i )
        {
            std::string     opt = i->first + ':' + GetOption(i->second, "size");
            g.push_back( Pool::Group::Options( opt.c_str() ) );
        }

        return g;

    } //}}}


    // Add a [Device:] definition for a --device-id passed on the command line.
    // And remember the last device added that way so that any subsequent
    // per-device options on the command line will be applied to it too.
    void AddDevice( const char *id )
    { //{{{

        m_curdev = stringprintf( "Device:%s", id );

        if( ! HasSection( m_curdev ) )
            AddSection( m_curdev );

    } //}}}

    // Set (or override) a per-device option from the command line.
    // If no --device-id has been passed yet, the option will be set in the
    // global [Devices] section, otherwise it will be set for the specific
    // [Device:] which was last requested.
    void SetDeviceOption( const std::string &option,
                          const std::string &value = std::string() )
    {
        AddOrUpdateOption( m_curdev.empty() ? "Devices" : m_curdev, option, value );
    }

    // Export the default [Devices] options to use for any devices which don't
    // have an explicit [Device:] configuration of their own.
    BitBabbler::Options GetDefaultDeviceOptions() const
    { //{{{

        if( HasSection("Devices") )
            return get_device_options("Devices");

        return BitBabbler::Options();

    } //}}}

    // Export a list of the individual [Device:] configuration options for each
    // device that has one configured for it.
    BitBabbler::Options::List GetDeviceOptions() const
    { //{{{

        BitBabbler::Options::List   bbol;
        BitBabbler::Options         default_options = GetDefaultDeviceOptions();
        const Sections             &s = GetSections("Device:");

        for( Sections::const_iterator i = s.begin(), e = s.end(); i != e; ++i )
        {
            bbol.push_back( get_device_options( i->second->GetName(),
                                                i->first,
                                                default_options ) );
        }

        return bbol;

    } //}}}


    // Add a new [Watch:] definition from options passed on the command line.
    void AddWatch( const std::string &arg )
    { //{{{

        using std::string;

        const Sections  &s          = GetSections("Watch:");
        unsigned         next_watch = 0;

        // If there are numbered Watch sections, find the current largest number.
        // If someone really uses a number larger than will fit in unsigned int,
        // and mixes a config file with command line watches, they'll get what
        // it is that they did to themselves.  But unless they really have > 4G
        // watches set, we will still probably find a safe number to use here
        // even with some truncated value(s) in the mix.
        //
        // The alternative would be to just iterate next_watch from 0 until we
        // find the first value which isn't a collision, but this way is probably
        // nicer since it orders all command line watches after any defined with
        // numeric identifiers in the config file.
        for( Sections::const_iterator i = s.begin(), e = s.end(); i != e; ++i )
        {
            try {
                unsigned isnum = StrToU( i->first );

                if( isnum >= next_watch )
                    next_watch = isnum + 1;
            }
            catch( const std::exception& )
            {
                // It's not an error for Watch identifiers to not be a number,
                // we just don't take those into account when generating one
                // for a watch specified on the command line.

                //Log<0>( "AddWatch: '%s' is not a number\n", i->first.c_str() );
            }
        }

        //Log<0>( "Next watch is %u\n", n );


        // Parse the options struct from a string of the form:
        // path:delay:block_size:total_bytes
        // where everything except the path portion is optional.
        //
        // This is similar to what is done in SecretSink::Options::ParseOptArg()
        // except we don't normalise the numeric values here, we just keep them
        // as the literal strings which were passed on the command line for now.
        // They'll get converted to numeric types when actually used.

        Section::Handle sect = AddSection( stringprintf("Watch:%u", next_watch) );
        size_t          n    = arg.find(':');
        size_t          n2;

        if( n == string::npos )
        {
            AddOption( sect, "path", arg );
            return;
        }
        AddOption( sect, "path", arg.substr(0, n) );

        ++n;
        n2 = arg.find( ':', n );

        if( n2 == string::npos )
        {
            AddOption( sect, "delay", arg.substr(n) );
            return;
        }
        AddOption( sect, "delay", arg.substr(n, n2 - n) );

        n = n2 + 1;
        n2 = arg.find( ':', n );

        if( n2 == string::npos )
        {
            AddOption( sect, "block-size", arg.substr(n) );
            return;
        }
        AddOption( sect, "block-size", arg.substr(n, n2 - n) );

        n = n2 + 1;

        AddOption( sect, "max-bytes", arg.substr(n) );

    } //}}}

    // Export a list of the source Watches to enable.
    SecretSink::Options::List GetWatchOptions() const
    { //{{{

        const Sections             &s = GetSections("Watch:");
        SecretSink::Options::List   w;

        for( Sections::const_iterator i = s.begin(),
                                      e = s.end(); i != e; ++i )
        {
            // Track which option we're applying, so that we
            // can report its name if an exception is thrown.
            std::string     opt;

            try {
                SecretSink::Options     sso;

                opt = "path";
                if( i->second->HasOption( opt ) )
                    sso.devpath = i->second->GetOption( opt );
                else
                    throw Error( _("No path defined to Watch") );

                opt = "delay";
                if( i->second->HasOption( opt ) )
                    sso.block_delay = StrToScaledUL( i->second->GetOption( opt ) );

                opt = "block-size";
                if( i->second->HasOption( opt ) )
                    sso.block_size = StrToScaledUL( i->second->GetOption( opt ), 1024 );

                opt = "max-bytes";
                if( i->second->HasOption( opt ) )
                    sso.bytes = StrToScaledUL( i->second->GetOption( opt ), 1024 );

                w.push_back( sso );
            }
            catch( const std::exception &e )
            {
                throw Error( _("Failed to apply [Watch:%s] option '%s': %s"),
                                    i->first.c_str(), opt.c_str(), e.what() );
            }
        }

        return w;

    } //}}}


    // Specialisation of IniData::INIStr() to output the expected sections in
    // a logical (for users) and deterministic (if using a hashed map) order.
    // This string may be saved and later passed to ImportFile() or Decode()
    // to recreate the current configuration state.
    std::string ConfigStr() const
    { //{{{

        std::string         out;
        Sections            s = GetSections(), ss;
        Sections::iterator  i = s.find("Service"), e;

        // Output the Service section
        if( i != s.end() )
        {
            out.append( i->second->INIStr() + '\n' );
            s.erase( i );
        }

        // Output the Pool section
        i = s.find("Pool");
        if( i != s.end() )
        {
            out.append( i->second->INIStr() + '\n' );
            s.erase( i );
        }

        // Output the PoolGroup section(s)
        ss = GetSections("PoolGroup:");
        for( i = ss.begin(), e = ss.end(); i != e; ++i )
        {
            out.append( i->second->INIStr() + '\n' );
            s.erase( i->second->GetName() );
        }

        // Output the Devices section
        i = s.find("Devices");
        if( i != s.end() )
        {
            out.append( i->second->INIStr() + '\n' );
            s.erase( i );
        }

        // Output the Device section(s)
        ss = GetSections("Device:");
        for( i = ss.begin(), e = ss.end(); i != e; ++i )
        {
            out.append( i->second->INIStr() + '\n' );
            s.erase( i->second->GetName() );
        }

        // Output the Watch section(s)
        ss = GetSections("Watch:");
        for( i = ss.begin(), e = ss.end(); i != e; ++i )
        {
            out.append( i->second->INIStr() + '\n' );
            s.erase( i->second->GetName() );
        }

        // Output whatever else is still left
        for( i = s.begin(), e = s.end(); i != e; ++i )
            out.append( i->second->INIStr() + '\n' );

        return out;

    } //}}}

}; //}}}


int main( int argc, char *argv[] )
{
  try {

    Config                      conf;
    unsigned                    opt_scan        = 0;
    size_t                      opt_bytes       = 0;
    unsigned                    opt_stdout      = 0;
    int                         opt_v           = 0;
    bool                        opt_genconf     = false;

    enum
    {
        SHELL_MR_OPT,
        FREEBIND_OPT,
        SOCKET_GROUP_OPT,
        KERNEL_DEVICE_OPT,
        KERNEL_REFILL_TIME_OPT,
        LATENCY_OPT,
        ENABLEMASK_OPT,
        IDLE_SLEEP_OPT,
        SUSPEND_AFTER_OPT,
        LOW_POWER_OPT,
        LIMIT_MAX_XFER,
        NOQA_OPT,
        WATCH_OPT,
        GENERATE_CONFIG_OPT,
        VERSION_OPT
    };

    struct option long_options[] =
    {
        { "scan",           no_argument,        NULL,      's' },
        { "shell-mr",       no_argument,        NULL,      SHELL_MR_OPT },
        { "config",         required_argument,  NULL,      'C' },
        { "device-id",      required_argument,  NULL,      'i' },
        { "bytes",          required_argument,  NULL,      'b' },
        { "stdout",         no_argument,        NULL,      'o' },
        { "daemon",         no_argument,        NULL,      'd' },
        { "kernel",         no_argument,        NULL,      'k' },
        { "ip-freebind",    no_argument,        NULL,      FREEBIND_OPT },
        { "udp-out",        required_argument,  NULL,      'u' },
        { "control-socket", required_argument,  NULL,      'c' },
        { "socket-group",   required_argument,  NULL,      SOCKET_GROUP_OPT },

        { "pool-size",      required_argument,  NULL,      'P' },
        { "kernel-device",  required_argument,  NULL,      KERNEL_DEVICE_OPT },
        { "kernel-refill",  required_argument,  NULL,      KERNEL_REFILL_TIME_OPT },
        { "group-size",     required_argument,  NULL,      'G' },

        { "bitrate",        required_argument,  NULL,      'r' },
        { "latency",        required_argument,  NULL,      LATENCY_OPT },
        { "fold",           required_argument,  NULL,      'f' },
        { "group",          required_argument,  NULL,      'g' },
        { "enable-mask",    required_argument,  NULL,      ENABLEMASK_OPT },
        { "idle-sleep",     required_argument,  NULL,      IDLE_SLEEP_OPT },
        { "suspend-after",  required_argument,  NULL,      SUSPEND_AFTER_OPT },
        { "low-power",      no_argument,        NULL,      LOW_POWER_OPT },
        { "limit-max-xfer", no_argument,        NULL,      LIMIT_MAX_XFER },
        { "no-qa",          no_argument,        NULL,      NOQA_OPT },

        { "watch",          required_argument,  NULL,      WATCH_OPT },

        { "gen-conf",       no_argument,        NULL,      GENERATE_CONFIG_OPT },
        { "verbose",        no_argument,        NULL,      'v' },
        { "help",           no_argument,        NULL,      '?' },
        { "version",        no_argument,        NULL,      VERSION_OPT },
        { 0, 0, 0, 0 }
    };

    int opt_index = 0;

    for(;;)
    { //{{{

        int c = getopt_long( argc, argv, ":sC:i:r:f:g:b:dku:oP:G:c:v?",
                                long_options, &opt_index );
        if( c == -1 )
            break;

        switch(c)
        {
            case 's':
                opt_scan = 1;
                break;

            case SHELL_MR_OPT:
                opt_scan = 2;
                break;

            case 'C':
                conf.ImportFile( optarg );
                break;

            case 'i':
                conf.AddDevice( optarg );
                break;

            case 'b':
                opt_bytes = StrToScaledUL( optarg, 1024 );
                break;

            case 'o':
                opt_stdout = 1;
                break;

            case 'd':
                conf.AddOrUpdateOption( "Service", "daemon" );
                break;

            case 'k':
                conf.AddOrUpdateOption( "Service", "kernel" );
                break;

            case FREEBIND_OPT:
                conf.AddOrUpdateOption( "Service", "ip-freebind" );
                break;

            case 'u':
                conf.AddOrUpdateOption( "Service", "udp-out", optarg );
                break;

            case 'c':
                conf.AddOrUpdateOption( "Service", "control-socket", optarg );
                break;

            case SOCKET_GROUP_OPT:
                conf.AddOrUpdateOption( "Service", "socket-group", optarg );
                break;

            case 'P':
                conf.AddOrUpdateOption( "Pool", "size", optarg );
                break;

            case KERNEL_DEVICE_OPT:
                conf.AddOrUpdateOption( "Pool", "kernel-device", optarg );
                break;

            case KERNEL_REFILL_TIME_OPT:
                conf.AddOrUpdateOption( "Pool", "kernel-refill", optarg );
                break;

            case 'G':
            {
                std::string     s( optarg );
                conf.AddOrUpdateOption( "PoolGroup:" + beforefirst(':', s),
                                        "size", afterfirst(':', s) );
                break;
            }

            case 'r':
                conf.SetDeviceOption( "bitrate", optarg );
                break;

            case LATENCY_OPT:
                conf.SetDeviceOption( "latency", optarg );
                break;

            case 'f':
                conf.SetDeviceOption( "fold", optarg );
                break;

            case 'g':
                conf.SetDeviceOption( "group", optarg );
                break;

            case ENABLEMASK_OPT:
                conf.SetDeviceOption( "enable-mask", optarg );
                break;

            case IDLE_SLEEP_OPT:
                conf.SetDeviceOption( "idle-sleep", optarg );
                break;

            case SUSPEND_AFTER_OPT:
                conf.SetDeviceOption( "suspend-after", optarg );
                break;

            case LOW_POWER_OPT:
                conf.SetDeviceOption( "low-power" );
                break;

            case LIMIT_MAX_XFER:
                conf.SetDeviceOption( "limit-max-xfer" );
                break;

            case NOQA_OPT:
                conf.SetDeviceOption( "no-qa" );
                break;

            case WATCH_OPT:
                conf.AddWatch( optarg );
                break;

            case GENERATE_CONFIG_OPT:
                opt_genconf = true;
                break;

            case 'v':
                ++opt_v;
                break;

            case '?':
                if( optopt != '?' && optopt != 0 )
                {
                    fprintf(stderr, "%s: invalid option -- '%c', try --help\n",
                                                            argv[0], optopt);
                    return EXIT_FAILURE;
                }

                // If we're generating a config, don't dump the usage to stdout
                // under any circumstances and do return an EXIT_FAILURE code.
                if( opt_genconf )
                {
                    fprintf(stderr, "%s: invalid option used, not generating config\n",
                                                                            argv[0]);
                    return EXIT_FAILURE;
                }

                usage();
                return EXIT_SUCCESS;

            case ':':
                fprintf(stderr, "%s: missing argument for '%s', try --help\n",
                                                    argv[0], argv[optind - 1] );
                return EXIT_FAILURE;

            case VERSION_OPT:
                printf("seedd " PACKAGE_VERSION "\n");
                return EXIT_SUCCESS;
        }

    } //}}}


    std::string     notify_socket = BitB::GetSystemdNotifySocket();

    // If we've been started by systemd in notify mode we need to stay in the
    // foreground regardless of what config options we may have been passed.
    if( ! notify_socket.empty() )
        conf.RemoveOption( "Service", "daemon" );


    // Just output a configuration file (based on the options passed) and exit.
    if( opt_genconf )
    { //{{{

        std::string     cmd_line;

        for( int i = 0; i < argc; ++i )
            cmd_line.append( stringprintf(" %s", argv[i]) );

        // We don't usually push the -v command line override into the config
        // but do it here, because we want that in the generated config if used.
        if( opt_v )
            conf.AddOrUpdateOption( "Service", "verbose", stringprintf("%d", opt_v) );

        printf( "# Generated configuration file for seedd(1), created %s using:\n"
                "# %s\n%s\n",
                BitB::timeprintf( "%F", BitB::GetWallTimeval() ).c_str(),
                cmd_line.c_str(), conf.ConfigStr().c_str() );

        return EXIT_SUCCESS;

    } //}}}


    // Pump up the volume (if asked to)
    if( opt_v )
        BitB::opt_verbose = opt_v;
    else if ( conf.HasOption("Service", "verbose") )
        BitB::opt_verbose = int(StrToU( conf.GetOption("Service", "verbose") ));

    // And send it to syslog if we'll be running in the background.
    if( conf.HasOption("Service", "daemon") && ! opt_scan )
        BitB::SendLogsToSyslog( argv[0] );


    if( ! notify_socket.empty() )
        Log<4>( "NOTIFY_SOCKET='%s'\n", notify_socket.c_str() );

    Log<2>( "Using configuration:\n%s", conf.ConfigStr().c_str() );

    // Extract and (initially) sanity check these before going to
    // the background if we're going to be running this as a daemon.
    Pool::Options               pool_options    = conf.GetPoolOptions();
    Pool::Group::Options::List  group_options   = conf.GetPoolGroupOptions();
    SecretSink::Options::List   watch_options   = conf.GetWatchOptions();
    BitBabbler::Options         default_options = conf.GetDefaultDeviceOptions();
    BitBabbler::Options::List   device_options  = conf.GetDeviceOptions();


   #if EM_PLATFORM_POSIX

    if( conf.HasOption("Service", "daemon") && ! opt_scan )
    {
        if( daemon(0,0) )
            throw SystemError( _("Failed to fork daemon") );

        umask( S_IWGRP | S_IROTH | S_IWOTH | S_IXOTH );
    }

    BitB::BlockSignals();

   #else

    // We could implement support for this if/when needed, but it's less useful
    // on systems where we don't sit in the background feeding the OS kernel.
    if( conf.HasOption("Service", "daemon") && ! opt_scan )
        throw Error( _("Daemon mode not supported on this platform.") );

   #endif


    BitB::Devices   d;

    if( opt_scan )
    {
        switch( opt_scan )
        {
            case 1:
                d.ListDevices();
                return EXIT_SUCCESS;

            case 2:
                d.ListDevicesShellMR();
                return EXIT_SUCCESS;
        }

        fprintf(stderr, "seedd: unknown device scan option %u\n", opt_scan );
        return EXIT_FAILURE;
    }
    else if( d.GetNumDevices() == 0 && ! d.HasHotplugSupport() )
    {
        // If we don't have hotplug support, and we don't have any devices now,
        // then there's no point waiting around, because none will appear later.
        fprintf( stderr, _("seedd: No devices found, and no hotplug support.  Aborting.\n") );
        return EXIT_FAILURE;
    }


    pthread_t       main_thread = pthread_self();
    Pool::Handle    pool        = new Pool( pool_options );

    for( Pool::Group::Options::List::iterator i = group_options.begin(),
                                              e = group_options.end(); i != e; ++i )
        pool->AddGroup( i->groupid, i->size );

    d.AddDevicesToPool( pool, default_options, device_options );


    SocketSource::Handle    ssrc;

    if( conf.HasOption("Service", "udp-out") )
    {
        opt_bytes = 0;
        ssrc = new SocketSource( pool, conf.GetOption("Service", "udp-out"),
                                       conf.HasOption("Service", "ip-freebind") );
    }

    if( conf.HasOption("Service", "kernel") )
    {
        opt_bytes = 0;
        pool->FeedKernelEntropyAsync();
    }

    if( opt_stdout || opt_bytes )
    {
        if( opt_bytes && ! conf.HasOption("Service", "control-socket") )
            conf.AddOrUpdateOption( "Service", "control-socket", "none" );

       #if EM_PLATFORM_MSW
        setmode( STDOUT_FILENO, O_BINARY );
       #endif
        pool->WriteToFDAsync( STDOUT_FILENO, opt_bytes, WriteCompletion, &main_thread );
    }

    SecretSink::List    watch_sinks;

    for( SecretSink::Options::List::iterator i = watch_options.begin(),
                                             e = watch_options.end(); i != e; ++i )
        watch_sinks.push_back( new SecretSink( *i ) );


    ControlSock::Handle ctl = CreateControlSocket( conf.GetOption( "Service", "control-socket",
                                                                   SEEDD_CONTROL_SOCKET ),
                                                   conf.GetOption( "Service", "socket-group",
                                                                   std::string() ),
                                                   conf.HasOption( "Service", "ip-freebind" ) );


    // If we've been started by systemd in notify mode, then notify it ...
    if( ! notify_socket.empty() )
        BitB::SystemdNotify( "READY=1", notify_socket );

   #if EM_PLATFORM_POSIX

    int sig;

    wait_for_the_signal:
    sig = BitB::SigWait( SIGINT, SIGQUIT, SIGTERM, SIGABRT, SIGTSTP, SIGRTMIN, SIGUSR1 );

    switch( sig )
    {
        case SIGTSTP:
            Log<0>( _("Stopped by signal %d (%s)\n"), sig, strsignal(sig) );
            raise( SIGSTOP );
            goto wait_for_the_signal;

        case SIGUSR1:
            // p.ReportState()
            goto wait_for_the_signal;

        default:
            // We can't switch on SIGRTMIN, it's not a constant.
            if( sig == SIGRTMIN )
                Log<1>( _("Wrote %zu bytes to stdout\n"), opt_bytes );
            else
                Log<0>( _("Terminated by signal %d (%s)\n"), sig, strsignal(sig) );
            break;
    }

   #else

    BitB::ScopedMutex   lock( &wait_mutex );

    while( ! done_waiting )
        pthread_cond_wait( &wait_cond, &wait_mutex );

   #endif


    // If we've been started by systemd in notify mode, humour it again ...
    //{{{
    // This is mostly useless, but not entirely, because exiting this scope
    // isn't the end of us yet, there's a bunch of shutdown still to be done
    // including terminating threads in the unwinding which happens next.
    // This really is still just the beginning of the end, some pathological
    // worst case could keep us hanging on for longer than we expected to.
    //}}}
    if( ! notify_socket.empty() )
        BitB::SystemdNotify( "STOPPING=1", notify_socket );

    return EXIT_SUCCESS;
  }
  BB_CATCH_ALL( 0, _("seedd fatal exception") )

  return EXIT_FAILURE;
}

// vi:sts=4:sw=4:et:foldmethod=marker
