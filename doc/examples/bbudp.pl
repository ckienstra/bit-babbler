#!/usr/bin/perl -w

# A trivial example for obtaining random bits from the seedd UDP socket.
# It assumes that the option '--udp-out 127.0.0.1:1200' was passed to a
# runing instance of seedd.
#
# It takes one command line parameter, the desired number of random bytes,
# and will output them formatted as hexadecimal digits.  For example:
#
#   $ ./bbudp 10
#   read: 8c46b4d2a9a1424cd587
#
# This file is distributed as part of the bit-babbler package.
# Copyright 2015,  Ron <ron@debian.org>

use strict;

use IO::Socket;

my $addr = '127.0.0.1';
my $port = 1200;
my $max_msg_size = 32768;
my $data;
my $flags;

my ($bytes_requested) = (shift // "") =~ /^(\d+)$/a
    or die "Usage: $0 <number of bytes to read>\n";

die "Not reading 0 bytes\n"              if $bytes_requested < 1;
die "Maximum request is $max_msg_size\n" if $bytes_requested > $max_msg_size;


my $sock = IO::Socket::INET->new(
    Proto    => 'udp',
    PeerAddr => $addr,
    PeerPort => $port,
) or die "Could not create socket: $!\n";

# Send the requested number of bytes as a network-order short.
my $msg = pack("n*", $bytes_requested);

$sock->send($msg) or die "Failed to send request for $bytes_requested bytes: $!\n";
$sock->recv($data,$max_msg_size,$flags) or die "Failed to read datagram reply: $!\n";

# Display the binary octets as hex digits
print "read: " . unpack("H*", $data) . "\n";

