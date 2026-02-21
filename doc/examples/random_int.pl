#!/usr/bin/perl -w

# This is mainly an example of one way to correctly generate random numbers in
# an arbitrary integer range from entropy obtained by a BitBabbler.  Since the
# BitBabbler itself just outputs a continuous stream of random bits, some care
# is needed if you want random numbers in a range which isn't a perfect power
# of two, but still require every value to have an equal probability of being
# selected.
#
# History gives us plenty of examples where naive attempts at generating random
# numbers in some range (like people applying a simple modulus to the output of
# rand(3) or a similar function) create an exploitable, or at least undesirable
# bias - where some numbers will be selected more or less frequently than would
# be expected by chance. So it's worth having an example of how to do it right.
#
# The algorithm used here is not the most efficient known in terms of the input
# entropy potentially needed to generate each number that is output - but it is
# very simple and easy to not get wrong in your own code, and unless you need a
# really large quantity of numbers quickly, the proportion of raw entropy which
# may be wasted here won't usually be a cause for concern when the rate that we
# can create it means it's not a scarce resource.
#
# The basic operation is quite trivial.  Obtain enough bits from the BitBabbler
# to give us a number that could be larger than the desired range.  Check if it
# is within the desired range.  If it is, we're done.  If it is not, we discard
# those bits then obtain another set.  Repeat until you have as many numbers as
# you require.
#
# For a range that does not have zero as its smallest number, we can optimise a
# little to only require enough bits to cover the distance between the smallest
# number in the range and the largest one.
#
# The actually interesting parts of this example for that are all found in the
# request_entropy() function, which gets raw bits from the BitBabbler via the
# UDP socket interface of seedd, and the get_next_number() function, which does
# the transformation to the desired range.  (all the rest is mostly just sanity
# checking the command line request and a self-test to sanity check the output)
#
# This example assumes seedd is providing a UDP socket on 127.0.0.1, port 1200,
# (i.e. '--udp-out 127.0.0.1:1200' was passed to a running instance of it) and
# has two main modes of operation:
#
#  - If invoked with up to three command line arguments, it will output numbers
#    in the selected range.
#
#      random_int.pl max [min] [count]
#
#    Where:
#
#    max:   is the largest number (inclusive) in the desired range.
#           This argument must be provided.
#
#    min:   is the smallest number (inclusive) in the desired range.
#           If not provided, it will default to 0.
#
#    count: is the quantity of numbers wanted in this range.
#           If not provided, it will default to 1.
#
#    The generated numbers will be sent to stdout, separated by newlines.
#    All other output is sent to stderr.
#
#  - If invoked with a fourth command line argument, it will operate in a self
#    test mode.
#
#      random_int.pl max [min] [count] [test-count]
#
#    In this mode, the 'count' argument is ignored, and enough numbers will be
#    generated in the min - max range to expect that an average of 'test-count'
#    occurrences of each possible value will be seen.
#
#    A report will then be generated to stdout which shows the actual number of
#    occurrences seen for each value, and the spread of frequencies with which
#    the individual values were seen.
#
#    On its own, this isn't a rigorous test of the quality of the distribution
#    of the output, but combined with the QA checking of the raw entropy that
#    is done by seedd, it should give an easy visual indication of any serious
#    problem that may be present in the transformation code for a given range.
#
# When all output is completed, the amount of entropy 'wasted' in obtaining the
# desired numbers will be shown (also to stderr).  On average it is expected
# that this will be proportional to how far from being a power of two the range
# of desired numbers is.  The greater that distance, the higher the probability
# is that we will randomly get (and so discard) a number that is outside of the
# requested range.
#
#
# This file is distributed as part of the bit-babbler package.
# Copyright 2017,  Ron <ron@debian.org>

use strict;

use IO::Socket;
use POSIX qw(ceil);
use List::Util qw(reduce max);


# The seedd UDP socket to connect to, and maximum allowed packet size.
my $addr = '127.0.0.1';
my $port = 1200;
my $max_msg_size = 32768;



# Get max, min, and counts from the command line with some sanity checking.
my ($max_val)   = (shift // "") =~ /^(\d+)$/a
    or die "Usage: $0 <max> [min] [count] [test-count]\n";

my ($min_val)   = (shift // 0) =~ /^(\d+)$/a;
my ($count)     = (shift // 1) =~ /^(\d+)$/a;
my ($testcount) = (shift // 0) =~ /^(\d+)$/a;

die "Max value ($max_val) must be greater than min value ($min_val).\n"
    unless $max_val > $min_val;

die "Count should be greater than 0 if you want more than this message.\n"
    unless $count > 0 || $testcount > 0;



# Define some math convenience functions for the calculations we need to do.
sub log2($)
{
    return log(shift) / log(2);
}

sub log10($)
{
    return log(shift) / log(10);
}

# Return the minimum number of bits needed to represent an integer value.
sub bits_needed_for($)
{
    # +1 because we want the number of bits needed to store the given value
    # not that number of values counting from zero.
    return ceil(log2((shift) + 1));
}

# Return the number of bytes needed to hold some number of bits.
sub bytes_needed_for($)
{
    return ceil((shift) / 8);
}

# Return the number of decimal digits needed to output an integer value.
sub digits_needed_for($)
{
    return ceil(log10((shift) + 1));
}



# Calculate how much entropy we need to obtain a number in the desired range
# and how to manipulate it into that range.
my $range     = $max_val - $min_val;
my $nbits     = bits_needed_for($range);
my $nbytes    = bytes_needed_for($nbits);
my $downshift = $nbytes * 8 - $nbits;

# Tell the user what we are going to do.
if ($testcount) {
    warn "Testing $testcount value" . ($testcount > 1 ? 's' : '')
       . " per bin between $min_val and $max_val.\n";
} else {
    warn "Requested $count value" . ($count > 1 ? 's' : '')
       . " between $min_val and $max_val.\n";
}

# Do some final sanity checking on what was requested.
die "Not reading 0 bytes\n"              if $nbytes < 1;
die "Maximum request is $max_msg_size\n" if $nbytes > $max_msg_size;

# And report some detail about how we're going to do it.
warn "Need to read $nbits significant bits ($nbytes bytes >> $downshift)"
   . " for range of $range.\n";



# We're ready, let's do this.  Create a socket for obtaining entropy.
my $sock = IO::Socket::INET->new(
    Proto    => 'udp',
    PeerAddr => $addr,
    PeerPort => $port,
) or die "Could not create socket: $!\n";

# And define a convenience function to read some amount of entropy from it.
sub request_entropy($)
{
    my $bytes_requested = shift;
    my $data;
    my $flags;

    # Send the requested number of bytes as a network-order short.
    my $msg = pack("n*", $bytes_requested);

    $sock->send($msg) or die "Failed to send request for $bytes_requested bytes: $!\n";
    $sock->recv($data,$max_msg_size,$flags) or die "Failed to read datagram reply: $!\n";

    # And return them as a block of binary data.
    return $data;
}



# Keep some statistics on how efficiently we obtained the desired numbers.
my $requests_made   = 0;
my $attempts_needed = 0;

# This function is the actual meat of this example, turning raw entropy that is
# read from the BitBabbler into a number within the desired range with an equal
# probability for obtaining every number in the range.
sub get_next_number()
{
    my $padding = pack('C8', 0);

    ++$requests_made;

    # Loop until we get a number that is in the requested range.
    # In theory, the number of requests this might take is unbounded, but in
    # practice, on average, the number of requests needed is proportional to
    # how far the requested range is from being the next larger power of 2.
    # (With enough samples, the average will converge on that proportion ever
    # more precisely, in the same way that Monte Carlo estimation of Pi does).
    while(1)
    {
        ++$attempts_needed;

        # Request the amount of entropy we need for the size of the range.
        my $entropy = request_entropy($nbytes);

        # Unpack the binary data as an unsigned "quad" (64 bit), little-endian
        # number, adding enough trailing (most significant bit) padding to the
        # entropy to ensure that we have at least 64 bits of data to unpack.
        # Then shift those bits to the right if needed, to obtain the smallest
        # (non byte aligned) number of bits needed to cover the whole range.
        # Finally, add the minimum value to that to put the result between the
        # desired floor and a ceiling greater than or equal to the maximum
        # value wanted.
        #
        # (For those who don't normally speak perl, the 'unpack' here is just
        # the equivalent of casting the raw data bits to a uint64_t type)
        my $n = (unpack("Q<", $entropy . $padding) >> $downshift) + $min_val;

        # Return the result if it is not larger than the range maximum (we
        # already know that it must be at least the minimum requested value).
        return $n if $n <= $max_val;

        # Otherwise, try again until we succeed.
        #warn "        rejected $n\n";
    }
}


# Finally, is this a test-run, or a request to output some numbers ...
if ($testcount) {

    # We're in self-test mode to produce some statistics about the distribution
    # of numbers that are actually obtained in the selected range.  We don't
    # do any numeric analysis here of whether the reported statistics are in a
    # normally expected range, but assuming that the raw entropy really is good
    # (which is already tested by the QA done in seedd), we would expect that
    # any bug in the transformation done here to map them to the desired range
    # should normally show up as a glaringly obvious glitch just by eye in what
    # we do already report here.  This is just a way for people to quickly
    # reassure themselves that things probably are in fact working as expected.

    my $numbins = $range + 1;
    my $trials  = $numbins * $testcount;
    my %bin;
    my %counts;

    warn "Collecting $trials test results, please wait ...\n";
    while ($trials--)
    {
        # Show a progress spinner, because this could take a while
        # depending on the range and number of trials chosen for it.
        print STDERR "\rRemaining trials $trials            " if $trials % 1000 == 0;
        ++$bin{get_next_number()};
    }
    warn "\n\n";


    # For each number in the desired range that was obtained, show how many
    # times it was returned in this trial.  On average each number should be
    # seen roughly $testcount number of times.
    print " Frequency of each value:\n";
    print " Note: not all values were observed at least once.\n"
            if $numbins != scalar(keys %bin);

    # Find the number of digits needed to align output with the maximum value
    my $dmax = digits_needed_for($max_val);

    for ($min_val .. $max_val)
    {
        if (exists $bin{$_}) {
            push @{$counts{$bin{$_}}}, $_;
            printf "  %*d: %d\n", $dmax, $_, $bin{$_};
        } else {
            # Report values that did not occur at all in the counts table,
            # but don't print them in this list.
            push @{$counts{0}}, $_;
        }
    }

    print "--------------------------\n\n";


    # Find the number of digits needed to nicely format the frequency count
    # and the number of values that were seen with each frequency.
    my $maxcount = reduce { my $n = scalar(@{$counts{$b}}); $a > $n ? $a : $n } 0, keys %counts;
    my $dcount   = digits_needed_for($maxcount);
    my $dfreq    = digits_needed_for(max keys %counts);

    # For each number of times a value was seen, show the values that were
    # seen that number of times.  For a sufficiently large number of trials
    # the number of values at each frequency should be normally distributed
    # around the expected average frequency of $testcount.
    print " Values at each frequency:\n";
    for (sort { $a <=> $b } keys %counts)
    {
        printf "  %*d: (%*d)", $dfreq, $_, $dcount, scalar(@{$counts{$_}});
        printf " %*s", $dmax, $_ for @{$counts{$_}};
        print "\n";
    }

} else {

    # Otherwise, just output the list of numbers requested with no other frills.
    while ($count--)
    {
        print get_next_number() . "\n";
    }
}

# Give a final report on the rate of rejected attempts in the given range.
printf STDERR "\nAverage number of attempts needed to obtain each number: %0.4f\n",
                                                $attempts_needed / $requests_made;
# Or the same thing alternatively stated:
#printf STDERR "\nProportion of rejected attempts for this numeric range: %0.2f%%\n",
#                                    (1 - $requests_made / $attempts_needed) * 100;

# vi:sts=4:sw=4:et:foldmethod=marker
