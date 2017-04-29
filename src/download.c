#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "libsentinel.h"

void print_help()
{
    puts("Usage:\n");
    puts("download -d <device> -f <num> -h -l -n <num> -t <num> -v\n");
    puts("-d <device> Which device to use, usually /dev/ttyUSB\n");
    puts("-f <num> Optional: Start downloading from this dive, list the dives first to see the number\n");
    puts("-h This help\n");
    puts("-l List the dives\n");
    puts("-n <num> Download this specific dive, list the dives first to see the number\n");
    puts("-t <num> Download the dives including this one, list the dives first to see the number\n");
    puts("-v Be more verbose\n");
    puts("\n");
}

int main(int argc, char **argv)
{
    int c = 0;
    int from_dive = 0;
    int to_dive   = 0;
    char *device_name;
    bool verbose    = false;
    bool list_dives = false;
    opterr = 0;

    while ((c = getopt (argc, argv, "d:f:hln:t:v")) != -1)
        switch (c)
            {
            case 'd': /* Set serial device to <device> */
                free(device_name);
                device_name = malloc( strlen( optarg ) * sizeof( char ) );
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
                break;
            case 'h': /* Print help and exit */
            default:
                print_help();
                exit(0);
            }

    /* Some rudimentary checks */
    /* Sanity checks on start and end dive number */
    if (from_dive > to_dive) {
        printf("ERROR: The start (%d) of the requested dive list is bigger than the end (%d) of the list, aborting\n", from_dive, to_dive);

        print_help();
        exit(1);
    }

    if( from_dive < 0 || to_dive < 0) {
        printf("ERROR: Either start (%d) of the requested dive list or the end (%d) of the requested dive list is less than zero, aborting\n", from_dive, to_dive);
        print_help();
        exit(1);
    }

    struct stat sb;

    if (stat(device_name, &sb) == -1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    if ( (sb.st_mode & S_IFMT) != S_IFCHR ) {
        printf("ERROR: Either device '%s' does not exist or it is not a character device, aborting\n", device_name);
        print_help();
        exit(1);
    }

    dprint(verbose, "Opening the serial device: %s", device_name);
    int fd = connect_sentinel(device_name);

    if (fd < 0) {
        printf("ERROR: Unable to connect to the device: %s\n", device_name);
        exit(1);
    }

    printf("Connected to: %s\n", device_name);

    if(list_dives) {
        dprint(verbose, "Printing the list of dives");
        char *buffer = malloc(1024 * 1024 * sizeof(char));
        
        get_sentinel_header(fd, buffer);
        disconnect_sentinel(fd);
    }

    puts("This is a shared library test...");
    return(0);
}
