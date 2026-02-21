//  This file is distributed as part of the bit-babbler package.
//  Copyright 2015 - 2021,  Ron <ron@debian.org>

#ifndef _BB_ALIGNED_RECAST_H
#define _BB_ALIGNED_RECAST_H

#include <bit-babbler/log.h>
#include <stdexcept>


namespace BitB
{

    template< typename T > struct related_type              { typedef void*         void_type; };
    template< typename T > struct related_type< const T* >  { typedef const void*   void_type; };

    template< typename T > struct alignment_of              { enum { value = __alignof__(T) }; };
    template< typename T > struct alignment_of< T* >        { enum { value = __alignof__(T) }; };



    // Return true if pointer p is aligned to some multiple of S.
    template< size_t S >
    BB_CONST
    bool IsAligned( const void *p )
    { //{{{

        if( S & (S - 1) )
        {
            // We shouldn't normally ever be here, the natural alignment of types
            // on most platforms is always a power of 2, and checking that should
            // be faster than the modulus here.  But since this should get compiled
            // out as dead code if we don't need it, there's no harm in also having
            // a fully generic implementation just in case we ever really do.
            if( __builtin_expect(reinterpret_cast<uintptr_t>(p) % S, 0) )
                return false;

        } else {

            if( __builtin_expect(reinterpret_cast<uintptr_t>(p) & (S - 1), 0) )
                return false;
        }

        return true;

    } //}}}

    // Return true if pointer p is aligned to some multiple of the alignment of type T.
    template< typename T >
    BB_CONST
    bool IsAligned( const void *p )
    {
        return IsAligned< alignment_of<T>::value >( p );
    }



    // For some reason GCC 4.9.2 thinks aligned_recast() can be declared const,
    // but that seems wrong because it can throw and calls stringprintf, and
    // empirically, if we declare these with the const attribute then the unit
    // tests fail ...  so squelch the warning.
    EM_TRY_PUSH_DIAGNOSTIC_IGNORE("-Wsuggest-attribute=const")

    // Safe cast back to a type with increased alignment.
    //
    // This will cast pointer p, to type T, after asserting that it is already
    // suitably aligned to some multiple of S.
    //
    // The main use for this is portably squelching -Wcast-align warnings where
    // it is certain that the actual alignment of the pointer being punned will
    // always be sufficient, with a runtime check to assert that really is true.
    template< typename T, size_t S, typename P >
    T aligned_recast( P p )
    { //{{{

        if( ! IsAligned<S>( p ) )
            throw std::invalid_argument(
                    stringprintf( "aligned_recast: %s %p has alignment < %zu in cast to %s",
                                                        EM_TYPEOF(P), p, S, EM_TYPEOF(T) ) );

        return reinterpret_cast<T>( static_cast< typename related_type<P>::void_type >( p ) );

    } //}}}

    // Cast pointer p, to type T, after asserting that it is already suitably
    // aligned to some multiple of the alignment of type T.
    template< typename T, typename P >
    T aligned_recast( P p )
    {
        return aligned_recast< T, alignment_of<T>::value >( p );
    }

    EM_POP_DIAGNOSTIC
}

#endif  // _BB_ALIGNED_RECAST_H

// vi:sts=4:sw=4:et:foldmethod=marker
