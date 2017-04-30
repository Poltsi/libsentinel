#include "libsentinel.h"

int connect_sentinel(char *device) {
    puts("Hello, I'm a shared library");
    int fd = open_sentinel_device(device);
    /** TODO: Make sure the device is open */
    struct termios options;
    tcgetattr(fd, &options);

    /* Set baud rate */

    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);

    options.c_cflag |= (CLOCAL | CREAD);

    options.c_cflag &= ~CSIZE; /* Mask the character size bits */
    options.c_cflag |= CS8;    /* Select 8 data bits */

    // Set parity - No Parity (8N1)

    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // Disable Software Flow control
    options.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Chose raw (not processed) output
    options.c_oflag &= ~OPOST;

    if (tcsetattr( fd, TCSANOW, &options ) == -1)
        printf ("Error with tcsetattr = %s\n", strerror (errno));
    else
        printf ("%s\n", "tcsetattr succeed");

    fcntl(fd, F_SETFL, FNDELAY);

    return(fd);
}

int open_sentinel_device(char *device) {
    int fd; /* File descriptor for the port */

    fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd == -1)
        return(0);
    else
        fcntl(fd, F_SETFL, FNDELAY);

    return (fd);
}

bool send_sentinel_command(int fd, const char *command) {
    int n = write(fd, command, strlen(command));

    if (n < 0) {
        fputs("write() of 4 bytes failed!\n", stderr);
        return(false);
    }

    printf ("Write succeed n = %i\n", n);
    return(true);
}

bool read_sentinel_data(int fd, char *buffer) {
    int n = read(fd, buffer, 1);

    if (n == -1) {
        printf ( "Error = %s\n", strerror( errno ) );
        return(false);
    }

    printf ( "Number of bytes to be read = %i\n", n );
    printf ( "Buf = %s\n", buffer );

    return(true);
}

bool disconnect_sentinel(int fd) {
    if(close(fd))
        return(true);
    else
        return(false);
}

bool download_sentinel_header(int fd, char *buffer) {
    send_sentinel_command(fd, SENTINEL_LIST_CMD);
    if (!read_sentinel_data(fd, buffer)) {
        printf("ERROR: Failed to read data from Sentinel\n");
        return(false);
    }

    return(true);
}

/* split_sentinel_header: Takes the raw header buffer and splits it to a string array, one item per dive */
bool split_sentinel_header(char *buffer, char **head_array) {
    int num_dive = 0;
    char wbuf[sizeof(buffer)];
    memcpy(wbuf, buffer, sizeof(&buffer));
    // one which is at the end of it
    char *dive_start = wbuf;
    // Get the end of the first dive metadata
    char *dive_end   = NULL;
 
    do {
        printf( "Checking dive: %d", num_dive );
        dive_end = strstr( dive_start, "d\r\n" );
        printf( "Reallocing head_array" );
        head_array = realloc( head_array, ( num_dive + 1 ) * sizeof( *head_array ) );

        int strsize = dive_end - dive_start;

        if( dive_end == NULL ) {/* if there is only one, or if this is the last dive */
            printf( "Last dive entry" );
            strsize = ( wbuf + strlen( wbuf ) ) - dive_start;
        }

        strsize++;
        printf( "New string length: %d", strsize );
        printf( "Mallocing head_array entry" );
        head_array[ num_dive ] = malloc( strsize * sizeof( char ) );
        printf( "Resetting head_array entry" );
        memset( head_array[ num_dive ], 0, strsize );
        printf( "Copying head_array entry" );
        strncpy( head_array[ num_dive ], dive_start, ( strsize - 1 ) );
        dive_start = dive_start + strsize + 2; /* Move the start pointer to the beginning of next */
        printf( "Header length: %d", strsize );
        num_dive++;
    }
    while( dive_end != NULL );

    return(true);
}

/**
 * parse_sentinel_header: Parse a single dive header to the dive header
 *                        struct from the given string buffer
 */

bool parse_sentinel_header(sentinel_header_t *header_struct, char *buffer) {
    int buffer_size = strlen(buffer);
    dprint(true, "Parsable buffer size: %d\n", buffer_size);
    char **h_lines = str_cut(buffer, "\r\n"); /* Cut it by lines */
    int line_idx = 0;

    while(strlen(h_lines[line_idx])) {
        if (strncmp(h_lines[line_idx], "ver=", 4)) {
            header_struct->version = malloc((strlen(h_lines[line_idx]) - 4) * sizeof(char));
            strncpy(header_struct->version, (h_lines[line_idx] + 4), (strlen(h_lines[line_idx]) - 4));
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Recint=", 7)) {
            header_struct->record_interval = atoi(h_lines[line_idx] + 7);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "SN=", 3)) {
            header_struct->serial_number = malloc((strlen(h_lines[line_idx]) - 3) * sizeof(char));
            strncpy(header_struct->serial_number, (h_lines[line_idx] + 3), (strlen(h_lines[line_idx]) - 3));
            line_idx++;
            continue;
        }
    }

    return(true);
}

bool get_sentinel_dive_list(int fd, char *buffer) {
    if (!download_sentinel_header(fd, buffer)) {
        printf("ERROR: Failed to get the Sentinel header\n");
        return(false);
    }

    sentinel_header_t *header_list = malloc(sizeof(sentinel_header_t));

    if (!parse_sentinel_header(header_list, buffer)) {
        printf("ERROR: Failed parse the Sentinel header\n");
        return(false);
    }

    return(true);
}

/*************************************************************************/
/* Minor helper functions used internally                                */
/*************************************************************************/

char **str_cut(char *orig_string, const char *delim) {
    char **str_array  = NULL; /* We store the splits here */
    int arr_idx = 0; /* Our index counter for str_array */
    char *start_ptr   = orig_string;
    char *end_ptr     = orig_string;
    const int win_len = strlen(delim); /* This is our moving window length */
    const int orig_len = strlen(orig_string);
    /* We move the end_ptr one char at a time forward, and at each step we compare
     * whether the next win_len chars are equal to the delim. If this is the case,
     * then we know that the string between start and end ptr is to be stored in our
     * str_array after which we move the start_ptr and end_ptr win_len forward. If
     * we come across a delim while start and end ptr are equal, we skip since this
     * is an empty string. The last item in the array is a 0 so we know that this is
     * where it ends */

    while ((end_ptr - orig_string) <= orig_len) {
        if (strncmp((end_ptr + 1), delim, win_len)) {
            if (start_ptr < end_ptr) {
                str_array = realloc(str_array, (arr_idx + 1) * sizeof(*str_array));
                str_array[arr_idx] = malloc((end_ptr - start_ptr + 1) * sizeof(char));
                strncpy(str_array[arr_idx], start_ptr, (end_ptr - start_ptr));
                arr_idx++;
            } /* Else we skip adding to the str_array as the string length is 0*/

            end_ptr += win_len; /* Jump over the delimiter string */
            start_ptr = end_ptr;
        } else {
            end_ptr++;
        }
    }

    str_array[arr_idx] = malloc(sizeof('\0'));
    str_array[arr_idx] = '\0';
    return(str_array);
}
