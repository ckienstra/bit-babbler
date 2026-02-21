////////////////////////////////////////////////////////////////////
//
//! @file iniparser.h
//! @ingroup INIParsing
//! @brief Parser for INI formatted data.
//
//  Copyright 2013 - 2021,  Ron <ron@debian.org>
//  This file is distributed as part of the bit-babbler package.
//
////////////////////////////////////////////////////////////////////

#ifndef _BB_INIPARSER_H
#define _BB_INIPARSER_H

#include <bit-babbler/refptr.h>
#include <list>


// We don't hash the Section and Option maps by default right now.
// Most of the structures we are expecting to handle at this stage will all be
// relatively small, so the speed benefit is likely to be minimal (or may even
// be non-existant or negative), and the deterministic ordering of a sorted map
// is more user friendly for data that users might see.
// If we ever need to enable this, we should benchmark it, and then possibly
// consider templating the IniData class to allow both.  For now, keep it simple.
//#define BB_HASH_INIDATA

#ifdef BB_HASH_INIDATA
 #include <bit-babbler/unordered_map>
#else
 #include <map>
#endif


// Enable these if you need low level debug output from IniData.
//#define BB_DEBUG_INIPARSER
//#define BB_DEBUG_INIVALIDATOR

// Allow this to be overridden so the unit tests can send it to stdout.
#ifndef BB_DEBUG_INI_LOGSTREAM
 //! Where to output debug logs.
 #define BB_DEBUG_INI_LOGSTREAM stderr
#endif

//! Debug logging for the parser implementation.
#ifdef BB_DEBUG_INIPARSER
 #define debug_inip(...) fprintf(BB_DEBUG_INI_LOGSTREAM, ##__VA_ARGS__);
#else
 #define debug_inip(...)
#endif

//! Debug logging for the validator implementation.
#ifdef BB_DEBUG_INIVALIDATOR
 #define debug_iniv(...) fprintf(BB_DEBUG_INI_LOGSTREAM, ##__VA_ARGS__);
#else
 #define debug_iniv(...)
#endif


namespace BitB
{

//! @defgroup INIParsing INI data parsing
//! @brief Support for INI formatted data.
//! @ingroup DataStorage
//!@{

    //! Parser and container class for INI format structured data.
    //{{{
    //! The data encoded in this format consists of @b Sections, @b Options and
    //! @b Values.  Since there is no standard definition for this format, the
    //! rules for this particular parser are defined as follows:
    //!
    //! A section definition begins on a line where its name is enclosed in
    //! square brackets.  The section name itself may contain any characters
    //! except square brackets, and they have no intrinsic special meaning
    //! except as a string identifier.  Any characters following the closing
    //! square bracket on the same line will simply be ignored.  While this may
    //! be used to add comments there, doing so should probably be discouraged
    //! as a stylistic idiom, and it may be desirable in the future to be more
    //! strict and reject any trailing 'junk' following a section header.
    //!
    //! All following option/value pairs belong to that section until the next
    //! section header occurs.  Option names may include any characters except
    //! whitespace.  Leading and trailing whitespace around option names and
    //! values is ignored.  Internal whitespace in option values is preserved.
    //! Options must be defined on a single line, and everything (except leading
    //! and trailing whitespace) following the option name up to the end of the
    //! line is part of the value.  Quote characters (of any sort) have no
    //! special meaning to this parser and will be included as a literal part of
    //! the value (individual applications however may apply any special meaning
    //! to any character in the option name or value that they please).  To this
    //! parser, both option names and their values are simply literal strings.
    //!
    //! Comments must appear on their own line, with the first (non-whitespace)
    //! character of the line being '#'.
    //!
    //! For example:
    //! @code
    //! [my-section]
    //!   # Option 1 does something important, this is a comment about it.
    //!   option1    value1
    //! # You may not need option 2.
    //! # option2    value2 3 and4
    //!
    //! [another-section]
    //!   # This option has the same name as one above,
    //!   # but is in a different section.
    //!   option2    some other value
    //!   watchout   1  #note: here '#' is part of the value, not a comment.
    //! @endcode
    //!
    //! Section names must be unique.  Multiple appearances of the same section
    //! name is an error.  When multiple sections of the same type are required,
    //! a sensible convention to use is the form:
    //!
    //! @code
    //! [name:label]
    //! @endcode
    //! Where @c name is the type of the section and @c label is a unique identifier
    //! of the particular instance being defined.  But this is merely a convention,
    //! the ':' character has no special significance beyond what an application
    //! attributes to particular section names.
    //!
    //! Option names must be unique within a section.  If an option name is used
    //! more than once in the same section, then a parsing error will be reported.
    //! Option values are optional.  An option with no value will be assigned the
    //! empty string as its value.
    //!
    //! As an exception to the normal uniqueness rules above, when additional INI
    //! format data is imported with the @c UpdateWith() method, then any sections
    //! or options which overlap with already existing data will be merged with it
    //! without error, allowing such things as reading multiple files with option
    //! settings in the later files overridding any value set previously.  This
    //! should be used with appropriate caution, since it could also allow typos
    //! to 'silently' stomp over some other configuration option accidentally.
    //!
    //! By default, the above rules are the only constraints applied when parsing
    //! the input data, no other limit is placed on what section names will be
    //! acceptable, what options they may contain, or what values those may be
    //! assigned.  Everything the parser reads which is in the correct format will
    //! be imported.  For most application use, some stronger checking will be
    //! desirable, to quickly spot typos in expected section or option names, or
    //! names which simply aren't valid for use in the current version.  And to
    //! sanity check that values are of some expected type or form or range.
    //! The @c Validator class provides a simple way to construct introspective
    //! checks of the data content to ensure that it is valid in more than just
    //! its form as generic INI data.
    //}}}
    class IniData : public RefCounted
    { //{{{
    public:

      //! @name Container types
      //@{ //{{{

        //! Container type for %IniData Options and their values.
       #ifdef EG_HASH_INIDATA
        typedef was_tr1::unordered_map< std::string, std::string >      Options;
       #else
        typedef std::map< std::string, std::string >                    Options;
       #endif

      //@} //}}}

      //! @name Handle type
      //@{ //{{{

        //! Handle type for a @c %IniData parser instance.
        typedef RefPtr< IniData >   Handle;

      //@} //}}}


        //! Container for the options in a single INI section.
        class Section : public RefCounted
        { //{{{
        private:

            std::string     m_name;     //!< The name of this section.
            Options         m_options;  //!< The Options it contains.


        public:

          //! @name Handle type
          //@{ //{{{

            //! Handle type for a @c %Section instance.
            typedef RefPtr< Section >   Handle;

          //@} //}}}

          //! @name Container types
          //@{ //{{{

            //! Container type for IniData Sections.
           #ifdef EG_HASH_INIDATA
            typedef was_tr1::unordered_map< std::string, Handle >   Map;
           #else
            typedef std::map< std::string, Handle >                 Map;
           #endif

          //@} //}}}


          //! @name Constructor
          //@{ //{{{

            //! Construct a new section with the given @a name.
            Section( const std::string &name )
                : m_name( name )
            {}

          //@} //}}}


          //! @name Section construction methods
          //@{ //{{{

            //! Add a new Option to this %Section.
            //{{{
            //! This method will not alter any existing data, it will just define
            //! an additional new Option in this %Section.
            //!
            //! @param option   The identifier name of the new option.
            //! @param value    The value to set for this option.  If not provided,
            //!                 the option's value will be an empty string.
            //!
            //! @exception  Error will be thrown if the @a option is already defined
            //!             in this %Section.
            //}}}
            void AddOption( const std::string &name,
                            const std::string &value = std::string() )
            { //{{{

                if( HasOption( name ) )
                    throw Error( "Duplicated option '%s' in Section '%s'",
                                            name.c_str(), m_name.c_str() );

                m_options[name] = value;

            } //}}}

            //! Change the value of an existing Option.
            //{{{
            //! The option being updated must already exist in this %Section.
            //!
            //! @param option   The identifier name of the option.
            //! @param value    The new value to set for this option.  If not provided,
            //!                 the option's value will be an empty string.
            //!
            //! @exception  Error will be thrown if the @a option is not already
            //!             defined in this %Section.
            //}}}
            void UpdateOption( const std::string &name,
                               const std::string &value = std::string() )
            { //{{{

                if( ! HasOption( name ) )
                    throw Error( "Option '%s' is not defined in Section '%s'",
                                            name.c_str(), m_name.c_str() );

                m_options[name] = value;

            } //}}}

            //! %Set or change the value of an Option.
            //{{{
            //! The option will be added if it does not already exist, else its value
            //! will be updated if it does.
            //!
            //! @param option   The identifier name of the option.
            //! @param value    The value to set for this option.  If not provided,
            //!                 the option's value will be an empty string.
            //}}}
            void AddOrUpdateOption( const std::string &name,
                                    const std::string &value = std::string() )
            {
                m_options[name] = value;
            }

          //@} //}}}

          //! @name Removal methods
          //@{ //{{{

            //! Remove an Option from this @c %Section.
            //{{{
            //! @param name The identifier name of the option to remove.
            //
            //! @return @c true if an option with that name existed in this section
            //!         and was removed.
            //}}}
            bool RemoveOption( const std::string &name )
            {
                return m_options.erase( name ) > 0;
            }

          //@} //}}}

          //! @name Accessor methods
          //@{ //{{{

            //! Return the name of this %Section.
            const std::string &GetName() const
            {
                return m_name;
            }

            //! Return @c true if option @a name is defined in this %Section.
            BB_PURE
            bool HasOption( const std::string &name ) const
            {
                return m_options.find( name ) != m_options.end();
            }

            //! Return the value of option @a name in this %Section.
            //{{{
            //! @exception  Error will be thrown if @a name is not defined in
            //!             this %Section.
            //}}}
            std::string GetOption( const std::string &name ) const
            { //{{{

                Options::const_iterator i = m_options.find( name );

                if( i == m_options.end() )
                    throw Error( "Section '%s' has no option '%s' defined",
                                            m_name.c_str(), name.c_str() );
                return i->second;

            } //}}}

            //! Query the value of option @a name in this %Section.
            //{{{
            //! This method will not throw if the option is not defined in this
            //! %Section, it will return the @a default_value provided for it
            //! instead.
            //!
            //! @param name             The option that a value is wanted for.
            //! @param default_value    A value to return if @a name is not
            //!                         defined in this %Section.
            //}}}
            std::string GetOption( const std::string &name,
                                   const std::string &default_value ) const
            { //{{{

                Options::const_iterator i = m_options.find( name );

                if( i == m_options.end() )
                    return default_value;

                return i->second;

            } //}}}

            //! Return a map of all options in this %Section.
            const Options &GetOptions() const
            {
                return m_options;
            }

          //@} //}}}

          //! @name Output methods
          //@{ //{{{

            //! Return an INI encoded string of this %Section and its Options.
            //{{{
            //! @note   The string returned may not be identical to the one that
            //!         was originally parsed.  The ordering of Options may not
            //!         be preserved and insignificant whitespace may be different.
            //}}}
            std::string INIStr() const
            { //{{{

                std::string     s( '[' + m_name + "]\n" );

                for( Options::const_iterator i = m_options.begin(),
                                             e = m_options.end(); i != e; ++i )
                {
                    if( i->second.empty() )
                        s.append( i->first + '\n' );
                    else
                        s.append( i->first + ' ' + i->second + '\n' );
                }

                return s;

            } //}}}

          //@} //}}}

        }; //}}}


      //! @name Container types
      //@{ //{{{

        //! Container type for %IniData Sections.
        typedef Section::Map    Sections;

      //@} //}}}


        //! Generic support for validating INI Sections and Options.
        //{{{
        //! This class makes it easy to define the set of allowable %Section names,
        //! or patterns of %Section names, and the names of Options and acceptable
        //! values for them in each of those sections.
        //}}}
        class Validator : public RefCounted
        { //{{{
        public:

          //! @name Test method signatures
          //@{ //{{{

            //! Signature type for functions used to match Section names.
            //{{{
            //! Functions with this signature are passed as the @a method parameter
            //! of @c Validator::Section() when creating a %Validator instance.
            //!
            //! @param expect   The string we expect to match against.
            //! @param seen     The string we are checking for a match.
            //!
            //! @return @c true if @a seen is a match to what we @a expect,
            //!         according to whatever criteria the implementation
            //!         intends to apply.
            //!
            //! Implementations of this are not usually expected to ever throw.
            //}}}
            typedef bool(*section_name_test)( const std::string &expect,
                                              const std::string &seen );

            //! Signature type for functions used to check option values.
            //{{{
            //! Functions with this signature are passed as the @a method parameter
            //! of @c OptionList::AddTest() when creating a %Validator instance.
            //!
            //! @param option   The name of the option being checked.
            //! @param value    The value it was assigned.
            //!
            //! @exception  Error is expected to be thrown if the value is
            //!             not acceptable, along with a message suitable
            //!             for presentation to the end-user explaining why.
            //}}}
            typedef void(*option_value_test)( const std::string &option,
                                              const std::string &value );

          //@} //}}}


          //! @name Section name test methods
          //! Standard comparison functions which may be passed as the
          //! @ref section_name_test parameter to Validator::Section().
          //! Alternative test methods may also be provided by other
          //! application code.
          //@{ //{{{

            //! A @ref section_name_test for section names strictly equal to @a expect.
            //{{{
            //! @param expect   The string this test expects to match to.
            //! @param seen     The string we are checking for a match.
            //!
            //! @return @c true if @a seen equals @a expect.
            //}}}
            static bool SectionNameEquals( const std::string &expect, const std::string &seen )
            {
                return expect == seen;
            }

            //! A @ref section_name_test for section names prefixed by @a expect.
            //{{{
            //! @param expect   The string prefix this test expects to match to.
            //! @param seen     The string we are checking for a match.
            //!
            //! @return @c true if @a seen starts with @a expect.
            //}}}
            static bool SectionNamePrefix( const std::string &expect, const std::string &seen )
            {
                return StartsWith( expect, seen );
            }

          //@} //}}}

          //! @name Option value test methods
          //! Standard comparison functions which may be passed as the
          //! @ref option_value_test parameter to OptionList::AddTest().
          //! Alternative test methods may also be provided by other
          //! application code.
          //@{ //{{{

            //! An @ref option_value_test for options which must have some value.
            //{{{
            //! @exception Error will be thrown if @a option does not have a
            //!            non-empty @a value assigned to it.
            //}}}
            static void OptionWithValue( const std::string &option, const std::string &value )
            { //{{{

                if( value.empty() )
                    throw Error( _("Option '%s' expects a value."), option.c_str() );

            } //}}}

            //! An @ref option_value_test for options which must not have a value.
            //{{{
            //! @exception Error will be thrown if @a option has any non-empty
            //!            @a value assigned to it.
            //}}}
            static void OptionWithoutValue( const std::string &option, const std::string &value )
            { //{{{

                if( ! value.empty() )
                    throw Error( _("Option '%s' should not have a value assigned."),
                                                                    option.c_str() );
            } //}}}

            //! An @ref option_value_test for options which may optionally have a value.
            //{{{
            //! Using this test permits validating that the @a option name is acceptable
            //! without placing any (initial) constraint upon its @a value during validation.
            //! It will accept any value, including an empty one.
            //}}}
            static void OptionWithAnyValue( const std::string &option, const std::string &value )
            {
                (void)option; (void)value;
            }

          //@} //}}}


            //! Container for a list of Option validation checks.
            class OptionList : public RefCounted
            { //{{{
            private:

              //! @name Container types
              //@{ //{{{

                //! Container type for Option names and the functions to test their values.
               #ifdef EG_HASH_INIDATA
                typedef was_tr1::unordered_map< std::string, option_value_test >    Tests;
               #else
                typedef std::map< std::string, option_value_test >                  Tests;
               #endif

              //@} //}}}


                //! The map of acceptable Option names to functions for testing their value.
                Tests   m_tests;


            public:

              //! @name Handle type
              //@{ //{{{

                //! Handle type for an @c %OptionList instance.
                typedef RefPtr< OptionList >    Handle;

              //@} //}}}


              //! @name Constructors
              //@{ //{{{

                //! Create a new, empty, %OptionList.
                OptionList() {}

                //! Create a new %OptionList initialised with a single option and its test.
                //{{{
                //! @param option_name  The option to recognise and apply this test to.
                //! @param method       The @ref option_value_test used to determine if
                //!                     the value assigned to this option is valid.
                //!
                //! This is equivalent to:
                //! @code
                //! OptionList().AddTest( option_name, method );
                //! @endcode
                //}}}
                OptionList( const std::string &option_name, option_value_test method )
                {
                    m_tests[option_name] = method;
                }

              //@} //}}}


              //! @name Initialiser methods
              //@{ //{{{

                //! Add (or alter) a test for some valid option name.
                //{{{
                //! If there was a previously existing test for @a option_name, it will
                //! silently be replaced.
                //!
                //! @param option_name  The option to recognise and apply this test to.
                //! @param method       The @ref option_value_test used to determine if
                //!                     the value assigned to this option is valid.
                //!
                //! @return A pointer to this @c %OptionList, so that multiple calls to
                //!         this method may be chained together when defining all the
                //!         valid options allowed in some section.
                //}}}
                OptionList *AddTest( const std::string &option_name, option_value_test method )
                {
                    m_tests[option_name] = method;
                    return this;
                }

              //@} //}}}

              //! @name Validation test methods
              //@{ //{{{

                //! Test that an option name and its value are valid.
                //{{{
                //! @param name     The name of the option to validate.
                //! @param value    The value it was assigned.
                //!
                //! @exception  Error will be thrown if the option name is unknown
                //!             or if the value is not acceptable according to the
                //!             criteria of the test which was specified for it.
                //}}}
                void CheckOption( const std::string &name, const std::string &value ) const
                { //{{{

                    debug_iniv( "   Check option '%s', value '%s'\n",
                                        name.c_str(), value.c_str() );

                    Tests::const_iterator   i = m_tests.find( name );

                    if( i == m_tests.end() )
                        throw Error( "Unknown option '%s'", name.c_str() );

                    debug_iniv( "     Validating '%s'\n", name.c_str() );
                    i->second( name, value );

                } //}}}

              //@} //}}}

            }; //}}}


        private:

            //! Container for the validation checks to be performed for some section.
            class SectionCheck
            { //{{{
            public:

              //! @name Container types
              //@{ //{{{

                //! Container for the list of Section checks used by a @c Validator.
                typedef std::list< SectionCheck >   List;

              //@} //}}}


            private:

                //! The name of the Section(s) this should check.
                std::string         m_name;

                //! How to compare m_name for a match to the actual section name.
                section_name_test   m_test;

                //! The list of Option validation tests for matching Sections.
                OptionList::Handle  m_options;


            public:

              //! @name Constructors
              //@{ //{{{

                //! Create a validation check for some INI Section.
                //{{{
                //! @param name     The string used to check if this set of tests
                //!                 are to be applied to a particular %Section.
                //! @param nametest The method used to compare @a name to the
                //!                 %Section identifier to see if these checks
                //!                 are to be applied.  It may test for a perfect
                //!                 match, or a prefix match, or use any other
                //!                 criteria appropriate to the application.
                //! @param options  A list of the validation tests to be applied
                //!                 to each of the Options in a matching Section.
                //}}}
                SectionCheck( const std::string        &name,
                              section_name_test         test,
                              const OptionList::Handle &options )
                    : m_name( name )
                    , m_test( test )
                    , m_options( options )
                {}

              //@} //}}}


              //! @name Accessors
              //@{ //{{{

                //! Test a Section against these validation criteria.
                //{{{
                //! @param s    The Section to validate.
                //!
                //! @return @c false if the Section identifier does not match this
                //!         set of tests according to the @c section_name_test being
                //!         used and the reference string it is being compared to.
                //!         @c true if the Section identifier was a match and all
                //!         Options it contained validated successfully.
                //!
                //! @exception Error will be thrown if this set of tests were applied
                //!            to the Section, but any Option defined in it failed
                //!            validation.
                //}}}
                bool CheckSection( const Section::Handle &s ) const
                { //{{{

                    // Is this the section we are looking for?
                    if( ! m_test( m_name, s->GetName() ) )
                        return false;

                    debug_iniv( "Checking [%s] with '%s' validator\n",
                                s->GetName().c_str(), m_name.c_str() );


                    // If so, are its options all valid?
                    const Options   &opts = s->GetOptions();

                    try {
                        for( Options::const_iterator i = opts.begin(),
                                                     e = opts.end(); i != e; ++i )
                            m_options->CheckOption( i->first, i->second );
                    }
                    catch( const std::exception &e )
                    {
                        throw Error( _("Section [%s]: %s"),
                                     s->GetName().c_str(), e.what() );
                    }

                    debug_iniv( "   Check [%s] passed\n", s->GetName().c_str() );
                    return true;

                } //}}}

                //! Return the string we are testing section names against.
                const std::string &TestStr() const  { return m_name; }

              //@} //}}}

            }; //}}}


            //! The list of Section validation checks to apply.
            SectionCheck::List  m_sections;


        public:

          //! @name Handle type
          //@{ //{{{

            //! Handle type for a @c %Validator instance.
            typedef RefPtr< Validator >     Handle;

          //@} //}}}


          //! @name Constructors
          //@{ //{{{

            //! Create a new %Validator instance.
            Validator() {}

          //@} //}}}


          //! @name Initialiser methods
          //@{ //{{{

            //! Add tests to validate Section names and the Options they may contain.
            //{{{
            //! @param name     The string used to check if this set of tests
            //!                 are to be applied to a particular %Section.
            //! @param method   The @ref section_name_test used to compare @a name
            //!                 to the %Section identifier to see if these checks
            //!                 are to be applied.  It may test for a perfect
            //!                 match, or a prefix match, or use any other
            //!                 criteria appropriate to the application.
            //! @param options  A list of the validation tests to be applied
            //!                 to each of the Options in a matching Section.
            //}}}
            void Section( const std::string        &name,
                          section_name_test         method,
                          const OptionList::Handle &options )
            {
                m_sections.push_back( SectionCheck( name, method, options ) );
            }

          //@} //}}}

          //! @name Validation test methods
          //@{ //{{{

            //! Test INI @a data against the constraints of this %Validator
            //{{{
            //! @exception  Error will be thrown if validation fails.
            //}}}
            void Validate( const IniData::Handle &data ) const
            { //{{{

                const Sections  &s = data->GetSections();

                debug_iniv( "Validating %zu INI sections\n", s.size() );

                for( Sections::const_iterator i = s.begin(), e = s.end(); i != e; ++i )
                {
                    debug_iniv( "Validate [%s]\n", i->first.c_str() );

                    for( SectionCheck::List::const_iterator ci = m_sections.begin(),
                                                            ce = m_sections.end(); ci != ce; ++ci )
                    {
                        if( ci->CheckSection( i->second ) )
                            goto check_next_section;

                        debug_iniv( "  not matched to '%s' validator\n", ci->TestStr().c_str() );
                    }

                    throw Error( "Unknown section [%s]", i->first.c_str() );

                    check_next_section:
                    ;
                }

                debug_iniv( "Validated %zu INI sections.\n", s.size() );

            } //}}}

            //! Test INI @a data against the constraints of this %Validator
            //{{{
            //! This method will never throw, if there is an error, then a
            //! description of it will be returned in the @a error parameter.
            //!
            //! If no error occurs, then the content of @a error will be untouched.
            //! Sane users will generally want to ensure it is empty before this
            //! method is called, but that is not a hard requirement.  If an error
            //! occurs the content of @a error will be replaced, not appended to.
            //!
            //! @return @c true if there was no error validating the @a data.
            //}}}
            bool Validate( const IniData::Handle &data, std::string &error ) const
            { //{{{

                try {
                    Validate( data );
                    return true;
                }
                catch( const abi::__forced_unwind& ) { throw; }
                catch( const std::exception &e )     { error = e.what(); }
                catch( ... )                         { error = "Unknown exception"; }

                return false;

            } //}}}

          //@} //}}}

        }; //}}}


    private:

        //! All sections mapped by name.
        Sections    m_sections;


      //! @name INI structure parsing
      //@{ //{{{

        //! Return the position of the first non-whitespace byte in @a data after @a pos.
        //{{{
        //! If there is no character that is not insignificant whitespace after
        //! @a pos, then @c std::string::npos will be returned.
        //}}}
        static size_t skip_whitespace( const std::string &data, size_t pos )
        { //{{{

            return data.find_first_not_of(" \t\n\r", pos);

        } //}}}

        //! Return the next line of @a data beginning at @a pos.
        static std::string get_next_line( const std::string &data, size_t &pos )
        { //{{{

            // Trim off any leading whitespace.
            size_t  b = skip_whitespace( data, pos );

            if( b != std::string::npos )
            {
                // Find the next line break.
                size_t  e = data.find_first_of( "\n\r", b );

                // Advance the read pointer to the non-whitespace
                // character which will start the following line.
                pos = skip_whitespace( data, e );

                //debug_inip( "b = %zu, e = %zu, pos = %zu\n", b, e, pos );

                return data.substr( b, e - b );
            }

            pos = b;
            return std::string();

        } //}}}


        //! Parse a line of text containing a Section header.
        Section::Handle parse_section( const std::string &s, bool allow_duplicates )
        { //{{{

            using std::string;

            debug_inip( "begin section: '%s'\n", s.c_str() );

            size_t  n = s.find_first_of(']');

            if( n == string::npos || s.size() < 3 )
                throw Error( "Invalid section '%s'", s.c_str() );

            string  name = s.substr( 1, n - 1 );

            debug_inip( "section name: '%s'\n", name.c_str() );

            if( allow_duplicates )
                return AddOrGetSection( name );

            return AddSection( name );

        } //}}}

        //! Parse a line of text containing an option for @a section.
        void parse_option( const Section::Handle   &section,
                           const std::string       &s,
                           bool                     allow_duplicates )
        { //{{{

            using std::string;

            // Caller already stripped leading whitespace from the line.
            size_t  n1 = s.find_first_of(" \t");            // Find end of option
            size_t  n2 = skip_whitespace( s, n1 );          // Find start of value
            size_t  n3 = s.find_last_not_of(" \t\n\r" );    // Strip trailing whitespace

            debug_inip( "scan option: '%s' -- n1 %zu, n2 %zu, n3 %zu\n",
                                                s.c_str(), n1, n2, n3);

            string  opt = s.substr( 0, n1 );
            string  val = (n2 != string::npos) ? s.substr( n2, n3 - (n2 - 1) ) : string();

            debug_inip( "have option: '%s', value: '%s'\n", opt.c_str(), val.c_str() );

            if( allow_duplicates )
                section->AddOrUpdateOption( opt, val );
            else
                section->AddOption( opt, val );

        } //}}}

        //! Parse a block of INI formatted data.
        void parse( const std::string &data, bool allow_duplicates = false )
        { //{{{

            using std::string;

            Section::Handle     current_section;
            size_t              pos = 0;

            while( pos != string::npos )
            {
                //debug_inip( "pos = %zu\n", pos );

                string  s = get_next_line( data, pos );

                if( s.empty() )
                    continue;

                switch( s[0] )
                {
                    case '#':
                        debug_inip( "skipping comment: '%s'\n", s.c_str() );
                        break;

                    case '[':
                        current_section = parse_section( s, allow_duplicates );
                        break;

                    default:
                        parse_option( current_section, s, allow_duplicates );
                }
            }

        } //}}}

      //@} //}}}


    public:

      //! @name Constructors
      //@{ //{{{

        //! Default constructor for a new empty parser.
        IniData() {}

        //! Construct a new instance, parsing a block of INI @a data from a string.
        //{{{
        //! @exception  Various exceptions may be thrown if the @ data string
        //!             is not a valid INI structure.
        //}}}
        IniData( const std::string &data )
        {
            parse( data );
        }

        //! Construct a new instance, parsing a block of INI @a data from a string.
        //{{{
        //! This constructor will never throw.  Instead, if there is an error,
        //! then a description of it will be returned in the @a error parameter.
        //! If an error is returned, then the content of this parser instance
        //! is undefined and no attempt should be made to access it.
        //!
        //! If no error occurs, then the content of @a error will be untouched.
        //! Sane users will generally want to ensure it is empty before this
        //! method is called, but that is not a hard requirement.  If an error
        //! occurs the content of @a error will be replaced, not appended to.
        //}}}
        IniData( const std::string &data, std::string &error )
        {
            Decode( data, error );
        }

      //@} //}}}


      //! @name Generic container operations
      //@{ //{{{

        //! Erases all data currently held in this parser instance.
        void clear()        { m_sections.clear(); }

        //! Return @c true if this parser contains no data.
        //{{{
        //! It will return @c false if it contains any sections, even if they
        //! have no options defined in them.
        //}}}
        bool empty() const  { return m_sections.empty(); }

      //@} //}}}

      //! @name Initialiser methods
      //@{ //{{{

        //! Decode a new block of INI @a data from a string.
        //{{{
        //! The existing content of this parser, if any, will be replaced by
        //! the new data.
        //!
        //! @exception  Various exceptions may be thrown if the @a data string
        //!             is not a valid INI structure.
        //!
        //! If this method throws an exception, then the parser should be
        //! considered to be in an indefinite state (at present, the options
        //! which were successfully parsed prior to the error will be included
        //! in it, while any following options will not - but applications
        //! should not rely on that behaviour in any way as it is strictly an
        //! implementation detail which could change without warning in some
        //! future revision).
        //}}}
        void Decode( const std::string &data )
        { //{{{

            clear();
            parse( data );

        } //}}}

        //! Decode a new block of INI @a data from a string.
        //{{{
        //! The existing content of this parser, if any, will be replaced by
        //! the new data.
        //!
        //! This method will never throw.  Instead, if there is an error, then
        //! a description of it will be returned in the @a error parameter.
        //! If an error is returned, then the content of this parser instance
        //! is undefined and no attempt should be made to access it.
        //!
        //! If no error occurs, then the content of @a error will be untouched.
        //! Sane users will generally want to ensure it is empty before this
        //! method is called, but that is not a hard requirement.  If an error
        //! occurs the content of @a error will be replaced, not appended to.
        //!
        //! @return @c true if there was no error parsing the @a data.
        //}}}
        bool Decode( const std::string &data, std::string &error )
        { //{{{

            try {
                Decode( data );
                return true;
            }
            catch( const abi::__forced_unwind& ) { throw; }
            catch( const std::exception &e )     { error = e.what(); }
            catch( ... )                         { error = "Unknown exception"; }

            return false;

        } //}}}


        //! Decode a(nother) block of INI @a data from a string.
        //{{{
        //! The existing content of this parser, if any, will @b not be replaced
        //! by the new data, it will simply be added to it in the same way as if
        //! it has been appended to any existing data when that was parsed. This
        //! means that any duplicate %Section names declared in this new @a data
        //! will be considered an error.
        //!
        //! @exception  Various exceptions may be thrown if the @a data string
        //!             is not a valid INI structure.
        //!
        //! If this method throws an exception, then the parser should be
        //! considered to be in an indefinite state (at present, the options
        //! which were successfully parsed prior to the error will be included
        //! in it, while any following options will not - but applications
        //! should not rely on that behaviour in any way as it is strictly an
        //! implementation detail which could change without warning in some
        //! future revision).
        //}}}
        void DecodeMore( const std::string &data )
        {
            parse( data );
        }

        //! Decode a(nother) block of INI @a data from a string.
        //{{{
        //! The existing content of this parser, if any, will @b not be replaced
        //! by the new data, it will simply be added to it in the same way as if
        //! it has been appended to any existing data when that was parsed. This
        //! means that any duplicate %Section names declared in this new @a data
        //! will be considered an error.
        //!
        //! This method will never throw.  Instead, if there is an error, then
        //! a description of it will be returned in the @a error parameter.
        //! If an error is returned, then the content of this parser instance
        //! is undefined and no attempt should be made to access it.
        //!
        //! If no error occurs, then the content of @a error will be untouched.
        //! Sane users will generally want to ensure it is empty before this
        //! method is called, but that is not a hard requirement.  If an error
        //! occurs the content of @a error will be replaced, not appended to.
        //!
        //! @return @c true if there was no error parsing the @a data.
        //}}}
        bool DecodeMore( const std::string &data, std::string &error )
        { //{{{

            try {
                parse( data );
                return true;
            }
            catch( const abi::__forced_unwind& ) { throw; }
            catch( const std::exception &e )     { error = e.what(); }
            catch( ... )                         { error = "Unknown exception"; }

            return false;

        } //}}}


        //! Update the existing options with a block of INI @a data from a string.
        //{{{
        //! The existing content of this parser, if any, will be appended to or
        //! updated by the new data.  It is not an error for it to contain
        //! %Sections and %Options which have been already defined, the new values
        //! will simply replace any old ones which already existed, and add any
        //! which previously did not.
        //!
        //! @exception  Various exceptions may be thrown if the @a data string
        //!             is not a valid INI structure.
        //!
        //! If this method throws an exception, then the parser should be
        //! considered to be in an indefinite state (at present, the options
        //! which were successfully parsed prior to the error will be included
        //! in it, while any following options will not - but applications
        //! should not rely on that behaviour in any way as it is strictly an
        //! implementation detail which could change without warning in some
        //! future revision).
        //}}}
        void UpdateWith( const std::string &data )
        {
            parse( data, true );
        }

        //! Update the existing options with a block of INI @a data from a string.
        //{{{
        //! The existing content of this parser, if any, will be appended to or
        //! updated by the new data.  It is not an error for it to contain
        //! %Sections and %Options which have been already defined, the new values
        //! will simply replace any old ones which already existed, and add any
        //! which previously did not.
        //!
        //! This method will never throw.  Instead, if there is an error, then
        //! a description of it will be returned in the @a error parameter.
        //! If an error is returned, then the content of this parser instance
        //! is undefined and no attempt should be made to access it.
        //!
        //! If no error occurs, then the content of @a error will be untouched.
        //! Sane users will generally want to ensure it is empty before this
        //! method is called, but that is not a hard requirement.  If an error
        //! occurs the content of @a error will be replaced, not appended to.
        //!
        //! @return @c true if there was no error parsing the @a data.
        //}}}
        bool UpdateWith( const std::string &data, std::string &error )
        { //{{{

            try {
                parse( data, true );
                return true;
            }
            catch( const abi::__forced_unwind& ) { throw; }
            catch( const std::exception &e )     { error = e.what(); }
            catch( ... )                         { error = "Unknown exception"; }

            return false;

        } //}}}


        //! Add a new @c %Section.
        //{{{
        //! This method will not alter any existing data, it will just create
        //! an additional new @c Section.
        //!
        //! @param name The identifier for the new section.
        //!
        //! @return A handle to the newly created section.
        //!
        //! @exception  Error will be thrown if @a name is already definied as
        //!             an existing section.
        //}}}
        Section::Handle AddSection( const std::string &name )
        { //{{{

            if( m_sections.find( name ) != m_sections.end() )
                throw Error( "Duplicated section [%s]", name.c_str() );

            m_sections[ name ] = new Section( name );

            return m_sections[name];

        } //}}}


        //! Add a new Option to a @c %Section.
        //{{{
        //! This method will not alter any existing data, it will just define
        //! an additional new Option in some @c Section.
        //!
        //! @param section  A handle to the section that the option is to be
        //!                 added to.
        //! @param option   The identifier name of the new option.
        //! @param value    The value to set for this option.  If not provided,
        //!                 the option's value will be an empty string.
        //!
        //! @note   It is the caller's responsibility to ensure that @a section
        //!         is a valid handle to an existing %Section.
        //!
        //! @exception  Error will be thrown if the @a option is already defined
        //!             in this @a section.
        //}}}
        void AddOption( const Section::Handle  &section,
                        const std::string      &option,
                        const std::string      &value = std::string() )
        {
            section->AddOption( option, value );
        }

        //! Add a new Option to a named @c %Section.
        //{{{
        //! This method will not alter any existing data, it will just define
        //! an additional new Option in some @c Section.
        //!
        //! @param section  The name of the section that the option is to be
        //!                 added to.
        //! @param option   The identifier name of the new option.
        //! @param value    The value to set for this option.  If not provided,
        //!                 the option's value will be an empty string.
        //!
        //! @exception  Error will be thrown if the @a option is already defined
        //!             in this @a section, or if the section does not already
        //!             exist.
        //}}}
        void AddOption( const std::string &section,
                        const std::string &option,
                        const std::string &value = std::string() )
        {
            GetSection( section )->AddOption( option, value );
        }

        //! Change the value of an existing Option.
        //{{{
        //! The option being updated must already exist in the given @a section.
        //!
        //! @param section  A handle to the section where the option is found.
        //! @param option   The identifier name of the option.
        //! @param value    The new value to set for this option.  If not provided,
        //!                 the option's value will be an empty string.
        //!
        //! @note   It is the caller's responsibility to ensure that @a section
        //!         is a valid handle to an existing %Section.
        //!
        //! @exception  Error will be thrown if the @a option is not already
        //!             defined in this @a section.
        //}}}
        void UpdateOption( const Section::Handle   &section,
                           const std::string       &option,
                           const std::string       &value = std::string() )
        {
            section->UpdateOption( option, value );
        }

        //! Change the value of an existing Option.
        //{{{
        //! The option being updated, and the @c Section it is contained in,
        //! must already exist.
        //!
        //! @param section  The name of the section that the option is to be
        //!                 added to.
        //! @param option   The identifier name of the option.
        //! @param value    The new value to set for this option.  If not provided,
        //!                 the option's value will be an empty string.
        //!
        //! @exception  Error will be thrown if the @a option is not already
        //!             defined in this @a section, or if the section does not
        //!             already exist.
        //}}}
        void UpdateOption( const std::string &section,
                           const std::string &option,
                           const std::string &value = std::string() )
        {
            GetSection( section )->UpdateOption( option, value );
        }

        //! %Set or change the value of an Option.
        //{{{
        //! The option will be added if it does not already exist, else its value
        //! will be updated if it does.
        //!
        //! @param section  A handle to the section where the option is found.
        //! @param option   The identifier name of the option.
        //! @param value    The value to set for this option.  If not provided,
        //!                 the option's value will be an empty string.
        //!
        //! @note   It is the caller's responsibility to ensure that @a section
        //!         is a valid handle to an existing %Section.
        //}}}
        void AddOrUpdateOption( const Section::Handle  &section,
                                const std::string      &option,
                                const std::string      &value = std::string() )
        {
            section->AddOrUpdateOption( option, value );
        }

        //! %Set or change the value of an Option.
        //{{{
        //! The option will be added if it does not already exist, else its value
        //! will be updated if it does.  If the section does not already exist,
        //! then it will be created too.
        //!
        //! @param section  The name of the section that the option is to be
        //!                 added to.
        //! @param option   The identifier name of the option.
        //! @param value    The value to set for this option.  If not provided,
        //!                 the option's value will be an empty string.
        //}}}
        void AddOrUpdateOption( const std::string &section,
                                const std::string &option,
                                const std::string &value = std::string() )
        {
            AddOrGetSection( section )->AddOrUpdateOption( option, value );
        }

      //@} //}}}

      //! @name Removal methods
      //@{ //{{{

        //! Remove a @c %Section.
        //{{{
        //! @param name The identifier of the section to remove.
        //!
        //! @return @c true if a section with that name existed and was removed.
        //}}}
        bool RemoveSection( const std::string &name )
        {
            return m_sections.erase( name ) > 0;
        }

        //! Remove an Option from a @c %Section.
        //{{{
        //! @param section  A handle to the section that the option is to be
        //!                 removed from.
        //! @param option   The identifier name of the option to remove.
        //!
        //! @note   It is the caller's responsibility to ensure that @a section
        //!         is a valid handle to an existing %Section.
        //!
        //! @return @c true if an option with that name existed in that section
        //!         and was removed.
        //}}}
        bool RemoveOption( const Section::Handle &section, const std::string &option )
        {
            return section->RemoveOption( option );
        }

        //! Remove an Option from a @c %Section.
        //{{{
        //! @param section  The name of the section that the option is to be
        //!                 removed from.
        //! @param option   The identifier name of the option to remove.
        //!
        //! @return @c true if an option with that name existed in a section
        //!         with that name and was removed.
        //}}}
        bool RemoveOption( const std::string &section, const std::string &option )
        { //{{{

            Sections::const_iterator    i = m_sections.find( section );

            if( i == m_sections.end() )
                return false;

            return i->second->RemoveOption( option );

        } //}}}

      //@} //}}}

      //! @name Accessor methods
      //@{ //{{{

        //! Return a map of all sections.
        const Sections &GetSections() const
        {
            return m_sections;
        }

        //! Return a map of all sections with names matching the given @a prefix.
        //{{{
        //! The keys of the returned map are the trailing portions of the matching
        //! section names not including the prefix.  To get the full section name
        //! you can call the @c Section::GetName() method (if the @a prefix is no
        //! longer available to prepend to the key).
        //}}}
        Sections GetSections( const std::string &prefix ) const
        { //{{{

            Sections    s;
            size_t      n = prefix.size();

            for( Sections::const_iterator i = m_sections.begin(),
                                          e = m_sections.end(); i != e; ++i )
            {
                if( StartsWith( prefix, i->first ) )
                    s[i->first.substr(n)] = i->second;
            }

            return s;

        } //}}}


        //! Return @c true if %Section @a name is defined.
        BB_PURE
        bool HasSection( const std::string &name ) const
        {
            return m_sections.find( name ) != m_sections.end();
        }

        //! Return a handle to %Section @a name.
        //{{{
        //! @param name     The identifier of the requested Section.
        //!
        //! @exception  Error will be thrown if the %Section is not defined.
        //}}}
        Section::Handle GetSection( const std::string &name ) const
        { //{{{

            Sections::const_iterator    i = m_sections.find( name );

            if( i == m_sections.end() )
                throw Error( "Section [%s] is not defined", name.c_str() );

            return i->second;

        } //}}}

        //! Return a handle to %Section @a name.
        //{{{
        //! If the @c Section was not already defined, it will be created and a
        //! handle to the new empty @c %Section structure will be returned.
        //!
        //! @param name     The identifier of the requested Section.
        //}}}
        Section::Handle AddOrGetSection( const std::string &name )
        { //{{{

            if( ! HasSection( name ) )
                return AddSection( name );

            return m_sections[name];

        } //}}}


        //! Return a map of all options defined in @a section.
        //{{{
        //! @note   It is the caller's responsibility to ensure that @a section
        //!         is a valid handle to an existing %Section.
        //}}}
        const Options &GetOptions( const Section::Handle &section ) const
        {
            return section->GetOptions();
        }

        //! Return a map of all options defined in the named @a section.
        //{{{
        //! @exception  Error will be thrown if @a section is not defined.
        //}}}
        const Options &GetOptions( const std::string &section ) const
        {
            return GetSection( section )->GetOptions();
        }


        //! Return @c true if @a option is defined in @a section.
        //{{{
        //! @note   It is the caller's responsibility to ensure that @a section
        //!         is a valid handle to an existing %Section.
        //}}}
        bool HasOption( const Section::Handle &section, const std::string &option ) const
        {
            return section->HasOption( option );
        }

        //! Return @c true if @a option is defined in the named @a section.
        //{{{
        //! This will return @c false if either the section itself is not defined,
        //! or if the option is not defined within it.
        //}}}
        bool HasOption( const std::string &section, const std::string &option ) const
        { //{{{

            Sections::const_iterator    i = m_sections.find( section );

            if( i == m_sections.end() )
                return false;

            return i->second->HasOption( option );

        } //}}}


        //! Return the value of @a option defined in @a section.
        //{{{
        //! @note   It is the caller's responsibility to ensure that @a section
        //!         is a valid handle to an existing %Section.
        //!
        //! @exception  Error will be thrown if @a option is not defined in
        //!             the given @a section.
        //}}}
        std::string GetOption( const Section::Handle   &section,
                               const std::string       &option ) const
        {
            return section->GetOption( option );
        }

        //! Return the value of @a option defined in the named @a section.
        //{{{
        //! @exception  Error will be thrown if @a option is not defined in
        //!             the given @a section, or if the section itself is not
        //!             defined.
        //}}}
        std::string GetOption( const std::string &section,
                               const std::string &option ) const
        {
            return GetSection( section )->GetOption( option );
        }

        //! Query the value of @a option in a @a section.
        //{{{
        //! This method will not throw if the option is not defined in the
        //! given section, it will return the @a default_value provided for
        //! it instead.
        //!
        //! @param section          A handle to the selected Section.
        //! @param option           The option that a value is wanted for.
        //! @param default_value    A value to return if @a option is not
        //!                         defined in the @a section.
        //!
        //! @note   It is the caller's responsibility to ensure that @a section
        //!         is a valid handle to an existing %Section.
        //}}}
        std::string GetOption( const Section::Handle   &section,
                               const std::string       &option,
                               const std::string       &default_value ) const
        {
            return section->GetOption( option, default_value );
        }

        //! Query the value of @a option in the named @a section.
        //{{{
        //! This method will not throw if the option is not defined in the
        //! given section, or if the section itself is not defined.  It will
        //! return the @a default_value provided for it instead.
        //!
        //! @param section          The name of the selected Section.
        //! @param option           The option that a value is wanted for.
        //! @param default_value    A value to return if @a option is not
        //!                         defined in the @a section.
        //}}}
        std::string GetOption( const std::string &section,
                               const std::string &option,
                               const std::string &default_value ) const
        { //{{{

            Sections::const_iterator    i = m_sections.find( section );

            if( i == m_sections.end() )
                return default_value;

            return i->second->GetOption( option, default_value );

        } //}}}

      //@} //}}}

      //! @name Output methods
      //@{ //{{{

        //! Return an INI encoded string of the structures in this parser
        //{{{
        //! @note   The string returned may not be identical to the one that
        //!         was originally parsed.  The ordering of Sections and
        //!         Options may not be preserved and insignificant whitespace
        //!         may be different.
        //}}}
        std::string INIStr() const
        { //{{{

            std::string     s;

            for( Sections::const_iterator i = m_sections.begin(),
                                          e = m_sections.end(); i != e; ++i )
                s.append( i->second->INIStr() + '\n' );

            return s;

        } //}}}

      //@} //}}}

    }; //}}}

//!@}

}   // BitB namespace


// Don't let these leak outside this file, nobody else should use them, and
// we avoid a possible conflict with some exernal dependency included later.
#undef debug_inip
#undef debug_iniv

#endif  // _BB_INIPARSER_H

// vi:sts=4:sw=4:et:foldmethod=marker
