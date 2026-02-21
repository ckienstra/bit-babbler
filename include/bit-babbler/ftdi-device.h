//  This file is distributed as part of the bit-babbler package.
//  Copyright 2010 - 2021,  Ron <ron@debian.org>

#ifndef _BB_FTDI_DEVICE_H
#define _BB_FTDI_DEVICE_H

#include <bit-babbler/usbcontext.h>


#define FTDI_VENDOR_ID      0x0403
#define FTDI_PRODUCT_ID     0x6014


// Sanity check the modem and line status bits in each packet.
//{{{
// There probably aren't many good reasons to disable this, but the exact set of
// values which may be considered sane could need tuning for a device that isn't
// running in MPSSE mode.  These checks are cheap to run, so it really only makes
// sense to disable them if you're in the process of adding something which would
// alter what the expected valid values should be.
//}}}
#define CHECK_LINE_STATUS


namespace BitB
{
    // Interface to an FTDI device
    class FTDI : public RefCounted
    { //{{{
    protected:

        // FTDI control request types
        enum RequestType
        { //{{{

            // All these implicitly include LIBUSB_RECIPIENT_DEVICE
            FTDI_DEVICE_OUT_REQ = int(LIBUSB_REQUEST_TYPE_VENDOR) | LIBUSB_ENDPOINT_OUT,
            FTDI_DEVICE_IN_REQ  = int(LIBUSB_REQUEST_TYPE_VENDOR) | LIBUSB_ENDPOINT_IN

        }; //}}}

        // FTDI control requests
        enum ControlRequest
        { //{{{

            FTDI_SIO_RESET              = 0x00,
            FTDI_SIO_MODEM_CTRL         = 0x01,
            FTDI_SIO_SET_FLOW_CTRL      = 0x02,
            FTDI_SIO_SET_BAUD_RATE      = 0x03,
            FTDI_SIO_SET_DATA           = 0x04, // Set data/parity/stop bits
            FTDI_SIO_GET_MODEM_STATUS   = 0x05,
            FTDI_SIO_SET_EVENT_CHAR     = 0x06,
            FTDI_SIO_SET_ERROR_CHAR     = 0x07,
            FTDI_SIO_SET_LATENCY_TIMER  = 0x09,
            FTDI_SIO_GET_LATENCY_TIMER  = 0x0A,
            FTDI_SIO_SET_BITMODE        = 0x0B,
            FTDI_SIO_READ_PINS          = 0x0C,
            FTDI_SIO_READ_EEPROM        = 0x90,
            FTDI_SIO_WRITE_EEPROM       = 0x91,
            FTDI_SIO_ERASE_EEPROM       = 0x92

        }; //}}}

        // Options for the FTDI_SIO_RESET request
        enum ResetOperation
        { //{{{

            FTDI_SIO_RESET_SIO      = 0,
            FTDI_SIO_RESET_PURGE_RX = 1,
            FTDI_SIO_RESET_PURGE_TX = 2

        }; //}}}

        // Options for the FTDI_SIO_SET_FLOW_CTRL request
        enum FlowControl
        { //{{{

            FLOW_NONE       = 0x0000,
            FLOW_RTS_CTS    = 0x0100,
            FLOW_DTR_DSR    = 0x0200,
            FLOW_XON_XOFF   = 0x0400

        }; //}}}

        // Options for the FTDI_SIO_SET_BITMODE request
        enum Bitmode
        { //{{{

            BITMODE_RESET           = 0x0000,   // Reset (to the default) I/O mode
            BITMODE_ASYNC_BITBANG   = 0x0100,   // Asynchronous bitbang mode
            BITMODE_MPSSE           = 0x0200,   // MPSSE mode
            BITMODE_SYNC_BITBANG    = 0x0400,   // Synchronous bitbang mode
            BITMODE_MCU_HOST        = 0x0800,   // MCU host bus emulation mode
            BITMODE_FAST_SERIAL     = 0x1000,   // Fast serial for opto-isolation
            BITMODE_CBUS_BITBANG    = 0x2000,   // CBUS bitbang mode
            BITMODE_SYNC_FIFO       = 0x4000    // Single channel synchronous 245 FIFO mode

        }; //}}}

        // Index for interface control requests
        enum InterfaceIndex
        { //{{{

            FTDI_INTERFACE_A = 1,
            FTDI_INTERFACE_B = 2,
            FTDI_INTERFACE_C = 3,
            FTDI_INTERFACE_D = 4

        }; //}}}


        enum ModemStatus
        { //{{{

            FTDI_MAX64  = 0x01, // wMaxPacketSize 64
            FTDI_MAX512 = 0x02, // wMaxPacketSize 512
            FTDI_CTS    = 0x10, // Clear to send
            FTDI_DSR    = 0x20, // Data set ready
            FTDI_RI     = 0x40, // Ring indicator
            FTDI_RLSD   = 0x80  // Receive line signal detect

        }; //}}}

        enum LineStatus
        { //{{{

            FTDI_DR     = 0x01, // Data ready
            FTDI_OE     = 0x02, // Overrun error
            FTDI_PE     = 0x04, // Parity error
            FTDI_FE     = 0x08, // Framing error
            FTDI_BI     = 0x10, // Break interrupt
            FTDI_THRE   = 0x20, // Transmit holding register empty
            FTDI_TEMT   = 0x40, // Transmitter empty
            FTDI_RCVR   = 0x80  // RCVR FIFO error

        }; //}}}


        enum MPSSECommand
        { //{{{

            // Data shifting commands.
            // Bit0: -ve CLK on write
            // Bit1: bit mode if 1, else byte mode
            // Bit2: -ve CLK on read
            // Bit3: LSB first if 1, else MSB first
            // Bit4: Write TDI
            // Bit5: Read  TDO
            // Bit6: Write TMS
            // Bit6: Always 0

            // Clock len data bytes in on +ve clock edge MSB first
            MPSSE_DATA_BYTE_IN_POS_MSB  = 0x20,

            // Clock len data bytes in on -ve clock edge MSB first
            MPSSE_DATA_BYTE_IN_NEG_MSB  = 0x24,

            // Clock len data bytes in on +ve clock edge LSB first
            MPSSE_DATA_BYTE_IN_POS_LSB  = 0x28,

            // Clock len data bytes in on -ve clock edge LSB first
            MPSSE_DATA_BYTE_IN_NEG_LSB  = 0x2C,


            // Set / Read Data Bits High / Low Bytes
            MPSSE_SET_DATABITS_LOW      = 0x80,     // Set low data pins
            MPSSE_SET_DATABITS_HIGH     = 0x82,     // Set high data pins

            // Loopback commands
            MPSSE_LOOPBACK              = 0x84,     // Connect TDI/DO to TDO/DI
            MPSSE_NO_LOOPBACK           = 0x85,     // Disable loopback operation

            // Set CLK divisor
            MPSSE_SET_CLK_DIVISOR       = 0x86,

            // MPSSE and MCU Host Emulation commands
            MPSSE_SEND_IMMEDIATE        = 0x87,     // Flush buffer to host

            // FT232H, FT2232H & FT4232H only
            MPSSE_NO_CLK_DIV5           = 0x8A,     // Disable clock divide by 5
            MPSSE_NO_3PHASE_CLK         = 0x8D,     // Disable 3 phase data clock
            MPSSE_NO_ADAPTIVE_CLK       = 0x97      // Disable adaptive clocking

        }; //}}}


        static const unsigned   FTDI_READ_RETRIES = 10;


    private:

        USBContext::Device::Handle          m_dev;
        USBContext::Device::Open::Handle    m_dh;

        unsigned                            m_timeout;
        unsigned                            m_latency;
        unsigned                            m_maxpacket;

        uint16_t /* InterfaceIndex */       m_index;
        uint8_t                             m_configuration;
        uint8_t                             m_interface;
        uint8_t                             m_altsetting;
        uint8_t                             m_epin;     // Endpoint device->host address
        uint8_t                             m_epout;    // Endpoint host->device address

        uint8_t                             m_linestatus;

        size_t                              m_chunksize;
        size_t                              m_chunkhead;
        size_t                              m_chunklen;
        uint8_t                            *m_chunkbuf;

        uint8_t                             m_expect_modemstatus;


    protected:

        void ftdi_reset()
        { //{{{

            ScopedCancelState   cancelstate;

            int ret = libusb_control_transfer( *m_dh, FTDI_DEVICE_OUT_REQ,
                                               FTDI_SIO_RESET, FTDI_SIO_RESET_SIO,
                                               m_index, NULL, 0, m_timeout );
            if( ret < 0 )
                ThrowUSBError( ret, _("FTDI: failed to reset device") );

        } //}}}

        void ftdi_set_bitmode( Bitmode b, uint8_t mask = 0 )
        { //{{{

            // The upper 8 bits of the 16-bit value here set the bitmode,
            // with the lower 8 bits configuring pins as input or output
            // in the modes where that is relevant (0 -> input, 1 -> output).

            ScopedCancelState   cancelstate;

            int ret = libusb_control_transfer( *m_dh, FTDI_DEVICE_OUT_REQ,
                                               FTDI_SIO_SET_BITMODE, uint16_t(b | mask),
                                               m_index, NULL, 0, m_timeout );
            if( ret < 0 )
                ThrowUSBError( ret, _("FTDI: failed to set bitmode 0x%04x"), b | mask );

        } //}}}

        // Set (or disable) special characters in the data stream signalling that
        // an event fired or an error occurred.
        void ftdi_set_special_chars( char event = '\0', bool evt_enable = false,
                                     char error = '\0', bool err_enable = false )
        { //{{{

            ScopedCancelState   cancelstate;

            int ret = libusb_control_transfer( *m_dh, FTDI_DEVICE_OUT_REQ,
                                               FTDI_SIO_SET_EVENT_CHAR,
                                               uint16_t(event | (evt_enable ? 0x100 : 0)),
                                               m_index, NULL, 0, m_timeout );
            if( ret < 0 )
            {
                if( evt_enable )
                    ThrowUSBError( ret, _("FTDI: failed to set event char 0x%02x"), event );
                else
                    ThrowUSBError( ret, _("FTDI: failed to disable event char") );
            }

            ret = libusb_control_transfer( *m_dh, FTDI_DEVICE_OUT_REQ,
                                           FTDI_SIO_SET_ERROR_CHAR,
                                           uint16_t(error | (err_enable ? 0x100 : 0)),
                                           m_index, NULL, 0, m_timeout );
            if( ret < 0 )
            {
                if( err_enable )
                    ThrowUSBError( ret, _("FTDI: failed to set error char 0x%02x"), error );
                else
                    ThrowUSBError( ret, _("FTDI: failed to disable error char") );
            }

        } //}}}

        void ftdi_set_latency_timer( uint8_t ms )
        { //{{{

            // The FT232H datasheet says this can be between 0 and 255ms,
            // while the D2XX API documentation says it must be >= 2ms ...
            // They are both wrong, it must be > 0.
            if( ms < 1 )
                ThrowError( _("Invalid latency timeout %u < 1ms"), ms );

            ScopedCancelState   cancelstate;

            int ret = libusb_control_transfer( *m_dh, FTDI_DEVICE_OUT_REQ,
                                               FTDI_SIO_SET_LATENCY_TIMER, ms,
                                               m_index, NULL, 0, m_timeout );
            if( ret < 0 )
                ThrowUSBError( ret, _("FTDI: failed to set latency timer to %ums"), ms );

        } //}}}

        void ftdi_set_flow_control( FlowControl mode )
        { //{{{

            // This one is a bit weird, the flow control mode is passed with the index
            // and the value is only used when setting FLOW_XON_XOFF to set the start
            // and stop characters using (stop << 8 | start).

            ScopedCancelState   cancelstate;

            int ret = libusb_control_transfer( *m_dh, FTDI_DEVICE_OUT_REQ,
                                               FTDI_SIO_SET_FLOW_CTRL, 0,
                                               uint16_t(mode) | m_index,
                                               NULL, 0, m_timeout );
            if( ret < 0 )
                ThrowUSBError( ret, _("FTDI: failed to set flow control mode 0x%04x"), mode );

        } //}}}

        // Modem status bits in the high byte, line status bits in the low byte.
        uint16_t ftdi_get_modem_status()
        { //{{{

            uint8_t  ms[2];

            int ret = libusb_control_transfer( *m_dh, FTDI_DEVICE_IN_REQ,
                                               FTDI_SIO_GET_MODEM_STATUS, 0,
                                               m_index, ms, 2, m_timeout );
            if( ret < 0 )
                ThrowUSBError( ret, _("FTDI: failed to get modem status") );

            if( ret != 2 )
                ThrowError( _("FTDI: get modem status returned %d bytes"), ret );

            return uint16_t( (ms[0] << 8) | ms[1] );

        } //}}}

        void ftdi_write( const uint8_t *buf, size_t len )
        { //{{{

           #ifdef CHECK_LINE_STATUS

            // This is a little bit stricter then it absolutely needs to be.
            //{{{
            // We can actually send off a couple more requests after these bits
            // become unset before everything falls over, but we don't actually
            // (need to) do that right now, and if we get too far ahead, then the
            // write request will effectively block indefinitely (since we don't
            // have a separate thread continually reading), and since we had to
            // disable cancellation because libusb is unsafe for that, it means
            // pretty much nothing short of SIGKILL will save us at that point.
            // This shouldn't ever happen in normal use, and we can still have
            // two requests currently in flight before making more will trigger
            // it, but be defensive because we can and should be.
            //}}}
            if( __builtin_expect(m_linestatus != (FTDI_THRE | FTDI_TEMT), 0) )
                ThrowError( _("FTDI: aborted write of len %zu with line status 0x%02x"),
                                                                    len, m_linestatus );
           #endif

            // The libusb_bulk_transfer function handles both reads and writes,
            // so its data buffer parameter isn't const, but it doesn't modify
            // the buffer passed to it for a write operation.
            unsigned char  *b = const_cast<unsigned char*>(buf);
            int             oldstate;

            while( len )
            {
                pthread_testcancel();
                pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldstate );

                int xfer;
                int n   = int(std::min( len, m_chunksize ));
                int ret = libusb_bulk_transfer( *m_dh, m_epout, b, n, &xfer, m_timeout );

                pthread_setcancelstate( oldstate, NULL );

                switch( ret )
                {
                    case 0:
                    case LIBUSB_ERROR_TIMEOUT:
                        // This shouldn't ever happen, but catch it here
                        // instead of doing something stupid if it does ...
                        if( __builtin_expect(xfer < 0 || size_t(xfer) > len,0) )
                            ThrowError( _("FTDI: OOPS write of %d returned %d ..."), n, xfer );

                        len -= unsigned(xfer);
                        b   += xfer;
                        break;

                    default:
                        ThrowUSBError( ret, _("FTDI: write of %d/%zu bytes failed"), n, len );
                }
            }

        } //}}}

        // Return the next largest multiple of m_maxpacket from n.
        size_t round_to_maxpacket( size_t n )
        {
            return n + m_maxpacket - 1 - (n - 1) % m_maxpacket;
        }

        // Note that the buffer passed to this MUST be a multiple of m_maxpacket
        // that is equal to or larger in size than len.  No more than m_chunksize
        // bytes will be returned from a single read regardless of the len passed.
        size_t ftdi_read_raw( uint8_t *buf, size_t len )
        { //{{{

            int     oldstate;
            int     xfer;
            int     n = int(std::min( len, m_chunksize ));

            // Ensure we always request a multiple of m_maxpacket, otherwise
            // we can get an overflow from the last packet that is received,
            // since the transfer size isn't sent to the device and it might
            // still send a 'full' packet even if we wanted less than that.
            n = int(round_to_maxpacket(size_t(n)));

            pthread_testcancel();
            pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, &oldstate );

            int ret = libusb_bulk_transfer( *m_dh, m_epin, buf, n, &xfer, m_timeout );

            pthread_setcancelstate( oldstate, NULL );

         // LogMsg<4>("ftdi_read: len %5zu, req %5d, got %5d, ret %d [%s ]", len, n, xfer, ret,
         //                     OctetsToHex( OctetString( buf, std::min(xfer, 8) ) ).c_str() );

            switch( ret )
            {
                case 0:
                case LIBUSB_ERROR_TIMEOUT:
                    // This shouldn't ever happen, but catch it here
                    // instead of doing something stupid if it does ...
                    if( __builtin_expect(xfer < 0 || xfer > n, 0) )
                        ThrowError( _("FTDI: OOPS read of %d returned %d ..."), n, xfer );

                    return size_t(xfer);
            }

            ThrowUSBError( ret, _("FTDI: read chunk of %d bytes failed"), n );

           #if EM_PLATFORM_MSW
            // Really we'll never get here, but mingw-w64 4.9.2-21+15.4
            // appears to be too stupid to figure that out ...
            return 0;
           #endif

        } //}}}

        size_t ftdi_read( uint8_t *buf, size_t len )
        { //{{{

            size_t  r = 0;

            while( len )
            {
                if( m_chunklen )
                {
                    // The first two bytes of every packet from the chip are used
                    // to signal 'modem status', so we need to strip those out.
                    size_t  packethead  = m_chunkhead % m_maxpacket;
                    size_t  packetlen   = m_maxpacket - packethead;

                   #ifdef CHECK_LINE_STATUS

                    size_t  skip        = 0;

                    switch( packethead )
                    {
                        case 0:
                            if( __builtin_expect(m_chunkbuf[m_chunkhead] != m_expect_modemstatus, 0) )
                            { //{{{

                                size_t  chunklen = m_chunklen;

                                m_chunklen = 0;
                                ThrowError( _("FTDI: read invalid packet: "
                                              " len %5zu, chead %zu, clen %zu [%s ]"),
                                                len, m_chunkhead, chunklen,
                                                OctetsToHex( OctetString( m_chunkbuf + m_chunkhead,
                                                                          std::min( chunklen,
                                                                                    size_t(8)) )
                                                           ).c_str() );
                            } //}}}

                            if( m_chunklen > 1 )
                            {
                                if( __builtin_expect(
                                    (m_chunkbuf[m_chunkhead + 1] & ~(FTDI_THRE | FTDI_TEMT)) != 0, 0) )
                                { //{{{

                                    size_t  chunklen = m_chunklen;

                                    m_chunklen = 0;
                                    ThrowError( _("FTDI: read unexpected line status: "
                                                  " len %5zu, chead %zu, clen %zu [%s ]"),
                                                    len, m_chunkhead, chunklen,
                                                    OctetsToHex( OctetString( m_chunkbuf + m_chunkhead,
                                                                              std::min( chunklen,
                                                                                        size_t(8)) )
                                                               ).c_str() );
                                } //}}}

                                m_linestatus = m_chunkbuf[m_chunkhead + 1];
                                skip = 2;

                            } else {

                                skip = 1;
                            }
                            break;

                        case 1:
                            if( __builtin_expect(
                                (m_chunkbuf[m_chunkhead] & ~(FTDI_THRE | FTDI_TEMT)) != 0, 0) )
                            { //{{{

                                size_t  chunklen = m_chunklen;

                                m_chunklen = 0;
                                ThrowError( _("FTDI: read unexpected line status: "
                                              " len %5zu, chead %zu, clen %zu [%s ]"),
                                                len, m_chunkhead, chunklen,
                                                OctetsToHex( OctetString( m_chunkbuf + m_chunkhead,
                                                                          std::min( chunklen,
                                                                                    size_t(8)) )
                                                           ).c_str() );
                            } //}}}

                            m_linestatus = m_chunkbuf[m_chunkhead];
                            skip = 1;
                            break;
                    }

                    m_chunkhead += skip;
                    m_chunklen  -= skip;
                    packetlen   -= skip;

                   #else    // ! CHECK_LINE_STATUS

                    if( packethead < 2 )
                    {
                        size_t  skip = std::min( m_chunklen, 2 - packethead );
                        m_chunkhead += skip;
                        m_chunklen  -= skip;
                        packetlen   -= skip;
                    }

                   #endif

                    // The actual data in this packet is then the the minimum of:
                    //    m_chunklen
                    //    len,
                    //    maxpacket - 2 (which can never be less than):
                    //    distance from m_chunkhead to the next packet boundary
                    size_t  n = std::min( len, std::min( packetlen, m_chunklen ) );

                 // LogMsg<2>("ftdi_read: len %5zu, chead %5zu, clen %5zu, phead %3zu, plen %3zu, n %zu",
                 //                             len, m_chunkhead, m_chunklen, packethead, packetlen, n);

                    memcpy( buf, m_chunkbuf + m_chunkhead, n );

                    m_chunkhead += n;
                    m_chunklen  -= n;
                    len         -= n;
                    buf         += n;
                    r           += n;

                    continue;
                }


                size_t  xfer = ftdi_read_raw( m_chunkbuf, len );

               #ifdef CHECK_LINE_STATUS

                if( __builtin_expect(xfer == 2, 0) )
                {
                    if( __builtin_expect(m_chunkbuf[0] != m_expect_modemstatus
                                     || (m_chunkbuf[1] & ~(FTDI_THRE | FTDI_TEMT)) != 0, 0) )
                        ThrowError( _("FTDI: read invalid packet: len %5zu, got %5zu [%s ]"),
                                        len, xfer,
                                        OctetsToHex( OctetString( m_chunkbuf,
                                                                  std::min(xfer, size_t(8)) )
                                                   ).c_str() );

                    m_linestatus = m_chunkbuf[1];
                    return r;
                }

                if( __builtin_expect(xfer < 2, 0) )
                    return r;

                m_chunkhead = 0;
                m_chunklen  = xfer;

               #else    // ! CHECK_LINE_STATUS

                if( xfer < 3 )
                    return r;

                m_chunkhead = 2;
                m_chunklen  = xfer - 2;

               #endif
            }

            return r;

        } //}}}


        USBContext::Device::Open::Handle GetDeviceHandle() const    { return m_dh; }

        unsigned GetUSBTimeout() const                              { return m_timeout; }

        uint16_t GetInterfaceIndex() const                          { return m_index; }

        uint8_t GetLineStatus() const                               { return m_linestatus; }

        size_t GetReadAhead() const                                 { return m_chunklen; }


        void WriteCommand( const OctetString &cmd )
        { //{{{

            if( __builtin_expect(opt_verbose >= 6, 0) )
                LogMsg<6>( "FTDI::WriteCommand(%s )", OctetsToHex(cmd).c_str() );

            ftdi_write( cmd.data(), cmd.size() );

        } //}}}

        void WriteCommand( const uint8_t *cmd, size_t len )
        { //{{{

            if( __builtin_expect(opt_verbose >= 6, 0) )
                return WriteCommand( OctetString( cmd, len ) );

            ftdi_write( cmd, len );

        } //}}}


    private:

        // Returns true if initial FTDI communication is successful
        bool check_sync( uint8_t cmd )
        { //{{{

            const uint8_t   msg[] = { cmd, MPSSE_SEND_IMMEDIATE };
            unsigned        n     = 0;
            uint8_t         buf[512];   // must be at least wMaxPacketSize

            LogMsg<3>( "FTDI::check_sync( %02x )", cmd );

            WriteCommand( msg, sizeof(msg) );

            do {
                size_t  ret = ftdi_read_raw( buf, sizeof(buf) );

                if( ret == 4 && buf[2] == 0xFA && buf[3] == cmd )
                {
                    LogMsg<3>( "have sync for 0x%x (n = %u)", cmd, n );
                    return true;
                }

                if( ret > 2 )
                {
                    LogMsg<3>( "sync returned %zu bytes (n = %u)", ret, n );

                    n = 0;

                    if( opt_verbose > 3 )
                    {
                        size_t  nbytes = 16;

                        LogMsg<4>( ret > nbytes ? "%s ..." : "%s",
                                   OctetsToShortHex( OctetString( buf, std::min(ret, nbytes) )
                                                   ).c_str() );
                    }
                }

            } while( ++n < FTDI_READ_RETRIES );

            return false;

        } //}}}


    protected:

        // Returns the total number of bytes purged
        size_t purge_read()
        { //{{{

            uint8_t     buf[8192];
            size_t      count = 0;
            unsigned    n = 0;

            LogMsg<3>( "FTDI::purge_read" );

            // This shouldn't ever happen, but things could go badly
            // in confusing and intermittent ways if it's not true.
            if( round_to_maxpacket( sizeof(buf) ) != sizeof(buf) )
                ThrowError( "FTDI::purge_read buffer %zu is not a multiple of wMaxPacketSize %u",
                                                                    sizeof(buf), m_maxpacket );

            do {
                // We can't reliably check the upper nybble of the modem status
                // byte because we may not always be in MPSSE mode here, and we
                // can't guarantee what state the UART signal pins might be in.
                //
                // And we can't reliably use the line status bits here, because
                // they might not be usefully set to indicate anything we care
                // about here.  We just want to drain any junk that might still
                // be in the buffer on the chip which a reset didn't get rid of.
                size_t  ret = ftdi_read_raw( buf, sizeof(buf) );

                if( ret > 2 )
                {
                    count += ret;
                    LogMsg<3>( "purged %zu / %zu (n = %u)", ret, count, n );
                    n = 0;

                    if( opt_verbose > 3 )
                    {
                        size_t  nbytes = 16;

                        LogMsg<4>( ret > nbytes ? "%s ..." : "%s",
                                   OctetsToShortHex( OctetString( buf, std::min(ret, nbytes) )
                                                   ).c_str() );
                    }
                }

            } while( ++n < FTDI_READ_RETRIES );

            return count;

        } //}}}


        // This is the maximum amount of data we allow for a single transfer.
        //{{{
        // Mostly it is chosen to limit the amount of time we might block on
        // waiting for a single transfer to complete.
        //
        // Returns the actual size that was set, which may have been clamped to
        // the maximum transfer size for the device, or rounded to the next
        // largest multiple of the maximum packet size.
        //
        // NOTE:
        // Changing the chunk size will recreate the internal chunk buffer and
        // discard any data that was still in it at the time.  There is no
        // internal locking of that here, so it is the caller's responsibility
        // to ensure nothing is concurrently trying to access it (ie. in a call
        // to ftdi_read) and that discarding any data in it will be safe and/or
        // acceptable.
        //}}}
        size_t SetChunkSize( size_t bytes )
        { //{{{

            if( bytes > m_dev->GetMaxTransferSize() )
                bytes = m_dev->GetMaxTransferSize();

            // Round up the desired chunksize to the next multiple of m_maxpacket.
            size_t  chunksize = bytes + m_maxpacket - 1 - (bytes - 1) % m_maxpacket;

            if( chunksize != m_chunksize )
            {
                if( m_chunkbuf )
                {
                    delete [] m_chunkbuf;
                    m_chunkbuf = NULL;      // Just in case new throws ...
                }

                m_chunkbuf  = new uint8_t[chunksize];
                m_chunksize = chunksize;
                m_chunkhead = 0;
                m_chunklen  = 0;
            }

            return m_chunksize;

        } //}}}

        // Set the timeout for completing short packets when there is no more data to send
        //{{{
        // It is usually better to use an explicit flush, like MPSSE_SEND_IMMEDIATE
        // or the other on-chip triggers, than to try to tune throughput with this.
        // So normally this should be set to a large enough value to permit complete
        // transfer of the largest expected packet size, without truncating them due
        // to a timer expiry.  There is some inherent latency in the chip which can
        // make that be a slightly longer time than the theoretical transfer time of
        // the data.
        //
        // NOTE:
        // Calling this does not in itself change the current latency setting (and
        // changing it on the fly between transfers is known to get the chip into a
        // confused state in some circumstances, so you should rethink if you really
        // want to do that anyway).  The desired latency must be calculated and set
        // before calling InitMPSSE(), since that is the only place at present where
        // the value set here will be used.
        //}}}
        void SetLatency( unsigned ms )
        { //{{{

            if( ms < 1 || ms > 255 )
                ThrowError( _("FTDI::SetLatency( %u ): invalid value, must be > 0 and < 255"), ms );

            m_latency = ms;

        } //}}}


        // Put the chip into MPSSE mode
        bool InitMPSSE()
        { //{{{

            // Initialise MPSSE mode (ref AN-135 4.2)
            ftdi_reset();

            purge_read();
            ftdi_set_special_chars();
            ftdi_set_latency_timer( uint8_t(m_latency) );
            ftdi_set_flow_control( FLOW_RTS_CTS );
            ftdi_set_bitmode( BITMODE_RESET );
            ftdi_set_bitmode( BITMODE_MPSSE );

            // Wait 50ms for all of that to settle
            usleep(50000);

            m_linestatus = uint8_t(ftdi_get_modem_status() & 0xFF);

            try {
                // Sometimes the first write here just won't get a response,
                // for reasons that seem to be something to do with persistent
                // state inside the FTDI that isn't cleared by any sort of soft
                // reset.  So retry it again once before starting another full
                // reset cycle, because that will often be enough to clear it.
                return (check_sync(0xAA) && check_sync(0xAB))
                    || (check_sync(0xAA) && check_sync(0xAB));
            }
            BB_CATCH_ALL( 0, _("FTDI::InitMPSSE: sync failed") )

            return false;

        } //}}}

        void ResetBitmode()
        { //{{{

            if( ! m_dh )
                return;

            try {
                // If any of these fail, the rest almost certainly will too.
                // The most likely reason being the device is already gone.
                purge_read();
                ftdi_set_bitmode( BITMODE_RESET );
                ftdi_reset();
            }
            BB_CATCH_ALL( 2, _("FTDI: ResetBitmode failed") )

        } //}}}


    public:

        FTDI( const USBContext::Device::Handle &dev, bool claim_now = true )
            : m_dev( dev )
            , m_timeout( 5000 )         // milliseconds
            , m_latency( 1 )
            , m_index( FTDI_INTERFACE_A )
            , m_configuration( 1 )      // bConfigurationValue
            , m_interface( 0 )          // bInterfaceNumber
            , m_altsetting( 0 )         // bAlternateSetting
            , m_linestatus( 0 )
            , m_chunksize( 0 )
            , m_chunkhead( 0 )
            , m_chunklen( 0 )
            , m_chunkbuf( NULL )
        { //{{{

            LogMsg<2>( "+ FTDI" );

            // Sanity check some things before we access them.
            try {
                const USBContext::Device::
                                AltSetting  &alt = m_dev->GetConfiguration( m_configuration )
                                                   .GetInterface( m_interface )
                                                   .GetAltSetting( m_altsetting );

                if( alt.endpoint.size() != 2 )
                    throw Error( _("Configuration %u, Interface %u, AltSetting %u "
                                   "has %zu endpoints, expecting 2"), m_configuration,
                                    m_interface, m_altsetting, alt.endpoint.size() );

                m_maxpacket = alt.endpoint[0].wMaxPacketSize;
                m_epin      = alt.endpoint[0].bEndpointAddress;
                m_epout     = alt.endpoint[1].bEndpointAddress;

                m_expect_modemstatus = uint8_t(FTDI_DSR | FTDI_CTS |
                                               (m_maxpacket == 64 ? FTDI_MAX64 : FTDI_MAX512));
            }
            catch( const std::exception &e )
            {
                ThrowError( _("FTDI: %s"), e.what() );
            }

            // We could probably default this one safely to 64 if we couldn't read it,
            // but that's also probably a sign of something bigger gone wrong too ...
            if( ! m_maxpacket )
                ThrowError( _("FTDI: failed to get maximum packet size") );

            // We get two 'modem status' junk bytes at the start of every packet,
            // so make sure there will actually be room for some data too.
            if( m_maxpacket <= 2 )
                ThrowError( _("FTDI: maximum packet size %u is smaller than the protocol overhead"),
                                                                                    m_maxpacket );

            if( USBContext::Device::Endpoint::Direction( m_epin ) != LIBUSB_ENDPOINT_IN )
                ThrowError( _("FTDI: device endpoint[0] direction is not 'IN'") );

            if( USBContext::Device::Endpoint::Direction( m_epout ) != LIBUSB_ENDPOINT_OUT )
                ThrowError( _("FTDI: device endpoint[1] direction is not 'OUT'") );

            if( claim_now )
                Claim();

            SetChunkSize( 65536 );

        } //}}}

        ~FTDI()
        { //{{{

            LogMsg<2>( "- FTDI" );

            Release();

            if( m_chunkbuf )
                delete [] m_chunkbuf;

        } //}}}


        // This may be called whether we've claimed the device interface or not.
        //{{{
        // If the device configuration cannot be restored, then the device
        // may be disconnected and reconnected, in which case this will then
        // throw a USBError with LIBUSB_ERROR_NOT_FOUND set, the claim on the
        // device interface (if it was held) will be released, and this FTDI
        // instance will henceforth be invalid (since the device it refers to
        // will no longer exist).
        //
        // If hotplug is enabled, the device should be re-enumerated again
        // normally (if it can be reconnected).  Otherwise you may need to
        // rescan the bus to find it again.
        //}}}
        void SoftReset()
        { //{{{

            if( ! m_dh )
            {
                m_dev->OpenDevice()->SoftReset();
                return;
            }

            try {
                m_dh->SoftReset();
            }
            catch( ... )
            {
                m_dh = NULL;
                throw;
            }

        } //}}}


        // Return true if we currently have a claim on the device interface.
        bool IsClaimed() const
        {
            return m_dh != NULL;
        }

        // Returns true if the device was (newly) claimed by calling this,
        //{{{
        // or false if it was already claimed by us.  Will throw if getting
        // a claim on the device fails.
        //
        // There is no reference count on claims.  No matter how many times
        // you call this, the first call to Release() will drop the claim.
        //}}}
        virtual bool Claim()
        { //{{{

            if( m_dh != NULL )
                return false;

            try {
                m_dh = m_dev->OpenDevice();
                m_dh->SetConfiguration( m_configuration );
                m_dh->ClaimInterface( m_interface );

                if( m_altsetting )
                    m_dh->SetAltInterface( m_interface, m_altsetting );

                return true;
            }
            catch( ... )
            {
                m_dh = NULL;
                throw;
            }

        } //}}}

        // Release the current claim on this device.
        //{{{
        // It is the responsibility of the caller to ensure that no other
        // functions which might access the device are called while a claim
        // on it is not held.
        //}}}
        virtual void Release()
        {
            m_dh = NULL;
        }


        // If the endpoint_address isn't specified explicitly, try to clear
        // a stall from all endpoints of the currently claimed interface(s).
        void ClearHalt( unsigned endpoint_address = 0x100 )
        {
            if( m_dh != NULL )
                m_dh->ClearHalt( endpoint_address );
        }


        // Return true if this is device d.
        bool IsDevice( const USBContext::Device::Handle &d )
        { //{{{

            // A null device never matches anything.  Just like SQL!
            if( ! m_dev || ! d )
                return false;

            return *m_dev == *d;

        } //}}}


        size_t GetChunkSize() const
        {
            return m_chunksize;
        }

        unsigned GetLatency() const
        {
            return m_latency;
        }

        unsigned GetMaxPacketSize() const
        {
            return m_maxpacket;
        }

        const std::string &GetManufacturer() const
        {
            return m_dev->GetManufacturer();
        }

        const std::string &GetProduct() const
        {
            return m_dev->GetProduct();
        }

        const std::string &GetSerial() const
        {
            return m_dev->GetSerial();
        }


        std::string ProductStr() const
        {
            return m_dev->ProductStr();
        }


        // Convenience methods for Logging
        //{{{

        BB_PRINTF_FORMAT(2,3)
        std::string ErrorStr( const char *format, ... ) const
        { //{{{

            va_list         arglist;
            std::string     msg( m_dev->GetSerial() );

            va_start( arglist, format );
            msg.append( ": " )
               .append( vstringprintf( format, arglist ) );
            va_end( arglist );

            return msg;

        } //}}}


        template< int N >
        BB_PRINTF_FORMAT(2,3)
        void LogError( const char *format, ... ) const
        { //{{{

            va_list         arglist;
            std::string     msg( m_dev->GetSerial() );

            va_start( arglist, format );
            msg.append( ": " )
               .append( vstringprintf( format, arglist ) );
            va_end( arglist );

            Log<N>( "%s\n", msg.c_str() );

        } //}}}

        BB_NORETURN BB_PRINTF_FORMAT(2,3)
        void ThrowError( const char *format, ... ) const
        { //{{{

            va_list         arglist;
            std::string     msg( m_dev->GetSerial() );

            va_start( arglist, format );
            msg.append( ": " )
               .append( vstringprintf( format, arglist ) );
            va_end( arglist );

            throw Error( msg );

        } //}}}


        template< int N >
        BB_PRINTF_FORMAT(3,4)
        void LogUSBError( int err, const char *format, ... ) const
        { //{{{

            va_list         arglist;
            std::string     msg( m_dev->GetSerial() );

            va_start( arglist, format );
            msg.append( ": " )
               .append( vstringprintf( format, arglist ) );
            va_end( arglist );

            Log<N>( "%s: %s\n", msg.c_str(), libusb_strerror(libusb_error(err)) );

        } //}}}

        BB_NORETURN BB_PRINTF_FORMAT(3,4)
        void ThrowUSBError( int err, const char *format, ... ) const
        { //{{{

            va_list         arglist;
            std::string     msg( m_dev->GetSerial() );

            va_start( arglist, format );
            msg.append( ": " )
               .append( vstringprintf( format, arglist ) );
            va_end( arglist );

            throw USBError( err, "%s", msg.c_str() );

        } //}}}


        BB_PRINTF_FORMAT(2,3)
        std::string MsgStr( const char *format, ... ) const
        { //{{{

            va_list         arglist;
            std::string     msg( m_dev->GetSerial() );

            va_start( arglist, format );
            msg.append( ": " )
               .append( vstringprintf( format, arglist ) );
            va_end( arglist );

            return msg;

        } //}}}

        template< int N >
        BB_PRINTF_FORMAT(2,3)
        void LogMsg( const char *format, ... ) const
        { //{{{

            va_list         arglist;
            std::string     msg( m_dev->GetSerial() );

            va_start( arglist, format );
            msg.append( ": " )
               .append( vstringprintf( format, arglist ) );
            va_end( arglist );

            Log<N>( "%s\n", msg.c_str() );

        } //}}}

        //}}}

    }; //}}}

}   // BitB namespace


#endif  // _BB_FTDI_DEVICE_H

// vi:sts=4:sw=4:et:foldmethod=marker
