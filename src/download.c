#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "libsentinel.h"

void print_help()
{
    printf("Usage:\n");
    printf("download -d <device> -f <num> -h -l -n <num> -t <num> -v\n");
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
        case 'f': /* List dives (from dive header) */
            from_dive = atoi(optarg);
            break;
        case 'l': /* List dives (from dive header) */
            list_dives = true;
            break;
        case 'n': /* Download dive #n */
            from_dive = to_dive = atoi(optarg);
            break;
        case 't': /* Download dive #n */
            to_dive = atoi(optarg);
            break;
        case 'v':
            verbose = true;
            dprint(verbose, "Verbose set\n");
            break;
        case 'h': /* Print help and exit */
        default:
            print_help();
            exit(0);
        }

    /* Some rudimentary checks */
    /* Sanity checks on from and to dive number if they are other than default (0 and 0)*/
    if (from_dive || to_dive) {
        if (from_dive > to_dive) {
            printf("ERROR: The start (%d) of the requested dive list is bigger than the end (%d) of the list, aborting\n", from_dive, to_dive);

            print_help();
            exit(1);
        }

        if (from_dive < 0 || to_dive < 0) {
            printf("ERROR: Either start (%d) of the requested dive list or the end (%d) of the requested dive list is less than zero, aborting\n", from_dive, to_dive);
            print_help();
            exit(1);
        }

        dprint(verbose, "Printing dives from %d to %d\n", from_dive, to_dive);
    }

    /* Is the device string empty */
    if (!strlen(device_name)) {
        printf("ERROR: No device defined\n");
        print_help();
        exit(1);
    }

    struct stat sb;

    dprint(verbose, "Testing whether we can stat device: %s\n", device_name);
    if (stat(device_name, &sb) == -1) {
        printf("ERROR: Non-existing device: %s\n", device_name);
        exit(1);
    }

    dprint(verbose, "Testing whether we can get the type of device: %s\n", device_name);
    if ((sb.st_mode & S_IFMT) != S_IFCHR) {
        printf("ERROR: Either device '%s' does not exist or it is not a character device, aborting\n", device_name);
        print_help();
        exit(1);
    }

    dprint(verbose, "Opening the serial device: %s\n", device_name);
    int fd = connect_sentinel(device_name);

    if (fd <= 0) {
        printf("ERROR: Unable to connect to the device: %s\n", device_name);
        exit(1);
    }

    int tries = 20;
    if (!is_sentinel_idle(fd, tries)) {
        printf("ERROR: Could not connect to Sentinel after %d tries, is Sentinel connected?\n", tries);
        exit(1);
    }

    printf("Connected to: %s\n", device_name);

    if (list_dives) {
        dprint(verbose, "Printing the list of dives\n");
        sentinel_header_t **header_list = NULL;
        get_sentinel_dive_list(fd, header_list);
        disconnect_sentinel(fd);
    }

    printf("This is a shared library test...\n");
    return(0);
}
