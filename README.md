# Libsentinel

The goal with this library is to provide an API with 3 main functions:

1. get_sentinel_dive_list which returns the header part of each dive found in the rebreather as a list of header structs with the log-member as null
2. get_sentinel_dives_all which returns essentially the same list above, with the addition of also having the log member populated
3. get_sentinel_dive returns the header and log part of a particular dive

There are commands to change the rebreather settings, but currently I have no plans on implementing any of these.

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

The emulator is a perl-script and is located in the mockup/sentinel_serial_emulator directory. There is a README file with more details on how to set up the emulator.

Once you have the binary compiled, socat and emulator running, you can either manually contact the emulator with the command:

```
usr/local/bin/download -d /dev/pts/<pts id> -l
```

Currently only the header list command (-l) is enabled.

Alternatively you can run the binary under valgrind with the Makefile target valgrind giving it the parameter PORT like so:

```
make valgrind PORT=/dev/pts/<pts id>
```

## TODO

- [ ] lib: Download a given dive data
- [ ] lib: Better verbose-handling
- [ ] lib: Check function return values
- [ ] exe: Printout of header list
