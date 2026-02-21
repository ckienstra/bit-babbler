//  This file is distributed as part of the bit-babbler package.
//  Copyright 2012 - 2016,  Ron <ron@debian.org>

#ifndef _BB_SIGNALS_H
#define _BB_SIGNALS_H

#if EM_PLATFORM_POSIX

#include <bit-babbler/exceptions.h>

#include <signal.h>

#ifdef _REENTRANT
#include <pthread.h>
#endif


// This is a kludge for proto-POSIX systems like MacOSX, which still don't
// define SIGRTMIN (FreeBSD added it in version 7).  It means that we can't
// safely use SIGUSR2 for anything else here, but we'll worry about that if
// we do ever explicitly need or want it for some other purpose later.
#if !HAVE_DECL_SIGRTMIN
#define SIGRTMIN    SIGUSR2
#endif


namespace BitB
{
    // There is no particular reason for these to be static inline, aside from
    // the fact that they'll mostly only ever be used in just one place in any
    // given application, so bundling them off into a separate impl file is a
    // touch on the overkill side right now.  If we later package all this up
    // into a convenience library for applications to use that's probably what
    // we should do with them though.

    static inline void BlockSignals( int sig1 = 0, int sig2 = 0, int sig3 = 0,
                                     int sig4 = 0, int sig5 = 0, int sig6 = 0 )
    { //{{{

        sigset_t    sigs;

        if( sig1 == 0 )
        {
            if( sigfillset( &sigs ) == -1 )
                throw SystemError( "BlockSignals: failed to fill signal set" );

            // SIGKILL, SIGSTOP, SIGCONT and SIGABRT are silently ignored and not blocked.
            if( sigdelset( &sigs, SIGBUS ) == -1
             || sigdelset( &sigs, SIGFPE ) == -1
             || sigdelset( &sigs, SIGILL ) == -1
             || sigdelset( &sigs, SIGSEGV ) == -1
             || sigdelset( &sigs, SIGTRAP ) == -1 )
                throw SystemError( "BlockSignals: failed to configure signal set" );

        } else {

            int s[] = { sig1, sig2, sig3, sig4, sig5, sig6, 0 };

            if( sigemptyset( &sigs ) == -1 )
                throw SystemError( "BlockSignals: failed to clear signal set" );

            for( unsigned i = 0; s[i]; ++i )
                if( sigaddset( &sigs, s[i] ) == -1 )
                    throw SystemError( "BlockSignals: failed to add signal %d (%s)", s[i],
                                                                           strsignal(s[i]) );
        }

       #ifdef _REENTRANT

        int ret = pthread_sigmask( SIG_BLOCK, &sigs, NULL );
        if( ret )
            throw SystemError( ret, "BlockSignals: failed to mask signals" );

       #else

        if( sigprocmask( SIG_BLOCK, &sigs, NULL ) == -1 )
            throw SystemError( "BlockSignals: failed to mask signals" );

       #endif

    } //}}}

    static inline int FindUnblockedSignal( int sig1 = 0, int sig2 = 0, int sig3 = 0,
                                           int sig4 = 0, int sig5 = 0, int sig6 = 0,
                                           int sig7 = 0, int sig8 = 0, int sig9 = 0 )
    { //{{{

        sigset_t    sigs;

        if( sigemptyset( &sigs ) == -1 )
            throw SystemError( "FindUnblockedSignal: failed to clear signal set" );

       #ifdef _REENTRANT

        int ret = pthread_sigmask( 0, NULL, &sigs );
        if( ret )
            throw SystemError( ret, "FindUnblockedSignal: failed to read signal mask" );

       #else

        if( sigprocmask( 0, NULL, &sigs ) == -1 )
            throw SystemError( "FindUnblockedSignal: failed to read signal mask" );

       #endif

        int sarg[] = { sig1, sig2, sig3, sig4, sig5, sig6, sig7, sig8, sig9, 0 };
        int sall[] = { SIGHUP, SIGINT, SIGQUIT, SIGUSR1, SIGUSR2, SIGPIPE,
                       SIGALRM, SIGTERM, SIGCHLD, SIGTSTP, SIGTTIN, SIGTTOU,
                       SIGURG, SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF, SIGWINCH,
                       SIGIO, SIGSYS,
                #ifdef SIGSTKFLT
                       SIGSTKFLT,
                #endif
                #ifdef SIGPWR
                       SIGPWR,
                #endif
                #ifdef SIGEMT
                       SIGEMT,
                #endif
                #ifdef SIGINFO
                       SIGINFO,
                #endif
                       0 };

        int *s = sig1 ? sarg : sall;

        for( unsigned i = 0; s[i]; ++i )
        {
            int result = sigismember( &sigs, s[i] );

            if( result == -1 )
                throw SystemError( "FindUnblockedSignal: failed to test mask for signal %d (%s)",
                                                                        s[i], strsignal(s[i]) );
            if( result == 0 )
                return s[i];
        }

        return 0;

    } //}}}

    static inline int SigWait( int sig1 = 0, int sig2 = 0, int sig3 = 0,
                               int sig4 = 0, int sig5 = 0, int sig6 = 0,
                               int sig7 = 0, int sig8 = 0, int sig9 = 0 )
    { //{{{

        sigset_t    signals;

        if( sig1 == 0 )
        {
            int u = FindUnblockedSignal();
            if( u )
                throw Error( "SigWait: signal %d (%s) is not blocked", u, strsignal(u) );

            if( sigfillset( &signals ) == -1 )
                throw SystemError( "SigWait: failed to fill signal set" );

        } else {

            int s[] = { sig1, sig2, sig3, sig4, sig5, sig6, sig7, sig8, sig9, 0 };
            int u   = FindUnblockedSignal( sig1, sig2, sig3, sig4, sig5, sig6,
                                                             sig7, sig8, sig9 );
            if( u )
                throw Error( "SigWait: signal %d (%s) is not blocked", u, strsignal(u) );

            if( sigemptyset( &signals ) == -1 )
                throw SystemError( "SigWait: failed to clear signal set" );

            for( unsigned i = 0; s[i]; ++i )
                if( sigaddset( &signals, s[i] ) == -1 )
                    throw SystemError( "SigWait: failed to add signal %d (%s)", s[i],
                                                                      strsignal(s[i]) );
        }

        int sig;
        int ret = sigwait( &signals, &sig );

        if( ret )
            throw SystemError( ret, "SigWait: error" );

        return sig;

    } //}}}

}

#endif  // EM_PLATFORM_POSIX

#endif  // _BB_SIGNALS_H

// vi:sts=4:sw=4:et:foldmethod=marker
