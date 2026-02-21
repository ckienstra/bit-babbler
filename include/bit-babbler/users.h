//  This file is distributed as part of the bit-babbler package.
//  Copyright 2004 - 2021,  Ron <ron@debian.org>

#ifndef _BB_USERS_H
#define _BB_USERS_H

#if EM_PLATFORM_POSIX

#include <bit-babbler/exceptions.h>

#include <grp.h>
#include <unistd.h>


namespace BitB
{
    // There is no particular reason for this to be static inline, aside from
    // the fact that it is only ever be used in just one place, and probably
    // won't be used anywhere else for now, so bundling it off into a separate
    // impl file is a touch on the overkill side.  If we later package all of
    // this up into a convenience library for applications to use that is most
    // probably what we should do with it though.

    static inline gid_t GetGID( const std::string &group )
    { //{{{

        if( group.empty() )
            return gid_t(-1);

        long            bufsize = sysconf(_SC_GETGR_R_SIZE_MAX);
        char           *buf;
        struct group    grent;
        struct group   *have_result;

        if( bufsize <= 0 )
            bufsize = 65536;

    try_again:

        buf = new char[bufsize];

        switch( getgrnam_r( group.c_str(), &grent, buf, size_t(bufsize), &have_result ) )
        {
            case 0:
            case ENOENT:
                break;

            case ERANGE:
                // Draw the line at some arbitrarily insane number
                if( bufsize < 4 * 1024 * 1024 )
                {
                    delete [] buf;
                    bufsize <<= 1;
                    goto try_again;
                }
                BB_FALLTHROUGH; // fall through

            default:
                delete [] buf;
                throw SystemError( "GetGID: failed to get group data for %s",
                                                            group.c_str() );
        }

        if( have_result == NULL )
        {
            delete [] buf;
            throw Error( "GetGID: failed to get group data for %s", group.c_str() );
        }

        gid_t   gid = grent.gr_gid;

        delete [] buf;
        return gid;

    } //}}}

}   // BitB namespace


#endif  // EM_PLATFORM_POSIX

#endif  // _BB_USERS_H

// vi:sts=4:sw=4:et:foldmethod=marker
