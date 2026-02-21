//  This file is distributed as part of the bit-babbler package.
//  Copyright 2004 - 2018,  Ron <ron@debian.org>

#ifndef _BB_CLIENT_SOCKET_H
#define _BB_CLIENT_SOCKET_H

#include <bit-babbler/json.h>
#include <bit-babbler/socket.h>


namespace BitB
{

    class ClientSock : public RefCounted
    { //{{{
    private:

       #if EM_PLATFORM_MSW
        WinsockScope    m_winsock;
       #endif

        std::string     m_id;
        int             m_fd;
        size_t          m_maxsize;
        size_t          m_fill;
        char           *m_buf;


        void ConnectSocket( int domain, int type, int protocol,
                            const struct sockaddr *addr, socklen_t addrlen )
        { //{{{

            m_fd = socket( domain, type, protocol );

            if( m_fd == -1 )
                throw SocketError( _("ClientSock( %s ): failed to create socket"),
                                                                    m_id.c_str() );

            if( connect( m_fd, addr, addrlen ) == -1 )
                throw SocketError( _("ClientSock( %s ): failed to connect socket"),
                                                                    m_id.c_str() );
        } //}}}

        void CreateUnixSocket( const std::string &path )
        { //{{{

           #if EM_PLATFORM_POSIX

            sockaddr_any_t  addr;

            if( path.size() >= sizeof(addr.un.sun_path) )
                throw Error( _("ClientSock: socket path '%s' is too long.  "
                               "Maximum length is %zu bytes."),
                               path.c_str(), sizeof(addr.un.sun_path) - 1 );

            addr.un.sun_family = AF_UNIX;

            path.copy( addr.un.sun_path, sizeof(addr.un.sun_path) - 1 );
            addr.un.sun_path[ path.size() ] = '\0';

            ConnectSocket( AF_UNIX, SOCK_STREAM, 0, &addr.any, sizeof(addr.un) );

           #else
            (void)path;
            throw Error("Unix sockets are not supported on this platform");
           #endif

        } //}}}

        void CreateTCPSocket( const std::string &addr )
        { //{{{

            SockAddr    sa( addr );

            sa.GetAddrInfo( SOCK_STREAM, AI_ADDRCONFIG );

            if( sa.addr.any.sa_family != AF_INET && sa.addr.any.sa_family != AF_INET6 )
                throw Error( _("ClientSock( %s ): not an IPv4 or IPv6 address (family %u)"),
                                                    m_id.c_str(), sa.addr.any.sa_family );

            ConnectSocket( sa.addr.any.sa_family, sa.addr_type, sa.addr_protocol,
                                                        &sa.addr.any, sa.addr_len );
        } //}}}

        size_t do_read( char *buf, size_t size, Json::Handle &json )
        { //{{{

            for(;;)
            {
                if( m_fill )
                {
                    size_t  len = strnlen( m_buf, m_fill );

                    if( len < m_fill )
                    {
                        // We have a null terminated reply
                        if( buf )
                        {
                            if( len >= size )
                                throw Error( _("ClientSock::read( %zu ): buffer too small"
                                                                     " for %zu byte reply"),
                                                                            size, len + 1 );
                            memcpy( buf, m_buf, len + 1 );
                        }
                        else
                            json = new Json( m_buf );


                        if( len == m_fill - 1 )
                        {
                            // and no bytes from the next reply yet
                            m_fill = 0;
                            return len + 1;
                        }

                        // there is (at least the start of) another reply
                        memmove( m_buf, m_buf + len + 1, m_fill - len - 1 );
                        m_fill -= len + 1;

                        return len + 1;

                    } else {

                        // We haven't read the whole reply yet
                        if( m_fill == m_maxsize )
                        {
                            // we have a whole buffer full of data now,
                            // but there was no request terminator seen,
                            // so whatever is in there must be invalid.

                            m_fill = 0;

                            throw Error( _("ClientSock::read( %zu ): max message size exceeded, "
                                           "read %zu bytes with no terminator"), size, m_maxsize );
                        }
                        // else
                            // we haven't filled the whole buffer yet,
                            // so maybe this was just a short read.
                            // Try to read some more.
                    }
                }


                ssize_t n = recv( m_fd, m_buf + m_fill, m_maxsize - m_fill, 0 );

                if( n < 0 )
                    throw SocketError( _("ClientSock::read( %zu ): failed"), size );

                if( n == 0 )
                {
                    m_fill = 0;
                    throw Error( _("ClientSock::read( %zu ): EOF"), size );
                }

                Log<4>( "ClientSock::read( %zu ): %zu bytes at %zu\n", size, n, m_fill );

                m_fill += size_t(n);
            }

        } //}}}


        void Close()
        { //{{{

            if( m_fd == -1 )
                return;

           #if EM_PLATFORM_MSW
            closesocket( m_fd );
           #else
            close( m_fd );
           #endif

        } //}}}


    public:

        typedef RefPtr< ClientSock >    Handle;


        ClientSock( const std::string &addr, size_t max_msg_size = 64 * 1024 * 1024 )
            : m_id( addr )
            , m_fd( -1 )
            , m_maxsize( max_msg_size )
            , m_fill( 0 )
        {
            Log<2>( "+ ClientSock( '%s', %zu )\n", m_id.c_str(), m_maxsize );

            if( addr.find("tcp:") == 0 )
                CreateTCPSocket( addr.substr(4) );
            else
                CreateUnixSocket( addr );

            m_buf = new char[m_maxsize];
        }

        ~ClientSock()
        {
            Log<2>( "- ClientSock( '%s', %zu )\n", m_id.c_str(), m_maxsize );

            Close();
            delete [] m_buf;
        }


        // Low-level I/O
        size_t write( const char *buf, size_t len )
        { //{{{

            ssize_t n = send( m_fd, buf, len, 0 );

            if( n < 0 )
                throw SocketError( _("ClientSock::write( %zu ): failed"), len );

            return size_t(n);

        } //}}}

        size_t read( char *buf, size_t size )
        {
            Json::Handle    unused;
            return do_read( buf, size, unused );
        }


        // High-level I/O
        void SendRequest( const std::string &req )
        { //{{{

            size_t  n = req.size() + 1;

            Log<3>( _("ClientSock::SendRequest: '%s'\n"), req.c_str() );

            for( size_t c = n; c; )
            {
                ssize_t w = send( m_fd, req.c_str() + n - c, c, 0 );

                if( w < 0 )
                    throw SocketError( _("ClientSock::SendRequest: write failed") );

                if( w == 0 )
                    throw Error( _("ClientSock::SendRequest: write EOF") );

                c -= size_t(w);
            }

        } //}}}

        Json::Handle Read()
        {
            Json::Handle    json;
            do_read( NULL, 0, json );

            return json;
        }

    }; //}}}

}   // BitB namespace


#endif  // _BB_CLIENT_SOCKET_H

// vi:sts=4:sw=4:et:foldmethod=marker
