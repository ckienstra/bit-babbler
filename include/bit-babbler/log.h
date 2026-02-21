//  This file is distributed as part of the bit-babbler package.
//  Copyright 2003 - 2021,  Ron <ron@debian.org>
//
// You must include bit-babbler/impl/log.h exactly once in some translation unit
// of any program using the Log() functions.

#ifndef _BB_LOG_H
#define _BB_LOG_H

#include <bit-babbler/exceptions.h>

#ifdef _REENTRANT
    #include <pthread.h>

  #if HAVE_PTHREAD_SET_NAME_NP
    #include <pthread_np.h>
  #endif
#endif

#if EM_PLATFORM_POSIX

    #include <syslog.h>

#else

    // Stub definitions of the syslog functionality we use.
    // The openlog and vsyslog functions will need to be implemented suitably
    // for platforms that don't provide them natively.

    // Option flag for openlog
    #define LOG_PID     0x01	// Log the Process ID with each message

    // Syslog facility
    #define LOG_DAEMON  (3<<3)  // system daemons

    // Syslog priority
    #define LOG_NOTICE  5       // normal but significant condition

    void openlog(const char *ident, int option, int facility);
    void syslog(int priority, const char *format, ...);
    void vsyslog(int priority, const char *format, va_list ap);

#endif

#if !HAVE_DECL_LOG_MAKEPRI

    // This is missing on OpenBSD 6.1, it is fairly widely supported
    // elsewhere, but isn't actually required by POSIX.1-2008 (SuSv4).
    #define LOG_MAKEPRI(facility, priority) ((facility) | (priority))

#endif


#include <sys/time.h>
#include <stdint.h>


#if EM_USE_GETTEXT

    #include <libintl.h>

    #define _(x)        gettext(x)
    #define P_(x,y,n)  ngettext(x,y,n)

#else

    #define _(x) x
    #define P_(singular, plural, number)    ( number == 1 ? singular : plural )

#endif


#define BB_CATCH_STD( LogLevel, Message, ... )                          \
    catch( const std::exception &e )                                    \
    {                                                                   \
        BitB::Log< LogLevel >( "%s: %s\n", Message, e.what() );         \
        __VA_ARGS__                                                     \
    }                                                                   \
    catch( ... )                                                        \
    {                                                                   \
        BitB::Log< LogLevel >( "%s\n", Message );                       \
        __VA_ARGS__                                                     \
    }

#define BB_CATCH_ALL( LogLevel, Message, ... )                          \
    catch( const abi::__forced_unwind& )                                \
    {                                                                   \
        BitB::Log< LogLevel >( "%s: thread cancelled\n", Message );     \
        __VA_ARGS__                                                     \
        throw;                                                          \
    }                                                                   \
    BB_CATCH_STD( LogLevel, Message, __VA_ARGS__ )


namespace BitB
{
  //! @name String length functions
  //! Overloaded for generic template friendliness.
  //@{ //{{{

    //! Return the (string) length of a @c char.
    inline size_t stringlength( char ) { return 1; }

    //! Return the length of a @c NULL terminated @c char* string.
    inline size_t stringlength( const char *s ) { return ( s ) ? strlen( s ) : 0; }

    //! Return the (string) length of a @c wchar_t.
    inline size_t stringlength( wchar_t ) { return 1; }

    //! Return the length of a @c NULL terminated @c wchar_t* string.
    inline size_t stringlength( const wchar_t *s ) { return ( s ) ? wcslen( s ) : 0; }

    //! Return the length of a string type (with a @c size() method).
    template< typename S >
    BB_PURE size_t stringlength( const S &s ) { return s.size(); }

  //@} //}}}


    //! Test if @a s starts with the string @a c
    //{{{
    //! @param c The substring to match.  It may be a:
    //!             - @c NULL terminated @c char* or @c wchar_t* string
    //!             - @c std::basic_string compatible type.
    //! @param s The string to match @a c in.  Must be a @c std::basic_string
    //!          compatible type.
    //!
    //! @return @c true if @a s starts with the string @a c
    //}}}
    template< typename C, typename S >
    bool StartsWith( C c, const S &s )
    { //{{{

        size_t  n = stringlength(c);

        if( s.size() < n )
            return false;

        return s.compare( 0, n, c ) == 0;

    } //}}}


    //! Returns all characters after the first occurrence of @a c, or empty if @a c is not in @a s.
    //{{{
    //! @param c The character or substring to match.  It may be a:
    //!             - single @c char or @c wchar_t type
    //!             - @c NULL terminated @c char* or @c wchar_t* string
    //!             - @c std::basic_string compatible type.
    //! @param s The string to match @a c in.  Must be a @c std::basic_string
    //!          compatible type.
    //!
    //! @return A string of type @a S, containing all characters after the first
    //!         occurrence of @a c, or an empty string if @a c is not in @a s.
    //}}}
    template< typename C, typename S >
    S afterfirst( C c, const S &s )
    { //{{{

        typename S::size_type   n = s.find( c );
        return ( n == S::npos || ( n += stringlength( c ), n == s.size() ) )
               ? S()
               : s.substr( n );

    } //}}}

    //! Return all characters before the first occurrence of @a c, or @a s if @a c is not in @a s.
    //{{{
    //! @param c The character or substring to match.  It may be a:
    //!             - single @c char or @c wchar_t type
    //!             - @c NULL terminated @c char* or @c wchar_t* string
    //!             - @c std::basic_string compatible type.
    //! @param s The string to match @a c in.  Must be a @c std::basic_string
    //!          compatible type.
    //!
    //! @return A string of type @a S, containing all characters before the
    //!         first occurrence of @a c, or @a s if @a c is not in @a s.
    //}}}
    template< typename C, typename S >
    S beforefirst( C c, const S &s )
    { //{{{

        typename S::size_type   n = s.find( c );

        return ( n == S::npos ) ? s
                                : ( n == 0 ) ? S()
                                             : S( s.substr( 0, n ) );
                   // cast here to keep the nested ternary operator happy

    } //}}}



    typedef std::basic_string<uint8_t>      OctetString;

    std::string OctetsToHex( const OctetString &octets, size_t wrap = 0, bool short_form = false );

    static inline std::string OctetsToShortHex( const OctetString &octets, size_t wrap = 0 )
    {
        return OctetsToHex( octets, wrap, true );
    }

    template< typename T >
    std::string AsBinary( T n )
    { //{{{

        std::string     s;

        for( int i = sizeof(T) * 8 - 1; i >= 0; --i )
            s.push_back( n & 1ull<<i ? '1' : '0' );

        return s;

    } //}}}

    static inline unsigned long StrToUL( const char *s, int base = 0 )
    { //{{{

        char           *e;
        unsigned long   v = strtoul( s, &e, base );

        if( e == s || *e != '\0' )
            throw Error( _("StrToUL(%s): not a number"), s );

        return v;

    } //}}}

    static inline unsigned long StrToUL( const std::string &s, int base = 0 )
    {
        return StrToUL( s.c_str(), base );
    }

    // These two mostly exist as a convenience to hush clang paranoia about
    // implicit conversions losing precision for unsigned int conversions.
    static inline unsigned StrToU( const char *s, int base = 0 )
    {
        return unsigned(StrToUL(s, base));
    }

    static inline unsigned StrToU( const std::string &s, int base = 0 )
    {
        return StrToU( s.c_str(), base );
    }


    unsigned long StrToScaledUL( const char *s, unsigned scale = 1000 );

    static inline unsigned long StrToScaledUL( const std::string &s, unsigned scale = 1000 )
    {
        return StrToScaledUL( s.c_str(), scale );
    }

    static inline unsigned StrToScaledU( const char *s, unsigned scale = 1000 )
    {
        return unsigned(StrToScaledUL(s, scale));
    }

    static inline unsigned StrToScaledU( const std::string &s, unsigned scale = 1000 )
    {
        return StrToScaledU( s.c_str(), scale );
    }


    double StrToScaledD( const char *s );

    static inline double StrToScaledD( const std::string &s )
    {
        return StrToScaledD( s.c_str() );
    }



    // On Windows we use the gnu_printf checking here (instead of letting it assume
    // printf checking should use ms_printf), because we already rely on other mingw
    // extensions, and with _GNU_SOURCE defined then (on C++) mingw will enable
    // __USE_MINGW_ANSI_STDIO to support the full set of POSIX format characters.

    BB_PRINTF_FORMAT(1,2)
    std::string stringprintf( const char *format, ... );

    BB_PRINTF_FORMAT(1,0)
    std::string vstringprintf( const char *format, va_list arglist );


    BB_COLD
    void GetFutureTimespec( timespec &ts, unsigned ms );

    static inline timeval GetWallTimeval()
    { //{{{

        timeval     t;

      #if HAVE_GETTIMEOFDAY

        if( gettimeofday( &t, NULL ) == -1 )
            throw Error( _("gettimeofday() failed") );

      #else
        // We can implement this in alternative ways if needed
        #error "No suitable wall time function available"
      #endif

        return t;

    } //}}}

    // And we use the gnu_strftime checking here because otherwise it will warn
    // about using %T and %F (which we do use, and which the msvcrt.dll does not
    // implement), but timeprintf will convert them to equivalents it is ok with
    // if the Windows implementation of strftime(3) is used.
    BB_STRFTIME_FORMAT(1)
    std::string timeprintf( const char *format, const timeval &tv = GetWallTimeval() );


   #ifdef _REENTRANT

    static inline void SetThreadName( const char *name, pthread_t tid = pthread_self() )
    { //{{{

       #if HAVE_PTHREAD_SETNAME_NP_GNU

        pthread_setname_np( tid, name );

       #elif HAVE_PTHREAD_SETNAME_NP_MAC

        // On MacOS this can only be used to set the name of the thread that is
        // calling it, so just do nothing here if that's not the current thread.
        if( pthread_equal( pthread_self(), tid ) )
            pthread_setname_np( name );

        (void)tid;

       #elif HAVE_PTHREAD_SET_NAME_NP

        pthread_set_name_np( tid, name );

       #else

        (void)name; (void)tid;

       #endif

    } //}}}

    static inline void SetThreadName( const std::string &name, pthread_t tid = pthread_self() )
    {
        SetThreadName( name.c_str(), tid );
    }

    const pthread_attr_t *GetDefaultThreadAttr();

   #endif


    extern bool     opt_syslog;
    extern bool     opt_timestamp;
    extern int      opt_verbose;


    static inline void SendLogsToSyslog( const char *id,
                                         int         option   = LOG_PID,
                                         int         facility = LOG_DAEMON )
    { //{{{

        opt_syslog = 1;
        openlog( id, option, facility );

    } //}}}


    template< int N >
    BB_PRINTF_FORMAT(1,0)
    void Logv( const char *format, va_list arglist )
    { //{{{

        if( opt_verbose < N )
            return;

        // On OpenBSD 6.1, vfprintf is an unsafe cancellation point, where a
        // thread can be cancelled while the _thread_flockfile mutex is still
        // held, which results in the next attempt to call it hanging forever.
        // So block cancellation here to work around that bug until it's fixed.
       #if HAVE_BROKEN_STDIO_LOCKING && defined(_REENTRANT)
        int oldstate;

        pthread_testcancel();
        pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldstate );
       #endif

        if( opt_timestamp )
        {
            std::string     msg = timeprintf("%T.%%u") + ": "
                                + vstringprintf( format, arglist );
            if( opt_syslog )
                syslog( LOG_MAKEPRI(LOG_DAEMON, LOG_NOTICE), "%s", msg.c_str() );
            else
                fprintf( stderr, "%s", msg.c_str() );

        } else {

            if( opt_syslog )
                vsyslog( LOG_MAKEPRI(LOG_DAEMON, LOG_NOTICE), format, arglist );
            else
                vfprintf( stderr, format, arglist );
        }

       #if HAVE_BROKEN_STDIO_LOCKING && defined(_REENTRANT)
        pthread_setcancelstate( oldstate, NULL );
       #endif

    } //}}}

    template< int N >
    BB_PRINTF_FORMAT(1,2)
    void Log( const char *format, ... )
    { //{{{

        va_list     arglist;

        va_start( arglist, format );
        Logv<N>( format, arglist );
        va_end( arglist );

    } //}}}

    template< int N >
    BB_PRINTF_FORMAT(1,2)
    void LogErr( const char *format, ... )
    { //{{{

        va_list   arglist;
        va_start( arglist, format );

        std::string msg = vstringprintf( format, arglist );

        if( msg.size() && msg[msg.size() - 1] == '\n' )
            msg.erase( msg.size() - 1 );

        Log<N>( "%s: %s\n", msg.c_str(), strerror(errno) );
        va_end( arglist );

    } //}}}

    template< int N >
    BB_PRINTF_FORMAT(2,3)
    void LogErr( int code, const char *format, ... )
    { //{{{

        va_list   arglist;
        va_start( arglist, format );

        std::string msg = vstringprintf( format, arglist );

        if( msg.size() && msg[msg.size() - 1] == '\n' )
            msg.erase( msg.size() - 1 );

        Log<N>( "%s: %s\n", msg.c_str(), strerror(code) );
        va_end( arglist );

    } //}}}


    std::string DemangleSymbol( const char *sym );

    #define EM_TYPEOF( T ) BitB::DemangleSymbol( typeid( T ).name() ).c_str()
}

#endif  // _BB_LOG_H

// vi:sts=4:sw=4:et:foldmethod=marker
