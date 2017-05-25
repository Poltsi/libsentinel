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

Note that if the emulator starts to complain about 'Read unknown command: P' continuously then you need to restart the emulator, switching the pts to the other one.

## How does it work

There are 3 files in the same directory, 1.txt, 2,txt and 3.txt. These are the actual data that comes out of the Sentinel rebreather when you send the download command (D<int> for a specific dive. The files are of different versions of the rebreather, V3.0C is the oldest version which does not contain the cell status, V009A is for rebreathers with serial number around 300, and the last Sentinel rebreathers may have V009B.

When the header list command (M) is sent, then the emulator sends the header part of the dive file up until gas as a response. This means that the V3.0C stops at Dusage, while the newer versions stop at Dcellhealth.

## Other means of debugging

If you are unsure that the communication is working correctly from the emulator, you can use a terminal software such as minicom to manually talk to the emulator (or the rebreather for that matter). Start minicom with the following parameters:


```
minicom -b 9600 -D /dev/pts/<pts id> -w -8
```

When you have started the program you should be seeing a steady stream of P coming in.

You probably want to also enable local echo so that you can see what you are actually typing. Once you have started the program, type the following keys: &lt;esc&gt;-A-Z-e

Single character commands, such as M can be typed directly to the terminal, but if you want to send multicharacter commands (like D1), then you need to copypaste them, as there would otherwise be a too long delay between the keystrokes. 

Note that you can not run the actual software concurrently as minicom locks the device.