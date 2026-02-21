//  This file is distributed as part of the bit-babbler package.
//  Copyright 2004 - 2018,  Ron <ron@debian.org>

#ifndef _BB_CONTROL_SOCKET_H
#define _BB_CONTROL_SOCKET_H

#include <bit-babbler/users.h>
#include <bit-babbler/health-monitor.h>
#include <bit-babbler/json.h>
#include <bit-babbler/socket.h>

#if EM_PLATFORM_POSIX
 #include <sys/stat.h>
 #include <sys/file.h>
 #include <fcntl.h>
#endif


namespace BitB
{

    class ControlSock : public RefCounted
    { //{{{
    private:

        class Connection : public RefCounted
        { //{{{
        private:

            ControlSock    *m_server;
            int             m_fd;
            pthread_t       m_connectionthread;


            void send_response( const std::string &msg )
            { //{{{

                size_t  n = msg.size() + 1;

                Log<3>( "ControlSock::Connection( %d )::send_response: %zu bytes\n", m_fd, n );

                for( size_t c = n; c; )
                {
                    ssize_t w = send( m_fd, msg.c_str() + n - c, c, 0 );

                    if( w < 0 )
                        throw SocketError( _("ControlSock::Connection( %d ): write failed"),
                                                                                    m_fd );
                    if( w == 0 )
                        throw Error( _("ControlSock::Connection( %d ): write EOF"), m_fd );

                    c -= size_t(w);
                }

            } //}}}

            void process_request( const std::string    &req,
                                  const std::string    &cmd,
                                  size_t                token = 0,
                                  const Json::Handle   &json  = Json::Handle() )
            { //{{{

                if( cmd == "GetIDs" )
                {
                    send_response( "[\"GetIDs\"," + stringprintf("%zu,", token)
                                                  + Monitor::GetIDs() + ']' );
                    return;
                }

                if( cmd == "ReportStats" )
                {
                    std::string     id;

                    if( json.IsNotNULL() )
                        id = json->Get<std::string>(2);

                    send_response( "[\"ReportStats\"," + stringprintf("%zu,", token)
                                                       + Monitor::GetStats(id) + ']' );
                    return;
                }

                if( cmd == "GetRawData" )
                {
                    std::string     id;

                    if( json.IsNotNULL() )
                        id = json->Get<std::string>(2);

                    send_response( "[\"GetRawData\"," + stringprintf("%zu,", token)
                                                      + Monitor::GetRawData(id) + ']' );
                    return;
                }

                if( cmd == "SetLogVerbosity" )
                {
                    if( json.IsNotNULL() )
                        opt_verbose = json[2]->As<int>();

                    Log<0>( "Log verbosity is now %d\n", opt_verbose );

                    send_response( stringprintf( "[\"SetLogVerbosity\",%zu,%d]",
                                                            token, opt_verbose ) );
                    return;
                }

                send_response( "[\"UnknownRequest\"," + stringprintf("%zu,\"", token)
                                                      + Json::Escape(req) + "\"]" );

            } //}}}

            void parse_request( const std::string &req )
            { //{{{

                std::string     error;
                Json            json( req, error );

                if( ! error.empty() )
                {
                    Log<0>( "ControlSock::Connection( %d )::parse_request: "
                            "bad request: '%s' -> '%s'\n", m_fd, req.c_str(), error.c_str() );

                    send_response( "[\"BadRequest\",0,"
                                    "{\"Error\":\""   + Json::Escape(error) + "\""
                                    ",\"Request\":\"" + Json::Escape(req)   + "\"}]" );
                    return;
                }

                Log<4>( "ControlSock::Connection( %d )::parse_request: '%s' -> '%s'\n",
                                            m_fd, req.c_str(), json.JSONStr().c_str() );


                if( json.RootType() == Json::StringType )
                {
                    process_request( req, json );
                    return;
                }

                if( json.RootType() == Json::ArrayType )
                {
                    try {
                        process_request( req, json[0], json[1]->As<size_t>(), json );
                        return;
                    }
                    catch( const abi::__forced_unwind& ) { throw; }
                    catch( const std::exception &e )     { error = e.what(); }
                    catch( ... )                         { error = "Unknown exception"; }

                    send_response( "[\"BadRequest\",0,"
                                    "{\"Error\":\""   + Json::Escape(error) + "\""
                                    ",\"Request\":\"" + Json::Escape(req)   + "\"}]" );
                    return;
                }

                send_response( "[\"BadRequest\",0,"
                                "{\"Error\":\"Invalid request, not an array or string\""
                                ",\"Request\":\"" + Json::Escape(req) + "\"}]" );
            } //}}}


            void do_connection_thread()
            { //{{{

                using std::string;

                SetThreadName( "control connect" );

                Log<3>( "ControlSock::Connection( %d ): begin connection_thread\n", m_fd );

                char        buf[1024];
                size_t      f = 0;

                for(;;)
                {
                    ssize_t n = recv( m_fd, buf + f, sizeof(buf) - f, 0 );

                    if( n < 0 )
                        throw SocketError( _("ControlSock::Connection( %d ): read failed"),
                                                                                    m_fd );
                    if( n == 0 )
                    {
                        Log<3>( "ControlSock::Connection( %d ): read EOF\n", m_fd );
                        return;
                    }

                    Log<3>( "ControlSock::Connection( %d ): read %zu bytes at %zu\n", m_fd, n, f );

                    f += size_t(n);

                    size_t  b = 0;

                    for(;;)
                    {
                        size_t  len = strnlen( buf + b, f - b );

                        if( len < f - b )
                        {
                            // We have a null terminated request
                            parse_request( string( buf + b, len ) );

                            if( len == f - b - 1 )
                            {
                                // and no bytes from the next request yet
                                f = 0;
                                break;
                            }

                            // there is (at least the start of) another request
                            b += len + 1;

                        } else {

                            // We haven't read the whole request yet
                            if( b > 0 )
                            {
                                // clear out the previous request(s) to make as
                                // much room as we can to read the rest of it
                                f -= b;
                                memmove( buf, buf + b, f );
                            }
                            else if( f == sizeof(buf) )
                            {
                                // we have a whole buffer full of data now,
                                // but there was no request terminator seen,
                                // so whatever is in there must be invalid.

                                send_response( "[\"BadRequest\",0,"
                                                "{\"Error\":\"Request too large\""
                                                ",\"Request\":\"" + Json::Escape( string(buf,f) )
                                              + "\"}]" );
                                f = 0;
                            }
                            // else
                                // we haven't filled the whole buffer yet,
                                // so maybe this was just a short read.
                                // Try to read some more.

                            break;
                        }
                    }
                }

            } //}}}

            static void *connection_thread( void *p )
            { //{{{

                Connection::Handle  c = static_cast<Connection*>(p);

                // Drop the 'virtual handle' from the ctor, we have a real one now.
                c->Unref();

                try {
                    c->do_connection_thread();
                }
                catch( const abi::__forced_unwind& )
                {
                    Log<3>( "ControlSock::Connection( %d ): connection_thread cancelled\n",
                                                                                c->m_fd );
                    throw;
                }
                BB_CATCH_STD( 0, _("uncaught ControlSock::connection_thread exception") )

                c->m_server->detach_connection( c );
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

            typedef RefPtr< Connection >    Handle;
            typedef std::list< Handle >     List;


            Connection( ControlSock *server, int fd )
                : m_server( server )
                , m_fd( fd )
            { //{{{

                Log<2>( "+ ControlSock::Connection( %d )\n", fd );

                // Bump the refcount until the thread is started, otherwise we may
                // lose a race with this Connection being released by the caller
                // before the thread can take its handle from the raw pointer.
                // Think of it as a virtual Handle passed with pthread_create.
                Ref();

                // We don't need to Unref() if this fails, because we'll throw
                // and it will never have been constructed to be destroyed ...
                int ret = pthread_create( &m_connectionthread, GetDefaultThreadAttr(),
                                          connection_thread, this );
                if( ret )
                {
                    Close();
                    throw SystemError( ret, _("ControlSock: failed to create connection thread") );
                }

            } //}}}

            ~Connection()
            {
                Log<2>( "- ControlSock::Connection( %d )\n", m_fd );
                Close();
            }


            pthread_t ThreadID() const
            {
                return m_connectionthread;
            }

        }; //}}}


       #if EM_PLATFORM_MSW
        WinsockScope            m_winsock;
       #endif

        std::string             m_id;
        int                     m_fd;
        int                     m_serverthread_err;
        pthread_t               m_serverthread;
        pthread_mutex_t         m_servermutex;
        Connection::List        m_connections;


        void detach_connection( const Connection::Handle &c )
        { //{{{

            ScopedMutex     lock( &m_servermutex );

            for( Connection::List::iterator i = m_connections.begin(),
                                            e = m_connections.end(); i != e; ++i )
            {
                if( (*i) == c )
                {
                    m_connections.erase( i );
                    pthread_detach( c->ThreadID() );
                    return;
                }
            }

        } //}}}

        bool is_fatal_error()
        { //{{{

           #if EM_PLATFORM_MSW

            int err = WSAGetLastError();
            return err != WSAEWOULDBLOCK || err != WSAEINTR;

           #else

            return errno != EAGAIN || errno != EINTR;

           #endif

        } //}}}

        BB_NORETURN
        void do_server_thread()
        { //{{{

            SetThreadName( "control socket" );

            Log<3>( "ControlSock( %s ): begin server_thread\n", m_id.c_str() );

            for(;;)
            {
                int fd = accept( m_fd, NULL, NULL );

                if( fd == -1 )
                {
                    if( is_fatal_error() )
                        throw SocketError( _("ControlSock( %s ): accept failed"), m_id.c_str() );

                    LogSocketErr<1>( _("ControlSock( %s ): accept failed"), m_id.c_str() );
                    continue;
                }

                try {
                    ScopedMutex     lock( &m_servermutex );
                    m_connections.push_back( new Connection( this, fd ) );
                }
                BB_CATCH_ALL( 0, _("ControlSock: failed to create new Connection") )
            }

        } //}}}

        static void *server_thread( void *p )
        { //{{{

            ControlSock     *c = static_cast<ControlSock*>( p );

            try {
                c->do_server_thread();
            }
            catch( const abi::__forced_unwind& )
            {
                Log<3>( "ControlSock( %s ): server_thread cancelled\n", c->m_id.c_str() );
                throw;
            }
            BB_CATCH_STD( 0, _("uncaught ControlSock::server_thread exception") )

            return NULL;

        } //}}}


    protected:

        void ListenSocket( int domain, int type, int protocol,
                           const struct sockaddr *addr, socklen_t addrlen,
                           bool freebind = false )
        { //{{{

            static const int    LISTEN_BACKLOG = 5;

            m_fd = socket( domain, type, protocol );

            if( m_fd == -1 )
                throw SocketError( _("ControlSock( %s ): failed to create socket"),
                                                                    m_id.c_str() );

            if( freebind )
                EnableFreebind( m_fd, stringprintf("ControlSock( %s )", m_id.c_str()) );

            if( bind( m_fd, addr, addrlen ) == -1 )
                throw SocketError( _("ControlSock( %s ): failed to bind socket"),
                                                                    m_id.c_str() );

            if( listen( m_fd, LISTEN_BACKLOG ) == -1 )
                throw SocketError( _("ControlSock( %s ): failed to listen on socket"),
                                                                    m_id.c_str() );
        } //}}}

        void start_server_thread()
        { //{{{

            m_serverthread_err = pthread_create( &m_serverthread, GetDefaultThreadAttr(),
                                                 server_thread, this );
            if( m_serverthread_err )
            {
                pthread_mutex_destroy( &m_servermutex );
                throw SystemError( m_serverthread_err,
                                   _("ControlSock( %s ): failed to create server thread"),
                                                                            m_id.c_str() );
            }

        } //}}}

        bool HaveSocket() const
        {
            return m_fd != -1;
        }

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

        typedef RefPtr< ControlSock >   Handle;


        ControlSock( const std::string &id )
            : m_id( id )
            , m_fd( -1 )
            , m_serverthread_err( ESRCH )
        {
            pthread_mutex_init( &m_servermutex, NULL );
        }

        ~ControlSock()
        { //{{{

            Log<2>( "- ControlSock( '%s' )\n", m_id.c_str() );

            if( m_serverthread_err == 0 )
            {
                Log<3>( "ControlSock: terminating server\n" );
                pthread_cancel( m_serverthread );

                Log<3>( "ControlSock: waiting for server termination\n" );
                pthread_join( m_serverthread, NULL );


                pthread_mutex_lock( &m_servermutex );

                Log<3>( "ControlSock: terminating connections\n" );
                for( Connection::List::iterator i = m_connections.begin(),
                                                e = m_connections.end(); i != e; ++i )
                    pthread_cancel( (*i)->ThreadID() );


                Log<3>( "ControlSock: waiting for connection termination\n" );
                while( ! m_connections.empty() )
                {
                    pthread_t   p = m_connections.back()->ThreadID();

                    m_connections.pop_back();

                    pthread_mutex_unlock( &m_servermutex );
                    pthread_join( p, NULL );
                    pthread_mutex_lock( &m_servermutex );
                }

                pthread_mutex_unlock( &m_servermutex );
            }
            pthread_mutex_destroy( &m_servermutex );

            Close();

        } //}}}

    }; //}}}


    class ControlSockUnix : public ControlSock
    { //{{{
    private:

       #if EM_PLATFORM_POSIX

        sockaddr_any_t      m_addr;
        std::string         m_group;
        gid_t               m_gid;
        int                 m_lockfd;


        bool using_group() const
        {
            return m_gid != gid_t(-1);
        }

        void create_socket_dir( const std::string &path )
        { //{{{

            // We already asserted path is not empty before calling this.

            if( path[0] != '/' )
                throw Error( _("ControlSock( '%s' ): path is not absolute"), path.c_str() );

            if( path[path.size() - 1] == '/' )
                throw Error( _("ControlSock( '%s' ): path ends with trailing '/'"), path.c_str() );

            std::string     dir = path.substr( 0, path.rfind('/') );

            if( dir.empty() )
                throw Error( _("ControlSock( '%s' ): "
                               "cowardly refusing to create socket in the root directory"),
                                                                             path.c_str() );

            mode_t  dirmode = S_IRUSR | S_IWUSR | S_IXUSR;

            if( using_group() )
                dirmode |= S_IRGRP | S_IWGRP | S_IXGRP;


        try_again:

            if( mkdir( dir.c_str(), dirmode ) == -1 )
            {
                switch( errno )
                {
                    case ENOENT:
                        create_socket_dir( dir );
                        goto try_again;

                    case EEXIST:
                    {
                        struct stat     s;

                        if( lstat( dir.c_str(), &s ) )
                            throw SystemError( _("ControlSock( %s ): failed to stat '%s'"),
                                                                path.c_str(), dir.c_str() );
                        if( ! S_ISDIR(s.st_mode) )
                            throw Error( _("ControlSock( %s ): '%s' exists and is not a directory"),
                                                                path.c_str(), dir.c_str() );
                        if( (s.st_mode & 07777) != dirmode )
                            throw Error( _("ControlSock( %s ): '%s' exists but is not mode %.4o"),
                                                                path.c_str(), dir.c_str(), dirmode );
                        if( s.st_uid != geteuid() )
                            throw Error( _("ControlSock( %s ): '%s' exists but is not owned by us"),
                                                                path.c_str(), dir.c_str() );

                        gid_t   gid = using_group() ? m_gid : getegid();

                        if( s.st_gid != gid )
                            throw Error( _("ControlSock( %s ): "
                                           "'%s' exists but is not in the expected group"),
                                                                path.c_str(), dir.c_str() );
                        return;
                    }

                    default:
                        throw SystemError( _("ControlSock( %s ): failed to create directory '%s'"),
                                                                        path.c_str(), dir.c_str() );
                }
            }

            // Force the desired mode, regardless of current umask.
            if( chmod( dir.c_str(), dirmode ) )
                throw SystemError( _("ControlSock( %s ): failed to chmod %.4o '%s'"),
                                                    path.c_str(), dirmode, dir.c_str() );

            if( using_group() && chown( dir.c_str(), uid_t(-1), m_gid ) )
                throw SystemError( _("ControlSock( %s ): failed to chown '%s' to group %s."),
                                                path.c_str(), dir.c_str(), m_group.c_str() );

        } //}}}

        void acquire_socket_lock( const std::string &path )
        { //{{{

            // Take a lock in the socket dir, if we can get it any existing
            // socket is stale and we can safely remove it.  If we can't,
            // then another process is running which still owns it.

            std::string     lockfile = path + ".lock";
            mode_t          mode     = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

            m_lockfd = open( lockfile.c_str(), O_RDWR|O_CREAT, mode );

            if( m_lockfd == -1 )
                throw SystemError( _("ControlSock: failed to open socket lock '%s'"),
                                                                    lockfile.c_str() );

            int ret = flock( m_lockfd, LOCK_EX | LOCK_NB );

            if( ret )
            {
                if( errno == EWOULDBLOCK )
                    throw Error( _("ControlSock( %s ): socket is owned by another process"),
                                                                            path.c_str() );

                throw SystemError( _("ControlSock: failed to obtain socket lock '%s'"),
                                                                    lockfile.c_str() );
            }


            // We own the lock.  Write our PID to it, not for any particular
            // reason we rely on elsewhere, just to entertain curious people.
            //
            // Or maybe don't do that, it should be reasonably safe for us to
            // lock some arbitrary file that doesn't really belong to us after
            // all (at worst it will DoS something else trying to flock() it),
            // but that's a lesser evil than destroying its content, for no
            // real benefit to ourselves from doing so.  We could do some more
            // paranoid sanity checking of it here too, but that's not really
            // worth it just to write something to it that it's likely nobody
            // will ever look at or care about anyway.
            #if 0
            std::string     pid = stringprintf( "%zd\n", ssize_t(getpid()) );

            if( write( m_lockfd, pid.c_str(), pid.size() ) )
                throw SystemError( _("ControlSock( %s ): failed to write pid to socket lock"),
                                                                            path.c_str() );

            if( ftruncate( m_lockfd, pid.size() ) )
                throw SystemError( _("ControlSock( %s ): failed to truncate socket lock"),
                                                                            path.c_str() );

            if( fdatasync( m_lockfd ) )
                throw SystemError( _("ControlSock( %s ): failed to sync socket lock"),
                                                                        path.c_str() );
            #endif


            // Remove a stale socket file only if it really looks like something
            // that we actually might have left behind from a previous ungraceful
            // exit.  We're privileged, we but don't have a licence to stomp upon
            // arbitrary files.  Or at least not those that don't end in .lock ...
            struct stat     s;

            if( lstat( path.c_str(), &s ) == 0 )
            {
                if( ! S_ISSOCK(s.st_mode) )
                    throw Error( _("ControlSock: '%s' exists and is not a socket"),
                                                                    path.c_str() );

                mode_t  sockmode = S_IRUSR | S_IWUSR;

                if( using_group() )
                    sockmode |= S_IRGRP | S_IWGRP;

                if( (s.st_mode & 07777) != sockmode )
                    throw Error( _("ControlSock: '%s' exists but is not mode %.4o"),
                                                            path.c_str(), sockmode );
                if( s.st_uid != geteuid() )
                    throw Error( _("ControlSock: '%s' exists but is not owned by us"),
                                                                    path.c_str() );

                gid_t   gid = using_group() ? m_gid : getegid();

                if( s.st_gid != gid )
                    throw Error( _("ControlSock: '%s' exists but is not in the expected group"),
                                                                    path.c_str() );

                Log<1>( _("ControlSock( %s ): removing stale socket\n"), path.c_str() );
                unlink( path.c_str() );
            }

        } //}}}

        void listen_on_socket( const std::string &path )
        { //{{{

            acquire_socket_lock( path );

            ListenSocket( AF_UNIX, SOCK_STREAM, 0, &m_addr.any, sizeof(m_addr.un) );

            // The portable way to control access to the socket is via the permission
            // of its parent directory, which we already handle above, but Linux also
            // respects the permission on the socket itself, so if we're granting the
            // members of a group access to it, we need to make sure that it will be
            // at least that permissive too, regardless of what the umask might have
            // done to us.  We could just 0666 it, but it's not much more work to be
            // explicit about what we intend.
            mode_t  sockmode = S_IRUSR | S_IWUSR;

            if( using_group() )
                sockmode |= S_IRGRP | S_IWGRP;

            if( chmod( path.c_str(), sockmode ) )
                throw SystemError( _("ControlSock( %s ): failed to chmod %.4o socket"),
                                                            path.c_str(), sockmode );

            if( using_group() && chown( path.c_str(), uid_t(-1), m_gid ) )
                throw SystemError( _("ControlSock( %s ): failed to chown socket to group %s."),
                                                        path.c_str(), m_group.c_str() );

            start_server_thread();

        } //}}}

        void cleanup_files()
        { //{{{

            // We don't try to remove the .lock file (for the same paranoid
            // reasons we don't currently write to it in acquire_socket_lock)
            // and we don't try to remove any directories we created because
            // it would be quite valid for multiple processes to share those
            // which makes it complicated to know with any certainty how far
            // back up the branch we ought to prune once the last user exits.
            // We don't necessarily want to remove all empty parent dirs just
            // because they are now empty, they might not be ours to remove,
            // even if they match our expected owner and mode.
            //
            // That's not a huge problem because usually they will be created
            // under (/var)/run, which on modern systems is typically reaped
            // at the next boot anyway (and sane users shouldn't be creating
            // dozens of them all over the place in normal use in any case).
            // The main way it could catch people is if they restart using a
            // different permitted group for socket access, but that should
            // be a rare thing, and isn't an entirely terrible sanity check
            // all of its own either.
            //
            // We do remove the socket though, mostly to avoid the dance with
            // checking if it really is stale and was ours at the next start.
            //
            // And we don't close m_fd here, the base destructor will do that.
            if( HaveSocket() )
                unlink( m_addr.un.sun_path );

            if( m_lockfd != -1 )
                close( m_lockfd );

        } //}}}

       #endif  // EM_PLATFORM_POSIX


    public:

        typedef RefPtr< ControlSockUnix >   Handle;


        ControlSockUnix( const std::string &path, const std::string &group = std::string() )
            : ControlSock( path )
            #if EM_PLATFORM_POSIX
            , m_group( group )
            , m_gid( GetGID( m_group ) )
            , m_lockfd( -1 )
            #endif
        { //{{{

           #if EM_PLATFORM_POSIX

            Log<2>( "+ ControlSockUnix( '%s' )\n", path.c_str() );

            if( path.empty() )
                throw Error( _("ControlSockUnix: no path specified") );

            if( path.size() >= sizeof(m_addr.un.sun_path) )
                throw Error( _("ControlSockUnix: socket path '%s' is too long.  "
                               "Maximum length is %zu bytes."),
                               path.c_str(), sizeof(m_addr.un.sun_path) - 1 );

            m_addr.un.sun_family = AF_UNIX;

            path.copy( m_addr.un.sun_path, sizeof(m_addr.un.sun_path) - 1 );
            m_addr.un.sun_path[ path.size() ] = '\0';

            create_socket_dir( path );

            try {
                listen_on_socket( path );
            }
            catch( ... )
            {
                cleanup_files();
                throw;
            }

           #else
            (void)group;
            throw Error("Unix sockets are not supported on this platform");
           #endif

        } //}}}

        ~ControlSockUnix()
        { //{{{

           #if EM_PLATFORM_POSIX
            cleanup_files();
           #endif

        } //}}}

    }; //}}}


    class ControlSockTCP : public ControlSock
    { //{{{
    public:

        typedef RefPtr< ControlSockTCP >    Handle;


        ControlSockTCP( const std::string &addr, bool freebind = false )
            : ControlSock( addr )
        {
            Log<2>( "+ ControlSockTCP( '%s' )\n", addr.c_str() );

            SockAddr    sa( addr );

            sa.GetAddrInfo( SOCK_STREAM, AI_ADDRCONFIG | AI_PASSIVE );

            if( sa.addr.any.sa_family != AF_INET && sa.addr.any.sa_family != AF_INET6 )
                throw Error( _("ControlSockTCP( %s ): not an IPv4 or IPv6 address (family %u)"),
                                                        addr.c_str(), sa.addr.any.sa_family );

            ListenSocket( sa.addr.any.sa_family, sa.addr_type, sa.addr_protocol,
                                            &sa.addr.any, sa.addr_len, freebind );
            start_server_thread();
        }

    }; //}}}


    static inline
    ControlSock::Handle CreateControlSocket( const std::string &addr,
                                             const std::string &group    = std::string(),
                                             bool               freebind = false )
    { //{{{

        if( addr == "none" )
            return NULL;

        if( addr.find("tcp:") == 0 )
            return new ControlSockTCP( addr.substr(4), freebind );

        return new ControlSockUnix( addr, group );

    } //}}}

}   // BitB namespace


#endif  // _BB_CONTROL_SOCKET_H

// vi:sts=4:sw=4:et:foldmethod=marker
