//  This file is distributed as part of the bit-babbler package.
//  Copyright 2015 - 2018,  Ron <ron@debian.org>

#ifndef _BB_SOCKET_SOURCE_H
#define _BB_SOCKET_SOURCE_H

#include <bit-babbler/secret-source.h>
#include <bit-babbler/socket.h>


namespace BitB
{
    class SocketSource : public RefCounted
    { //{{{
    private:

       #if EM_PLATFORM_MSW
        WinsockScope            m_winsock;
       #endif

        Pool::Handle            m_pool;
        SockAddr                m_sa;
        int                     m_fd;
        pthread_t               m_serverthread;


        BB_NORETURN
        void do_server_thread()
        { //{{{

            SetThreadName( "UDP out" );

            std::string     addr = m_sa.AddrStr();

            Log<3>( "SocketSource( %s ): begin server_thread\n", addr.c_str() );

            const size_t    MAX_BYTES = 32768;
            union {
                uint16_t    len;
                char        buf[8];
            };
            uint8_t         rbuf[MAX_BYTES];
            sockaddr_any_t  peeraddr;
            HealthMonitor   qa( "UDP" );

            for(;;)
            {
                socklen_t   peeraddrlen = sizeof( peeraddr.ss );

                ssize_t n = recvfrom( m_fd, buf, sizeof(buf), 0, &peeraddr.any, &peeraddrlen );

                if( n == 2 )
                {
                    size_t  bytes = ntohs(len);
                    size_t  r;

                    Log<5>( "SocketSource( %s ): request for %zu bytes\n", addr.c_str(), bytes );

                    if( bytes < 1 || bytes > MAX_BYTES)
                    {
                        Log<2>( _("SocketSource( %s ): ignoring %zd byte request\n"),
                                                                addr.c_str(), bytes );
                        continue;
                    }

                    do {
                        r = m_pool->read( rbuf, bytes );
                    }
                    while( ! qa.Check( rbuf, r ) );

                    Log<5>( "SocketSource( %s ): returning %zu bytes\n", addr.c_str(), r );

                   #if EM_PLATFORM_MSW
                    n = sendto( m_fd, reinterpret_cast<const char*>(rbuf), r, 0,
                                                    &peeraddr.any, peeraddrlen );
                   #else
                    n = sendto( m_fd, rbuf, r, 0, &peeraddr.any, peeraddrlen );
                   #endif

                    if( n == -1 )
                        LogSocketErr<1>( _("SocketSource( %s ): sendto failed"), addr.c_str() );
                    else if( size_t(n) != r )
                        Log<2>( _("SocketSource( %s ): only %zd of %zu bytes sent\n"),
                                                                    addr.c_str(), n, r );
                }
                else if( n == -1 )
                    LogSocketErr<1>( _("SocketSource( %s ): recvfrom failed"), addr.c_str() );
                else
                    Log<2>( _("SocketSource( %s ): ignoring %zd byte message\n"),
                                                                addr.c_str(), n );
            }

        } //}}}

        static void *server_thread( void *p )
        { //{{{

            SocketSource    *s = static_cast<SocketSource*>( p );

            try {
                s->do_server_thread();
            }
            catch( const abi::__forced_unwind& )
            {
                Log<3>( "SocketSource( '%s' ): server_thread cancelled\n",
                                                s->m_sa.AddrStr().c_str() );
                throw;
            }
            BB_CATCH_STD( 0, _("uncaught SocketSource::server_thread exception") )

            return NULL;

        } //}}}


        void Close()
        { //{{{

           #if EM_PLATFORM_MSW
            closesocket( m_fd );
           #else
            close( m_fd );
           #endif

        } //}}}


    public:

        typedef RefPtr< SocketSource >  Handle;


        SocketSource( const Pool::Handle &pool, const std::string &addr, bool freebind = false )
            : m_pool( pool )
            , m_sa( addr )
        { //{{{

            Log<2>( "+ SocketSource( '%s' )\n", addr.c_str() );

            m_sa.GetAddrInfo( SOCK_DGRAM, AI_ADDRCONFIG | AI_PASSIVE );

            if( m_sa.addr.any.sa_family != AF_INET && m_sa.addr.any.sa_family != AF_INET6 )
                throw Error( _("SocketSource( %s ): not an IPv4 or IPv6 address (family %u)"),
                                                        addr.c_str(), m_sa.addr.any.sa_family );


            m_fd = socket( m_sa.addr.any.sa_family, m_sa.addr_type, m_sa.addr_protocol );

            if( m_fd == -1 )
                throw SocketError( _("SocketSource( %s ): failed to open socket"),
                                                                    addr.c_str() );
            try {
                if( freebind )
                    EnableFreebind( m_fd, stringprintf("SocketSource( %s )", addr.c_str()) );

                if( bind( m_fd, &m_sa.addr.any, m_sa.addr_len ) == -1 )
                    throw SocketError( _("SocketSource( %s ): bind failed"), addr.c_str() );

                int ret = pthread_create( &m_serverthread, GetDefaultThreadAttr(), server_thread,
                                                                                            this );
                if( ret )
                    throw SystemError( ret, _("SocketSource( %s ): failed to create server thread"),
                                                                                    addr.c_str() );
            }
            catch( ... )
            {
                Close();
                throw;
            }

        } //}}}

        ~SocketSource()
        { //{{{

            std::string     addr = m_sa.AddrStr();

            Log<2>( _("- SocketSource( %s )\n"), addr.c_str() );

            Log<3>( _("SocketSource( %s ): terminating server\n"), addr.c_str() );
            pthread_cancel( m_serverthread );

            Log<3>( _("SocketSource( %s ): waiting for server termination\n"), addr.c_str() );
            pthread_join( m_serverthread, NULL );

            Close();

        } //}}}

    }; //}}}

}   // BitB namespace


#endif  // _BB_SOCKET_SOURCE_H

// vi:sts=4:sw=4:et:foldmethod=marker
