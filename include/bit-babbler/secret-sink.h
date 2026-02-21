//  This file is distributed as part of the bit-babbler package.
//  Copyright 2014 - 2018,  Ron <ron@debian.org>

#ifndef _BB_SECRET_SINK_H
#define _BB_SECRET_SINK_H

#include <bit-babbler/health-monitor.h>

#include <fcntl.h>
#include <unistd.h>

namespace BitB
{

    class SecretSink : public RefCounted
    { //{{{
    public:

        typedef RefPtr< SecretSink >    Handle;
        typedef std::list< Handle >     List;


        struct Options
        { //{{{

            typedef std::list< Options >    List;

            std::string     devpath;
            size_t          block_delay;
            size_t          block_size;
            size_t          bytes;

            Options()
                : block_delay( 0 )
                , block_size( 65536 )
                , bytes( 0 )
            {}

            // We don't need these anymore with the new seedd config parser.
            // But hang on to them a bit longer in case something else does.
           #if 0

            Options( const std::string &path, size_t bd = 0, size_t bs = 65536, size_t n = 0 )
                : devpath( path )
                , block_delay( bd )
                , block_size( bs )
                , bytes( n )
            {}


            static Options ParseOptArg( const std::string &arg )
            { //{{{

                // Creates an Options struct from a string of the form:
                // path:delay:block_size:total_bytes
                // where everything except the path portion is optional.

                size_t  n = arg.find( ':' );

                if( n == std::string::npos )
                    return Options( arg );

                std::string     path = arg.substr( 0, n );

                ++n;

                size_t  n2 = arg.find( ':', n );

                if( n2 == std::string::npos )
                    return Options( path, StrToScaledUL(arg.substr(n)) );

                size_t  delay = StrToScaledUL( arg.substr(n, n2 - n) );

                n = n2 + 1;
                n2 = arg.find( ':', n );

                if( n2 == std::string::npos )
                    return Options( path, delay, StrToScaledUL(arg.substr(n), 1024) );

                size_t  block = StrToScaledUL( arg.substr(n, n2 - n), 1024 );

                n = n2 + 1;

                return Options( path, delay, block, StrToScaledUL(arg.substr(n), 1024) );

            } //}}}

           #endif

        }; //}}}


    private:

        Options             m_options;
        HealthMonitor       m_qa;
        int                 m_fd;
        pthread_t           m_thread;


        void do_read_thread()
        { //{{{

            SetThreadName( stringprintf("QA %s", m_options.devpath.c_str()).c_str() );

            Log<3>( "SecretSink( %s ): begin read_thread\n", m_options.devpath.c_str() );

            uint8_t     buf[ m_options.block_size ];
            size_t      bytes = 0;
            size_t      n     = 0;

            for(;;)
            {
                while( n < m_options.block_size )
                {
                    ssize_t r = read( m_fd, buf + n, m_options.block_size - n );

                    if( r < 0 )
                        throw SystemError( _("SecretSink( %s )::read( %zu ) failed"),
                                                        m_options.devpath.c_str(),
                                                        m_options.block_size - n );
                    if( r == 0 )
                        throw Error( _("SecretSink( %s )::read EOF"), m_options.devpath.c_str() );

                    n += size_t(r);
                }

                m_qa.Check( buf, n );

                bytes += n;
                n = 0;

                if( m_options.bytes && bytes >= m_options.bytes )
                {
                    Log<3>( "SecretSink( %s ): read_thread completed, read %zu bytes\n",
                                                    m_options.devpath.c_str(), bytes );
                    return;
                }

                if( m_options.block_delay )
                    usleep( useconds_t(m_options.block_delay * 1000) );
            }

        } //}}}

        static void *read_thread( void *p )
        { //{{{

            SecretSink  *s = static_cast<SecretSink*>( p );

            try {
                s->do_read_thread();
            }
            catch( const abi::__forced_unwind& )
            {
                Log<3>( "SecretSink( %s ): read_thread cancelled\n",
                                        s->m_options.devpath.c_str() );
                throw;
            }
            BB_CATCH_STD( 0, _("uncaught SecretSink::read_thread exception") )

            return NULL;

        } //}}}


    public:

        SecretSink( const Options &options )
            : m_options( options )
            , m_qa( m_options.devpath )
        { //{{{

            Log<2>( "+ SecretSink( '%s' )\n", m_options.devpath.c_str() );

            m_fd = open( m_options.devpath.c_str(), O_RDONLY );

            if( m_fd < 0 )
                throw SystemError( _("SecretSink: failed to open '%s'"),
                                            m_options.devpath.c_str() );

            int ret = pthread_create( &m_thread, GetDefaultThreadAttr(), read_thread, this );

            if( ret )
            {
                close( m_fd );
                throw SystemError( ret, _("SecretSink( %s ) failed to create thread"),
                                                            m_options.devpath.c_str() );
            }

        } //}}}

        ~SecretSink()
        { //{{{

            Log<2>( "- SecretSink( '%s' )\n", m_options.devpath.c_str() );

            Log<3>( "SecretSink( %s ): terminating read_thread\n",
                                        m_options.devpath.c_str() );
            pthread_cancel( m_thread );

            Log<3>( "SecretSink( %s ): waiting for read_thread termination\n",
                                                    m_options.devpath.c_str() );
            pthread_join( m_thread, NULL );

            close( m_fd );

        } //}}}

    }; //}}}

}   // BitB namespace


#endif  // _BB_SECRET_SINK_H

// vi:sts=4:sw=4:et:foldmethod=marker
