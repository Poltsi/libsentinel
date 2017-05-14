# Sentinel rebreather emulator

Written in Perl.

To run this you probably need to install the perl Serial-modules.

In Ubuntu it is the `libdevice-serialport-perl`

In Fedora, you need to install the package `perl-Device-SerialPort`

The emulator has been tested on Fedora 23/24/25, as well as Ubuntu 16.10 and 17.04

## How to get started

Start socat with the following command:

```
socat -d -d -d pty,b9600,echo=1,raw,csize=3,ispeed=9600,ospeed=9600,user=poltsi pty,b9600,echo=1,raw,csize=3,ispeed=9600,ospeed=9600,user=<your username>
```

Note that you need to replace the username in the command above

Observe which pts's it connects to, and after that you can fire up the emulator in another terminal with the command:

```
./sentinel_serial_emulator.pl /dev/pts/<pts id>
```

Note that if the emulator starts to complain about 'Unknown command P' continuously then you need to restart the emulator, switching the pts to the other one.

## How does it work

There are 3 files in the same directory, 1.txt, 2,txt and 3.txt. These are the actual data that comes out of the Sentinel rebreather when you send the download command for a specific dive. The files are of different versions 