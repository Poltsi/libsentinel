#!/usr/bin/perl
##
 # Sentinel Serial Emulator
 #
 # Copyright (C) 2017 Paul-Erik Törrönen
 #
 # This library is free software; you can redistribute it and/or
 # modify it under the terms of the GNU Lesser General Public
 # License as published by the Free Software Foundation; either
 # version 2.1 of the License, or (at your option) any later version.
 #
 # This library is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 # Lesser General Public License for more details.
 #
 # You should have received a copy of the GNU Lesser General Public
 # License along with this library; if not, write to the Free Software
 # Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 # MA 02110-1301 USA
 ##

use strict;
use warnings;
use Device::SerialPort qw( :PARAM :STAT 0.07 );
use Time::HiRes qw( usleep );
use Data::Dumper;

# Read the dive data
# TODO: Detect version of dive data and accordingly also recognize the number of header lines.
#       The header data should essentially contain everything up to the gas list.
#       For V3.0C, which does not have the cell health, this is to line 15, while for V009A and
#       V009B it is up to line 28
my @diveData;
@{ $diveData[ 0 ] } = &readDiveData( '1.txt' );
@{ $diveData[ 1 ] } = &readDiveData( '2.txt' );
@{ $diveData[ 2 ] } = &readDiveData( '3.txt' );
@{ $diveData[ 3 ] } = &readDiveData( '4.txt' );
@{ $diveData[ 4 ] } = &readDiveData( '5.txt' );
@{ $diveData[ 5 ] } = &readDiveData( '6.txt' );

$| = 1;

my $port = shift;

if( ! length( $port ) )
{
    print( "ERROR! You forgot to give the serial port as an argument. Exiting...\n" );
    exit( 1 );
}

my $conf = '~/.sentinel_serial.conf';
my $ob = Device::SerialPort->new( $port, 1 ) || die "Can't open $port: $!";
my $baudRate = 9600;
# 8 bits + 2 stop bits
my $byteRate = $baudRate / 10;

my $STALL_DEFAULT = 10;
my $timeout       = $STALL_DEFAULT;

my $arb    = $ob->can_arbitrary_baud;
my $data   = $ob->databits( 8 );
my $baud   = $ob->baudrate( $baudRate );
my $parity = $ob->parity( "even" );
my $hshake = $ob->handshake( "rts" );
my $stop   = $ob->can_stopbits;
my $rs     = $ob->is_rs232; 
my $total  = $ob->can_total_timeout;
my $serialBufferSize = 4096;

$ob->stopbits( 1 );
$ob->buffers( $serialBufferSize, $serialBufferSize );
$ob->can_baud;
$ob->can_databits;
$ob->can_dtrdsr;
$ob->can_handshake;
$ob->can_parity_check;
$ob->can_parity_config;
$ob->can_parity_enable;
$ob->can_rtscts;
$ob->can_xonxoff;
$ob->can_xon_char;
$ob->can_spec_char;
$ob->can_interval_timeout;
$ob->can_ioctl;
$ob->can_status;
$ob->can_write_done;
$ob->can_modemlines;
$ob->can_wait_modemlines;
$ob->can_intr_count;
$ob->write_settings;

if( $ob->can_write_done )
{
    print( "Serial write done polling enabled\n" );
}
else
{
    print( "Serial write done polling is disabled\n" );
}


my $jump    = 1;
my $counter = 0;

while( $jump )
{
    if( $counter > 1000 * 1500 )
    {
        my $payload = "P";
        &printToDevice( $ob, $payload );
        $counter = 0;
    }

    my $inBytes = 4;
    my ( $count_in, $string_in ) = $ob->read( $inBytes );

    #warn "read unsuccessful\n" unless ( $count_in == $inBytes );
    if( $count_in > 0 )
    {
        if( $string_in eq 'RN' )
        {
            &printNumberOfDives( $ob );
        }
        elsif( $string_in eq 'M' )
        {
            &printDiveEntries( $ob );
        }
        elsif( $string_in =~ m/D(\d+)/ )
        {
            &printSingleDive( $ob, $1 );
        }
        else
        {
            print( "Read unknown command: " . $string_in . "\n" );
        }
    }

    usleep( 1500 );
    $counter += 1500;
}

$ob->close;
undef $ob;

exit( 0 );

###################################################################################
# Subfunctions
###################################################################################


###################################################################################
#
# readDiveData: Reads the dive data from the given file into an array
#

sub readDiveData
{
    my $fileName = shift;
    print( "Loading to array file: " . $fileName . "\n" );
    open my $handle, '<', $fileName;
    $/ = "\r\n";
    chomp( my @lines = <$handle> );
    close $handle;

    return( @lines );
}

###################################################################################
#
# printDiveEntries: Prints the header part of the dives. Depending of the version
#                   of the log, this varies but should be everything up until the
#                   list of gases
#

sub printDiveEntries
{
    my $ob = shift;
    # The actual data to be sent
    my $payload = "";
    my $idx = 0;

    while( exists( $diveData[ $idx ] ) )
    {
        # Skip the first line as it is the <\s>d\r\n
        my $ldx = 1;
        $payload .= "d\r\n";

        while( $diveData[ $idx ][ $ldx ] !~ m/^Gas\s/ )
        {
            $payload .= $diveData[ $idx ][ $ldx ] . "\r\n";
            $ldx++;
        }

        $idx++;
    }

    # Last line should be End\r\n
    $payload .= "End\r\n";
    &printToDevice( $ob, $payload );

    return();
}

###################################################################################
#
# printNumberOfDives: Print how many dives we have
#

sub printNumberOfDives
{
    my $ob = shift;
    # First the character for initialization
    my $payload = "d\r\n";
    print( "Printing the dive info\n" );
    &printToDevice( $ob, $payload );
    
    $payload = "ver=V009B\r\n";
    &printToDevice( $ob, $payload );

    return();
}

###################################################################################
#
# printSingleDive: Print the selected dive
#

sub printSingleDive
{
    my $ob = $_[ 0 ];
    my $diveNumber = 0;

    if( defined( $_[ 1 ] ) )
    {
        $diveNumber = $_[ 1 ];
    }

    print( "=============================================================\n" );
    print( "printSingleDive: Called with dive number: " . $diveNumber . "\n" );

    # TODO: Check that the divenumber actually exists
    my $payload = join( "\r\n", @{$diveData[ $diveNumber ]} );

    my $output = &printToDevice( $ob, $payload );
    
    print( "Wrote data body, length: " . $output . "\n" );

    print( "Wrote data dump for dive: " . $diveNumber . "\nPayload length: " . length( $payload ) . "\n" );

    return();
}

###################################################################################
#
# printToDevice: Takes a data string and prints it to the device in the same manner
#                as Sentinel does, one byte at the time
#

sub printToDevice
{
    my $ob   = $_[ 0 ];
    my $data = $_[ 1 ];

    #print( "printToDevice: Called with data length: " . length( $data ) . "\n" );
    my $offset = 0;

    for my $char ( unpack '(a1)*', $data )
    {
        my $count_out = $ob->write( $char );

        if( ! $count_out )
        {
            print( "ERROR: write failed after " . $offset . " writes\n" );
            last;
        }
        else
        {
            if( $count_out != length( $char ) )
            {
                warn( "WARNING: Chunk write incomplete after " . $offset . " writes\n" );
                last;
            }
        }
        usleep( 300 );
        $offset++;
    }

    print( "printToDevice: Wrote " . $offset . " bytes\n" );
    $ob->write_drain;

    return( $offset );
}
