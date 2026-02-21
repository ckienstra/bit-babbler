//  This file is distributed as part of the bit-babbler package.
//  Copyright 2003 - 2021,  Ron <ron@debian.org>

#ifndef _BB_EXCEPTIONS_H
#define _BB_EXCEPTIONS_H

#include <string>
#include <cxxabi.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <cerrno>


// If the thread cancellation exceptions are not supported, provide our own
// definition of the exception type.  The main trick here is that we can't
// put it directly into namespace abi if that is really an alias to some
// other internal namespace name.  If it's an alias to some other name than
// __cxxabiv1, then this will explode at build time and we'll need to add
// a new configure test for other possible aliases, but right now that is
// what is used on all platforms with the abi namespace so far.
#if !HAVE_ABI_FORCED_UNWIND

   #if HAVE_ABI_ALIAS_TO_CXXABIV1
    namespace __cxxabiv1
   #else
    namespace abi
   #endif
    {
        struct __forced_unwind {};
    }

#endif


namespace BitB
{

    BB_PRINTF_FORMAT(2,0)
    static inline int Vasprintf( char **strp, const char *format, va_list arglist )
    { //{{{

      #if HAVE_VASPRINTF

        return vasprintf( strp, format, arglist );

      #else
        // We can implement this in alternative ways if needed
        #error "vasprintf is not supported"
      #endif

    } //}}}


    class Exception : public std::exception
    { //{{{
    private:

        std::string     m_msg;


    public:

        Exception() throw() {}

        Exception( const std::string &msg ) throw()
            : m_msg( msg )
        {}

        BB_PRINTF_FORMAT(2,3)
        Exception( const char *format, ... ) throw()
        {
            va_list     arglist;

            va_start( arglist, format );
            SetMessage( format, arglist );
            va_end( arglist );
        }

        // Default copy ctor and assignment oper ok.

        ~Exception() throw() {}


        void SetMessage( const std::string &msg ) throw()
        {
            m_msg = msg;
        }

        BB_PRINTF_FORMAT(2,0)
        void SetMessage( const char *format, va_list args ) throw()
        {
            char    *msg = NULL;

            if( Vasprintf( &msg, format, args ) >= 0 )
            {
                m_msg = msg;
                free( msg );
            }
            else
                m_msg.append( " *** Error in BitB::Exception::SetMessage" );
        }

        BB_PRINTF_FORMAT(2,3)
        void SetMessage( const char *format, ... ) throw()
        {
            va_list     arglist;

            va_start( arglist, format );
            SetMessage( format, arglist );
            va_end( arglist );
        }

        void AppendMessage( const std::string &msg ) throw()
        {
            m_msg.append( msg );
        }

        const char *what() const throw()
        {
            return m_msg.empty() ? "Unspecified BitB::Exception" : m_msg.c_str();
        }

    }; //}}}


    class Error : public Exception
    { //{{{
    public:

        Error() throw() {}

        Error( const std::string &msg ) throw()
            : Exception( msg )
        {}

        BB_PRINTF_FORMAT(2,3)
        Error( const char *format, ... ) throw()
        {
            va_list     arglist;

            va_start( arglist, format );
            SetMessage( format, arglist );
            va_end( arglist );
        }

    }; //}}}


    class SystemError : public Error
    { //{{{
    private:

        int     m_errno;


    public:

        SystemError() throw()
            : m_errno( errno )
        {
            SetMessage( "System Error: %s", strerror( m_errno ) );
        }

        SystemError( const std::string &msg ) throw()
            : Error( msg + ": " + strerror(errno) )
            , m_errno( errno )
        {}

        BB_PRINTF_FORMAT(2,3)
        SystemError( const char *format, ... ) throw()
            : m_errno( errno )
        {
            va_list   arglist;
            va_start( arglist, format );
            SetMessage( format, arglist );
            va_end( arglist );

            AppendMessage( std::string(": ") + strerror(m_errno) );
        }

        BB_PRINTF_FORMAT(3,4)
        SystemError( int code, const char *format, ... ) throw()
            : m_errno( code )
        {
            va_list   arglist;
            va_start( arglist, format );
            SetMessage( format, arglist );
            va_end( arglist );

            AppendMessage( std::string(": ") + strerror(m_errno) );
        }


        int GetErrorCode() const { return m_errno; }

    }; //}}}

}

#endif  // _BB_EXCEPTIONS_H

// vi:sts=4:sw=4:et:foldmethod=marker
