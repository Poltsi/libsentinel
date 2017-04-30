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

/** split_sentinel_header: Takes the raw header buffer and splits it to
 *                         a string array, one item per dive
 */

bool  split_sentinel_header(char *buffer, char **head_array) {
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

    while(h_lines[line_idx] != NULL) {
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
        if (strncmp(h_lines[line_idx], "Mem ", 4)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->log_lines = atoi(fields[3]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Start ", 6)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->start_s = sentinel_to_unix_timestamp(atoi(fields[2]));
            header_struct->start_time = sentinel_to_utc_datestring(atoi(fields[2]));
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Finish ", 7)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->end_s = sentinel_to_unix_timestamp(atoi(fields[2]));
            header_struct->end_time = sentinel_to_utc_datestring(atoi(fields[2]));
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "MaxD ", 5)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->max_depth = atof(fields[2]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Status ", 7)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->status = atoi(fields[2]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "OTU ", 4)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->otu = atoi(fields[2]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DAtmos ", 7)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->atm = atoi(fields[2]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DStack ", 7)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->stack = atoi(fields[2]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DUsage ", 7)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->usage = atoi(fields[2]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DCNS ", 5)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->cns = atof(fields[2]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DSafety ", 8)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->safety = atof(fields[2]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Dexpert, ", 9)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->expert = atoi(fields[1]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Dtpm, ", 6)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->tpm = atoi(fields[1]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DDecoAlg ", 9)) {
            char **fields = str_cut(h_lines[line_idx], " ");
            header_struct->decoalg = malloc((strlen(fields[1]) + 1) * sizeof(char));
            strncpy(header_struct->decoalg, fields[1], strlen(fields[1]));
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DVGMMaxDSafety ", 15)) {
            header_struct->vgm_max_safety = atof(h_lines[line_idx] + 15);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DVGMStopSafety ", 15)) {
            header_struct->vgm_stop_safety = atof(h_lines[line_idx] + 15);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DVGMMidSafety ", 14)) {
            header_struct->vgm_stop_safety = atof(h_lines[line_idx] + 14);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Dfiltertype, ", 13)) {
            header_struct->filter_type = atoi(h_lines[line_idx] + 13);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Dcellhealth ", 12)) {
            char **fields = str_cut((h_lines[line_idx] + 12), ", ");
            int cell_idx = atoi(fields[0]) - 1;
            header_struct->cell_health[cell_idx] = atoi(fields[1]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Gas ", 4)) {
            char **fields = str_cut((h_lines[line_idx] + 4), ", ");
            int gas_idx = atoi(fields[0]) - 4010;
            header_struct->gas[gas_idx].n2 = atoi(fields[1]);
            header_struct->gas[gas_idx].he = atoi(fields[2]);
            header_struct->gas[gas_idx].o2 = 100 - header_struct->gas[gas_idx].n2 - header_struct->gas[gas_idx].he;
            header_struct->gas[gas_idx].max_depth = atoi(fields[3]);
            header_struct->gas[gas_idx].enabled = atoi(fields[4]);
            free(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Tissue ", 7)) {
            char **fields = str_cut((h_lines[line_idx] + 7), ", ");
            int tissue_idx = atoi(fields[0]) - 4020;
            header_struct->tissue[tissue_idx].t1 = atoi(fields[1]);
            header_struct->tissue[tissue_idx].t2 = atoi(fields[2]);
            free(fields);
            line_idx++;
            continue;
        }
    }

    header_struct->log = NULL;
    return(true);
}

/**
 * parse_sentinel_log_line: Parse the single log line of a dive
 */

bool parse_sentinel_log_line(int interval, sentinel_dive_log_line_t *line, char *linestr) {
    int buffer_size = strlen(linestr);
    dprint(true, "Line length: %d\n", buffer_size);
    char **log_field = str_cut(linestr, ","); /* Cut it by comma */
    int field_idx = 0;

    /* We presume that the first 15 fields are always in the same order
     * and represent the same thing */
    line->time_idx             = atoi(log_field[0] + 1);
    line->time_s               = line->time_idx * interval;
    line->time_string          = seconds_to_hms(line->time_s);
    line->depth                = atoi(log_field[1]) / 10.0;
    line->po2                  = atoi(log_field[3]) / 100.0;
    line->temperature          = atoi(log_field[5] + 1);
    line->scrubber_left        = atoi(log_field[6] + 1) / 10.0;
    line->primary_battery_V    = atoi(log_field[7] + 1) / 100.0;
    line->secondary_battery_V  = atoi(log_field[8] + 1) / 100.0;
    line->diluent_pressure     = atoi(log_field[9] + 1);
    line->o2_pressure          = atoi(log_field[10] + 1);
    line->cell_o2[0]           = atoi(log_field[11] + 1) / 100.0;
    line->cell_o2[1]           = atoi(log_field[12] + 1) / 100.0;
    line->cell_o2[2]           = atoi(log_field[13] + 1) / 100.0;
    line->setpoint             = atoi(log_field[14] + 1) / 100.0;
    line->ceiling              = atoi(log_field[15] + 1);
    /* Now if the following field does not start with a S, then we have a note */
    field_idx = 16;
    int note_idx = 0;

    while (strncmp(log_field[field_idx], "S", 1) != 0) {
        line->note = realloc(line->note, (note_idx + 1) * sizeof(sentinel_note_t));
        get_sentinel_note(log_field[field_idx], line->note[note_idx]);
        note_idx++;
        field_idx++;
    }

    line->tempstick_value[0]  = atoi(log_field[field_idx++] + 1) / 10.0;
    line->tempstick_value[1]  = atoi(log_field[field_idx++] + 1) / 10.0;
    line->tempstick_value[2]  = atoi(log_field[field_idx++] + 1) / 10.0;
    line->tempstick_value[3]  = atoi(log_field[field_idx++] + 1) / 10.0;
    line->tempstick_value[4]  = atoi(log_field[field_idx++] + 1) / 10.0;
    line->tempstick_value[5]  = atoi(log_field[field_idx++] + 1) / 10.0;
    line->tempstick_value[6]  = atoi(log_field[field_idx++] + 1) / 10.0;
    line->tempstick_value[7]  = atoi(log_field[field_idx++] + 1) / 10.0;
    line->co2                 = atoi(log_field[field_idx + 2] + 1);

    return(true);
}

bool get_sentinel_dive_list(int fd, char *buffer, sentinel_header_t **header_list) {
    if (!download_sentinel_header(fd, buffer)) {
        printf("ERROR: Failed to get the Sentinel header\n");
        return(false);
    }

    char **head_array = str_cut(buffer, "d\r\n");
    int header_idx = 0;

    while (head_array[header_idx] != NULL) {
        header_list[header_idx] = malloc(sizeof(sentinel_header_t));

        if (!parse_sentinel_header(header_list[header_idx], head_array[header_idx])) {
            printf("ERROR: Failed parse the Sentinel header\n");
            return(false);
        }

        header_idx++;
    }

    /* Again, the last in the array is null so that we know when the array ends */
    header_list[header_idx] = NULL;
    /* TODO: Invert the list as it is now from the newest to the oldest */
    return(true);
}

bool get_sentinel_note(char *note_str, sentinel_note_t *note) {
    // sentinel_note_t *note = malloc(sizeof(sentinel_note));
    note->note = malloc((strlen(note_str) + 1) * sizeof(char));
    strncpy(note->note, note_str, strlen(note_str));

    if (strcmp(note_str, "ASCENT")) {
        note->type  = 3;
        note->description = "Ascent";
    } else if (strcmp(note_str, "ASCENT FAST")) {
        note->type  = 3;
        note->description = "High ascent rate";
    } else if (strcmp(note_str, "CELLmV ERROR")) {
        note->type  = 20;
        note->description = "Cell voltage error";
    } else if (strcmp(note_str, "DECO ALARM")) {
        note->type  = 1;
        note->description = "Deco alarm";
    } else if (strcmp(note_str, "FILTERREDDIFF")) {
        note->type  = 20;
        note->description = "Filter reading difference";
    } else if (strcmp(note_str, "HPRATE HI")) {
        note->type  = 20;
        note->description = "High pressure rate";
    } else if (strcmp(note_str, "PPO2 <HIGH")) {
        note->type  = 20;
        note->description = "PO2 very high";
    } else if (strcmp(note_str, "PPO2 HIGH")) {
        note->type  = 20;
        note->description = "PO2 high";
    } else if (strcmp(note_str, "PPO2 LOW")) {
        note->type  = 20;
        note->description = "PO2 low";
    } else if (strcmp(note_str, "PPO2 mHIGH")) {
        note->type  = 20;
        note->description = "PO2 medium high";
    } else if (strcmp(note_str, "PPO2 mLOW")) {
        note->type  = 20;
        note->description = "PO2 medium low";
    } else if (strcmp(note_str, "PPO2 OFF")) {
        note->type  = 20;
        note->description = "No pO2-reading";
    } else if (strcmp(note_str, "PPO2 SPINC")) {
        note->type  = 20;
        note->description = "SP change";
    } else if (strcmp(note_str, "PPO2 VHIGH")) {
        note->type  = 20;
        note->description = "PO2 very high";
    } else if (strcmp(note_str, "PREDIVE ABORT")) {
        note->type  = 20;
        note->description = "No predive check done";
    } else if (strcmp(note_str, "VALVE")) {
        note->type  = 20;
        note->description = "Valve issue detected";
    } else {
        printf("Warning: Unknown note: %s\n", note_str);
    }

    return(note);
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
            } /* Else we skip adding to the str_array as the string length is 0 */

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

int sentinel_to_unix_timestamp(const int sentinel_time) {
    return(sentinel_time + SENTINEL_TIME_START);
}

char *sentinel_to_utc_datestring(const int sentinel_time) {
    time_t t = (sentinel_time + SENTINEL_TIME_START);
    const char *format = default_format;
    char *outstr = malloc(200 * sizeof(char));
    struct tm lt;
    localtime_r(&t, &lt);

    if (strftime(outstr, sizeof(outstr), format, &lt) == 0) {
        fprintf(stderr, "strftime returned 0");
        return(0);
    }

    return(outstr);
}

char *seconds_to_hms(const int seconds) {
    char *outstr = malloc(200 * sizeof(char));
    int hours    = seconds / 3600;
    int mins     = seconds / 60;
    int secs     = seconds % 60;
    sprintf(outstr, "%.2d:%.2d:%d.02d", hours, mins, secs);
    return(outstr);
}
