# Libsentinel

The goal with this library is to provide an API with 2 main functions to access the (older) Sentinel rebreather previously manufactured by VR Technologies and currently (up until the RedHead) by [VMS](http://www.vmsrebreathers.com/):

1. get_sentinel_dive_list which returns the header part of each dive found in the rebreather as a list of header structs with the log-member as null. This will take a list of dive header structs as the argument, expand and populate it with the dive metadata
2. download_sentinel_dive which fetches the full dive data for the given dive. This takes a pointer to a dive header struct and populates the log-part with the actual dive data

With the first one you get the list of dives stored on the rebreather(*) with most of the metadata (such as time, max depth, OTU, CNS etc). With the second you can retrieve all the data of a particular dive.

*) Although the rebreather only retains about 10h worth of actual dive data, the data of older dives will most probably be corrupted,

All data is stored in structs and arrays of structs with the last item as NULL to indicate the end of the array. In a similar way the arrays of strings have their last item as NULL.

There are also commands to change the rebreather settings, but currently I have no plans on implementing any of these.

## Development

A simple Makefile is provided. Assuming that all the necessary development packages (see below for the list of Fedora packages) are already present, the compilation is a simple execution of:

```
make
```

The outcome are two files, libsentinel.so and download, located under the usr/local, like so:

```
└── usr
    └── local
        ├── bin
        │   └── download
        └── lib
            └── libsentinel.so
```

The usage of download is:

```
download -d <device> -f <num> -h -l -n <num> -t <num> -v
-d <device> Which device to use, usually /dev/ttyUSB0
-f <num> Optional: Start downloading from this dive, list the dives first to see the number
-h This help
-l List the dives
-n <num> Download this specific dive, list the dives first to see the number
-t <num> Download the dives including this one, list the dives first to see the number
-v Be more verbose
```

In order to do development on the library, you most probably want to use the emulator.

The emulator is a perl-script and is located in the mockup/sentinel_serial_emulator directory. There is a README file with more details on how to set up the emulator. If you follow the instructions, you should be able to connect to the emulator by using /tmp/sent1 as the device. Note that the program will give an error about not setting the RTS, this can be disregarded when using the the emulator.

Once you have the binary compiled, socat and emulator running, you can either manually contact the emulator with the command:

```
usr/local/bin/download -d /tmp/sent1 -l
```

Alternatively you can run the binary under valgrind with the Makefile target valgrind giving it the parameter PORT like so:

```
make valgrind PORT=/tmp/sent1
```

Currently you can use the -f, -t or -n to indicate the start/end, or what specific dive you want to download or -l to list the dives on the rebreather.

## Commands and responses over the serial port

These have been eeked out by listening to the communication between the original piece of software (ProLink) and the rebreather.

Hex | Command | Meaning
----|---------|--------
4D | M | Get the list of dive headers
44 &lt;hex&gt; | D&lt;hex&gt; | Get the dive data of the given dive, you need to have the list of dive headers to know which one is what. The hex starts at 30 (0 in decimal) for the most recent dive, and depending on how many there are, goes on, even beyond 39 (9)
52 | R | Get the record interval as well as some other settings of the rebreather, the response should be r\r\n&lt;int&gt;\r\n
48 | H | Unknown
53 &lt;hex&gt; 46 | S&lt;int&gt;F | Set the recording interval to &lt;int&gt; seconds, response should be something like: s\r\nfound 2\r\n&lt;int&gt;F\r\n+^+\r\n&lt;int&gt;\r\n
50 | P | wait byte from the rebreather indicating that it will accept commands

## TODO

- [*] lib: Download the list of dives
- [*] lib: Download a given dive data
- [ ] lib: Better verbose-handling
- [ ] lib: Check function return values
- [x] exe: Printout of header list

## Caveats and disclaimers

This piece of software is provided as-is without any warranties nor any fitness for a particular purpose and none of the makers can be held responsible for any kind of damage. In short, if you use this software and it breaks your stuff, you can keep both pieces.

The software is distributed under the LGPLv2 license (https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt), please familiarize yourself with the terms.

The makers of this software do not have any affiliation with neither VR Technologies nor VMS, so there is no point trying to contact them regarding this piece of software.
