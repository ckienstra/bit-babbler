//  This file is distributed as part of the bit-babbler package.
//  Copyright 2014 - 2015,  Ron <ron@debian.org>
//
// You must include bit-babbler/impl/health-monitor.h exactly once in some
// translation unit of any program using the HealthMonitor.

#ifndef _BB_HEALTH_MONITOR_H
#define _BB_HEALTH_MONITOR_H

#include <bit-babbler/qa.h>

#include <list>


namespace BitB
{

    class Monitor
    { //{{{
    private:

        typedef std::list< Monitor* >   List;

        static List                 ms_list;
        static pthread_mutex_t      ms_mutex;

        std::string     m_id;


        // You cannot copy this class
        Monitor( const Monitor& );
        Monitor &operator=( const Monitor& );


        virtual std::string ReportJSON() const = 0;
        virtual std::string RawDataJSON() const = 0;


    public:

        Monitor( const std::string &id )
            : m_id( id )
        {}

        virtual ~Monitor()
        {}


        void Register()
        { //{{{

            pthread_mutex_lock( &ms_mutex );
            ms_list.push_back( this );
            pthread_mutex_unlock( &ms_mutex );

        } //}}}

        void Deregister()
        { //{{{

            pthread_mutex_lock( &ms_mutex );
            ms_list.remove( this );
            pthread_mutex_unlock( &ms_mutex );

        } //}}}


        const std::string &GetID() const { return m_id; }


        static std::string GetIDs()
        { //{{{

            ScopedMutex     lock( &ms_mutex );
            std::string     ids( 1, '[' );
            bool            first = true;

            for( Monitor::List::iterator i = ms_list.begin(),
                                         e = ms_list.end(); i != e; ++i )
            {
                if( first )
                    first = false;
                else
                    ids += ',';

                ids += '"' + (*i)->m_id + '"';
            }

            return ids + ']';

        } //}}}

        static std::string GetStats( const std::string &id = std::string() )
        { //{{{

            ScopedMutex     lock( &ms_mutex );
            std::string     report( 1, '{' );
            bool            first = true;

            for( Monitor::List::iterator i = ms_list.begin(),
                                         e = ms_list.end(); i != e; ++i )
            {
                if( id.empty() || id == (*i)->m_id )
                {
                    if( first )
                        first = false;
                    else
                        report += ',';

                    report += '"' + (*i)->m_id + "\":" + (*i)->ReportJSON();
                }
            }

            return report + '}';

        } //}}}

        static std::string GetRawData( const std::string &id = std::string() )
        { //{{{

            ScopedMutex     lock( &ms_mutex );
            std::string     report( 1, '{' );
            bool            first = true;

            for( Monitor::List::iterator i = ms_list.begin(),
                                         e = ms_list.end(); i != e; ++i )
            {
                if( id.empty() || id == (*i)->m_id )
                {
                    if( first )
                        first = false;
                    else
                        report += ',';

                    report += '"' + (*i)->m_id + "\":" + (*i)->RawDataJSON();
                }
            }

            return report + '}';

        } //}}}

    }; //}}}


    class HealthMonitor : public Monitor
    { //{{{
    private:

        mutable pthread_mutex_t     m_mutex;

        uint8_t                     m_fipsbuf[ QA::FIPS::BUFFER_SIZE ];
        size_t                      m_fipsextra;

        unsigned long long          m_bytes_analysed;
        unsigned long long          m_bytes_passed;

        QA::FIPS                    m_fips;
        QA::Ent8                    m_ent;
        QA::Ent16                   m_ent16;

        bool                        m_fips_ok;
        bool                        m_ent_ok;
        bool                        m_ent16_ok;


        // You must hold m_mutex when calling this
        std::string QAResultsAsJSON() const
        { //{{{

            return stringprintf( "\"QA\":{"
                                          "\"BytesAnalysed\":%llu,"
                                          "\"BytesPassed\":%llu"
                                        "}",
                                 m_bytes_analysed, m_bytes_passed );
        } //}}}


    public:

        HealthMonitor( const std::string &id, bool assume_ent8_ok = true )
            : Monitor( id )
            , m_fipsextra( 0 )
            , m_bytes_analysed( 0 )
            , m_bytes_passed( 0 )
            , m_fips_ok( false )
            , m_ent_ok( assume_ent8_ok )
            , m_ent16_ok( true )
        { //{{{

            // We assume the results are not ok until we have some positive
            // measure to the contrary from at least the FIPS tests.  If we
            // wanted to be a bit more paranoid we could also wait until we
            // get a non-failing result back from the Ent tests too, however
            // there is a tradeoff there in that we won't get back the first
            // result from the Ent8 test until it has tested 500,000 8-bit
            // samples while the Ent16 test needs 100 million 16-bit samples
            // before returning its first result.
            //
            // A source providing 1Mbps will take about 4 seconds to get the
            // first Ent8 result and more than 26 minutes for an Ent16 result.
            // Which is probably a bit on the long side to be waiting before
            // a probably ok entropy source is brought online.  If there is a
            // source feeding 5Mbps or above though it tips the balance toward
            // it making good sense to default the Ent8 test to not being ok
            // until we've seen a passable result from it too.
            //
            // So we let the caller decide whether they want to wait for the
            // Ent8 results or not, we assume FIPS is bad until it actually
            // passes (by the more restrictive recovery margin), and we let
            // the Ent16 test just be a sanity check for long term abnormal
            // behaviour that may be evident in the larger sample space.

            Log<2>( "+ HealthMonitor( %s )\n", GetID().c_str() );

            pthread_mutex_init( &m_mutex, NULL );
            Register();

        } //}}}

        ~HealthMonitor()
        {
            Log<2>( "- HealthMonitor( %s )\n", GetID().c_str() );

            Deregister();
            pthread_mutex_destroy( &m_mutex );
        }


        bool Check( const uint8_t *buf, size_t len )
        { //{{{

            using QA::FIPS;

            ScopedMutex     lock( &m_mutex );
            size_t          b = len;

            m_ent.Analyse( buf, len );
            m_ent16.Analyse( buf, len );

            if( m_ent.HaveResults() )
                m_ent_ok = m_ent.IsOk( m_ent_ok );

            if( m_ent16.HaveResults() )
                m_ent16_ok = m_ent16.IsOk( m_ent16_ok );


            if( m_fipsextra )
            {
                size_t  n = std::min( FIPS::BUFFER_SIZE - m_fipsextra, len );

                memcpy( m_fipsbuf + m_fipsextra, buf, n );

                len         -= n;
                buf         += n;
                m_fipsextra += n;

                if( m_fipsextra == FIPS::BUFFER_SIZE )
                {
                    m_fips.Analyse( m_fipsbuf );

                    m_fips_ok   = m_fips.IsOk( m_fips_ok );
                    m_fipsextra = 0;
                }
            }

            while( len >= FIPS::BUFFER_SIZE )
            {
                m_fips.Analyse( buf );

                m_fips_ok = m_fips.IsOk( m_fips_ok );

                len -= FIPS::BUFFER_SIZE;
                buf += FIPS::BUFFER_SIZE;
            }

            if( len )
            {
                memcpy( m_fipsbuf, buf, len );
                m_fipsextra = len;
            }


            m_bytes_analysed += b;

            if( m_ent_ok && m_ent16_ok && m_fips_ok )
            {
                m_bytes_passed += b;
                return true;
            }

            return false;

        } //}}}


        virtual std::string ReportJSON() const
        { //{{{

            ScopedMutex     lock( &m_mutex );
            std::string     report = '{' + QAResultsAsJSON();

            report += ',' + m_fips.ResultsAsJSON();

            if( m_ent.HaveResults() )
                report += ',' + m_ent.ResultsAsJSON();

            if( m_ent16.HaveResults() )
                report += ',' + m_ent16.ResultsAsJSON();

            return report + '}';

        } //}}}

        virtual std::string RawDataJSON() const
        { //{{{

            ScopedMutex     lock( &m_mutex );
            std::string     s( 1, '{' );

            if( m_ent.HaveResults() )
                s += m_ent.AsJSON();

            if( m_ent16.HaveResults() )
                s += ',' + m_ent16.AsJSON();

            s += '}';

            return s;

        } //}}}

    }; //}}}

}   // BitB namespace


#endif  // _BB_HEALTH_MONITOR_H

// vi:sts=4:sw=4:et:foldmethod=marker
