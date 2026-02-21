//  This file is distributed as part of the bit-babbler package.
//  Copyright 2017,  Ron <ron@debian.org>

#ifndef _BB_UNORDERED_MAP_H
#define _BB_UNORDERED_MAP_H

// Hide the implementation detail of whether we have std::unordered_map
// or std::tr1::unordered_map available on the current platform.
// Code should #include <bit-babbler/unordered_map.h> and use the alias
// namespace was_tr1::unordered_map wherever needed.
#if HAVE_UNORDERED_MAP

    #include <unordered_map>
    namespace was_tr1 = std;

#elif HAVE_TR1_UNORDERED_MAP

    #include <tr1/unordered_map>
    namespace was_tr1 = std::tr1;

#else

    #error "No unordered map type supported on this platform"

#endif

#endif  // _BB_UNORDERED_MAP_H

// vi:sts=4:sw=4:et:foldmethod=marker
