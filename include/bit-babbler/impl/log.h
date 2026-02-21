//  This file is distributed as part of the bit-babbler package.
//  Copyright 2003 - 2021,  Ron <ron@debian.org>
//
// This file provides the implementation detail for bit-babbler/log.h
// which must be defined only once in an application.

#ifdef _BBIMPL_LOG_H
#error bit-babbler/impl/log.h must be included only once.
#endif

#define _BBIMPL_LOG_H

#include <bit-babbler/log.h>
#include <time.h>


namespace BitB
{
    bool    opt_syslog      = 0;
    bool    opt_timestamp   = 1;
    int     opt_verbose     = 0;


    std::string OctetsToHex( const OctetString &octets, size_t wrap, bool short_form )
    { //{{{

        std::string     s;
        char            b[6];

        for( size_t i = 0, e = octets.size(); i < e; ++i )
        {
            int     n = snprintf(b, sizeof(b), short_form ? " %02x" : " 0x%02x", octets[i]);

            #ifdef PARANOID_STRING_CHECKING
            // This should never happen here
            if( __builtin_expect( n < 0, 0 ) )
                return std::string( "OctetsToHex failed" );

            // And this should be even less likely
            if( __builtin_expect( unsigned(n) > sizeof(b), 0 ) )
                s.append( b, sizeof(b) ).append("(truncated)");
            #endif

            s.append( b, unsigned(n) );

            if( wrap && i % wrap == wrap - 1 )
                s.append( 1, '\n' );
        }

        return s;

    } //}}}

    unsigned long StrToScaledUL( const char *s, unsigned scale )
    { //{{{

        char           *e;
        unsigned long   r = strtoul( s, &e, 10 );

        if( e == s )
            throw Error( _("StrToScaledUL( '%s' ): not a number"), s );

        if( *e == '\0' )
            return r;

        // Always let ibibytes be explicitly forced whatever the default scale
        if( *(e+1) == 'i' )
            scale = 1024;

        switch( *e )
        {
            case '\0':
                return r;

            case 'k':
            case 'K':           // Accept this abomination too, because people.
                return r * scale;

            case 'M':
                return r * scale * scale;

            case 'G':
                return r * scale * scale * scale;

            case 'T':
                return r * scale * scale * scale * scale;
        }

        throw Error( _("StrToScaledUL( '%s' ): '%c' is not a recognised scale"), s, *e );

    } //}}}

    double StrToScaledD( const char *s )
    { //{{{

        errno = 0;

        char       *e;
        double      r     = strtod( s, &e );
        double      scale = 1000.0;

        if( errno )
            throw SystemError( errno, _("StrToScaledD( '%s' ) failed"), s );

        if( e == s )
            throw Error( _("StrToScaledD( '%s' ): not a number"), s );

        if( *e == '\0' )
            return r;

        if( *(e+1) == 'i' )
            scale = 1024.0;

        switch( *e )
        {
            case 'p':
                return r / (scale * scale * scale * scale);

            case 'n':
                return r / (scale * scale * scale);

            case 'u':
                return r / (scale * scale);

            case 'm':
                return r / scale;

            case '\0':
                return r;

            case 'k':
            case 'K':           // Accept this abomination too, because people.
                return r * scale;

            case 'M':
                return r * (scale * scale);

            case 'G':
                return r * (scale * scale * scale);

            case 'T':
                return r * (scale * scale * scale * scale);
        }

        throw Error( _("StrToScaledD( '%s' ): '%c' is not a recognised scale"), s, *e );

    } //}}}


    // This is a workaround for (at least) GCC 8.3.0, which annoyingly suggests
    // that we flag this as cold, iff we annotate the declaration with BB_COLD.
    // It is a known bug, that empirically is not seen in GCC 10.2.1 and should
    // not be seen before GCC 8, since -Wsuggest-attribute=cold (and malloc)
    // were not implemented before then.
   #if EM_COMPILER_GCC(8,0) && ! EM_COMPILER_GCC(10,2)
    EM_PUSH_DIAGNOSTIC_IGNORE("-Wsuggest-attribute=cold")
   #endif

    void GetFutureTimespec( timespec &ts, unsigned ms )
    { //{{{

        // This one doesn't really belong in with the log.h code, but that does
        // already depend on the only other time functions we're using, and it
        // probably isn't worth splitting that out on its own at this point.

        unsigned long long  nsec;

      #if HAVE_CLOCK_GETTIME

        if( clock_gettime(CLOCK_REALTIME, &ts) )
            throw SystemError("clock_gettime failed");

        nsec = ms * 1000000ULL + static_cast<unsigned long>(ts.tv_nsec);

      #else

        timeval     tv;

        if( gettimeofday(&tv, NULL) )
            throw SystemError("gettimeofday failed");

        ts.tv_sec   = tv.tv_sec;
        nsec        = ms * 1000000ULL + static_cast<unsigned long>(tv.tv_usec) * 1000ULL;

      #endif

        if( nsec < 1000000000 )
        {
            ts.tv_nsec = long(nsec);
        } else {
            ts.tv_sec += nsec / 1000000000;
            ts.tv_nsec = nsec % 1000000000;
        }

    } //}}}

   #if EM_COMPILER_GCC(8,0) && ! EM_COMPILER_GCC(10,2)
    EM_POP_DIAGNOSTIC
   #endif


    std::string stringprintf( const char *format, ... )
    { //{{{

        // We don't call vstringprintf here because it would mean an
        // extra copy operation to save a couple of lines of simple
        // code (and this comment reminding me why I didn't factor it
        // out the last time I looked at it).

        va_list         arglist;
        char           *s = NULL;
        std::string     str;

        va_start( arglist, format );
        int len = Vasprintf( &s, format, arglist );
        if( len >= 0 )
        {
            if( len > 0 )
                str = s;
            free( s );
        }
        else
        {
            va_end( arglist );
            // Note it is not safe to free s here, it is allowed to be
            // undefined on failure so we assume it was not allocated.
            throw SystemError( _("stringprintf failed to expand format") );
        }

        va_end( arglist );
        return str;

    } //}}}

    std::string vstringprintf( const char *format, va_list arglist )
    { //{{{

        char           *s = NULL;
        std::string     str;

        int len = Vasprintf( &s, format, arglist );
        if( len >= 0 )
        {
            if( len > 0 )
                str = s;
            free( s );
        }
        else
        {
            // Note it is not safe to free s here, it is allowed to be
            // undefined on failure so we assume it was not allocated.
            throw SystemError( _("vstringprintf failed to expand format") );
        }

        return str;

    } //}}}

    std::string timeprintf( const char *format, const timeval &tv )
    { //{{{

        using std::string;

        const size_t        MAX_LEN = 1024;
        string              timestr;
        struct tm           tm;
        string::size_type   n = 0;

        // time_t may not be the same size as tv_sec, and indeed on 64-bit Windows
        // (and possibly OpenBSD among others) time_t is long long while tv_sec is
        // only long.
        time_t              sec = tv.tv_sec;

        // First format a string according to the standard specifiers.
       #if HAVE_LOCALTIME_R
        localtime_r( &sec, &tm );
       #else
        memcpy( &tm, localtime( &sec ), sizeof( tm ) );
       #endif


       #if EM_PLATFORM_MSW

        // We lose the ability to have compile time format checking of strftime
        // by doing this but for that function it's of fairly limited value and
        // as of gcc-7 we need to disable it here anyway because it doesn't see
        // that this function itself is format-checked and so still whines that
        // the format is not a literal even though it just checked that it is!
        string  fmt = format;

        // Windows doesn't support the %T format option, so expand it manually here.
        while( ( n = fmt.find( "%T", n, 2 ) ) != string::npos )
            fmt.replace( n, 2, "%H:%M:%S" );

        n = 0;

        // Windows doesn't support the %F format option, so expand it manually here.
        while( ( n = fmt.find( "%F", n, 2 ) ) != string::npos )
            fmt.replace( n, 2, "%Y-%m-%d" );

        n = 0;

       #endif


        // gcc-7 complains about the format parameter passed to strftime, even
        // though it already checked it when this function itself was called.
        EM_PUSH_DIAGNOSTIC_IGNORE("-Wformat-nonliteral")

       #if EM_PLATFORM_LINUX

        // Querying the string length with a NULL for the output is a GNU extension.
        // The glibc docs specify that behaviour but SUSv4 does not define it.
        // Windows doesn't support it, nor does FreeBSD 11 or MacOS 10.12 (Sierra).
        // It's also tricky to actually test for that, since on systems where it
        // doesn't work, there is no clear indication except perhaps a segfault.
        // So we'll be conservative and only use it where we know it really works.
        size_t  size = strftime((char[MAX_LEN]){}, MAX_LEN, format, &tm );

       #else

        size_t  size = MAX_LEN;

       #endif

        if( size > 0 )
        {
            char    buf[ size + 1 ];

           #if EM_PLATFORM_MSW

            if( strftime( buf, size + 1, fmt.c_str(), &tm ) > 0 )
                timestr = buf;

           #else

            if( strftime( buf, size + 1, format, &tm ) > 0 )
                timestr = buf;

           #endif
        }

        EM_POP_DIAGNOSTIC


        // Then substitute microseconds if required.
        while( ( n = timestr.find( '%', n ) ) != string::npos )
        {
            string::size_type   end = timestr.find( 'u', n );

            if( end != string::npos )
            {
                string::size_type   len = end - n;

                char    usec[ 7 ];
                snprintf( usec, sizeof( usec ), "%06ld", long(tv.tv_usec) );

                if( len == 1 )
                {
                    timestr.replace( n, len + 1, usec, 6 );
                }
                else if( len == 2 )
                {
                    unsigned int width;
                    if( sscanf( string( timestr, n + 1, len - 1 ).c_str(), "%u", &width ) == 1 )
                        timestr.replace( n, len + 1, usec, std::min(width,6u) );
                }
                n = end;
            }
        }

        return timestr;

    } //}}}


    std::string DemangleSymbol( const char *sym )
    { //{{{

        int     status;
        char   *name = abi::__cxa_demangle( sym, NULL, NULL, &status );

        switch( status )
        {
            case 0:
            {
                std::string  s( name );
                free( name );
                return s;
            }
            case -2:
            {
                std::string  s( 1, '\'' );
                s.append( sym );
                s.append( "' (not demangled)" );
                return s;
            }
            case -1:
                throw Error( "Memory allocation failure in DemangleSymbol" );
            case -3:
                throw Error( "Invalid argument in DemangleSymbol" );
            default:
                throw Error( "Unknown return status (%d) in DemangleSymbol", status );
        }

    } //}}}


   #ifdef _REENTRANT

    #if ! THREAD_STACK_SIZE
    BB_CONST
    #endif
    const pthread_attr_t *GetDefaultThreadAttr()
    { //{{{

       #if THREAD_STACK_SIZE

        static pthread_attr_t   a;
        static bool             need_init = true;

        if( __builtin_expect( need_init, false ) )
        {
            size_t n = THREAD_STACK_SIZE * 1024;
            size_t d;

            need_init = false;
            pthread_attr_init( &a );

            int ret = pthread_attr_getstacksize( &a, &d );

            if( ret )
                throw SystemError( ret, "Failed to get default stack size\n" );

            Log<5>( "Initialising pthread_attr for stack size %zu (default is %zu)\n", n, d );

            ret = pthread_attr_setstacksize( &a, n );

            if( ret )
                throw SystemError( ret, "Failed to initialise pthread_attr for stack size %zu\n", n );
        }

        return &a;

       #else

        // Use the default attributes if we aren't overriding anything
        return NULL;

       #endif

    } //}}}

   #endif
}

#if EM_PLATFORM_MSW

  // Mingw W64 6.3.0 suggests these functions should be marked 'const', while
  // the '10-win32' release (rightly) complains if they are ...  So once again
  // muffle the buggy suggestion just for the compiler versions making it, and
  // we can get rid of this workaround once none of need be supported anymore.
  // 6.3.0 released with Stretch, so we've got a few more years still before
  // it falls out of ELTS support.
 #if EM_COMPILER_GCC(6,0) && ! EM_COMPILER_GCC(10,0)
  EM_PUSH_DIAGNOSTIC_IGNORE("-Wsuggest-attribute=const")
 #endif

void openlog(const char *ident, int option, int facility)
{
    (void)ident; (void)option; (void)facility;
}

void syslog(int priority, const char *format, ...)
{
    (void)priority; (void)format;
}

void vsyslog(int priority, const char *format, va_list ap)
{
    (void)priority; (void)format; (void)ap;
}

 #if EM_COMPILER_GCC(6,0) && ! EM_COMPILER_GCC(10,0)
  EM_POP_DIAGNOSTIC
 #endif

#endif

// vi:sts=4:sw=4:et:foldmethod=marker
