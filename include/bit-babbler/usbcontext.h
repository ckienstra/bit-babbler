//  This file is distributed as part of the bit-babbler package.
//  Copyright 2010 - 2018,  Ron <ron@debian.org>

#ifndef _BB_USBCONTEXT_H
#define _BB_USBCONTEXT_H

#include <bit-babbler/refptr.h>

#if EM_PLATFORM_MSW
 // We don't actually need this (socket support) here, but the crazy windows API
 // requires winsock2.h to be included before windows.h, and the libusb.h header
 // includes the latter - not because it actually needs it, but because they do
 // a horrible hack to avoid horrible windows redefining the word 'interface' ...
 #include <ws2tcpip.h>
#endif

#include LIBUSB_HEADER

#if EM_PLATFORM_LINUX
 #include <linux/usbdevice_fs.h>     // for reading device capabilities
 #include <sys/ioctl.h>
 #include <fcntl.h>
#endif

#include <set>
#include <list>
#include <vector>
#include <bit-babbler/unordered_map.h>
#include <unistd.h>


#ifdef LIBUSBX_API_VERSION
    #define LIBUSB_SINCE(x) ((LIBUSBX_API_VERSION) >= (x))
#elif defined(LIBUSB_API_VERSION)
    #define LIBUSB_SINCE(x) ((LIBUSB_API_VERSION) >= (x))
#else
    #define LIBUSB_SINCE(x) 0
#endif

// We can't use LIBUSB_API_VERSION to test for this, because the version wasn't
// changed when this was added - so it's back to the tried and tested method of
// testing for features in autoconf.  It is a bit fast and loose for the other
// things we are currently wrapping with it too - but at least those will just
// hide some functions that might otherwise be present and not break the build
// like this one did on Wheezy with 1.0.11
#if ! HAVE_LIBUSB_STRERROR
static inline const char * LIBUSB_CALL libusb_strerror(enum libusb_error errcode)
{
    return libusb_error_name(errcode);
}
#endif


namespace BitB
{
    // Exception class for errors from libusb
    class USBError : public Error
    { //{{{
    private:

        libusb_error    m_usberr;


    public:

        BB_PRINTF_FORMAT(3,4)
        USBError( int code, const char *format, ... ) throw()
            : m_usberr( libusb_error(code) )
        {
            va_list         arglist;

            va_start( arglist, format );
            SetMessage( vstringprintf( format, arglist )
                        + ": " + libusb_strerror(m_usberr) );
            va_end( arglist );
        }


        libusb_error GetErrorCode() const { return m_usberr; }

    }; //}}}


    // Manage a libusb context and the devices associated with it
    class USBContext
    { //{{{
    private:

        USBContext( const USBContext& );
        USBContext &operator=( const USBContext& );


    public:

        // A USB vendor and product ID identifier
        //{{{
        // This can be passed as a string of the form VVVV:PPPP, where V and P
        // are the hexadecimal vendor and product IDs respectively.  Either of
        // the parts may be omitted or set to 0, in which case it will signify
        // that part should match any ID (but the ':' must always be included).
        //}}}
        struct ProductID
        { //{{{
        private:

            void parse_id_string( const std::string &id )
            { //{{{

                // Leave it with an 'invalid' ID if this fails.
                vid = 0xFFFF;
                pid = 0xFFFF;

                size_t  n = id.find(':');

                if( n == std::string::npos || id.size() > 9 )
                    throw Error( _("Invalid product ID '%s'"), id.c_str() );

                try {
                    if( n == 0 )
                        vid = 0;

                    else if( (vid = StrToU( id.substr(0, n), 16 )) > 0xFFFF )
                        throw 1;

                } catch( const abi::__forced_unwind& ) {
                    throw;
                } catch( ... ) {
                    throw Error( _("ProductID: invalid vendor ID '%s'"), id.c_str() );
                }

                try {
                    if( n + 1 == id.size() )
                        pid = 0;

                    else if( (pid = StrToU( id.substr(n + 1), 16 )) > 0xFFFF )
                        throw 1;

                } catch( const abi::__forced_unwind& ) {
                    throw;
                } catch( ... ) {
                    throw Error( _("ProductID: invalid product ID '%s'"), id.c_str() );
                }

            } //}}}


        public:

            typedef std::list< ProductID >      List;


            unsigned    vid;    // Vendor ID
            unsigned    pid;    // Product ID


            ProductID()
                : vid( 0 )
                , pid( 0 )
            {}

            ProductID( unsigned vendor, unsigned product )
                : vid( vendor )
                , pid( product )
            {}

            ProductID( const std::string &id )
            {
                parse_id_string( id );
            }


            ProductID &operator=( const std::string &id )
            {
                parse_id_string( id );
                return *this;
            }

            std::string Str() const
            {
                return stringprintf( "%04x:%04x", vid, pid );
            }

        }; //}}}


        // A reference to an individual device
        struct Device : public RefCounted
        { //{{{
        public:

            typedef RefPtr< Device >        Handle;
            typedef std::list< Handle >     List;


            // An identifier that can uniquely indicate an individual device,
            //{{{
            // This can be either its serial number, its bus and device number,
            // or its bus and device port.
            //
            // A device port string is of the form B-P[.P ...]
            // Where B is the bus number and P is the dot separated string of
            // port numbers which follow the topology to the desired device.
            //
            // A device number indication is of the form [B:]N
            // Where B is the bus number and N is the device address on the bus.
            // If the bus part is omitted the ID may not be unique, and the
            // device which will be identified if it is not is unspecified (it
            // will probably be the first match found, but that may not be the
            // same device everywhere this is used).
            //
            // The bus number and device address must be decimal integers.
            //
            // A serial number is an arbitrary string consisting of upper case
            // letter and numbers.  It will not contain either a '-' or ':' and
            // must not be a number < 127 (which ensures it cannot be mistaken
            // for a device address).  It must be at least 4 characters long,
            // (since in practice the current minimum is always 6).
            //}}}
            struct ID
            { //{{{
            private:

                void parse_id_string( const std::string &id )
                { //{{{

                    // busnum can be 0, at least on some platforms like FreeBSD,
                    // but devnum should never be since that is the 'global'
                    // address used during enumeration before one is assigned.

                    // Is the id string a device port?
                    size_t  n = id.find('-');

                    if( n != std::string::npos )
                    {
                        try {
                            if( (busnum = StrToU( id.substr(0, n), 10 )) > 127 )
                                throw 1;

                        } catch( const abi::__forced_unwind& ) {
                            throw;
                        } catch( ... ) {
                            throw Error( _("Device::ID: invalid bus number '%s'"), id.c_str() );
                        }

                        if( n + 1 >= id.size() )
                            throw Error( _("Device::ID: invalid device port '%s'"), id.c_str() );

                        devport = id.substr( n + 1 );

                        return;
                    }


                    // Is it a bus and device address?
                    n = id.find(':');

                    if( n != std::string::npos )
                    {
                        try {
                            if( (busnum = StrToU( id.substr(0, n), 10 )) > 127 )
                                throw 1;

                        } catch( const abi::__forced_unwind& ) {
                            throw;
                        } catch( ... ) {
                            throw Error( _("Device::ID: invalid bus number '%s'"), id.c_str() );
                        }

                        if( n + 1 >= id.size() )
                            throw Error( _("Device::ID: invalid device address '%s'"), id.c_str() );

                        try {
                            devnum = StrToU( id.substr(n + 1), 10 );

                            if( devnum < 1 || devnum > 127 )
                                throw 1;

                        } catch( const abi::__forced_unwind& ) {
                            throw;
                        } catch( ... ) {
                            throw Error( _("Device::ID: invalid device address '%s'"), id.c_str() );
                        }

                        return;
                    }


                    // Is it a device address without a bus number?
                    if( id.size() < 4 )
                    {
                        try {
                            devnum = StrToU( id, 10 );

                            if( devnum < 1 || devnum > 127 )
                                throw 1;

                        } catch( const abi::__forced_unwind& ) {
                            throw;
                        } catch( ... ) {
                            throw Error( _("Device::ID: invalid device address '%s'"), id.c_str() );
                        }

                        return;
                    }


                    // Consider it to be a serial number then.
                    serial = id;

                } //}}}


            public:

                typedef std::list< ID >     List;

                enum IDType
                { //{{{

                    NONE,
                    DEVADDR,    // Logical Bus:Device address
                    DEVPORT,    // Physical Bus-Port.Port.Port location
                    SERIAL      // Device serial number

                }; //}}}


                unsigned        busnum;
                unsigned        devnum;
                std::string     devport;
                std::string     serial;


                ID()
                    : busnum( unsigned(-1) )
                    , devnum( unsigned(-1) )
                {}

                ID( const std::string &id )
                    : busnum( unsigned(-1) )
                    , devnum( unsigned(-1) )
                {
                    parse_id_string( id );
                }


                ID &operator=( const std::string &id )
                { //{{{

                    busnum = unsigned(-1);
                    devnum = unsigned(-1);
                    devport.clear();
                    serial.clear();

                    parse_id_string( id );

                    return *this;

                } //}}}


                // Return true if this ID matches the given Device
                bool Matches( const Device::Handle &d ) const
                { //{{{

                    switch( Type() )
                    {
                        case Device::ID::NONE:
                            break;

                        case Device::ID::DEVADDR:
                            return devnum == d->GetDeviceNumber()
                                && (busnum == unsigned(-1) || busnum == d->GetBusNumber());

                        case Device::ID::DEVPORT:
                            return busnum == d->GetBusNumber()
                                && devport == d->GetDevicePort();

                        case Device::ID::SERIAL:
                            return serial == d->GetSerial();
                    }

                    return false;

                } //}}}


                bool IsDevicePort() const       { return ! devport.empty(); }
                bool IsDeviceAddress() const    { return devnum != unsigned(-1); }
                bool IsSerialNumber() const     { return ! serial.empty(); }

                IDType Type() const
                { //{{{

                    if( IsSerialNumber() )
                        return SERIAL;

                    if( IsDeviceAddress() )
                        return DEVADDR;

                    if( IsDevicePort() )
                        return DEVPORT;

                    return NONE;

                } //}}}

                std::string Str() const
                { //{{{

                    switch( Type() )
                    {
                        case NONE:
                            return "No device selected";

                        case DEVADDR:
                            if( busnum != unsigned(-1) )
                                return stringprintf( "Bus:Device %03u:%03u", busnum, devnum );

                            return stringprintf( "Bus:Device *:%03u", devnum );

                        case DEVPORT:
                            return stringprintf( "Port %u-%s", busnum, devport.c_str() );

                        case SERIAL:
                            return "Serial '" + serial + "'";
                    }

                    return std::string();

                } //}}}

            }; //}}}


            // We keep a minimal cache of the device configration, with only
            // the things that are actually interesting to us somewhere.
            // We do at least need to know wMaxPacketSize for the endpoints
            // that we use, and the endpoint address(es).
            struct Endpoint
            { //{{{

                typedef std::vector< Endpoint >     Vector;
                typedef std::set< uint8_t >         AddressSet;


                // Returns:
                // LIBUSB_ENDPOINT_IN  for a device -> host endpoint
                // LIBUSB_ENDPOINT_OUT for a host -> device endpoint
                static libusb_endpoint_direction Direction( uint8_t addr )
                { //{{{

                    if( addr & 0x80 )
                        return LIBUSB_ENDPOINT_IN;

                    return LIBUSB_ENDPOINT_OUT;

                } //}}}


                uint16_t    wMaxPacketSize;
                uint8_t     bEndpointAddress;


                Endpoint( const libusb_endpoint_descriptor &ep )
                    : wMaxPacketSize( ep.wMaxPacketSize )
                    , bEndpointAddress( ep.bEndpointAddress )
                {}


                // Return the endpoint number.
                unsigned GetNumber() const
                {
                    return bEndpointAddress & 0x0f;
                }

                libusb_endpoint_direction GetDirection() const
                {
                    return Direction( bEndpointAddress );
                }


                std::string Str() const
                {
                    return stringprintf( "Endpoint %u %s, address 0x%02x, max packet %u",
                                         GetNumber(),
                                         GetDirection() == LIBUSB_ENDPOINT_IN ? " In" : "Out",
                                         bEndpointAddress, wMaxPacketSize );
                }

            }; //}}}

            struct AltSetting
            { //{{{

                typedef std::vector< AltSetting >   Vector;

                // USB Endpoints are numbered from 1, so endpoint[0] here should be Endpoint 1.
                Endpoint::Vector    endpoint;


                AltSetting( const libusb_interface_descriptor &alt )
                { //{{{

                    for( size_t i = 0; i < alt.bNumEndpoints; ++i )
                    {
                        #if 0
                        // The endpoint addresses aren't guaranteed to be sequential.
                        // I have at least one device here with only Endpoint 1 and
                        // Endpoint 3 (on separate interfaces).
                        if( (alt.endpoint[i].bEndpointAddress & 0x0f) != i + 1 )
                            throw Error( _("Interface %u AltSetting %u: endpoint %zu has address 0x%02x"),
                                                alt.bInterfaceNumber, alt.bAlternateSetting, i + 1,
                                                alt.endpoint[i].bEndpointAddress );
                        #endif

                        endpoint.push_back( Endpoint( alt.endpoint[i] ) );
                    }

                } //}}}


                void GetEndpointAddresses( Endpoint::AddressSet &a ) const
                { //{{{

                    for( Endpoint::Vector::const_iterator i = endpoint.begin(),
                                                          e = endpoint.end(); i != e; ++i )
                        a.insert( i->bEndpointAddress );

                } //}}}


                std::string Str() const
                { //{{{

                    std::string     s;
                    for( Endpoint::Vector::const_iterator i = endpoint.begin(),
                                                          e = endpoint.end(); i != e; ++i )
                        s.append( "     - " + i->Str() + "\n" );

                    return s;

                } //}}}

            }; //}}}

            struct Interface
            { //{{{

                typedef std::vector< Interface >    Vector;

                // USB AlternateSettings are numbered from 0, so alt[0] should be AlternateSetting 0.
                AltSetting::Vector  alt;
                uint8_t             bInterfaceNumber;


                Interface( const libusb_interface &iface )
                    : bInterfaceNumber( iface.num_altsetting > 0
                                        ? iface.altsetting[0].bInterfaceNumber
                                        : uint8_t(-1) )
                { //{{{

                    for( int i = 0; i < iface.num_altsetting; ++i )
                    {
                        if( iface.altsetting[i].bAlternateSetting != i )
                            throw Error( _("Interface %u AltSetting %u: has alt value %u"),
                                                iface.altsetting[i].bInterfaceNumber, i,
                                                iface.altsetting[i].bAlternateSetting );

                        alt.push_back( AltSetting( iface.altsetting[i] ) );
                    }

                } //}}}


                void GetEndpointAddresses( Endpoint::AddressSet &a ) const
                { //{{{

                    // Ideally this should probably only recurse into the current
                    // alt setting, but libusb doesn't appear to be able to get
                    // that for us.  It can set it, but not query it (portably).
                    for( AltSetting::Vector::const_iterator i = alt.begin(),
                                                            e = alt.end(); i != e; ++i )
                        i->GetEndpointAddresses( a );

                } //}}}

                const AltSetting &GetAltSetting( uint8_t bAlternateSetting ) const
                { //{{{

                    if( __builtin_expect( alt.size() <= bAlternateSetting, 0 ) )
                        throw Error( _("Interface %u has no alt setting %u"),
                                        bInterfaceNumber, bAlternateSetting );

                    return alt[bAlternateSetting];

                } //}}}


                std::string Str() const
                { //{{{

                    std::string     s;
                    for( size_t i = 0, e = alt.size(); i < e; ++i )
                        s.append( stringprintf( "   - AltSetting %zu\n", i ) )
                         .append( alt[i].Str() );

                    return s;

                } //}}}

            }; //}}}

            struct Config
            { //{{{

                typedef std::vector< Config >   Vector;


                // Populate an array with the USB Configurations for a device.
                template< typename Array >
                static void Get( Array                          &configs,
                                 libusb_device                  *dev,
                                 const libusb_device_descriptor &desc )
                { //{{{

                    // USB Configurations are numbered from 1, so configs[0] should be Configuration 1.
                    configs.clear();

                    ScopedCancelState   cancelstate;

                    for( size_t i = 0; i < desc.bNumConfigurations; ++i )
                    {
                        libusb_config_descriptor    *c;

                        int ret = libusb_get_config_descriptor( dev, uint8_t(i), &c );
                        if( ret )
                            throw USBError( ret, _("USBContext::Device::Config: "
                                                   "failed to get configuration %zu descriptor"), i );

                        if( c->bConfigurationValue != i + 1 )
                        {
                            uint8_t val = c->bConfigurationValue;

                            libusb_free_config_descriptor( c );
                            throw Error( _("Configuration %zu: has configuration number %u"),
                                                                                i + 1, val );
                        }

                        configs.push_back( Config( c ) );

                        libusb_free_config_descriptor( c );
                    }

                } //}}}

                // Dump the contents of an array of USB Configurations.
                template< typename Array >
                static std::string Dump( const Array &configs )
                { //{{{

                    std::string     s;
                    for( size_t i = 0, e = configs.size(); i < e; ++i )
                        s.append( stringprintf( "Configuration %zu\n", i + 1 ) )
                         .append( configs[i].Str() );

                    return s;

                } //}}}


                // USB Interfaces are numbered from 0, so interface[0] should be Interface 0.
                Interface::Vector   interface;
                uint8_t             bConfigurationValue;


                Config( libusb_config_descriptor *c )
                    : bConfigurationValue( c->bConfigurationValue )
                { //{{{

                    try {
                        for( size_t i = 0; i < c->bNumInterfaces; ++i )
                        {
                            if( c->interface[i].altsetting[0].bInterfaceNumber != i )
                                throw Error( _("Configuration %u Interface %zu: has interface number %u"),
                                                        bConfigurationValue, i,
                                                        c->interface[i].altsetting[0].bInterfaceNumber );

                            interface.push_back( Interface( c->interface[i] ) );
                        }
                    }
                    catch( const std::exception &e )
                    {
                        throw Error( _("Configuration %u: %s"), bConfigurationValue, e.what() );
                    }

                } //}}}


                void GetEndpointAddresses( Endpoint::AddressSet &a ) const
                { //{{{

                    for( Interface::Vector::const_iterator i = interface.begin(),
                                                           e = interface.end(); i != e; ++i )
                        i->GetEndpointAddresses( a );

                } //}}}

                const Interface &GetInterface( uint8_t bInterfaceNumber ) const
                { //{{{

                    if( __builtin_expect( interface.size() <= bInterfaceNumber, 0 ) )
                        throw Error( _("Configuration %u has no interface %u"),
                                        bConfigurationValue, bInterfaceNumber );

                    return interface[bInterfaceNumber];

                } //}}}


                std::string Str() const
                { //{{{

                    std::string     s;
                    for( size_t i = 0, e = interface.size(); i < e; ++i )
                        s.append( stringprintf( " - Interface %zu\n", i ) )
                         .append( interface[i].Str() );

                    return s;

                } //}}}

            }; //}}}


            // Scoped container for open device handles
            class Open : public RefCounted
            { //{{{

                // Let Device get at the private constructor
                friend struct Device;

            private:

                typedef std::set< uint8_t >                             ClaimSet;
                typedef was_tr1::unordered_map< uint8_t, uint8_t >      AltMap;

                Device::Handle          m_device;
                libusb_device_handle   *m_handle;
                ClaimSet                m_claims;
                AltMap                  m_altmap;


                void do_open( libusb_device *dev )
                { //{{{

                    ScopedCancelState   cancelstate;

                    int ret = libusb_open( dev, &m_handle );
                    if( ret < 0 )
                        throw USBError( ret, _("Device::Open failed") );

                } //}}}

                void release_interface( uint8_t n )
                { //{{{

                    int ret = libusb_release_interface( m_handle, n );
                    if( ret < 0 )
                        LogUSBError<2>( ret, _("Device::Open( %s ): failed to release interface %u"),
                                                                    m_device->IDStr().c_str(), n );
                } //}}}

                void release_claims()
                { //{{{

                    for( ClaimSet::iterator i = m_claims.begin(), e = m_claims.end(); i != e; ++i )
                        release_interface( *i );

                    m_claims.clear();
                    m_altmap.clear();

                } //}}}


                // This one is only for use in the Device constructor, where
                // we can't safely take a Handle to a partly constructed Device.
                Open( libusb_device *dev, const Device *d )
                { //{{{

                    do_open( dev );
                    Log<3>( "+ Device::Open( %p %03u:%03u )\n", m_handle,
                                                d->m_busnum, d->m_devnum );
                } //}}}


            public:

                typedef RefPtr< Open >      Handle;

                Open( const Device::Handle &d )
                    : m_device( d )
                {
                    do_open( m_device->m_dev );
                    Log<3>( "+ Device::Open( %s )\n", m_device->IDStr().c_str() );
                }

                ~Open()
                { //{{{

                    if( __builtin_expect( ! m_device, 0 ) )
                        Log<3>( "- Device::Open( %p )\n", m_handle );
                    else
                        Log<3>( "- Device::Open( %s )\n", m_device->IDStr().c_str() );

                    ScopedCancelState   cancelstate;

                    release_claims();
                    libusb_close(m_handle);

                } //}}}


                // We cannot claim interfaces if the device is bound to another driver.
                void ForceDetach( uint8_t bInterfaceNumber )
                { //{{{

                    ScopedCancelState   cancelstate;

                    int ret = libusb_detach_kernel_driver( m_handle, bInterfaceNumber );
                    if( ret )
                        throw USBError( ret, _("Device( %s ): failed to detach interface %u"),
                                                m_device->IDStr().c_str(), bInterfaceNumber );

                    Log<1>( "Detached interface %u of %s\n", bInterfaceNumber,
                                                             m_device->IDStr().c_str() );
                } //}}}


                // This may be called whether we've claimed any interfaces or not.
                //{{{
                // If the device configuration cannot be restored, then the device
                // may be disconnected and reconnected, in which case this will then
                // throw a USBError with LIBUSB_ERROR_NOT_FOUND set, and this handle
                // will no longer be valid. If hotplug is enabled, the device should
                // be re-enumerated in that case. If not, you may need to rescan the
                // bus to find it again.
                //}}}
                void SoftReset()
                { //{{{

                    ScopedCancelState   cancelstate;

                    int ret = libusb_reset_device( m_handle );
                    if( ret )
                        throw USBError( ret, _("Device( %s ): SoftReset failed"),
                                                    m_device->IDStr().c_str() );

                    Log<1>( "Reset %s\n", m_device->IDStr().c_str() );

                } //}}}


                // This cannot be called on a device that is already claimed.
                void SetConfiguration( uint8_t bConfigurationValue )
                { //{{{

                    ScopedCancelState   cancelstate;

                    int ret = libusb_set_configuration( m_handle, bConfigurationValue );
                    if( ret < 0 )
                        throw USBError( ret, _("Device( %s ): failed to set configuration %u"),
                                                m_device->IDStr().c_str(), bConfigurationValue );
                } //}}}

                // Return the currently active bConfigurationValue.
                uint8_t GetConfiguration()
                { //{{{

                    ScopedCancelState   cancelstate;
                    int                 config;

                    int ret = libusb_get_configuration( m_handle, &config );
                    if( ret )
                        throw USBError( ret, _("Device( %s ): failed to get current configuration"),
                                                                        m_device->IDStr().c_str() );

                    // Valid USB bConfigurationValue starts at 1.
                    if( __builtin_expect( config < 1 || config > 255, 0 ) )
                        throw Error( _("Device( %s ): invalid current config (1 < %d < 256)"),
                                                        m_device->IDStr().c_str(), config );
                    return uint8_t(config);

                } //}}}


                void ClaimInterface( uint8_t bInterfaceNumber )
                { //{{{

                    ScopedCancelState   cancelstate;

                    int ret = libusb_claim_interface( m_handle, bInterfaceNumber );
                    if( ret < 0 )
                        throw USBError( ret, _("Device( %s ): failed to claim interface %u"),
                                                m_device->IDStr().c_str(), bInterfaceNumber );

                    m_claims.insert( bInterfaceNumber );

                } //}}}

                void ClaimAllInterfaces()
                { //{{{

                    ScopedCancelState       cancelstate;
                    uint8_t                 bConfigurationValue = GetConfiguration();
                    const Device::Config   &c = m_device->GetConfiguration( bConfigurationValue );

                    for( Interface::Vector::const_iterator i = c.interface.begin(),
                                                           e = c.interface.end(); i != e; ++i )
                    {
                        try {
                            ClaimInterface( i->bInterfaceNumber );
                        }
                        catch( ... )
                        {
                            release_claims();
                            throw;
                        }
                    }

                } //}}}

                void ReleaseInterface( uint8_t bInterfaceNumber )
                { //{{{

                    ScopedCancelState   cancelstate;

                    release_interface( bInterfaceNumber );
                    m_claims.erase( bInterfaceNumber );
                    m_altmap.erase( bInterfaceNumber );

                } //}}}

                void ReleaseAllInterfaces()
                { //{{{

                    ScopedCancelState   cancelstate;

                    release_claims();

                } //}}}


                // We must hold the claim to the device interface to call this.
                void SetAltInterface( uint8_t bInterfaceNumber, uint8_t bAlternateSetting )
                { //{{{

                    ScopedCancelState   cancelstate;

                    int ret = libusb_set_interface_alt_setting( m_handle, bInterfaceNumber,
                                                                          bAlternateSetting );
                    if( ret < 0 )
                        throw USBError( ret, _("Device( %s ): failed to set interface %u, alt %u"),
                                                    m_device->IDStr().c_str(), bInterfaceNumber,
                                                                               bAlternateSetting );
                    m_altmap[bInterfaceNumber] = bAlternateSetting;

                } //}}}


                // If the endpoint_address isn't specified explicitly, try to clear
                // a stall from all endpoints of the currently claimed interface(s).
                //
                // We must hold the claim to the device interface to call this.
                void ClearHalt( unsigned endpoint_address = 0x100 )
                { //{{{

                    ScopedCancelState   cancelstate;

                    if( endpoint_address == 0x100 )
                    {
                        Endpoint::AddressSet    a;
                        uint8_t                 bConfigurationValue = GetConfiguration();
                        const Device::Config   &c = m_device->GetConfiguration( bConfigurationValue );

                        for( ClaimSet::iterator i = m_claims.begin(),
                                                e = m_claims.end(); i != e; ++ i )
                        {
                            uint8_t alt = 0;

                            if( m_altmap.find( *i ) != m_altmap.end() )
                                alt = m_altmap[ *i ];

                            c.GetInterface( *i ).GetAltSetting( alt ).GetEndpointAddresses( a );
                        }

                        for( Endpoint::AddressSet::iterator i = a.begin(), e = a.end(); i != e; ++i )
                            ClearHalt( *i );

                        return;
                    }

                    int ret = libusb_clear_halt( m_handle, uint8_t(endpoint_address) );
                    if( ret )
                        throw USBError( ret, _("Device( %s ): ClearHalt failed for endpoint %02x"),
                                                    m_device->IDStr().c_str(), endpoint_address );

                    Log<1>( "Device( %s ): cleared halt on endpoint %02x\n",
                                m_device->IDStr().c_str(), endpoint_address );
                } //}}}


                operator libusb_device_handle*()
                {
                    return m_handle;
                }

            }; //}}}


        private:

            //{{{
            // This is semi-arbitrarily chosen, since the real limit is not just
            // OS dependent, but driver and controller dependent too.  It's big
            // enough that we'll need to do something very different to what we
            // currently are before we ever hit it, and is smaller than most of
            // the modern OS and driver limits that I could find documented.
            //
            // The main practical constraint is that this should be a multiple
            // of wMaxPacketSize, but any sensible value already always will be.
            //}}}
            static const size_t DEFAULT_MAX_TRANSFER_SIZE = 1024 * 1024;

            libusb_device      *m_dev;
            Config::Vector      m_configs;
            size_t              m_maxtransfer;

            unsigned            m_vendorid;
            unsigned            m_productid;
            unsigned            m_busnum;
            unsigned            m_devnum;
            std::string         m_mfg;
            std::string         m_product;
            std::string         m_serial;
            std::string         m_devport;

            std::string         m_devpath;


            std::string get_string( const Open::Handle &dev, uint8_t idx )
            { //{{{

                if( idx == 0 )
                    return std::string();

                ScopedCancelState   cancelstate;
                unsigned char       s[128];
                unsigned            retries = 0;

            try_again:

                int ret = libusb_get_string_descriptor_ascii( *dev, idx, s, sizeof(s) );
                if( ret < 0 )
                {
                    if( ++retries <= 3 )
                    {
                        switch( ret )
                        {
                            case LIBUSB_ERROR_PIPE:
                                // A control endpoint can't really stall, but it may still
                                // return this if some other error occurs, so just try again.
                                LogUSBError<1>( ret, _("USB Device( %03u:%03u ): "
                                                       "failed to get string descriptor %u "
                                                       "on attempt %u, retrying"),
                                                        m_busnum, m_devnum, idx, retries );
                                goto try_again;

                            case LIBUSB_ERROR_TIMEOUT:
                            case LIBUSB_ERROR_OTHER:
                                LogUSBError<1>( ret, _("USB Device( %03u:%03u ): "
                                                       "failed to get string descriptor %u "
                                                       "on attempt %u, resetting device"),
                                                        m_busnum, m_devnum, idx, retries );

                                // We can't call dev->SoftReset here, because we're still in
                                // the Device constructor, and it wants to access m_device,
                                // so just do it the old fashioned way here.
                                ret = libusb_reset_device( *dev );
                                if( ret )
                                    throw USBError( ret, _("USB Device( %03u:%03u ): reset failed"),
                                                                            m_busnum, m_devnum );
                                goto try_again;

                            default:
                                break;
                        }
                    }

                    LogUSBError<1>( ret, _("USB Device( %03u:%03u ): "
                                           "failed to get string descriptor %u"),
                                                        m_busnum, m_devnum, idx );
                    return std::string();
                }

                s[ret] = '\0';
                return reinterpret_cast<char*>(s);

            } //}}}

            void get_device_config( const libusb_device_descriptor &desc )
            { //{{{

                ScopedCancelState   cancelstate;
                bool                fetch_strings = true;

               #if HAVE_LIBUSB_GET_PORT_NUMBERS

                uint8_t     ports[8];
                int         ret = libusb_get_port_numbers( m_dev, ports, sizeof(ports) );

                if( ret > 0 )
                {
                    m_devport = stringprintf("%d", ports[0]);

                    for( int i = 1; i < ret; ++i )
                        m_devport += stringprintf(".%d", ports[i]);
                }
                else if( ret < 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED )
                    throw USBError( ret, _("USB Device( %03u:%03u ): failed to get port numbers"),
                                                                            m_busnum, m_devnum );
               #endif


               #if EM_PLATFORM_LINUX

                // Prior to Linux 3.3, usbfs had a somewhat arbitrary limit of 16kB
                //{{{
                // on the size of a bulk URB.  For transfers larger than that, libusb
                // would try to hack around that limit by splitting them into smaller
                // blocks and submitting multiple URBs together.  For extra fun, they
                // try to grep a kernel version out of what uname(2) returns, and if
                // it looks like 2.6.32 or later, they unconditionally enable the use
                // of bulk continuation ...  Which would be great, except that it is
                // utterly broken with USB3 XHCI controllers since they don't stop on
                // short packets regardless of whether USBDEVFS_URB_SHORT_NOT_OK is
                // set or not.
                //
                // The USBDEVFS_GET_CAPABILITIES ioctl was added in Linux 3.6, and
                // correctly announces that USBDEVFS_CAP_BULK_CONTINUATION is not
                // available for XHCI controllers.  So unless we have that available
                // to query (and therefore libusb does as well), the only safe thing
                // we can do is limit transfers from our side to the 16kB limit.
                //
                // For bonus fun, libusb itself didn't support that until 1.0.13-rc1
                // which fortunately was the same release where LIBUSBX_API_VERSION
                // was added, so since Debian Wheezy still has 1.0.11, and could be
                // running a backported kernel, check for that too.
                //
                // For extra bonus fun, RHEL/CentOS 6 backported that ioctl to their
                // 2.6.32 kernel, but didn't pull in the scatter-gather patch that
                // was committed to the mainline kernel along with it, or a recent
                // enough libusb to actually use it (they shipped with 1.0.9), so we
                // need to work around their mongrel kernel, to cater for the case of
                // users installing a later libusb(x) version on those systems.
                // Which is a thing people really do to use later software there.
                //}}}
                m_maxtransfer = 16384;


                #if defined(USBDEVFS_GET_CAPABILITIES) && LIBUSB_SINCE(0x01000100)

                std::string     usbfs_path = stringprintf("/dev/bus/usb/%03u/%03u",
                                                                m_busnum, m_devnum );
                int             fd = open( usbfs_path.c_str(), O_RDWR );

                if( fd < 0 )
                {
                    LogErr<1>( _("USBContext::Device failed to open %s"),
                                                    usbfs_path.c_str() );

                    // If this fails, there's no point trying to read the string
                    // descriptors because doing that needs to open the same device
                    // in the same way too, at least in libusb up to 1.0.20 anyway.
                    fetch_strings = false;

                } else {

                    uint32_t    devcaps;
                    int         ret = ioctl( fd, USBDEVFS_GET_CAPABILITIES, &devcaps );

                    if( ret < 0 )
                    {
                        LogErr<1>( _("Device %03u:%03u failed to get capabilities"),
                                                                m_busnum, m_devnum );
                    } else {

                        Log<2>( _("Device %03u:%03u has capabilities 0x%02x\n"),
                                                    m_busnum, m_devnum, devcaps );

                        #ifdef USBDEVFS_CAP_BULK_SCATTER_GATHER
                        // Any of these capabilities should be enough to allow
                        // safely relaxing the 16kB transfer limit on our side.
                        if( devcaps & (USBDEVFS_CAP_BULK_CONTINUATION |
                                       USBDEVFS_CAP_NO_PACKET_SIZE_LIM |
                                       USBDEVFS_CAP_BULK_SCATTER_GATHER) )
                        #else
                        // But RHEL 6 backported USBDEVFS_GET_CAPABILITIES
                        // without pulling in the scatter-gather patch ...
                        if( devcaps & (USBDEVFS_CAP_BULK_CONTINUATION |
                                       USBDEVFS_CAP_NO_PACKET_SIZE_LIM) )
                        #endif
                            m_maxtransfer = DEFAULT_MAX_TRANSFER_SIZE;
                    }
                    close( fd );
                }

                #endif

               #endif


                if( fetch_strings ) try
                {
                    // We can't use OpenDevice here - we're currently still in the
                    // Device constructor so if this Unref's it the world will end.
                    Open::Handle    h = new Open( m_dev, this );

                    m_mfg       = get_string( h, desc.iManufacturer );
                    m_product   = get_string( h, desc.iProduct );
                    m_serial    = get_string( h, desc.iSerialNumber );
                }
                BB_CATCH_ALL( 1, _("USBContext::Device failed to read string data") )


                try {
                    Config::Get( m_configs, m_dev, desc );
                }
                catch( const std::exception &e )
                {
                    throw Error( _("Device %s: %s"), IDStr().c_str(), e.what() );
                }

            } //}}}


        public:

            Device( libusb_device *dev )
                : m_dev( dev )
                , m_maxtransfer( DEFAULT_MAX_TRANSFER_SIZE )
                , m_busnum( libusb_get_bus_number(m_dev) )
                , m_devnum( libusb_get_device_address(m_dev) )
            { //{{{

                Log<2>( "+ Device( %03u:%03u )\n", m_busnum, m_devnum );

                ScopedCancelState           cancelstate;
                libusb_device_descriptor    desc;

                int ret = libusb_get_device_descriptor( m_dev, &desc );
                if( ret < 0 )
                    throw USBError( ret, _("Device( %03u:%03u ): failed to get descriptor"),
                                                                        m_busnum, m_devnum );

                m_vendorid  = desc.idVendor;
                m_productid = desc.idProduct;
                get_device_config( desc );
                libusb_ref_device( m_dev );

            } //}}}

            Device( libusb_device *dev, const libusb_device_descriptor &desc )
                : m_dev( dev )
                , m_maxtransfer( DEFAULT_MAX_TRANSFER_SIZE )
                , m_vendorid( desc.idVendor )
                , m_productid( desc.idProduct )
                , m_busnum( libusb_get_bus_number(m_dev) )
                , m_devnum( libusb_get_device_address(m_dev) )
            { //{{{

                Log<2>( "+ Device( %03u:%03u )\n", m_busnum, m_devnum );

                ScopedCancelState   cancelstate;

                get_device_config( desc );
                libusb_ref_device( m_dev );

            } //}}}

            ~Device()
            { //{{{

                Log<2>( "- Device( %03u:%03u )\n", m_busnum, m_devnum );

                ScopedCancelState   cancelstate;
                libusb_unref_device( m_dev );

            } //}}}


            bool operator==( const Device &d ) const
            {
                return m_busnum == d.m_busnum && m_devnum == d.m_devnum;
            }


            Open::Handle OpenDevice()
            {
                return new Open( this );
            }


            // Device info accessors
            //{{{

            void SetManufacturer( const std::string &id )   { m_mfg = id; }
            void SetProduct( const std::string &id )        { m_product = id; }
            void SetSerial( const std::string &id )         { m_serial = id; }

            void SetDevicePort( const std::string &str )    { m_devport = str; }
            void SetDevpath( const std::string &str )       { m_devpath = str; }


            unsigned GetVendorID() const                    { return m_vendorid; }
            unsigned GetProductID() const                   { return m_productid; }
            const std::string &GetManufacturer() const      { return m_mfg; }
            const std::string &GetProduct() const           { return m_product; }
            const std::string &GetSerial() const            { return m_serial; }

            unsigned GetBusNumber() const                   { return m_busnum; }
            unsigned GetDeviceNumber() const                { return m_devnum; }
            const std::string &GetDevicePort() const        { return m_devport; }
            const std::string &GetDevpath() const           { return m_devpath; }

            unsigned GetNumConfigurations() const           { return unsigned(m_configs.size()); }
            const Config::Vector &GetConfigurations() const { return m_configs; }

            const Config &GetConfiguration( size_t n ) const
            { //{{{

                for( Config::Vector::const_iterator i = m_configs.begin(),
                                                    e = m_configs.end(); i != e; ++i )
                {
                    if( i->bConfigurationValue == n )
                        return *i;
                }

                throw Error( _("Device::GetConfiguration( %zu ) no such configuration for %s"),
                                                                        n, IDStr().c_str() );
            } //}}}

            size_t GetMaxTransferSize() const               { return m_maxtransfer; }


            std::string BusAddressStr() const
            {
                return stringprintf( "%03u:%03u", m_busnum, m_devnum );
            }

            std::string DevicePortStr() const
            {
                return stringprintf( "%u-", m_busnum ) + m_devport;
            }

            std::string IDStr() const
            { //{{{

                if( m_devport.empty() )
                    return stringprintf( "%03u:%03u Serial '%s'",
                                         m_busnum, m_devnum, m_serial.c_str() );

                return stringprintf( "%03u:%03u Serial '%s', port %u-%s",
                                     m_busnum, m_devnum, m_serial.c_str(),
                                     m_busnum, m_devport.c_str() );
            } //}}}

            std::string ProductStr() const
            {
                return stringprintf( "Serial '%s', Mfg '%s', Product '%s'",
                                     m_serial.c_str(), m_mfg.c_str(), m_product.c_str() );
            }

            std::string VerboseStr() const
            { //{{{

                // We might not always know which physical port the device is attached to.
                // We can get that from udev, or from libusb 1.0.16 or later (on some of
                // the platforms it supports), but we aren't guaranteed to have those.
                if( m_devport.empty() )
                    return stringprintf( "%03u:%03u %04x:%04x Serial '%s', Mfg '%s', Product '%s'",
                                         m_busnum, m_devnum, m_vendorid, m_productid,
                                         m_serial.c_str(), m_mfg.c_str(), m_product.c_str() );

                return stringprintf( "%03u:%03u %04x:%04x Serial '%s', Mfg '%s', Product '%s', port %s",
                                     m_busnum, m_devnum, m_vendorid, m_productid, m_serial.c_str(),
                                     m_mfg.c_str(), m_product.c_str(), DevicePortStr().c_str() );
            } //}}}


            // Return the device information as a string of nul separated fields.
            //{{{
            // This is primarily intended for importing device data into shell scripts
            // in a way that is both safe and easy to parse.  The string will contain
            // 9 fields, each terminated by a trailing nul.  Some fields may be empty.
            //
            // The fields contain (in the order they are output):
            //
            //  \nD:          - Start of record magic.  Used to sanity check that we have
            //                  the first field when multiple devices are output together,
            //                  and as a promise of the format and content of the following
            //                  data. If we ever need to break that promise, the magic will
            //                  change too to signal whatever the new promise may be.
            //
            //  Bus number    - A 3 digit, 0-padded, decimal number.  The USB bus that the
            //                  device is on.
            //
            //  Device number - A 3 digit, 0-padded, decimal number.  The logical address
            //                  of the device on the given USB bus.
            //
            //  Vendor ID     - A 4 digit hexadecimal number.  The device vendor's USB ID.
            //
            //  Product ID    - A 4 digit hexadecimal number.  The device's USB product ID.
            //
            //  Serial number - An arbitrary string containing the device serial data.
            //                  This may be empty.
            //
            //  Manufacturer  - An arbitrary string containing the manufacturer's name.
            //                  This may be empty.
            //
            //  Product       - An arbitrary string containing the product's name.
            //                  This may be empty.
            //
            //  Device port   - A period separated string of the physical port numbers that
            //                  are the device's physical address on the given USB bus.
            //                  This may be empty, since we aren't always able to obtain
            //                  this information of every platform.
            //}}}
            std::string ShellMrStr() const
            { //{{{

                std::string     s( "\nD:" );

                s.append( 1, '\0' );
                s.append( stringprintf( "%03u", m_busnum ) ).append( 1, '\0' );
                s.append( stringprintf( "%03u", m_devnum ) ).append( 1, '\0' );
                s.append( stringprintf( "%04x", m_vendorid ) ).append( 1, '\0' );
                s.append( stringprintf( "%04x", m_productid ) ).append( 1, '\0' );
                s.append( m_serial ).append( 1, '\0' );
                s.append( m_mfg ).append( 1, '\0' );
                s.append( m_product ).append( 1, '\0' );
                s.append( m_devport ).append( 1, '\0' );

                return s;

            } //}}}

            //}}}

        }; //}}}


    private:

        libusb_context             *m_usb;

        mutable pthread_mutex_t     m_device_mutex;
        Device::List                m_devices;


    protected:

        // Derived classes can override these to get hotplug notification.
        virtual void DeviceAdded( const Device::Handle &d )     { (void)d; }
        virtual void DeviceRemoved( const Device::Handle &d )   { (void)d; }


        libusb_context *GetContext() const                      { return m_usb; }


        Device::Handle find_device( unsigned busnum, unsigned devnum )
        { //{{{

            ScopedCancelState   cancelstate;
            Device::Handle      h;
            libusb_device     **devs;
            ssize_t             ret = libusb_get_device_list( m_usb, &devs );

            if( ret < 0 )
                throw USBError( int(ret), _("USBContext: failed to enumerate devices") );

            for( libusb_device **dev = devs; *dev; ++dev )
            {
                libusb_device               *d = *dev;

                if( libusb_get_bus_number(d) == busnum
                 && libusb_get_device_address(d) == devnum )
                {
                    h = new Device( d );
                    break;
                }
            }

            libusb_free_device_list( devs, 1 );

            return h;

        } //}}}


        void AddDevice( const Device::Handle &d )
        { //{{{

            ScopedMutex     lock( &m_device_mutex );

            for( Device::List::iterator i = m_devices.begin(),
                                        e = m_devices.end(); i != e; ++i )
            {
                if( **i == *d )
                {
                    // This shouldn't usually happen, but the libusb hotplug support
                    // can sometimes report a device twice if it loses the wrong race
                    // and it wouldn't be impossible for some derived class to bork
                    // this up and try to add a device more than once somehow.
                    //
                    // In the (very) worst case we'd only need to scan a few hundred
                    // devices here, so this isn't a very expensive sanity check.
                    Log<1>( _("USBContext::AddDevice: already have device %s\n"),
                                                        d->VerboseStr().c_str() );
                    return;
                }
            }


            Log<2>( _("USBContext::AddDevice: %s %s\n"), d->VerboseStr().c_str(),
                                                         d->GetDevpath().c_str() );
            m_devices.push_back( d );

            try {
                DeviceAdded( d );
            }
            BB_CATCH_ALL( 0, "USBContext::AddDevice failed" )

        } //}}}

        void RemoveDevice( libusb_device *dev )
        { //{{{

            ScopedMutex     lock( &m_device_mutex );
            unsigned        busnum = libusb_get_bus_number( dev );
            unsigned        devnum = libusb_get_device_address( dev );

            for( Device::List::iterator i = m_devices.begin(),
                                        e = m_devices.end(); i != e; ++i )
            {
                if( (*i)->GetBusNumber() == busnum && (*i)->GetDeviceNumber() == devnum )
                {
                    Device::Handle  d = *i;

                    m_devices.erase( i );

                    Log<2>( _("USBContext::RemoveDevice: removed %s\n"),
                                                d->VerboseStr().c_str() );
                    try {
                        DeviceRemoved( d );
                    }
                    BB_CATCH_ALL( 0, "USBContext::RemoveDevice failed" )

                    return;
                }
            }

            Log<4>( _("USBContext::RemoveDevice: no matching device for %03u:%03u\n"),
                                                                    busnum, devnum );
        } //}}}

        void RemoveDeviceByDevpath( const char *devpath )
        { //{{{

            ScopedMutex     lock( &m_device_mutex );

            for( Device::List::iterator i = m_devices.begin(),
                                        e = m_devices.end(); i != e; ++i )
            {
                if( (*i)->GetDevpath() == devpath )
                {
                    Device::Handle  d = *i;

                    m_devices.erase( i );

                    Log<2>( _("USBContext::RemoveDeviceByDevpath( %s ): removed %s\n"),
                                                    devpath, d->VerboseStr().c_str() );
                    try {
                        DeviceRemoved( d );
                    }
                    BB_CATCH_ALL( 0, "USBContext::RemoveDeviceByDevpath failed" )

                    return;
                }
            }

            Log<4>( _("USBContext::RemoveDeviceByDevpath( %s ): no matching device\n"),
                                                                            devpath );
        } //}}}

        void WarmplugAllDevices()
        { //{{{

            ScopedMutex     lock( &m_device_mutex );

            for( Device::List::const_iterator i = m_devices.begin(),
                                              e = m_devices.end(); i != e; ++i )
            {
                try {
                    DeviceAdded( *i );
                }
                BB_CATCH_ALL( 0, "USBContext: warmplug failed" )
            }

        } //}}}


    public:

        USBContext()
        { //{{{

            Log<2>( "+ USBContext\n" );

            ScopedCancelState   cancelstate;

            int ret = libusb_init( &m_usb );
            if( ret )
                throw USBError( ret, _("USBContext: failed to create libusb context") );

            pthread_mutex_init( &m_device_mutex, NULL );

        } //}}}

        virtual ~USBContext()
        { //{{{

            Log<2>( "- USBContext\n" );

            // Release all the device references before destroying the context.
            // The libusb_device doesn't keep a reference count for the context
            // but it does have a pointer to it which something might access.
            {
                ScopedMutex     lock( &m_device_mutex );
                m_devices.clear();
            }

            pthread_mutex_destroy( &m_device_mutex );

            // Extra debug log checkpoints here, because on FreeBSD 11, the
            // call to libusb_exit can take 4 seconds to complete, which can
            // otherwise look like our code has become hung up somewhere.
            Log<4>( "USBContext: waiting for libusb_exit\n" );

            ScopedCancelState   cancelstate;
            libusb_exit( m_usb );

            Log<4>( "USBContext: libusb_exit completed\n" );

        } //}}}


        // Return true if this build, on this platform, has device hotplug support.
        virtual bool HasHotplugSupport() const  { return false; }


        // If vendorid and productid are 0, enumerate all devices.
        // Otherwise only those matching the given VID:PID.
        // If append is true, add them to any existing list of devices, otherwise replace them.
        void EnumerateDevices( unsigned vendorid = 0, unsigned productid = 0, bool append = false )
        { //{{{

            ScopedMutex         lock( &m_device_mutex );
            ScopedCancelState   cancelstate;

            libusb_device     **devs;
            ssize_t             ret = libusb_get_device_list( m_usb, &devs );

            if( ret < 0 )
                throw USBError( int(ret), _("USBContext: failed to enumerate devices") );

            if( ! append )
                m_devices.clear();

            for( libusb_device **dev = devs; *dev; ++dev )
            {
                libusb_device               *d = *dev;
                libusb_device_descriptor    desc;

                int r = libusb_get_device_descriptor(d, &desc);
                if( r < 0 )
                {
                    LogUSBError<1>( r, _("USBContext::EnumerateDevices: failed to get descriptor") );
                    continue;
                }

                if( (vendorid == 0 && productid == 0)
                 || (desc.idVendor == vendorid && desc.idProduct == productid) )
                    m_devices.push_back( new Device(d, desc) );
                else
                    Log<4>( "USBContext: ignoring %04x:%04x\n", desc.idVendor, desc.idProduct );
            }

            libusb_free_device_list( devs, 1 );

        } //}}}

        // Replace the current list of enumerated devices with only those matching id.
        void EnumerateDevices( const ProductID &id )
        {
            EnumerateDevices( id.vid, id.pid, false );
        }

        // Append to the current list of enumerated devices any matching id.
        void AppendDevices( const ProductID &id )
        {
            EnumerateDevices( id.vid, id.pid, true );
        }


        unsigned GetNumDevices() const
        { //{{{

            ScopedMutex     lock( &m_device_mutex );
            return unsigned(m_devices.size());

        } //}}}

        // Get a device by logical or physical address, or by serial number.
        Device::Handle GetDevice( const Device::ID &id ) const
        { //{{{

            ScopedMutex     lock( &m_device_mutex );

            for( Device::List::const_iterator i = m_devices.begin(),
                                              e = m_devices.end(); i != e; ++i )
                if( id.Matches( *i ) )
                    return *i;

            throw Error( _("USBContext::GetDevice( %s ): no such device"),
                                                        id.Str().c_str() );
        } //}}}

        // Return a list of all currently available devices.
        //{{{
        // Most things shouldn't ever (need to) use this, unless they really do
        // want an 'unchanging' snapshot of the current state which won't have
        // devices added (or removed) if hotplug events occur.
        //}}}
        Device::List GetDevices() const
        { //{{{

            ScopedMutex     lock( &m_device_mutex );
            return m_devices;

        } //}}}


        // List all available devices in a human readable form
        void ListDevices() const
        { //{{{

            ScopedMutex     lock( &m_device_mutex );

            if( m_devices.empty() )
            {
                printf( _("No devices found.\n") );
                return;
            }

            size_t  n = m_devices.size();

            printf( P_("Have %zu device:\n",
                       "Have %zu devices:\n", n), n );

            if( n > 0 )
            {
                if( opt_verbose )
                    printf( "  Bus:Dev  VID:PID\n" );
                else
                    printf( "  Bus:Device\n" );
            }

            for( Device::List::const_iterator i = m_devices.begin(),
                                              e = m_devices.end(); i != e; ++i )
            {
                if( opt_verbose )
                    printf( "  %s\n", (*i)->VerboseStr().c_str() );
                else
                    printf( "  %s\n", (*i)->IDStr().c_str() );
            }

        } //}}}

        // List all available devices in a machine readable form
        // that is suitable for importing into shell scripts.
        void ListDevicesShellMR() const
        { //{{{

            std::string     s;

            for( Device::List::const_iterator i = m_devices.begin(),
                                              e = m_devices.end(); i != e; ++i )
                s.append( (*i)->ShellMrStr() );

            if( ! s.empty() )
                if( write( STDOUT_FILENO, s.data(), s.size() ) )
                { /* Suppress the unused return warning here */ }

        } //}}}


        template< int N >
        BB_PRINTF_FORMAT(2,3)
        static void LogUSBError( int err, const char *format, ... )
        { //{{{

            va_list         arglist;
            std::string     msg;

            va_start( arglist, format );
            msg = vstringprintf( format, arglist );
            Log<N>( "%s: %s\n", msg.c_str(), libusb_strerror(libusb_error(err)) );
            va_end( arglist );

        } //}}}

    }; //}}}

}   // BitB namespace


#endif  // _BB_USBCONTEXT_H

// vi:sts=4:sw=4:et:foldmethod=marker
