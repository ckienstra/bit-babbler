//  This file is distributed as part of the bit-babbler package.
//  Copyright 1998 - 2018,  Ron <ron@debian.org>

#ifndef _BB_SOCKET_H
#define _BB_SOCKET_H

#include <bit-babbler/log.h>

#if EM_PLATFORM_POSIX

    #include <sys/socket.h>
    #include <sys/un.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>

#elif EM_PLATFORM_MSW

    #include <ws2tcpip.h>

#else

    #error Unsupported platform

#endif


namespace BitB
{
    union sockaddr_any_t
    {
        struct sockaddr         any; // Generic socket address.
        struct sockaddr_storage ss;  // Largest available socket address space.

        struct sockaddr_in      in;  // IPv4 domain socket address.
        struct sockaddr_in6     in6; // IPv6 domain socket address.

       #if EM_PLATFORM_POSIX
        struct sockaddr_un      un;  // Unix domain socket address.
       #endif
    };


    struct SockAddr
    { //{{{

        std::string     host;
        std::string     service;

        int             addr_type;
        int             addr_protocol;
        socklen_t       addr_len;
        sockaddr_any_t  addr;


        // Parse an address string of the form 'host:service'
        // where the host part (but not the colon) is optional.
        // INADDR_ANY is assumed if no host is provided.
        SockAddr( const std::string &addrstr )
        { //{{{

            size_t  n = addrstr.rfind(':');

            if( n != std::string::npos && n + 1 < addrstr.size() )
            {
                service = addrstr.substr( n + 1 );

                if( addrstr[0] == '[' && n > 2 )
                    host = addrstr.substr( 1, n - 2 );
                else
                    host = addrstr.substr( 0, n );
            }

            if( service.empty() )
                throw Error( _("SockAddr( '%s' ): no service address"),
                                                    addrstr.c_str() );
        } //}}}


        std::string AddrStr() const
        { //{{{

            if( host.find(':') != std::string::npos )
                return '[' + host + "]:" + service;

            return host + ':' + service;

        } //}}}

        void GetAddrInfo( int socktype, int flags )
        { //{{{

            addrinfo    hints;
            addrinfo   *addrinf;

            memset( &hints, 0, sizeof(addrinfo) );

            hints.ai_flags      = flags;
            hints.ai_family     = AF_UNSPEC;
            hints.ai_socktype   = socktype;
         // hints.ai_protocol   = 0;
         // hints.ai_addrlen    = 0;
         // hints.ai_addr       = NULL;
         // hints.ai_canonname  = NULL;
         // hints.ai_next       = NULL;

            int err = ::getaddrinfo( host.empty() ? NULL : host.c_str(),
                                     service.c_str(), &hints, &addrinf );
            if( err )
                throw Error( _("SockAddr( '%s' ): failed to get address: %s"),
                                        AddrStr().c_str(), gai_strerror( err ) );

            if( addrinf->ai_addrlen > sizeof(sockaddr_storage) )
            {
                freeaddrinfo( addrinf );
                throw Error( _("SockAddr( '%s' ): ai_addrlen %ju > sockaddr_storage %zu"),
                                        AddrStr().c_str(), uintmax_t(addrinf->ai_addrlen),
                                                                sizeof(sockaddr_storage) );
            }

            addr_type       = addrinf->ai_socktype;
            addr_protocol   = addrinf->ai_protocol;
            addr_len        = addrinf->ai_addrlen;

            memcpy( &addr.any, addrinf->ai_addr, addr_len );
            memset( reinterpret_cast<uint8_t*>(&addr.any) + addr_len, 0,
                    sizeof(sockaddr_storage) - addr_len );

            freeaddrinfo( addrinf );

        } //}}}

    }; //}}}



   #if EM_PLATFORM_MSW

    class SocketError : public Error
    { //{{{
    private:

        int     m_errno;
        char    m_errmsg[65536];


        char *GetSysMsg()
        {
            m_errmsg[0] = '\0';
            FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            0, m_errno, 0, m_errmsg, sizeof(m_errmsg), NULL );
            return m_errmsg;
        }


    public:

        SocketError() throw()
            : m_errno( WSAGetLastError() )
        {
            SetMessage( "Socket Error: %s", GetSysMsg() );
        }

        SocketError( const std::string &msg ) throw()
            : m_errno( WSAGetLastError() )
        {
            SetMessage( "%s", (msg + ": " + GetSysMsg()).c_str() );
        }

        BB_PRINTF_FORMAT(2,3)
        SocketError( const char *format, ... ) throw()
            : m_errno( WSAGetLastError() )
        {
            va_list   arglist;
            va_start( arglist, format );
            SetMessage( format, arglist );
            va_end( arglist );

            AppendMessage( std::string(": ") + GetSysMsg() );
        }

        BB_PRINTF_FORMAT(3,4)
        SocketError( int code, const char *format, ... ) throw()
            : m_errno( code )
        {
            va_list   arglist;
            va_start( arglist, format );
            SetMessage( format, arglist );
            va_end( arglist );

            AppendMessage( std::string(": ") + GetSysMsg() );
        }


        int GetErrorCode() const { return m_errno; }

    }; //}}}


    template< int N >
    BB_PRINTF_FORMAT(1,2)
    void LogSocketErr( const char *format, ... )
    { //{{{

        char    errmsg[65536];
        int     errnum = WSAGetLastError();

        va_list   arglist;
        va_start( arglist, format );

        std::string msg = vstringprintf( format, arglist );

        if( msg.size() && msg[msg.size() - 1] == '\n' )
            msg.erase( msg.size() - 1 );

        errmsg[0] = '\0';
        FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        0, errnum, 0, errmsg, sizeof(errmsg), NULL );

        Log<N>( "%s: %s\n", msg.c_str(), errmsg );
        va_end( arglist );

    } //}}}


    class WinsockScope
    { //{{{
    private:

        WSADATA m_wsa;

    public:

        WinsockScope()
        {
            int ret = WSAStartup(MAKEWORD(2,2), &m_wsa);

            if( ret )
                throw Error( "WSAStartup failed with error %d", ret );
        }

        ~WinsockScope()
        {
            WSACleanup();
        }

    }; //}}}

   #else  // ! EM_PLATFORM_MSW

    #define SocketError     SystemError
    #define LogSocketErr    LogErr

    struct WinsockScope { WinsockScope() {} };

   #endif


    static inline void EnableFreebind( int fd, const std::string &where = std::string() )
    { //{{{

       #ifdef IP_FREEBIND

        int i = 1;
        if( setsockopt( fd, IPPROTO_IP, IP_FREEBIND, &i, sizeof(i) ) == -1 )
            throw SocketError( _("%s: Failed to set IP_FREEBIND"), where.c_str() );

       #elif defined(IP_BINDANY)

        // FreeBSD variant, requires PRIV_NETINET_BINDANY privilege to enable.
        int i = 1;
        if( setsockopt( fd, IPPROTO_IP, IP_BINDANY, &i, sizeof(i) ) == -1 )
            throw SocketError( _("%s: Failed to set IP_FREEBIND (IP_BINDANY)"),
                                                                where.c_str() );
       #elif defined(SO_BINDANY)

        // OpenBSD variant, requires superuser privilege to enable.
        int i = 1;
        if( setsockopt( fd, SOL_SOCKET, SO_BINDANY, &i, sizeof(i) ) == -1 )
            throw SocketError( _("%s: Failed to set IP_FREEBIND (SO_BINDANY)"),
                                                                where.c_str() );
       #else

        (void)fd;
        Log<0>( _("%s: IP_FREEBIND is not supported on this platform\n"), where.c_str() );

       #endif

    } //}}}


    // Check if systemd is expecting us to acknowledge it.
    //{{{
    // The actually documented guarantees here leave something to be desired,
    // but if NOTIFY_SOCKET is set in the environment, and contains either an
    // absolute path, or a string begining with an '@', then it is probably
    // systemd indicating that it wants notification sent to either a named
    // unix domain socket or an abstract socket, respectively.  The actual
    // address of the abstract socket is obtained by replacing the @ with a
    // null character.  Though that seems academic, because in practice it
    // appears to always use a named socket.  If something else sets this,
    // then the caller who did that gets to keep all the pieces ...
    //
    // If this returns a non-empty string, the above conditions have been met.
    //}}}
    static inline std::string GetSystemdNotifySocket()
    { //{{{

        char  *s = getenv("NOTIFY_SOCKET");

        if( ! s || (s[0] != '@' && s[0] != '/') || s[1] == '\0' )
            return std::string();

        return s;

    } //}}}

    // Send a notification message to systemd.
    //{{{
    // This will do nothing if the NOTIFY_SOCKET was not set, otherwise it will
    // try to send the given message to the indicated address and throw if we
    // aren't able to do that.  Since systemd doesn't actually acknowledge our
    // acknowledgement, there's no way to know if this actually did anything
    // aside from squirting a datagram out into the void.  If it really was
    // systemd expecting something from us, then it will terminate this process
    // if it doesn't get a READY message before its timeout expires.  It will
    // also reject the message if the sender's SCM_CREDENTIALS are not included
    // in the packet sent, but sendto(2) will include those for us, without
    // needing to bloat the code here with some useless Trying To Look Clever,
    // and then needing to guard most of that to try and keep it all portable.
    //}}}
    static inline void SystemdNotify( const std::string &msg,
                                      const std::string &ns = GetSystemdNotifySocket() )
    { //{{{

       #if EM_PLATFORM_POSIX

        if( ns.empty() )
            return;

        sockaddr_any_t  addr;
        socklen_t       addrlen = socklen_t(offsetof(sockaddr_un, sun_path) + ns.size());

        if( ns.size() >= sizeof(addr.un.sun_path) )
            throw Error( _("SystemdNotify: socket path '%s' is too long.  "
                           "Maximum length is %zu bytes."),
                           ns.c_str(), sizeof(addr.un.sun_path) - 1 );

        addr.un.sun_family = AF_UNIX;
        ns.copy( addr.un.sun_path, sizeof(addr.un.sun_path) - 1 );
        addr.un.sun_path[ ns.size() ] = '\0';

        // Systemd passes abstract socket addresses with an initial '@',
        // but Linux identifies them by using a null as the first byte.
        if( ns[0] == '@' )
            addr.un.sun_path[0] = '\0';


        int fd = socket( AF_UNIX, SOCK_DGRAM, 0 );

        if( fd == -1 )
            throw SocketError( _("SystemdNotify( %s, %s ): failed to create socket"),
                                                            msg.c_str(), ns.c_str() );

        ssize_t n = sendto( fd, msg.c_str(), msg.size(), 0, &addr.any, addrlen );

        close(fd);

        if( n < 0 )
            throw SocketError( _("SystemdNotify( %s, %s ): failed to send message"),
                                                            msg.c_str(), ns.c_str() );
        if( size_t(n) < msg.size() )
            throw Error( _("SystemdNotify( %s, %s ): failed to send entire message"
                                                            " (only %zd/%zu bytes)"),
                                            msg.c_str(), ns.c_str(), n, msg.size() );
       #else

        (void)msg; (void)ns;

       #endif

    } //}}}

}   // BitB namespace

#endif // _BB_SOCKET_H

// vi:sts=4:sw=4:et:foldmethod=marker
