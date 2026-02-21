////////////////////////////////////////////////////////////////////
//
//! @file json.h
//! @ingroup JsonParsing
//! @brief Parser for JSON formatted data.
//
//  Copyright 2013 - 2021,  Ron <ron@debian.org>
//  This file is distributed as part of the bit-babbler package.
//
////////////////////////////////////////////////////////////////////

#ifndef _BB_JSON_H
#define _BB_JSON_H

#include <bit-babbler/refptr.h>
#include <bit-babbler/unicode.h>

#include <vector>
#include <list>
#include <stdexcept>
#include <climits>
#include <locale.h>

#if HAVE_XLOCALE_H
 #include <xlocale.h>
#endif

// We don't hash the Object member map by default right now.
// Most of the structures we are expecting to handle at this stage will all be
// relatively small, so the speed benefit is likely to be minimal (or may even
// be non-existant or negative), and the deterministic ordering of a sorted map
// is more user friendly for data that users might see.
// If we ever need to enable this, we should benchmark it, and then possibly
// consider templating the Json class to allow both.  For now, keep it simple.
//#define BB_HASH_JSON_OBJECT_MEMBERS

#ifdef BB_HASH_JSON_OBJECT_MEMBERS
 #include <bit-babbler/unordered_map.h>
#else
 #include <map>
#endif


namespace BitB
{

//! @defgroup JsonParsing JSON data parsing
//! @brief Support for JSON formatted data.
//! @ingroup DataStorage
//!@{

    //! Parser and container class for JSON encoded data
    //{{{
    //! This class may be used to decompose and/or generate UTF-8 encoded JSON
    //! data strings in the format described by
    //! <a href='http://tools.ietf.org/html/rfc7159'>RFC&nbsp;7159</a>.
    //!
    //! If the macro @c STRICT_RFC4627_COMPATIBILITY is defined before including
    //! the @c json.h header, then the older semantics of requiring that the root
    //! of the JSON text be either an object or an array will be enforced, as
    //! described in the obsoleted
    //! <a href='http://tools.ietf.org/html/rfc4627'>RFC&nbsp;4627</a> specification.
    //! Applications built using the RFC&nbsp;7159 mode can still check JSON text for
    //! RFC&nbsp;4627 compliance at runtime by confirming the @c Json::RootType() is
    //! a @c Json::ObjectType or @c Json::ArrayType.
    //!
    //! In the present implementation only Objects with unique member names are
    //! supported.  This is an <a href='http://tools.ietf.org/html/rfc2119'>
    //! RFC&nbsp;2119</a> @b SHOULD requirement, and currently there are no users
    //! of this with extenuating circumstances that warrant the extra complexity
    //! to end use which would come with being an exception to that.  A lookup
    //! by key name will always return either zero or one value, never a list
    //! that may in turn need to be iterated over to obtain the desired value.
    //}}}
    class Json : public RefCounted
    { //{{{
    public:

      //! @name Data types
      //@{ //{{{

        //! JSON data type identifiers
        enum DataType
        {
            NullType,       //!< The JSON @c null primitive type
            BoolType,       //!< The JSON boolean primitive type
            NumberType,     //!< The JSON number primitive type
            StringType,     //!< The JSON string primitive type
            ArrayType,      //!< The JSON array structured type
            ObjectType      //!< The JSON object structured type
        };

        //! Container type for a list of Object member names
        typedef std::list< std::string >    MemberList;

      //@} //}}}

      //! @name Data type strings
      //@{ //{{{

        //! Return a descriptive string for a JSON @c DataType
        static const char *DataTypeStr( DataType t )
        {
            switch( t )
            {
                case NullType:      return "Null";
                case BoolType:      return "Bool";
                case NumberType:    return "Number";
                case StringType:    return "String";
                case ArrayType:     return "Array";
                case ObjectType:    return "Object";
            }
            return "Unknown JSON type";
        }

      //@} //}}}


      //! @name String escaping
      //@{ //{{{

        //! Return a JSON escaped copy of a string
        static std::string Escape( const std::string &str )
        { //{{{

            std::string     s;

            for( std::string::const_iterator i = str.begin(), e = str.end(); i != e; ++i )
            {
                switch( *i )
                {
                    case '"':   s += "\\\""; break;
                    case '\\':  s += "\\\\"; break;
                //  case '/':   s += "\\/";  break;     // escaping this one is optional
                    case '\b':  s += "\\b";  break;
                    case '\f':  s += "\\f";  break;
                    case '\n':  s += "\\n";  break;
                    case '\r':  s += "\\r";  break;
                    case '\t':  s += "\\t";  break;

                    default:
                        if( uint8_t(*i) < 0x20 )
                            s += stringprintf("\\u00%02x", *i);
                        else
                            s.push_back( *i );

                        break;
                }
            }

            return s;

        } //}}}

        static uint16_t HexStrTo16( const char *str )
        { //{{{

            uint16_t    v;

            switch( sscanf( str, "%hx", &v ) )
            {
                case 1:
                    return v;

                case 0:
                    throw Error( "HexStrTo16( '%s' ): invalid conversion", str );

                case EOF:
                    throw Error( "HexStrTo16( '%s' ): invalid input", str );

                default:
                    throw SystemError( "HexStrTo16( '%s' ): sscanf failure", str );
            }

        } //}}}

        //! Return a copy of a string with any JSON escaping undone
        static std::string Unescape( const std::string &str )
        { //{{{

            std::string     s;
            size_t          p = 0;
            uint32_t        lead_surrogate = 0;
            size_t          trail_surrogate_p = 0;

            for(;;)
            {
                size_t  n = str.find('\\', p);

                if( n == std::string::npos )
                {
                    s.append( str.substr(p) );
                    break;
                }

                s.append( str.substr(p, n - p) );

                switch( str[++n] )
                {
                    case '"':
                    case '/':
                        p = n;
                        break;

                    case '\\': s += '\\'; p = n + 1; break;
                    case 'b':  s += '\b'; p = n + 1; break;
                    case 'f':  s += '\f'; p = n + 1; break;
                    case 'n':  s += '\n'; p = n + 1; break;
                    case 'r':  s += '\r'; p = n + 1; break;
                    case 't':  s += '\t'; p = n + 1; break;

                    case 'u':
                    {
                        // This could throw, but if it does that's ok, since it
                        // would mean the string we are unescaping is invalid.
                        std::string     h = str.substr( n + 1, 4 );
                        uint16_t        v = HexStrTo16( h.c_str() );

                        if( IsUTF16LeadingSurrogate(v) )
                        {
                            lead_surrogate      = uint32_t(v) << 10;
                            trail_surrogate_p   = n + 6;
                        }
                        else if( IsUTF16TrailingSurrogate(v) )
                        {
                            const uint32_t surrogate_offset = (0xD800u << 10) + 0xDC00 - 0x10000;

                            if( n == trail_surrogate_p )
                                AppendAsUTF8( s, v + lead_surrogate - surrogate_offset );
                        }
                        else
                            AppendAsUTF8( s, v );

                        p = n + 5;
                        break;
                    }
                }
            }

            return s;

        } //}}}

      //@} //}}}



        //! Container for an individual data element in the JSON structure
        class Data : public RefCounted
        { //{{{
        public:

            //! %Handle type for @c Json data structures.
            //{{{
            //! We derive this type from @c RefPtr rather than using a simple
            //! typedef so that we can provide additional operators to directly
            //! access @c Object members and @c Array elements without needing
            //! clumsy syntax to first dereference the @c %Handle.
            //}}}
            template< typename T >
            class HandleType : public RefPtr< T >
            { //{{{

            public:

              //! @name Constructors
              //@{ //{{{

                //! Contruct a handle to a raw @c Data @a ptr.
                //{{{
                //! This constructor takes ownership of @a ptr and will destroy
                //! it when the last handle to it is destroyed.
                //}}}
                HandleType( T *ptr = NULL )
                    : RefPtr<T>( ptr )
                {}

                //! Construct a handle from a reference to an object that we should not destroy.
                //{{{
                //! This constructor is complementary to the one which takes a pointer
                //! to an object in that it will create a handle to the object, but it
                //! will @e not take ownership of that object if it is not already
                //! owned by another RefPtr to it.
                //! It may be used to create a handle to a @c RefCounted object that
                //! was created on the stack or in some other context which already
                //! manages its lifetime.
                //!
                //! If the reference count of @a obj is zero when it is passed to
                //! this constructor, it will be incremented before normal reference
                //! counting begins and the object's reference count will never drop
                //! to zero without manual intervention.  It is the responsibility of
                //! the implementer to ensure that the object is correctly destroyed
                //! @e and that it outlives this @c RefPtr handle and any of its copies.
                //!
                //! If the reference count of @a obj is non zero then this constructor
                //! will effectively behave the same as if <tt>RefPtr( T* )</tt> was
                //! called.
                //!
                //! It is possible to safety check @a obj prior to destruction by
                //! testing its refcount which should be 0 if a handle to it was
                //! never created, 1 if handles were created that have now all been
                //! destroyed, and > 1 if handles still exist that have not been
                //! destroyed yet.
                //}}}
                HandleType( T &obj )
                    : RefPtr<T>( obj )
                {}

                //! Copy constructor.
                //{{{
                //! @note This may look irrelevant, but the templated version
                //!       will NOT stop the compiler from creating a default
                //!       copy constructor.
                //}}}
                HandleType( const Handle &ptr )
                    : RefPtr<T>( ptr )
                {}

              //@} //}}}


              //! @name Primitive data type accessors
              //@{ //{{{

                //! Implicit conversion of JSON numeric primitive @c Data
                //{{{
                //! @exception  Error will be thrown if this is not a handle
                //!             to a @c NumberData type.
                //}}}
                operator double() const
                {
                    return RefPtr<T>::Raw()->Number();
                }

                //! Implicit conversion of JSON string primitive @c Data
                //{{{
                //! @exception  Error will be thrown if this is not a handle
                //!             to a @c StringData type.
                //}}}
                operator const std::string&() const
                {
                    return RefPtr<T>::Raw()->String();
                }

              //@} //}}}

              //! @name Structured data type accessors
              //@{ //{{{

                //! Return a member of a JSON Object or element of a JSON Array
                //{{{
                //! @param key_or_index The Object @a key string or Array @a index number
                //!                     for the data being requested.
                //!
                //! If a string @a key is passed, then an Object lookup will be performed.
                //! If an integer @a index is passed then an Array access will be attempted.
                //! If any other type is passed then a compile time failure should occur.
                //!
                //! For Object member requests:
                //!
                //! @exception  Error will be thrown if the member does not exist,
                //!             or if this is not a handle to an @c ObjectData type.
                //!
                //! For Array element requests:
                //!
                //! @exception  Error will be thrown if this is not a handle to an
                //!             @c ArrayData type.
                //! @exception  std::out_of_range will be thrown if the @a index is
                //!             greater than the number of elements contained in the
                //!             Array.
                //}}}
                template< typename U >
                HandleType< Data > operator[]( const U &key_or_index ) const
                {
                    return (*RefPtr<T>::Raw())[key_or_index];
                }

              //@} //}}}

              //! @name Comparison operators
              //@{ //{{{

                //! Return @c true if there is an object to dereference.
                //{{{
                //! Because this specialised @c RefPtr has an implicit conversion
                //! to @c double operator, we can't just use the normal idiom for
                //! testing if it actually contains anything.
                //!
                //! For example:
                //! @code
                //! // This is what we'd usually do, and clearly says what it means,
                //! // but ISO C++ says these two possible resolutions are ambiguous:
                //! //   RefPtr<T>::operator!=(T*)
                //! //   operator!=(double, long int)
                //! // Thanks to the historically silly definition of NULL in C++.
                //! if( handle != NULL )
                //!     ...
                //!
                //! // This resolves that, but eww ...
                //! if( handle != static_cast< HandleType<T> >(NULL) )
                //!     ...
                //!
                //! // This also works, but the double negation trick isn't usually
                //! // something it's nice to scatter through user-facing code.
                //! if( !!handle )
                //!     ...
                //!
                //! // So we provide this easy-to-read-at-a-glance option instead.
                //! if( handle.IsNotNULL() )
                //!     ...
                //! @endcode
                //!
                //! So the only trick here now, is to not confuse this functionality
                //! with the operation of the @c Data::IsNull accessor for the JSON
                //! @c null literal primitive type.  Note the distinction between
                //! @c NULL, and @c Null or @c null.
                //}}}
                bool IsNotNULL() const
                {
                    return RefPtr<T>::Raw() != NULL;
                }

              //@} //}}}

            }; //}}}


          //! @name Handle type
          //@{ //{{{

            //! %Handle type for a @c Json::Data element.
            typedef HandleType< Data >      Handle;

          //@} //}}}


          //! @name Data type query
          //@{ //{{{

            //! Return the JSON @c #DataType of this element
            virtual DataType Type() const = 0;

          //@} //}}}

          //! @name Generic container operations
          //@{ //{{{

            //! Return @c true if this is an object or array which contains no data
            //{{{
            //! Primitive data types are never empty, not even the @c null type
            //! or an empty string, so they will always return @c false here.
            //! An 'empty' primitive type does not exist to be able to call this
            //! method on at all.
            //}}}
            virtual bool empty() const
            {
                return false;
            }

          //@} //}}}


          //! @name JSON object construction methods
          //@{ //{{{

            //! Add a new @c null member to this Object
            virtual void AddMember( const std::string &name )
            {
                throw Error( "%s::AddMember( %s ): not an Object type (%s)",
                             EM_TYPEOF(*this), name.c_str(), JSONStr().c_str() );
            }

            //! Add a new boolean member to this Object
            virtual void AddMember( const std::string &name, bool value )
            {
                (void)value;
                throw Error( "%s::AddMember( %s ): not an Object type (%s)",
                             EM_TYPEOF(*this), name.c_str(), JSONStr().c_str() );
            }


            //! Add a new numeric member to this Object
            virtual void AddMember( const std::string &name, double value )
            {
                (void)value;
                throw Error( "%s::AddMember( %s ): not an Object type (%s)",
                             EM_TYPEOF(*this), name.c_str(), JSONStr().c_str() );
            }

            //! Add a new numeric member to this Object
            virtual void AddMember( const std::string &name, int value )
            {
                AddMember( name, double(value) );
            }

            //! Add a new numeric member to this Object
            virtual void AddMember( const std::string &name, long value )
            {
                AddMember( name, double(value) );
            }

            //! Add a new numeric member to this Object
            virtual void AddMember( const std::string &name, long long value )
            {
                AddMember( name, double(value) );
            }

            //! Add a new numeric member to this Object
            virtual void AddMember( const std::string &name, unsigned int value )
            {
                AddMember( name, double(value) );
            }

            //! Add a new numeric member to this Object
            virtual void AddMember( const std::string &name, unsigned long value )
            {
                AddMember( name, double(value) );
            }

            //! Add a new numeric member to this Object
            virtual void AddMember( const std::string &name, unsigned long long value )
            {
                AddMember( name, double(value) );
            }


            //! Add a new string member to this Object
            virtual void AddMember( const std::string &name, const std::string &value )
            {
                (void)value;
                throw Error( "%s::AddMember( %s ): not an Object type (%s)",
                             EM_TYPEOF(*this), name.c_str(), JSONStr().c_str() );
            }

            //! Add a new string member to this Object
            virtual void AddMember( const std::string &name, const char *value )
            {
                AddMember( name, std::string(value) );
            }


            //! Add a new object member to this Object
            //{{{
            //! @return A handle to the newly created object which can then
            //!         be used to add members to it.
            //}}}
            virtual Data::Handle AddObject( const std::string &name )
            {
                throw Error( "%s::AddObject( %s ): not an Object type (%s)",
                             EM_TYPEOF(*this), name.c_str(), JSONStr().c_str() );
            }

            //! Add a new array member to this Object
            //{{{
            //! @return A handle to the newly created array which can then
            //!         be used to add elements to it.
            //}}}
            virtual Data::Handle AddArray( const std::string &name )
            {
                throw Error( "%s::AddArray( %s ): not an Object type (%s)",
                             EM_TYPEOF(*this), name.c_str(), JSONStr().c_str() );
            }

          //@} //}}}

          //! @name JSON array construction methods
          //@{ //{{{

            //! Append a new @c null element to this Array
            virtual void AddElement()
            {
                throw Error( "%s::AddElement(): not an Array type (%s)",
                                    EM_TYPEOF(*this), JSONStr().c_str() );
            }

            //! Append a new boolean element to this Array
            virtual void AddElement( bool value )
            {
                throw Error( "%s::AddElement( %s ): not an Array type (%s)",
                             EM_TYPEOF(*this), value ? "true" : "false", JSONStr().c_str() );
            }


            //! Append a new numeric element to this Array
            virtual void AddElement( double value )
            {
                throw Error( "%s::AddElement( %g ): not an Array type (%s)",
                             EM_TYPEOF(*this), value, JSONStr().c_str() );
            }

            //! Append a new numeric element to this Array
            virtual void AddElement( long long value )
            {
                throw Error( "%s::AddElement( %lld ): not an Array type (%s)",
                             EM_TYPEOF(*this), value, JSONStr().c_str() );
            }

            //! Append a new numeric element to this Array
            virtual void AddElement( long value )
            {
                throw Error( "%s::AddElement( %ld ): not an Array type (%s)",
                             EM_TYPEOF(*this), value, JSONStr().c_str() );
            }

            //! Append a new numeric element to this Array
            virtual void AddElement( int value )
            {
                AddElement( long(value) );
            }

            //! Append a new numeric element to this Array
            virtual void AddElement( unsigned long long value )
            {
                throw Error( "%s::AddElement( %llu ): not an Array type (%s)",
                             EM_TYPEOF(*this), value, JSONStr().c_str() );
            }

            //! Append a new numeric element to this Array
            virtual void AddElement( unsigned long value )
            {
                throw Error( "%s::AddElement( %lu ): not an Array type (%s)",
                             EM_TYPEOF(*this), value, JSONStr().c_str() );
            }

            //! Append a new numeric element to this Array
            virtual void AddElement( unsigned int value )
            {
                AddElement( static_cast<unsigned long>(value) );
            }


            //! Append a new string element to this Array
            virtual void AddElement( const std::string &value )
            {
                throw Error( "%s::AddElement( %s ): not an Array type (%s)",
                             EM_TYPEOF(*this), value.c_str(), JSONStr().c_str() );
            }

            //! Append a new string element to this Array
            virtual void AddElement( const char *value )
            {
                AddElement( std::string(value) );
            }


            //! Append a new object element to this Array
            //{{{
            //! @return A handle to the newly created object which can then
            //!         be used to add members to it.
            //}}}
            virtual Data::Handle AddObject()
            {
                throw Error( "%s::AddObject(): not an Array type (%s)",
                                EM_TYPEOF(*this), JSONStr().c_str() );
            }

            //! Append a new array element to this Array
            //{{{
            //! @return A handle to the newly created array which can then
            //!         be used to add elements to it.
            //}}}
            virtual Data::Handle AddArray()
            {
                throw Error( "%s::AddArray(): not an Array type (%s)",
                                EM_TYPEOF(*this), JSONStr().c_str() );
            }

          //@} //}}}


          //! @name Primitive type accessors
          //@{ //{{{

            //! Return @c true if this element is the @c null literal primitive type
            bool IsNull() const
            {
                return Type() == NullType;
            }

            //! @brief Return the boolean value of this element
            //!
            //! @exception Error will be thrown if this is not a boolean primitive type
            virtual bool IsTrue() const
            {
                throw Error( "%s is not a Boolean type (%s)", EM_TYPEOF(*this), JSONStr().c_str() );
            }

            //! @brief Return the numeric value of this element
            //!
            //! @exception Error will be thrown if this is not a numeric primitive type
            virtual double Number() const
            {
                throw Error( "%s is not a Number type (%s)", EM_TYPEOF(*this), JSONStr().c_str() );
            }

            //! @brief Return the string value of this element
            //!
            //! @exception Error will be thrown if this is not a string primitive type
            virtual const std::string &String() const
            {
                throw Error( "%s is not a String type (%s)", EM_TYPEOF(*this), JSONStr().c_str() );
            }


            //! @brief Implicit conversion of numeric primitive @c %Data
            //!
            //! @exception Error will be thrown if this is not a numeric primitive type
            operator double() const
            {
                return Number();
            }

            //! @brief Implicit conversion of string primitive @c %Data
            //!
            //! @exception Error will be thrown if this is not a string primitive type
            operator const std::string&() const
            {
                return String();
            }


            //! Return the value of a numeric primitive as type @a T
            //{{{
            //! RFC 7159 recommends that for interoperability an implementation
            //! should expect numeric primitives to have the precision and range
            //! of an IEEE 754 @c double precision floating point type.  And JSON
            //! itself makes no distinction between integer and floating point
            //! numeric values, to it they are all just the same primitive type.
            //!
            //! However, in any real use, it is likely that values which are strictly
            //! always integers will be encoded and decoded.  This method can be used
            //! for safe conversion of a JSON numeric primitive to any other numeric
            //! type which the software calling it requires.  A compile time error
            //! will occur if the @c double type cannot be @c static_cast to type @a T,
            //! and a runtime exception will be thrown if this is not a JSON numeric
            //! primitive.
            //!
            //! @tparam T   The desired numeric type.
            //!
            //! @note This method hides the normal @c RefCounted::As dynamic cast
            //!       operator, since it has the same semantics and the narrowed
            //!       scope of only applying this operation to numeric primitives
            //!       is appropriate here.  The base class method can still be
            //!       called explicitly if needed by some specialised case though.
            //!
            //! @exception Error will be thrown if there is no data or this is not a
            //!                  numeric primitive type
            //}}}
            template< typename T >
            T As() const
            {
                return static_cast<T>( Number() );
            }

          //@} //}}}

          //! @name Object accessors
          //@{ //{{{

            //! Return a primitive type value for an Object member
            //{{{
            //! This method may be used to obtain a primitive type value for a member
            //! which may or may not exist in the object.  If the requested member
            //! does not exist then a default value for it will be returned instead.
            //!
            //! @tparam T               The type of data to return.
            //!
            //! @param key              The name of the desired member.
            //! @param default_value    The value to return if there is no member
            //!                         with the requested name in this object.
            //!
            //! If the member does exist, its value must be a compatible type to what
            //! has been requested.
            //!
            //! @exception Error will be thrown if this is not an Object, or if the
            //!                  member exists but is not a compatible type to @a T.
            //}}}
            template< typename T >
            T Get( const std::string &key, const T &default_value = T() ) const
            {
                if( Type() != ObjectType )
                    throw Error( "%s::Get<%s>( %s ) is not an Object type (%s)",
                                    EM_TYPEOF(*this), EM_TYPEOF(T), key.c_str(),
                                                              JSONStr().c_str() );
                Handle  d = Get( key );

                if( ! d )
                    return default_value;

                return static_cast<T>(d);
            }

            //! Return a named member of a JSON Object
            //{{{
            //! This method may be used to query for optional member data that
            //! may not always be present in a particular data structure.
            //!
            //! If this @c %Data element is not an object, or a member with the
            //! requested name does not exist, then a @c NULL handle will be
            //! returned.
            //}}}
            virtual Handle Get( const std::string &key ) const
            {
                (void)key;
                return NULL;
            }

            //! Return a named member of a JSON Object
            //{{{
            //! This method may be used to retrieve the value of a known or
            //! required member of an object.
            //!
            //! @exception  Error will be thrown if this @c %Data element is not
            //!             an Object or if @a key is not a member of it.
            //}}}
            virtual Handle operator[]( const std::string &key ) const
            {
                throw Error( "%s::operator[%s]: not an Object type (%s)",
                             EM_TYPEOF(*this), key.c_str(), JSONStr().c_str() );
            }

            //! Return a named member of a JSON Object
            //{{{
            //! This method may be used to retrieve the value of a known or
            //! required member of an object.
            //!
            //! @exception  Error will be thrown if this @c %Data element is not
            //!             an Object or if @a key is not a member of it.
            //}}}
            virtual Handle operator[]( const char *key ) const
            {
                return operator[]( std::string( key ) );
            }

            //! @brief Populate a @a list with the names of all Object members
            //!
            //! @exception  Error will be thrown if this is not an object type.
            virtual void GetMembers( MemberList &list ) const
            {
                (void)list;
                throw Error( "%s::GetMembers(): not an Object type (%s)",
                                    EM_TYPEOF(*this), JSONStr().c_str() );
            }

          //@} //}}}

          //! @name Array accessors
          //@{ //{{{

            //! Return a primitive type value for an Array element
            //{{{
            //! This method may be used to obtain a primitive type value for an element
            //! which may or may not exist in the array.  If the requested element does
            //! not exist then a default value for it will be returned instead.
            //!
            //! @tparam T               The type of data to return.
            //!
            //! @param index            The index of the desired element.
            //! @param default_value    The value to return if there is no element
            //!                         at the requested index in this array.
            //!
            //! If the element does exist, its value must be a compatible type to what
            //! has been requested.
            //!
            //! @exception Error will be thrown if this is not an Array, or if the
            //!                  element exists but is not a compatible type to @a T.
            //!
            //! This method is probably less useful than the one which queries for
            //! optional object members, since requesting random array elements is
            //! not generally a very useful thing to do, but we include it for
            //! symmetry since there may be times when a hard error is undesirable
            //! if an expected element may not actually be present.
            //}}}
            template< typename T >
            T Get( unsigned long index, const T &default_value = T() ) const
            { //{{{

                if( Type() != ArrayType )
                    throw Error( "%s::Get<%s>( %lu ) is not an Array type (%s)",
                                        EM_TYPEOF(*this), EM_TYPEOF(T), index,
                                                            JSONStr().c_str() );
                Handle  d = Get( index );

                if( ! d )
                    return default_value;

                return static_cast<T>(d);

            } //}}}

            //! Return an element of a JSON Array
            //{{{
            //! If this @c %Data element is not an array, or an element with the
            //! requested index does not exist, then a @c NULL handle will be
            //! returned.
            //!
            //! This method is probably less useful than the one which queries for
            //! optional object members, since requesting random array elements is
            //! not generally a very useful thing to do, but we include it for
            //! symmetry since there may be times when a hard error is undesirable
            //! if an expected element may not actually be present.
            //}}}
            virtual Handle Get( unsigned long index ) const
            {
                (void)index;
                return NULL;
            }


            //! Return an element of a JSON Array
            //{{{
            //! This method may be used to retrieve the value of a known or
            //! required element of an array.
            //!
            //! @exception  Error will be thrown if this @c %Data element is not
            //!             an Array.
            //! @exception  std::out_of_range will be thrown if the @a index is
            //!             greater than the number of elements it contains.
            //}}}
            virtual Handle operator[]( unsigned long index ) const
            {
                throw Error( "%s::operator[%lu]: not an Array type (%s)",
                             EM_TYPEOF(*this), index, JSONStr().c_str() );
            }

            //! Return an element of a JSON Array
            //{{{
            //! This method may be used to retrieve the value of a known or
            //! required element of an array.
            //!
            //! @exception  Error will be thrown if this @c %Data element is not
            //!             an Array.
            //! @exception  std::out_of_range will be thrown if the @a index is
            //!             greater than the number of elements it contains, or
            //!             greater than @c ULONG_MAX.
            //}}}
            virtual Handle operator[]( unsigned long long index ) const
            { //{{{

               #if ULONG_MAX != ULLONG_MAX
                using std::out_of_range;

                if( index > ULONG_MAX )
                    throw out_of_range( stringprintf("Json::Array[%llu] index > %lu",
                                                                    index, ULONG_MAX) );
               #endif

                return operator[]( static_cast<unsigned long>(index) );

            } //}}}

            //! Return an element of a JSON Array
            //{{{
            //! This method may be used to retrieve the value of a known or
            //! required element of an array.
            //!
            //! @exception  Error will be thrown if this @c %Data element is not
            //!             an Array.
            //! @exception  std::out_of_range will be thrown if the @a index is
            //!             greater than the number of elements it contains.
            //}}}
            virtual Handle operator[]( unsigned index ) const
            {
                return operator[]( static_cast<unsigned long>(index) );
            }


            //! Return an element of a JSON Array
            //{{{
            //! This method may be used to retrieve the value of a known or
            //! required element of an array.
            //!
            //! @exception  Error will be thrown if this @c %Data element is not
            //!             an Array.
            //! @exception  std::out_of_range will be thrown if the @a index is
            //!             greater than the number of elements it contains, or
            //!             less than 0.
            //}}}
            virtual Handle operator[]( long index ) const
            { //{{{

                using std::out_of_range;

                if( index < 0 )
                    throw out_of_range( stringprintf("Json::Array[%ld] index < 0", index) );

                return operator[]( static_cast<unsigned long>(index) );

            } //}}}

            //! Return an element of a JSON Array
            //{{{
            //! This method may be used to retrieve the value of a known or
            //! required element of an array.
            //!
            //! @exception  Error will be thrown if this @c %Data element is not
            //!             an Array.
            //! @exception  std::out_of_range will be thrown if the @a index is
            //!             greater than the number of elements it contains, or
            //!             less than 0, or is greater than @c LONG_MAX.
            //}}}
            virtual Handle operator[]( long long index ) const
            { //{{{

               #if LONG_MAX != LLONG_MAX
                using std::out_of_range;

                if( index > LONG_MAX )
                    throw out_of_range( stringprintf("Json::Array[%lld] index > %ld",
                                                                    index, LONG_MAX) );
               #endif

                return operator[]( long(index) );

            } //}}}

            //! Return an element of a JSON Array
            //{{{
            //! This method may be used to retrieve the value of a known or
            //! required element of an array.
            //!
            //! @exception  Error will be thrown if this @c %Data element is not
            //!             an Array.
            //! @exception  std::out_of_range will be thrown if the @a index is
            //!             greater than the number of elements it contains, or
            //!             less than 0.
            //}}}
            virtual Handle operator[]( int index ) const
            {
                return operator[]( long(index) );
            }


            //! @brief Return the number of elements in an Array
            //!
            //! @exception  Error will be thrown if this is not an array type.
            virtual size_t GetArraySize() const
            {
                throw Error( "%s::GetArraySize(): not an Array type (%s)",
                                    EM_TYPEOF(*this), JSONStr().c_str() );
            }

          //@} //}}}


          //! @name Output methods
          //@{ //{{{

            //! Return a compact JSON encoded string of this data element
            virtual std::string JSONStr() const = 0;

          //@} //}}}

        }; //}}}



      //! @name Handle type
      //@{ //{{{

        //! Handle type for a @c %Json parser instance.
        typedef Data::HandleType< Json >      Handle;

      //@} //}}}


    private:

        //! Specialised container for the JSON literal values
        class EnumData : public Data
        { //{{{
        private:

            //! JSON literal value type identifiers
            enum Value
            {
                Null,   //!< The JSON literal @c null
                False,  //!< The JSCON literal @c false
                True    //!< The JSON literal @c true
            };


            //! Parse a JSON literal value from @a data beginning at @a pos
            //{{{
            //! This method expects to find the start of a JSON literal at @a pos.
            //! No whitespace or other characters are skipped before testing this.
            //!
            //! @param data The JSON text to parse.
            //! @param pos  The byte position within @a data to begin parsing.
            //!             Upon successful return, this will contain the position
            //!             of the byte following the last character of the JSON
            //!             literal value.  In the event of failure (an exception
            //!             being thrown) it will remain unchanged.
            //!
            //! @return The enumerated @c Value for the literal.
            //!
            //! @exception  Error will be thrown if the data does not begin with a
            //!             valid JSON literal value.
            //}}}
            static Value parse( const std::string &data, size_t &pos )
            { //{{{

                if( data.find("null", pos) == pos )
                {
                    pos += 4;
                    return Null;
                }

                if( data.find("false", pos) == pos )
                {
                    pos += 5;
                    return False;
                }

                if( data.find("true", pos) == pos )
                {
                    pos += 4;
                    return True;
                }

                throw Error( "Invalid JSON, not a literal at position %zu in '%s'",
                                                                pos, data.c_str() );
            } //}}}


            //! The enumerated JSON literal value
            Value       m_value;


        public:

          //! @name Constructors
          //@{ //{{{

            //! Create a new EnumData instance containing @c null
            EnumData()
                : m_value( Null )
            {}

            //! Create a new EnumData instance containing a boolean value
            EnumData( bool value )
                : m_value( value ? True : False )
            {}

            //! Parse a new literal from the @a data at @a pos
            EnumData( const std::string &data, size_t &pos )
                : m_value( parse(data, pos) )
            {}

          //@} //}}}


          //! @name Data type query
          //@{ //{{{

            virtual DataType Type() const
            {
                switch( m_value )
                {
                    case Null:
                        return NullType;

                    case False:
                    case True:
                        return BoolType;
                }

                // This should never happen, it's just to hush a bogus compiler warning.
                throw Error( "Bad Json::EnumData value %d", m_value );
            }

          //@} //}}}

          //! @name Primitive type accessors
          //@{ //{{{

            virtual bool IsTrue() const
            {
                switch( m_value )
                {
                    case Null:
                        break;

                    case False:
                        return false;

                    case True:
                        return true;
                }

                throw Error( "Json::EnumData(%d) is not a Boolean type", m_value );
            }

          //@} //}}}


          //! @name Output methods
          //@{ //{{{

            virtual std::string JSONStr() const
            {
                switch( m_value )
                {
                    case Null:  return "null";
                    case False: return "false";
                    case True:  return "true";
                }

                // This should never happen, it's just to hush a bogus compiler warning.
                throw Error( "Bad Json::EnumData value %d", m_value );
            }

          //@} //}}}

        }; //}}}


        //! Specialised container for JSON numeric values
        class NumberData : public Data
        { //{{{
        private:

            //! The floating point numerical value
            double          m_value;


        public:

          //! @name Constructors
          //@{ //{{{

            //! Create a new NumberData instance containing @a value
            NumberData( double value )
                : m_value( value )
            {}

            //! Parse a new numeric value from the @a data at @a pos
            //{{{
            //! This constructor expects to find the start of a JSON number value
            //! at @a pos.  No whitespace or other characters are skipped before
            //! testing this.
            //!
            //! @param data The JSON text to parse.
            //! @param pos  The byte position within @a data to begin parsing.
            //!             Upon successful return, this will contain the position
            //!             of the byte following the last character of the JSON
            //!             numeric value.  In the event of failure (an exception
            //!             being thrown) it will remain unchanged.
            //!
            //! @exception  Error will be thrown if the data does not begin with a
            //!             valid JSON numeric value.
            //}}}
            NumberData( const std::string &data, size_t &pos )
            {
                // Always use the C locale for strtod, since we always want the
                // radix character to be '.' even in locales where the decimal
                // point might be a comma instead.
               #if HAVE_NEWLOCALE
                static locale_t     clocale = newlocale(LC_ALL_MASK, "C", NULL);
               #elif HAVE__CREATE_LOCALE
                static _locale_t    clocale = _create_locale(LC_ALL, "C");
               #endif

                const char  *b = data.c_str() + pos;
                char        *e;

                // This isn't quite right, it would allow the number to be given in hex,
                // which RFC 7159 doesn't permit, but that's probably not a problem unless
                // someone wants to use this code as a strict validator.  We could always
                // check for the hex prefix if we want or need to be fussy about that.
               #if HAVE_STRTOD_L
                m_value = strtod_l( b, &e, clocale );
               #elif HAVE__STRTOD_L
                m_value = _strtod_l( b, &e, clocale );
               #else
                // Fall back to using strtod in the current locale.  There's not much
                // else we can safely do here except advise the user (which is done
                // by the configure test) that they'll need to run this using the "C"
                // locale if their default locale treats decimal numbers differently
                // to that.
                (void)clocale;
                m_value = strtod( b, &e );
               #endif

                if( b == e )
                    throw Error( "Invalid JSON, bad number conversion at position %zu in '%s'",
                                                                        pos, data.c_str() );
                pos += size_t(e - b);
            }

          //@} //}}}


          //! @name Data type query
          //@{ //{{{

            virtual DataType Type() const
            {
                return NumberType;
            }

          //@} //}}}

          //! @name Primitive type accessors
          //@{ //{{{

            virtual double Number() const
            {
                return m_value;
            }

          //@} //}}}


          //! @name Output methods
          //@{ //{{{

            virtual std::string JSONStr() const
            {
                return stringprintf( "%.12g", m_value );
            }

          //@} //}}}

        }; //}}}


        // Forward declaration for the StringData friend
        class ObjectData;

        //! Specialised container for JSON string values
        class StringData : public Data
        { //{{{

            // Allow ObjectData to use our parse() method
            friend class Json::ObjectData;

        private:

            //! The string content
            std::string     m_value;


            //! Parse a JSON string value from @a data beginning at @a pos
            //{{{
            //! This method expects to find the start of a JSON string at @a pos.
            //! No whitespace or other characters are skipped before testing this.
            //!
            //! @param data     The JSON text to parse.
            //! @param pos      The byte position within @a data to begin parsing, this
            //!                 is expected to be the first byte of the string itself,
            //!                 immediately following the opening quote character.
            //!                 Upon successful return, this will contain the position
            //!                 of the byte following the terminating quote of the JSON
            //!                 string value.  In the event of failure (an exception
            //!                 being thrown) it will remain unchanged.
            //! @param context  The context to use when reporting an error. This is just
            //!                 an informative string that has no internal meaning.  We
            //!                 use this method for parsing object names too, so this
            //!                 lets us say a bit more explicitly exactly what operation
            //!                 has failed if it does.
            //!
            //! @return The raw string value with any JSON escaping undone.
            //!
            //! @exception  Error will be thrown if no terminating quote can be found
            //!             to mark the end of the string.
            //}}}
            static std::string parse( const std::string &data, size_t &pos, const char *context )
            { //{{{

                size_t  b = pos;
                size_t  e = pos;

                for(;;)
                {
                    e = data.find('"', e);

                    if( e == pos )
                    {
                        // This is an empty "" string, we're all done.
                        ++pos;
                        return std::string();
                    }

                    if( e == std::string::npos )
                        throw Error( "Invalid JSON, unterminated %s at position %zu in '%s'",
                                                                context, pos, data.c_str() );

                    // Check if the quote we found is escaped.  If there is an even number
                    // of backslashes preceding it (and hence an odd number of characters
                    // back to the first non-backslash character) then it is not.
                    // If it is, then keep searching for an unescaped one.
                    size_t  n = data.find_last_not_of('\\', e - 1);

                    // If it's backslashes all the way down, then only measure back to the
                    // opening quote where this string started.
                    if( n == std::string::npos || n < pos )
                        n = pos - 1;

                    if( (e - n) % 2 )
                        break;

                    ++e;
                }

                pos = e + 1;

                return Unescape( data.substr(b, e - b) );

            } //}}}


        public:

          //! @name Constructors
          //@{ //{{{

            //! Create a new StringData instance containing @a value
            StringData( const std::string &value )
                : m_value( value )
            {}

            //! Parse a new string from the @a data at @a pos
            StringData( const std::string &data, size_t &pos )
                : m_value( parse(data, pos, "string") )
            {}

          //@} //}}}


          //! @name Data type query
          //@{ //{{{

            virtual DataType Type() const
            {
                return StringType;
            }

          //@} //}}}

          //! @name Primitive type accessors
          //@{ //{{{

            virtual const std::string &String() const
            {
                return m_value;
            }

          //@} //}}}

          //! @name Output methods
          //@{ //{{{

            virtual std::string JSONStr() const
            {
                return '"' + Escape(m_value) + '"';
            }

          //@} //}}}

        }; //}}}


        //! Specialised container for JSON array structures
        class ArrayData : public Data
        { //{{{
        private:

            //! Container type for JSON Array elements
            typedef std::vector< Data::Handle >     Elements;


            //! The array of JSON elements
            Elements    m_elements;


        public:

          //! @name Constructors
          //@{ //{{{

            //! Create a new empty ArrayData instance
            ArrayData() {}

            //! Parse a new JSON array from the @a data at @a pos
            //{{{
            //! This constructor expects to find the start of JSON array values
            //! at @a pos.  Insignificant whitespace will be skipped both before
            //! and between the array element values.
            //!
            //! @param data The JSON text to parse.
            //! @param pos  The byte position within @a data to begin parsing, this
            //!             is expected to be the first byte after the opening '['
            //!             character of the array.
            //!             Upon successful return, this will contain the position
            //!             of the byte following the closing ']' of the JSON array
            //!             structure.  In the event of failure (an exception being
            //!             thrown) it will likely point to the place where processing
            //!             failed, but no guarantee about where it may point is made.
            //!
            //! @exception  Error will be thrown if no closing ']' is found to mark
            //!             the end of the array, or if any character other than ','
            //!             or insignificant whitespace is found between the element
            //!             values.
            //! @exception  Various other exceptions may be thrown if there is an
            //!             error parsing the content of the array element types.
            //}}}
            ArrayData( const std::string &data, size_t &pos )
            { //{{{

                size_t n = skip_whitespace( data, pos );

                if( n == std::string::npos )
                    throw Error("Invalid JSON, unexpected end of array after position %zu in '%s'",
                                                                            pos, data.c_str() );
                if( data[n] == ']' )
                {
                    pos = n + 1;
                    return;
                }

                for(;;)
                {
                    m_elements.push_back( parse_value(data, pos) );

                    n = skip_whitespace( data, pos );

                    if( n == std::string::npos )
                        throw Error("Invalid JSON, unexpected end of array after position %zu in '%s'",
                                                                                pos, data.c_str() );
                    switch( data[n] )
                    {
                        case ',':
                            pos = n + 1;
                            break;

                        case ']':
                            pos = n + 1;
                            return;

                        default:
                            throw Error("Invalid JSON, unexpected character '%c' at position %zu in '%s'",
                                                                            data[n], n, data.c_str() );
                    }
                }

            } //}}}

          //@} //}}}


          //! @name Data type query
          //@{ //{{{

            virtual DataType Type() const
            {
                return ArrayType;
            }

          //@} //}}}

          //! @name Generic container operations
          //@{ //{{{

            virtual bool empty() const
            {
                return m_elements.empty();
            }

          //@} //}}}


          //! @name JSON array construction methods
          //@{ //{{{

            using Data::AddObject;  //(const std::string &name)
            using Data::AddArray;   //(const std::string &name)
            using Data::AddElement; //(int value), (unsigned value), (const char*)

            virtual void AddElement()
            {
                m_elements.push_back( new EnumData );
            }

            virtual void AddElement( bool value )
            {
                m_elements.push_back( new EnumData(value) );
            }

            virtual void AddElement( double value )
            {
                m_elements.push_back( new NumberData(value) );
            }

            virtual void AddElement( long long value )
            {
                AddElement( double(value) );
            }

            virtual void AddElement( long value )
            {
                AddElement( double(value) );
            }

            virtual void AddElement( unsigned long long value )
            {
                AddElement( double(value) );
            }

            virtual void AddElement( unsigned long value )
            {
                AddElement( double(value) );
            }

            virtual void AddElement( const std::string &value )
            {
                m_elements.push_back( new StringData(value) );
            }


            virtual Data::Handle AddObject()
            {
                Handle  d = new ObjectData;

                m_elements.push_back( d );
                return d;
            }

            virtual Data::Handle AddArray()
            {
                Handle  d = new ArrayData;

                m_elements.push_back( d );
                return d;
            }

          //@} //}}}

          //! @name Array accessors
          //@{ //{{{

            // Don't hide the overloads that we don't implement here
            using Data::Get;        //(const std::string&);
            using Data::operator[]; //(const std::string&);


            virtual Handle Get( unsigned long index ) const
            {
                if( index < m_elements.size() )
                    return m_elements[index];

                return NULL;
            }

            virtual Handle operator[]( unsigned long index ) const
            {
                using std::out_of_range;

                if( index < m_elements.size() )
                    return m_elements[index];

                throw out_of_range( stringprintf("Json::Array[%lu] index out of bounds"
                                                           " (array has %zu elements)",
                                                              index, m_elements.size()) );
            }

            virtual size_t GetArraySize() const
            {
                return m_elements.size();
            }

          //@} //}}}


          //! @name Output methods
          //@{ //{{{

            virtual std::string JSONStr() const
            {
                if( m_elements.empty() )
                    return "[]";

                std::string     s("[ ");
                const char     *sep = "";

                for( Elements::const_iterator i = m_elements.begin(),
                                              e = m_elements.end(); i != e; ++i )
                {
                    s.append( sep );
                    s.append( (*i)->JSONStr() );
                    sep = ", ";
                }

                return s + " ]";
            }

          //@} //}}}

        }; //}}}


        //! Specialised container for JSON object structures
        class ObjectData : public Data
        { //{{{
        private:

            //! Container type for JSON Object members
           #ifdef BB_HASH_JSON_OBJECT_MEMBERS
            typedef was_tr1::unordered_map< std::string, Data::Handle >     Members;
           #else
            typedef std::map< std::string, Data::Handle >   Members;
           #endif


            //! The members of this JSON Object
            Members     m_members;


            //! Parse a JSON object member name from @a data beginning at @a pos
            //{{{
            //! This method expects to find the start of a JSON member name string
            //! immediately after skipping any initial insignificant whitespace.
            //!
            //! @param data The JSON text to parse.
            //! @param pos  The byte position within @a data to begin parsing.
            //!             Upon successful return, this will contain the position
            //!             of the byte following the last character of the JSON
            //!             member name.  In the event of failure (an exception
            //!             being thrown) it will likely point to the place where
            //!             processing failed, but no guarantee about where it may
            //!             point is made.
            //!
            //! @return The raw string member name with any JSON escaping undone.
            //!
            //! @exception  Error will be thrown if the data does not begin with an
            //!             object member name after skipping insignificant whitespace.
            //}}}
            std::string parse_name( const std::string &data, size_t &pos )
            { //{{{

                size_t  b = skip_whitespace( data, pos );

                if( b == std::string::npos || data[b] != '"' )
                    throw Error( "Invalid JSON, expecting object member name at position %zu in '%s'",
                                                pos, data.c_str() );

                std::string     name = StringData::parse( data, ++b, "object member name" );
                size_t          s    = skip_whitespace( data, b );

                if( s == std::string::npos || data[s] != ':' )
                    throw Error( "Invalid JSON, no object name separator at position %zu in '%s'",
                                                b, data.c_str() );

                pos = s + 1;

                return name;

            } //}}}


        public:

          //! @name Constructors
          //@{ //{{{

            //! Create a new empty ObjectData instance
            ObjectData() {}

            //! Parse a new JSON object from the @a data at @a pos
            //{{{
            //! This constructor expects to find the start of JSON object members
            //! at @a pos.  Insignificant whitespace will be skipped both before
            //! and between the object member values.
            //!
            //! @param data The JSON text to parse.
            //! @param pos  The byte position within @a data to begin parsing, this
            //!             is expected to be the first byte after the opening '{'
            //!             character of the object.
            //!             Upon successful return, this will contain the position
            //!             of the byte following the closing '}' of the JSON object
            //!             structure.  In the event of failure (an exception being
            //!             thrown) it will likely point to the place where processing
            //!             failed, but no guarantee about where it may point is made.
            //!
            //! @exception  Error will be thrown if no closing '}' is found to mark
            //!             the end of the object, or if any character other than ','
            //!             or insignificant whitespace is found between the member
            //!             name/value pairs.
            //! @exception  Various other exceptions may be thrown if there is an
            //!             error parsing the content of the array element types.
            //}}}
            ObjectData( const std::string &data, size_t &pos )
            { //{{{

                size_t n = skip_whitespace( data, pos );

                if( n == std::string::npos )
                    throw Error("Invalid JSON, unexpected end of object after position %zu in '%s'",
                                                                                pos, data.c_str() );
                if( data[n] == '}' )
                {
                    pos = n + 1;
                    return;
                }

                for(;;)
                {
                    std::string     name = parse_name( data, pos );
                    Data::Handle    v    = parse_value( data, pos );

                    m_members[ name ] = v;

                    n = skip_whitespace( data, pos );

                    if( n == std::string::npos )
                        throw Error("Invalid JSON, unexpected end of object after position %zu in '%s'",
                                                                                pos, data.c_str() );
                    switch( data[n] )
                    {
                        case ',':
                            pos = n + 1;
                            break;

                        case '}':
                            pos = n + 1;
                            return;

                        default:
                            throw Error("Invalid JSON, unexpected character '%c' at position %zu in '%s'",
                                                                            data[n], n, data.c_str() );
                    }
                }

            } //}}}

          //@} //}}}


          //! @name Data type query
          //@{ //{{{

            virtual DataType Type() const
            {
                return ObjectType;
            }

          //@} //}}}

          //! @name Generic container operations
          //@{ //{{{

            virtual bool empty() const
            {
                return m_members.empty();
            }

          //@} //}}}


          //! @name JSON object construction methods
          //@{ //{{{

            using Data::AddObject;  //(void)
            using Data::AddArray;   //(void)
            using Data::AddMember;  //({unsigned,}{int, long}), (const char*)

            virtual void AddMember( const std::string &name )
            {
                m_members[ name ] = new EnumData;
            }

            virtual void AddMember( const std::string &name, bool value )
            {
                m_members[ name ] = new EnumData( value );
            }

            virtual void AddMember( const std::string &name, double value )
            {
                m_members[ name ] = new NumberData( value );
            }

            virtual void AddMember( const std::string &name, const std::string &value )
            {
                m_members[ name ] = new StringData( value );
            }


            virtual Data::Handle AddObject( const std::string &name )
            {
                return m_members[ name ] = new ObjectData;
            }

            virtual Data::Handle AddArray( const std::string &name )
            {
                return m_members[ name ] = new ArrayData;
            }

          //@} //}}}

          //! @name Object accessors
          //@{ //{{{

            // Don't hide the overloads that we don't implement here
            using Data::Get;        //(unsigned);
            using Data::operator[]; //(unsigned);


            virtual Handle Get( const std::string &key ) const
            {
                Members::const_iterator i = m_members.find( key );

                if( i != m_members.end() )
                    return i->second;

                return NULL;
            }

            virtual Handle operator[]( const std::string &key ) const
            {
                Members::const_iterator i = m_members.find( key );

                if( i != m_members.end() )
                    return i->second;

                throw Error( "Json::Object[%s] no such member", key.c_str() );
            }

            virtual void GetMembers( MemberList &list ) const
            {
                for( Members::const_iterator i = m_members.begin(),
                                             e = m_members.end(); i != e; ++i )
                    list.push_back( i->first );
            }

          //@} //}}}


          //! @name Output methods
          //@{ //{{{

            virtual std::string JSONStr() const
            {
                if( m_members.empty() )
                    return "{}";

                std::string     s("{ ");
                const char     *sep = "";

                for( Members::const_iterator i = m_members.begin(),
                                             e = m_members.end(); i != e; ++i )
                {
                    s.append( sep );
                    s.append( 1, '"' );
                    s.append( Escape(i->first) );
                    s.append( "\": " );
                    s.append( i->second->JSONStr() );
                    sep = ", ";
                }

                return s + " }";
            }

          //@} //}}}

        }; //}}}


      //! @name JSON structure parsing
      //@{ //{{{

        //! Return the position of the first non-whitespace byte in @a data after @a pos
        //{{{
        //! If there is no character that is not insignificant whitespace after
        //! @a pos, then @c std::string::npos will be returned.
        //}}}
        static size_t skip_whitespace( const std::string &data, size_t pos )
        { //{{{

            return data.find_first_not_of(" \t\n\r", pos);

        } //}}}

        //! Parse a single JSON value from @a data beginning at @a pos
        //{{{
        //! This method expects to find the start of a JSON primitive or structural
        //! type immediately after skipping any initial insignificant whitespace.
        //!
        //! @param data The JSON text to parse.
        //! @param pos  The byte position within @a data to begin parsing.
        //!             Upon successful return, this will contain the position
        //!             of the byte following the last character of the JSON
        //!             value element.  In the event of failure (an exception
        //!             being thrown) it will likely point to the place where
        //!             processing failed, but no guarantee about where it may
        //!             point is made.
        //!
        //! @return A handle to the @c Json::Data for this value.
        //!
        //! @exception  Error will be thrown if the data does not begin with a
        //!             JSON data type after skipping insignificant whitespace.
        //! @exception  Various other exceptions may be thrown if there is an
        //!             error parsing the content of the JSON data type.
        //}}}
        static Data::Handle parse_value( const std::string &data, size_t &pos )
        { //{{{

            size_t  b = skip_whitespace( data, pos );

            if( b == std::string::npos )
                throw Error( "Invalid JSON, expecting value at position %zu in '%s'",
                                                                pos, data.c_str() );
            pos = b;

            EM_TRY_PUSH_DIAGNOSTIC_IGNORE("-Wgnu-case-range")

            switch( data[pos] )
            {
                case '{':
                    return new ObjectData( data, ++pos );

                case '[':
                    return new ArrayData( data, ++pos );

                case '"':
                    return new StringData( data, ++pos );

                case '-':
                case '0' ... '9':
                    return new NumberData( data, pos );

                case 'f':
                case 'n':
                case 't':
                    return new EnumData( data, pos );

                default:
                    throw Error( "Invalid JSON value at position %zu in '%s'",
                                                pos, data.c_str() );
            }
            EM_POP_DIAGNOSTIC

        } //}}}

        //! Parse the root of a JSON @a data string, starting at byte @a pos
        //{{{
        //! This method expects to find the start of an unnamed Object or Array
        //! immediately after skipping any initial insignificant whitespace.
        //!
        //! @param data The JSON text to parse.
        //! @param pos  The byte position within @a data to begin parsing.
        //!             Upon successful return, this will contain the position
        //!             of the byte following the last character of the JSON
        //!             structure.  In the event of failure (an exception being
        //!             thrown) it will likely point to the place where processing
        //!             failed, but no guarantee about where it may point is made.
        //!
        //! @return A handle to the root object or array.  A NULL handle will
        //!         be returned if @a data is an empty string or contains only
        //!         insignificant whitespace.
        //!
        //! @exception  Error will be thrown if the data does not begin with
        //!             a valid JSON value and is not empty after skipping
        //!             insignificant whitespace.
        //! @exception  Various other exceptions may be thrown if there is an
        //!             error parsing the content of the JSON structure.
        //}}}
        static Data::Handle parse_root( const std::string &data, size_t &pos )
        { //{{{

            pos = skip_whitespace( data, pos );

        #ifdef STRICT_RFC4627_COMPATIBILITY

            if( pos != std::string::npos )
            {
                switch( data[pos] )
                {
                    case '{':
                        return new ObjectData( data, ++pos );

                    case '[':
                        return new ArrayData( data, ++pos );

                    default:
                        throw Error( "Invalid JSON at position %zu in '%s'",
                                                    pos, data.c_str() );
                }
            }

            return NULL;

        #else    // RFC 7159 mode

            if( pos == std::string::npos )
                return NULL;

           #ifndef JSON_REJECT_TRAILING_JUNK

            return parse_value( data, pos );

           #else

            // Note: enabling this mode will break the Decode() behaviour of
            // returning the number of bytes consumed.  RFC 7159 complicates
            // the decision about what is the Right Thing To Do when there is
            // trailing junk, because compared to the RFC 4627 behaviour some
            // strings of JSON text can be parsed quite differently.
            //
            // For example:  "foo": 1 }
            // is simply invalid to RFC 4627 as the opening brace is missing,
            // but in RFC 7159 mode, it is the valid primitive string "foo",
            // and the rest is trailing junk.
            //
            // Applications using RFC 7159 mode that care about detecting that
            // sort of error are probably still best off NOT enabling this and
            // simply checking if the type of the root was an object (or array)
            // if that is what they require.

            Data::Handle    v = parse_value( data, pos );
            size_t          n = skip_whitespace( data, pos );

            if( n == std::string::npos )
                return v;

            throw Error( "Invalid JSON, unexpected character '%c' at position %zu in '%s'",
                                                                data[n], n, data.c_str() );
           #endif

        #endif

        } //}}}

        //! Parse the structure of a JSON @a data string
        static Data::Handle parse( const std::string &data )
        { //{{{

            size_t  pos = 0;

            return parse_root( data, pos );

        } //}}}

      //@} //}}}


        //! The unnamed root Object or Array structure
        Data::Handle    m_root;


    public:

      //! @name Constructors
      //@{ //{{{

        //! Default constructor for a new empty parser
        Json() {}

        //! Construct a new instance, parsing the JSON @a data string
        //{{{
        //! If there is trailing content after a valid JSON structure then it
        //! will simply be ignored.
        //!
        //! @exception  Various exceptions may be thrown if the initial part
        //!             of the @a data string is not a valid JSON structure.
        //}}}
        Json( const std::string &data )
            : m_root( parse(data) )
        {}

        //! Construct a new instance, parsing the JSON @a data string
        //{{{
        //! If there is trailing content after a valid JSON structure then it
        //! will simply be ignored.
        //!
        //! This constructor will never throw.  Instead, if there is an error,
        //! then a description of it will be returned in the @a error parameter.
        //! If an error is returned, then the content of this parser instance
        //! is undefined and no attempt should be made to use it.
        //!
        //! If no error occurs, then the content of @a error will be untouched.
        //! Sane users will generally want to ensure it is empty before this
        //! method is called, but that is not a hard requirement.  If an error
        //! occurs the content will be replaced, not appended to.
        //}}}
        Json( const std::string &data, std::string &error )
        {
            Decode( data, error );
        }

      //@} //}}}


      //! @name Generic container operations
      //@{ //{{{

        //! Erases all data currently held in this parser instance
        void clear()        { m_root = NULL; }

        //! Return @c true if this parser contains no data
        //{{{
        //! It will still return @c false if it contains only a root object
        //! or array but that structure is currently empty.
        //}}}
        bool empty() const  { return ! m_root ? true : m_root->empty(); }

      //@} //}}}

      //! @name Initialiser methods
      //@{ //{{{

        //! Decode a new JSON @a data string
        //{{{
        //! The existing content of this parser, if any, will be replaced by
        //! the new data.
        //!
        //! @return The number of bytes consumed from @a data.  This may be
        //!         less than the size of @a data if there is trailing content
        //!         that is not part of the JSON root Object or Array.  For a
        //!         returned value of @a n, <tt>data[<em>n</em>]</tt> will be
        //!         the first byte of trailing non-JSON data in the string.
        //!
        //! @exception  Various exceptions may be thrown if the initial part
        //!             of the @a data string is not a valid JSON structure.
        //}}}
        size_t Decode( const std::string &data )
        { //{{{

            size_t  pos = 0;

            m_root = parse_root( data, pos );
            return pos;

        } //}}}

        //! Decode a new JSON @a data string
        //{{{
        //! The existing content of this parser, if any, will be replaced by
        //! the new data.
        //!
        //! This method will never throw.  Instead, if there is an error, then
        //! a description of it will be returned in the @a error parameter.
        //! If an error is returned, then the content of this parser instance
        //! (and the return value of this method) is undefined and no attempt
        //! should be made to use them.
        //!
        //! If no error occurs, then the content of @a error will be untouched.
        //! Sane users will generally want to ensure it is empty before this
        //! method is called, but that is not a hard requirement.  If an error
        //! occurs the content will be replaced, not appended to.
        //!
        //! @return The number of bytes consumed from @a data.  This may be
        //!         less than the size of @a data if there is trailing content
        //!         that is not part of the JSON root Object or Array.  For a
        //!         returned value of @a n, <tt>data[<em>n</em>]</tt> will be
        //!         the first byte of trailing non-JSON data in the string.
        //}}}
        size_t Decode( const std::string &data, std::string &error )
        { //{{{

            size_t  pos = 0;

            try {
                m_root = parse_root( data, pos );
            }
            catch( const abi::__forced_unwind& ) { throw; }
            catch( const std::exception &e )     { error = e.what(); }
            catch( ... )                         { error = "Unknown exception"; }

            return pos;

        } //}}}


        //! Create a JSON object structure
        //{{{
        //! This method will destroy any existing data that may be contained in
        //! this parser instance, and return a handle to a new Object that can
        //! be populated with members.
        //}}}
        Data::Handle NewObject()
        { //{{{

            m_root = new ObjectData;
            return m_root;

        } //}}}

        //! Create a JSON array structure
        //{{{
        //! This method will destroy any existing data that may be contained in
        //! this parser instance, and return a handle to a new Array that can
        //! be populated with elements.
        //}}}
        Data::Handle NewArray()
        { //{{{

            m_root = new ArrayData;
            return m_root;

        } //}}}

      //@} //}}}

      //! @name JSON object construction methods
      //@{ //{{{

        //! Add a new @c null member to the root Object
        void AddMember( const std::string &name )
        { //{{{

            if( ! m_root )
                throw Error( "Json::AddMember( %s ): no root Object to add to", name.c_str() );

            m_root->AddMember( name );

        } //}}}

        //! Add a new member to the root Object
        template< typename T >
        void AddMember( const std::string &name, T value )
        { //{{{

            if( ! m_root )
                throw Error( "Json::AddMember( %s ): no root Object to add to", name.c_str() );

            m_root->AddMember( name, value );

        } //}}}

        //! Add a new object member to the root Object
        //{{{
        //! @return A handle to the newly created object which can then
        //!         be used to add members to it.
        //}}}
        Data::Handle AddObject( const std::string &name )
        { //{{{

            if( ! m_root )
                throw Error( "Json::AddObject( %s ): no root Object to add to", name.c_str() );

            return m_root->AddObject( name );

        } //}}}

        //! Add a new array member to the root Object
        //{{{
        //! @return A handle to the newly created array which can then
        //!         be used to add elements to it.
        //}}}
        Data::Handle AddArray( const std::string &name )
        { //{{{

            if( ! m_root )
                throw Error( "Json::AddArray( %s ): no root Object to add to", name.c_str() );

            return m_root->AddArray( name );

        } //}}}

      //@} //}}}

      //! @name JSON array construction methods
      //@{ //{{{

        //! Append a new @c null element to the root Array
        void AddElement()
        { //{{{

            if( ! m_root )
                throw Error( "Json::AddElement(): no root Array to add to" );

            m_root->AddElement();

        } //}}}

        //! Append a new element to the root Array
        template< typename T >
        void AddElement( T value )
        { //{{{

            if( ! m_root )
                throw Error( "Json::AddElement( %s ): no root Array to add to", EM_TYPEOF(T) );

            m_root->AddElement( value );

        } //}}}

        //! Append a new object element to this Array
        //{{{
        //! @return A handle to the newly created object which can then
        //!         be used to add members to it.
        //}}}
        Data::Handle AddObject()
        { //{{{

            if( ! m_root )
                throw Error( "Json::AddElement(): no root Array to add to" );

            return m_root->AddObject();

        } //}}}

        //! Append a new array element to this Array
        //{{{
        //! @return A handle to the newly created array which can then
        //!         be used to add elements to it.
        //}}}
        Data::Handle AddArray()
        { //{{{

            if( ! m_root )
                throw Error( "Json::AddElement(): no root Array to add to" );

            return m_root->AddArray();

        } //}}}

      //@} //}}}


      //! @name Root accessors
      //@{ //{{{

        //! Return the @c #DataType of the root JSON value
        //{{{
        //! If the parser is currently empty it will return the @c #NullType.
        //!
        //! When built with @c STRICT_RFC4627_COMPATIBILITY defined, the only
        //! (other) valid root types are @c Object or @c Array structured
        //! data, but RFC&nbsp;7159 allows valid JSON text to consist of any
        //! JSON value including just a single primitive type.
        //}}}
        DataType RootType() const
        {
            return ! m_root ? NullType : m_root->Type();
        }

        //! Return the value of the JSON text root
        //{{{
        //! If the parser is currently empty it will return a @c NULL handle
        //! (not to be confused with a value of the primitive @c #NullType).
        //!
        //! When built with @c STRICT_RFC4627_COMPATIBILITY defined, the only
        //! (other) valid root values are @c Object or @c Array structured
        //! data, but RFC&nbsp;7159 allows valid JSON text to consist of any
        //! JSON value including just a single primitive type.
        //}}}
        const Data::Handle &GetRoot() const
        {
            return m_root;
        }

      //@} //}}}

      //! @name Primitive type accessors
      //@{ //{{{

        //! Return @c true if the only data is a @c null literal primitive type
        //
        //! @exception Error will be thrown if there is no data
        bool IsNull() const
        {
            if( ! m_root )
                throw Error( "Json::IsNull: no data" );

            return m_root->IsNull();
        }

        //! Return @c true or @c false if the only data is a boolean primitive
        //
        //! @exception Error will be thrown if there is no data or this is not a
        //!                  boolean primitive type
        bool IsTrue() const
        {
            if( ! m_root )
                throw Error( "Json::IsTrue: no data" );

            return m_root->IsTrue();
        }

        //! Return the value if the only data is a numeric primitive
        //
        //! @exception Error will be thrown if there is no data or this is not a
        //!                  numeric primitive type
        double Number() const
        {
            if( ! m_root )
                throw Error( "Json::Number: no data" );

            return m_root->Number();
        }

        //! Return the value if the only data is a string primitive
        //
        //! @exception Error will be thrown if there is no data or this is not a
        //!                  string primitive type
        const std::string &String() const
        {
            if( ! m_root )
                throw Error( "Json::String: no data" );

            return m_root->String();
        }


        //! Implicit conversion of numeric primitive data
        //
        //! @exception Error will be thrown if there is no data or this is not a
        //!                  numeric primitive type
        operator double() const
        {
            return Number();
        }

        //! Implicit conversion of string primitive data
        //
        //! @exception Error will be thrown if there is no data or this is not a
        //!                  string primitive type
        operator const std::string&() const
        {
            return String();
        }


        //! Return the value of a numeric primitive as type @a T
        //{{{
        //! RFC 7159 recommends that for interoperability an implementation
        //! should expect numeric primitives to have the precision and range
        //! of an IEEE 754 @c double precision floating point type.  And JSON
        //! itself makes no distinction between integer and floating point
        //! numeric values, to it they are all just the same primitive type.
        //!
        //! However, in any real use, it is likely that values which are strictly
        //! always integers will be encoded and decoded.  This method can be used
        //! for safe conversion of a JSON numeric primitive to any other numeric
        //! type which the software calling it requires.  A compile time error
        //! will occur if the @c double type cannot be @c static_cast to type @a T,
        //! and a runtime exception will be thrown if this is not a JSON numeric
        //! primitive.
        //!
        //! @tparam T   The desired numeric type.
        //!
        //! @note This method hides the normal @c RefCounted::As dynamic cast
        //!       operator, since it has the same semantics and the narrowed
        //!       scope of only applying this operation to numeric primitives
        //!       is appropriate here.  The base class method can still be
        //!       called explicitly if needed by some specialised case though.
        //!
        //! @exception Error will be thrown if there is no data or this is not a
        //!                  numeric primitive type
        //}}}
        template< typename T >
        T As() const
        {
            return static_cast<T>( Number() );
        }

      //@} //}}}

      //! @name Object accessors
      //@{ //{{{

        //! Return a primitive type value for an Object member
        //{{{
        //! This method may be used to obtain a primitive type value for a member
        //! which may or may not exist in the object.  If the requested member
        //! does not exist then a default value for it will be returned instead.
        //!
        //! @tparam T               The type of data to return.
        //!
        //! @param key              The name of the desired member.
        //! @param default_value    The value to return if there is no member
        //!                         with the requested name in this object.
        //!
        //! If the member does exist, its value must be a compatible type to what
        //! has been requested.
        //!
        //! @exception Error will be thrown if this is not an Object, or if the
        //!                  member exists but is not a compatible type to @a T.
        //}}}
        template< typename T >
        T Get( const std::string &key, const T &default_value = T() ) const
        {
            if( ! m_root )
                return default_value;

            return m_root->Get<T>( key, default_value );
        }

        //! Return the value of the Object member named by @a key
        //{{{
        //! This method may be used to query for optional member data that
        //! may not always be present in a particular data structure.
        //!
        //! If the root structure is not an object, or a member with the
        //! requested name does not exist, then a @c NULL handle will be
        //! returned.
        //}}}
        Data::Handle Get( const std::string &key ) const
        { //{{{

            if( ! m_root )
                return NULL;

            return m_root->Get( key );

        } //}}}

        //! Return the value of the Object member named by @a key
        //{{{
        //! This method may be used to retrieve the value of a known or
        //! required member of an object.
        //!
        //! @exception  Error will be thrown if the member does not exist,
        //!             or the root structure is not an object.
        //}}}
        Data::Handle operator[]( const std::string &key ) const
        { //{{{

            if( ! m_root )
                throw Error( "Json::operator[%s]: no data to access", key.c_str() );

            return m_root[key];

        } //}}}

        //! Return the value of the Object member named by @a key
        //{{{
        //! This method may be used to retrieve the value of a known or
        //! required member of an object.
        //!
        //! @exception  Error will be thrown if the member does not exist,
        //!             or the root structure is not an object.
        //}}}
        Data::Handle operator[]( const char *key ) const
        {
            return operator[]( std::string( key ) );
        }


        //! Populate a @a list with the names of all Object members
        //{{{
        //! @exception  Error will be thrown if the root structure is not
        //!             an object.
        //}}}
        void GetMembers( MemberList &list ) const
        { //{{{

            if( ! m_root )
                throw Error( "Json::GetMembers(): no data to access" );

            m_root->GetMembers( list );

        } //}}}

      //@} //}}}

      //! @name Array accessors
      //@{ //{{{

        //! Return a primitive type value for an Array element
        //{{{
        //! This method may be used to obtain a primitive type value for an element
        //! which may or may not exist in the array.  If the requested element does
        //! not exist then a default value for it will be returned instead.
        //!
        //! @tparam T               The type of data to return.
        //!
        //! @param index            The index of the desired element.
        //! @param default_value    The value to return if there is no element
        //!                         at the requested index in this array.
        //!
        //! If the element does exist, its value must be a compatible type to what
        //! has been requested.
        //!
        //! @exception Error will be thrown if this is not an Array, or if the
        //!                  element exists but is not a compatible type to @a T.
        //!
        //! This method is probably less useful than the one which queries for
        //! optional object members, since requesting random array elements is
        //! not generally a very useful thing to do, but we include it for
        //! symmetry since there may be times when a hard error is undesirable
        //! if an expected element may not actually be present.
        //}}}
        template< typename T >
        T Get( unsigned long index, const T &default_value = T() ) const
        {
            if( ! m_root )
                return default_value;

            return m_root->Get<T>( index, default_value );
        }

        //! Return the value of the Array element at @a index
        //{{{
        //! If the root structure is not an array, or an element with the
        //! requested index does not exist, then a @c NULL handle will be
        //! returned.
        //!
        //! This method is probably less useful than the one which queries for
        //! optional object members, since requesting random array elements is
        //! not generally a very useful thing to do, but we include it for
        //! symmetry since there may be times when a hard error is undesirable
        //! if an expected element may not actually be present.
        //}}}
        Data::Handle Get( unsigned long index ) const
        { //{{{

            if( ! m_root )
                return NULL;

            return m_root->Get( index );

        } //}}}


        //! Return the value of the Array element at @a index
        //{{{
        //! This method may be used to retrieve the value of a known or
        //! required element of an array.
        //!
        //! @exception  Error will be thrown if the root structure is not
        //!             an array.
        //! @exception  std::out_of_range will be thrown if the @a index is
        //!             greater than the number of elements it contains.
        //}}}
        Data::Handle operator[]( unsigned long index ) const
        { //{{{

            if( ! m_root )
                throw Error( "Json::operator[%lu]: no data to access", index );

            return m_root[index];

        } //}}}

        //! Return the value of the Array element at @a index
        //{{{
        //! This method may be used to retrieve the value of a known or
        //! required element of an array.
        //!
        //! @exception  Error will be thrown if the root structure is not
        //!             an array.
        //! @exception  std::out_of_range will be thrown if the @a index is
        //!             greater than the number of elements it contains, or
        //!             greater than @c ULONG_MAX.
        //}}}
        Data::Handle operator[]( unsigned long long index ) const
        { //{{{

           #if ULONG_MAX != ULLONG_MAX
            using std::out_of_range;

            if( index > ULONG_MAX )
                throw out_of_range( stringprintf("Json::Array[%llu] index > %lu",
                                                                index, ULONG_MAX) );
           #endif

            return operator[]( static_cast<unsigned long>(index) );

        } //}}}

        //! Return the value of the Array element at @a index
        //{{{
        //! This method may be used to retrieve the value of a known or
        //! required element of an array.
        //!
        //! @exception  Error will be thrown if the root structure is not
        //!             an array.
        //! @exception  std::out_of_range will be thrown if the @a index is
        //!             greater than the number of elements it contains.
        //}}}
        Data::Handle operator[]( unsigned index ) const
        {
            return operator[]( static_cast<unsigned long>(index) );
        }


        //! Return the value of the Array element at @a index
        //{{{
        //! This method may be used to retrieve the value of a known or
        //! required element of an array.
        //!
        //! @exception  Error will be thrown if the root structure is not
        //!             an array.
        //! @exception  std::out_of_range will be thrown if the @a index is
        //!             greater than the number of elements it contains, or
        //!             less than 0.
        //}}}
        Data::Handle operator[]( long index ) const
        { //{{{

            using std::out_of_range;

            if( index < 0 )
                throw out_of_range( stringprintf("Json::Array[%ld] index < 0", index) );

            return operator[]( static_cast<unsigned long>(index) );

        } //}}}

        //! Return the value of the Array element at @a index
        //{{{
        //! This method may be used to retrieve the value of a known or
        //! required element of an array.
        //!
        //! @exception  Error will be thrown if the root structure is not
        //!             an array.
        //! @exception  std::out_of_range will be thrown if the @a index is
        //!             greater than the number of elements it contains, or
        //!             less than 0, or is greater than @c LONG_MAX.
        //}}}
        Data::Handle operator[]( long long index ) const
        { //{{{

           #if LONG_MAX != LLONG_MAX
            using std::out_of_range;

            if( index > LONG_MAX )
                throw out_of_range( stringprintf("Json::Array[%lld] index > %ld",
                                                                index, LONG_MAX) );
           #endif

            return operator[]( long(index) );

        } //}}}

        //! Return the value of the Array element at @a index
        //{{{
        //! This method may be used to retrieve the value of a known or
        //! required element of an array.
        //!
        //! @exception  Error will be thrown if the root structure is not
        //!             an array.
        //! @exception  std::out_of_range will be thrown if the @a index is
        //!             greater than the number of elements it contains, or
        //!             less than 0.
        //}}}
        Data::Handle operator[]( int index ) const
        {
            return operator[]( long(index) );
        }


        //! Return the number of elements in an Array
        //{{{
        //! @exception  Error will be thrown if the root structure is not
        //!             an array.
        //}}}
        size_t GetArraySize() const
        { //{{{

            if( ! m_root )
                throw Error( "Json::GetArraySize(): no data to access" );

            return m_root->GetArraySize();

        } //}}}

      //@} //}}}


      //! @name Output methods
      //@{ //{{{

        //! Return a JSON encoded string of the structures in this parser
        //{{{
        //! @note   The string returned may not be bit identical to the one
        //!         that was originally parsed.  The ordering of members in an
        //!         Object may not be preserved and insignificant whitespace
        //!         may be different.
        //!
        //! @exception  Error will be thrown if there is no data to output.
        //}}}
        std::string JSONStr() const
        {
            if( ! m_root )
                throw Error("JSONStr(): no data to output");

            return m_root->JSONStr();
        }

      //@} //}}}

    }; //}}}


   #ifndef __DOXYGEN_PASS__

    // Specialisations for boolean type data
    template<>
    inline bool Json::Data::Get<bool>( const std::string &key, const bool &default_value ) const
    { //{{{

        if( Type() != ObjectType )
            throw Error( "%s::Get<bool>( %s ) is not an Object type (%s)",
                         EM_TYPEOF(*this), key.c_str(), JSONStr().c_str() );

        Handle  d = Get( key );

        if( ! d )
            return default_value;

        return d->IsTrue();

    } //}}}

    template<>
    inline bool Json::Data::Get<bool>( unsigned long index, const bool &default_value ) const
    { //{{{

        if( Type() != ArrayType )
            throw Error( "%s::Get<bool>( %lu ) is not an Array type (%s)",
                            EM_TYPEOF(*this), index, JSONStr().c_str() );

        Handle  d = Get( index );

        if( ! d )
            return default_value;

        return d->IsTrue();

    } //}}}

   #endif

//!@}

}   // BitB namespace


#endif  // _BB_JSON_H

// vi:sts=4:sw=4:et:foldmethod=marker
