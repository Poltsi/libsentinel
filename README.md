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

In order to do development on the library, you most probably want to use the emulator.

The emulator is a perl-script and is located in the mockup/sentinel_serial_emulator directory. There is a README file with more details on how to set up the emulator.

Once you have the socat and emulator running, you can either ma