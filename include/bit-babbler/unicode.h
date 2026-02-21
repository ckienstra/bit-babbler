////////////////////////////////////////////////////////////////////
//
//! @file unicode.h
//! @ingroup UTFhelpers
//! @brief Functions for handling Unicode.
//
//  Copyright 2013 - 2016,  Ron <ron@debian.org>
//  This file is distributed as part of the bit-babbler package.
//
////////////////////////////////////////////////////////////////////

#ifndef _BB_UNICODE_H
#define _BB_UNICODE_H

#include <string>
#include <stdint.h>

// We could jump through some extra hoops here to support insane systems
// but since they should really just die in a fire, and since the whole
// point of the code here is to avoid having to use heavy machinery like
// iconv for the simple conversions, just bark at them for now ...
//
// This is only really a problem for the wstring functions at present,
// if we ever come to depend on C++11, then we could use u32string and
// the char32_t types which are explicitly UTF-32, and/or the u16string
// and char16_t types which are explicitly UTF-16.
//#define USE_APPEND_WSTRING_AS_UTF8
#ifdef USE_APPEND_WSTRING_AS_UTF8
 // Since we don't actually need the wstring version of AppendAsUTF8 in
 // the BitBabbler code anywhere, just disable it for windows users.
 #ifndef __STDC_ISO_10646__
 #error "The wchar_t type on this system is not UTF-32.  You're hosed."
 #endif
#endif

namespace BitB
{

//! @defgroup UTFhelpers Unicode support
//! @brief Miscellaneous functions for handling Unicode.
//! @ingroup Strings
//!@{

  //! @name UTF-8 conversion
  //@{ //{{{

    //! Append a UTF-32 codepoint as UTF-8 octets to the string @a s
    //{{{
    //! If the codepoint is outside the valid UTF-32 range (greater than
    //! 0x10FFFF) then it will simply be ignored.
    //}}}
    static inline void AppendAsUTF8( std::string &s, uint32_t codepoint )
    { //{{{

        if( codepoint < 0x80 )
        {
            s.push_back( char(codepoint) );
        }
        else if( codepoint < 0x0800 )
        {
            s.push_back( char(0xc0 | (codepoint >> 6)) );
            s.push_back( char(0x80 | (codepoint & 0x3f)) );
        }
        else if( codepoint < 0x10000 )
        {
            s.push_back( char(0xe0 | (codepoint >> 12)) );
            s.push_back( char(0x80 | ((codepoint >> 6) & 0x3f)) );
            s.push_back( char(0x80 | (codepoint & 0x3f)) );
        }
        else if( codepoint < 0x110000 )
        {
            s.push_back( char(0xf0 | (codepoint >> 18)) );
            s.push_back( char(0x80 | ((codepoint >> 12) & 0x3f)) );
            s.push_back( char(0x80 | ((codepoint >> 6) & 0x3f)) );
            s.push_back( char(0x80 | (codepoint & 0x3f)) );
        }

    } //}}}

   #ifdef USE_APPEND_WSTRING_AS_UTF8

    //! Append a UTF-32 string as UTF-8 octets to the string @a dest
    static inline const std::string &AppendAsUTF8( std::string &dest, const std::wstring &src )
    { //{{{

        for( std::wstring::const_iterator i = src.begin(), e = src.end(); i != e; ++i )
            AppendAsUTF8( dest, uint32_t(*i) );

        return dest;

    } //}}}

   #endif

  //@} //}}}


  //! @name UTF-16 surrogate pair support
  //@{ //{{{

    //! Return @c true if the 16 bit @a value is a valid UTF-16 leading surrogate
    static inline bool IsUTF16LeadingSurrogate( uint16_t value )
    {
        return (value & 0xfc00) == 0xd800;
    }

    //! Return @c true if the 16 bit @a value is a valid UTF-16 trailing surrogate
    static inline bool IsUTF16TrailingSurrogate( uint16_t value )
    {
        return (value & 0xfc00) == 0xdc00;
    }

    //! Convert a UTF-16 surrogate pair to a UTF-32 codepoint
    //{{{
    //! @param lead     The leading surrogate
    //! @param trail    The trailing surrogate
    //!
    //! @return The UTF-32 codepoint
    //!
    //! @note This function does nothing to validate the surrogate pair.
    //!       It assumes the caller has already done that.
    //
    //  http://www.unicode.org/faq/utf_bom.html#utf16-4
    //}}}
    static inline uint32_t UTF16SurrogateToUTF32( uint16_t lead, uint16_t trail )
    {
        const uint32_t surrogate_offset = uint32_t(0x10000 - (0xD800u << 10) - 0xDC00);
        return (uint32_t(lead) << 10) + trail + surrogate_offset;
    }

    //! Convert a UTF-32 codepoint to a UTF-16 surrogate pair
    //{{{
    //! @param codepoint    The UTF-32 codepoint to convert
    //! @param lead         The 16 bit leading surrogate value
    //! @param trail        The 16 bit trailing surrogate value
    //!
    //! @note This function does nothing to validate if the @a codepoint needs
    //!       to be encoded as a surrogate pair.  It assumes the caller has
    //!       already determined the codepoint is greater than 0xFFFF.
    //}}}
    static inline void UTF32toUTF16Surrogate( uint32_t codepoint, uint16_t &lead,
                                                                  uint16_t &trail )
    {
        const uint16_t lead_offset = 0xD800 - (0x10000u >> 10);

        lead = uint16_t(lead_offset + (codepoint >> 10));
        trail = 0xDC00 + (codepoint & 0x3FF);
    }

  //@} //}}}

//!@}

}   // BitB namespace


#endif  // _BB_UNICODE_H

// vi:sts=4:sw=4:et:foldmethod=marker
