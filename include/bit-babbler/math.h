//  This file is distributed as part of the bit-babbler package.
//  Copyright 2013 - 2021,  Ron <ron@debian.org>

#ifndef _BB_MATH_H
#define _BB_MATH_H

#include <stdint.h>


namespace BitB
{

    // Implicit modulo 1<<32 is part of the algorithm, so tell UBSan to relax.
    BB_NO_SANITIZE_UNSIGNED_INTEGER_OVERFLOW
    static inline uint32_t popcount( uint32_t v )
    {
        // This can be notoriously crappy, but we don't care a lot here
        // this isn't the hottest operation we could usefully optimise.
        //return __builtin_popcount( v );

        v = v - ((v >> 1) & 0x55555555);
        v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
        return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
    }

    static inline unsigned fls( unsigned v )
    {
        return v ? sizeof(unsigned) * 8 - unsigned(__builtin_clz(v)) : 0;
    }

    static inline unsigned fls( unsigned long v )
    {
        return v ? sizeof(unsigned long) * 8 - unsigned(__builtin_clzl(v)) : 0;
    }

    static inline unsigned fls( unsigned long long v )
    {
        return v ? sizeof(unsigned long long) * 8 - unsigned(__builtin_clzll(v)) : 0;
    }

    template< typename T >
    BB_CONST T powof2_down( T v )
    {
        return T(1) << (fls(v) - 1);
    }

    template< typename T >
    BB_CONST T powof2_up( T v )
    {
        return T(1) << fls(v - 1);
    }

}

#endif  // _BB_MATH_H

// vi:sts=4:sw=4:et:foldmethod=marker
