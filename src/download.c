/*
 * libsentinel
 *
 * Copyright (C) 2017 Paul-Erik Törrönen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "libsentinel.h"

void print_help()
{
    printf("Usage:\n");
    printf("download -d <device> [ [-f <num>]  [-t <num>] | [-n <num>] ] [-v] | -h | -l\n");
    printf("Default behavior is to download all dives\n");
    printf("-d <device> Which device to use, usually /dev/ttyUSB0\n");
    printf("-f <num> Optional: Start downloading from this dive, list the dives first to see the number\n");
    printf("-h This help\n");
    printf("-l List the dives\n");
    printf("-n <num> Download this specific dive, list the dives first to see the number\n");
    printf("-t <num> Download the dives including this one, list the dives first to see the number\n");
    printf("-v Be more verbose\n");
    printf("\n");
}

int main(int argc, char **argv) {
    int c = 0;
    int from_dive = 0;
    int to_dive   = 0;
    char *device_name = malloc(sizeof(char));
    bool verbose    = false;
    bool list_dives = false;
    opterr = 0;

    while ((c = getopt (argc, argv, "d:f:hln:t:v")) != -1)
        switch (c) {
        case 'd': /* Set serial device to <device> */
            device_name = realloc(device_name, (strlen(optarg) + 1));
            strcpy(device_name, optarg);
            break;
        case 'f': /* Download dives (from dive header) */
            from_dive = atoi(optarg);
            break;
        case 'l': /* List dives (from dive header) */
            list_dives = true;
            break;
        case 'n': /* Download dive #n */
            from_dive = to_dive = atoi(optarg);
            break;
        case 't': /* Download all dives up to #n */
            to_dive = atoi(optarg);
            break;
        case 'v':
            verbose = true;
            dprint(verbose, "%s", "Verbose set");
            break;
        case 'h': /* Print help and exit */
        default:
            print_help();
            exit(0);
        }

    /* Some rudimentary checks */
    /* Sanity checks on from and to dive number if they are other than default (0 and 0)*/
    if (from_dive || to_dive) {
        if (list_dives) {
            eprint("%s", "Requesting both the list as well as downloading of dives is not supported. please choose either one");
            print_help();
            exit(1);
        }
        if (from_dive > to_dive) {
            eprint("The start (%d) of the requested dive list is bigger than the end (%d) of the list, aborting", from_dive, to_dive);
            print_help();
            exit(1);
        }

        if (from_dive < 0 || to_dive < 0) {
            eprint("Either start (%d) of the requested dive list or the end (%d) of the requested dive list is less than zero, aborting", from_dive, to_dive);
            print_help();
            exit(1);
        }

        dprint(verbose, "Printing dives from %d to %d", from_dive, to_dive);
    }

    /* Is the device string empty */
    if (!strlen(device_name)) {
        eprint("%s", "No device defined");
        print_help();
        exit(1);
    }

    struct stat sb;

    dprint(verbose, "Testing whether we can stat device: %s", device_name);
    if (stat(device_name, &sb) == -1) {
        eprint("Non-existing device: %s", device_name);
        exit(1);
    }

    dprint(verbose, "Testing whether we can get the type of device: %s", device_name);

    if ((sb.st_mode & S_IFMT) != S_IFCHR) {
        eprint("Either device '%s' does not exist or it is not a character device, aborting", device_name);
        print_help();
        exit(1);
    }

    dprint(verbose, "Opening the serial device: %s", device_name);
    int fd = connect_sentinel(device_name);

    if (fd <= 0) {
        eprint("Unable to connect to the device: %s", device_name);
        exit(1);
    }

    int tries = 20;
    if (!is_sentinel_idle(fd, tries)) {
        eprint("Could not connect to Sentinel after %d tries, is Sentinel connected?", tries);
        disconnect_sentinel(fd);
        exit(1);
    }

    dprint(verbose, "Connected to: %s", device_name);
    free(device_name);

    if (list_dives) {
        dprint(verbose, "%s", "Printing the list of dives");
        sentinel_header_t **header_list = NULL;
        bool res = get_sentinel_dive_list(fd, &header_list);
        disconnect_sentinel(fd);

        if (res && (header_list != NULL)) {
            int i = 0;

            dprint(verbose, "%s", "######################################################################");
            while (header_list[i] != NULL) {
                short_print_sentinel_header(i, header_list[i]);
                i++;
            }

            dprint(verbose, "%s", "######################################################################");
        }

        if (header_list != NULL) free_sentinel_header_list(header_list);
    } else {
        // Download all dives
        // First, get the list of dive headers
        dprint(verbose, "%s", "Get the list of dives");
        sentinel_header_t **header_list = NULL;
        bool res = get_sentinel_dive_list(fd, &header_list);
        // Next download each dive data
        if (res && (header_list != NULL)) {
            int i = from_dive;
            dprint(verbose, "Fetching dives from %d to %d", from_dive, to_dive);
            dprint(verbose, "%s", "######################################################################");
            while (header_list[i] != NULL && i <= to_dive) {
                dprint(verbose, "Downloading dive number: %d", i);
                download_sentinel_dive(fd, i, &header_list[i]);
                full_print_sentinel_dive(header_list[i]);
                i++;
            }

            dprint(verbose, "%s", "######################################################################");
        }

        disconnect_sentinel(fd);

        if (header_list != NULL) free_sentinel_header_list(header_list);
    }

    dprint(verbose, "Printing dives from %d to %d", from_dive, to_dive);
    dprint(verbose, "%s", "Task completed");
    exit(0);
}
