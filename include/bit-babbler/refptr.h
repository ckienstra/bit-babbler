//  This file is distributed as part of the bit-babbler package.
//  Copyright 2003 - 2016,  Ron <ron@debian.org>

#ifndef _BB_REFPTR_H
#define _BB_REFPTR_H

#include <bit-babbler/log.h>


namespace BitB
{

    template< typename T >
    class RefPtr
    { //{{{
    private:

        T   *m_refptr;


    public:

        typedef T                   Object;

        typedef RefPtr< const T >   Const;


        RefPtr( T *ptr = NULL )
            : m_refptr( ptr )
        {
            if( m_refptr )
                m_refptr->Ref();
        }

        RefPtr( T &obj )
            : m_refptr( &obj )
        {
            if( m_refptr->GetRefCount() == 0 )
                m_refptr->Ref();
            m_refptr->Ref();
        }

        RefPtr( const RefPtr &ptr )
            : m_refptr( ptr.Raw() )
        {
            if( m_refptr )
                m_refptr->Ref();
        }

        template< typename U >
        RefPtr( const RefPtr< U > &ptr )
            : m_refptr( ptr.Raw() )
        {
            if( m_refptr )
                m_refptr->Ref();
        }

        ~RefPtr()
        {
            if( m_refptr )
                m_refptr->Unref();
        }


        RefPtr &operator=( T *ptr )
        {
            if( ptr )
                ptr->Ref();
            if( m_refptr )
                m_refptr->Unref();

            m_refptr = ptr;
            return *this;
        }

        RefPtr &operator=( const RefPtr &ptr )
        {
            if( ptr.m_refptr )
                ptr.m_refptr->Ref();
            if( m_refptr )
                m_refptr->Unref();

            m_refptr = ptr.m_refptr;
            return *this;
        }

        template< typename U >
        RefPtr &operator=( const RefPtr< U > &ptr )
        {
            U   *p = ptr.Raw();

            if( p )
                p->Ref();
            if( m_refptr )
                m_refptr->Unref();

            m_refptr = p;
            return *this;
        }


        T &operator*() const
        {
            if( __builtin_expect(m_refptr == NULL, 0) )
                throw Error( "Attempt to dereference NULL %s", EM_TYPEOF( m_refptr ) );

            return *m_refptr;
        }

        T *operator->() const
        {
            if( __builtin_expect(m_refptr == NULL, 0) )
                throw Error( "Attempt to dereference NULL %s", EM_TYPEOF( m_refptr ) );

            return m_refptr;
        }

        T* Raw() const          { return m_refptr; }


        template< typename U >
        RefPtr< U > Downcast() const
        {
            U   *u = dynamic_cast< U* >( m_refptr );
            return RefPtr< U >( u );
        }


        template< typename U >
        bool operator==( const RefPtr< U > &ptr ) const
        {
            return m_refptr == ptr.Raw();
        }

        template< typename U >
        bool operator!=( const RefPtr< U > &ptr ) const
        {
            return m_refptr != ptr.Raw();
        }

        bool operator==( T *ptr ) const
        {
            return m_refptr == ptr;
        }

        bool operator!=( T *ptr ) const
        {
            return m_refptr != ptr;
        }

        bool operator!() const
        {
            return m_refptr == NULL;
        }

    }; //}}}


    template< typename T >
    class RefCountedBy
    { //{{{
    private:

        mutable T                   m_count;

       #ifdef _REENTRANT
        mutable pthread_mutex_t     m_mutex;

        void mutex_lock() const     { pthread_mutex_lock(&m_mutex); }
        void mutex_unlock() const   { pthread_mutex_unlock(&m_mutex); }
       #else
        void mutex_lock() const     {}
        void mutex_unlock() const   {}
       #endif


        RefCountedBy( const RefCountedBy& );
        RefCountedBy &operator=( const RefCountedBy& );


    public:

        typedef RefPtr< RefCountedBy >  Handle;


        RefCountedBy()
            : m_count( 0 )
        {
           #ifdef _REENTRANT
            pthread_mutex_init(&m_mutex, NULL);
           #endif
        }

        virtual ~RefCountedBy()
        {
           #ifdef _REENTRANT
            pthread_mutex_destroy(&m_mutex);
           #endif
        }


        virtual void Ref() const
        {
            mutex_lock();

            ++m_count;

            if( __builtin_expect(m_count == 0, 0) )
            {
                mutex_unlock();
                throw Error( "Ref with zero ref count.  RefCountedBy<%s> overflow",
                                                                    EM_TYPEOF(T) );
            }
            mutex_unlock();
        }

        virtual void Unref() const
        {
            mutex_lock();

            if( __builtin_expect(m_count == 0, 0) )
            {
                mutex_unlock();
                throw Error( "Unref with zero ref count in RefCountedBy<%s>",
                                                                EM_TYPEOF(T) );
            }

            if( --m_count == 0 )
            {
                mutex_unlock();
                delete this;
                return;
            }
            mutex_unlock();
        }

        T GetRefCount() const { return m_count; }


        template< typename U >
        U &As()
        {
            return dynamic_cast< U& >( *this );
        }

    }; //}}}

    typedef RefCountedBy<size_t>    RefCounted;


   #ifdef _REENTRANT

    class ScopedMutex
    { //{{{
    private:

        pthread_mutex_t    *m_mutex;


        ScopedMutex( const ScopedMutex& );
        ScopedMutex &operator=( const ScopedMutex& );


    public:

        explicit ScopedMutex( pthread_mutex_t *mutex = NULL )
            : m_mutex( mutex )
        {
            if( __builtin_expect(m_mutex != NULL,1) )
            {
                int ret = pthread_mutex_lock( m_mutex );

                if( __builtin_expect(ret,0) )
                    throw SystemError( ret, "ScopedMutex: failed to acquire mutex" );
            }
        }

        ~ScopedMutex()
        {
            if( m_mutex )
            {
                int ret = pthread_mutex_unlock( m_mutex );

                if( __builtin_expect(ret,0) )
                    LogErr<0>( ret, "~ScopedMutex: failed to release mutex" );
            }
        }


        void Lock( pthread_mutex_t *mutex )
        {
            if( __builtin_expect(m_mutex != NULL,0) )
                throw Error( "ScopedMutex::Lock: Another mutex is already held" );

            int ret = pthread_mutex_lock( mutex );

            if( __builtin_expect(ret,0) )
                throw SystemError( ret, "ScopedMutex::Lock: failed to acquire mutex" );

            m_mutex = mutex;
        }

        void Unlock()
        {
            if( __builtin_expect(m_mutex == NULL,0) )
                throw Error( "ScopedMutex::Unlock: No mutex held" );

            pthread_mutex_t    *m = m_mutex;

            m_mutex = NULL;

            int ret = pthread_mutex_unlock( m );

            if( __builtin_expect(ret,0) )
                throw SystemError( ret, "ScopedMutex::Unlock: failed to release mutex" );
        }

    }; //}}}


    class ScopedCancelState
    { //{{{
    private:

        int     m_oldstate;


        ScopedCancelState( const ScopedCancelState& );
        ScopedCancelState &operator=( const ScopedCancelState& );


    public:

        explicit ScopedCancelState( int state = PTHREAD_CANCEL_DISABLE )
        {
            int ret = pthread_setcancelstate( state, &m_oldstate );

            if( __builtin_expect(ret,0) )
                throw SystemError( ret, "ScopedCancelState: failed" );
        }

        ~ScopedCancelState()
        {
            int ret = pthread_setcancelstate( m_oldstate, NULL );

            if( __builtin_expect(ret,0) )
                LogErr<0>( ret, "~ScopedCancelState: failed" );
        }


        void Restore()
        {
            int ret = pthread_setcancelstate( m_oldstate, NULL );

            if( __builtin_expect(ret,0) )
                throw SystemError( ret, "ScopedCancelState::Restore failed" );
        }

    }; //}}}

   #endif   // _REENTRANT
}

#endif  // _BB_REFPTR_H

// vi:sts=4:sw=4:et:foldmethod=marker
