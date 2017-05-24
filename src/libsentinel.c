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

#include "libsentinel.h"

/**
 * connect_sentinel: Connects to the given serial port and returns the file descriptor for it
 **/

int connect_sentinel(char* device) {
    int fd = open_sentinel_device(device);
    if (fd == 0) {
        printf("ERROR: Could not open device: %s\n", device);
        return(0);
    }

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
        printf ("ERROR: Unable to set tcsetattr\n");
    else
        printf ("%s\n", "tcsetattr succeed");

    fcntl(fd, F_SETFL, FNDELAY);

    return(fd);
}

/**
 * open_sentinel_device: Opens the given filedescriptor of serial device with the appropriate settings
 **/

int open_sentinel_device(char* device) {
    int fd; /* File descriptor for the port */

    fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd == -1)
        return(0);
    else
        fcntl(fd, F_SETFL, FNDELAY);

    return (fd);
}

/**
 * is_sentinel_idle: Checks whether we can read the wait byte (P) 
 *                   from the serial connection, actually we want
 *                   to read 3 of them to be sure. It will also flush
 *                   the buffer if we read something else
 **/

bool is_sentinel_idle(int fd, const int tries) {
    unsigned char read_byte[ 3 ]  = {0,0,0};
    unsigned char store_byte[ 4 ] = {0,0,0,0};
    /* We expect to get at least 3 consecutive bytes, PPP */
    unsigned char expected[ 4 ]   = {0x50,0x50,0x50,0};
    int i    = tries;
    int p    = 0;
    int m    = memcmp( store_byte, expected, sizeof( expected ) );
    int flushed_bytes = 0;

    while (m != 0) {
        if (i < 1) {
            return(false);
        }

        int n = read(fd, read_byte, sizeof(read_byte));

        if (n > 0) {
            for (int i = 0; i < n; i++) {
                store_byte[p] = read_byte[i];
                p++;
                p = p % 3;
            }

            /* We reset the tries as this is really flushing the buffer */
            i = tries;
        }

        flushed_bytes += n;
        m = memcmp(store_byte, expected, sizeof(expected));

        if (m == 0) {
            if (flushed_bytes > 0) printf("Flushed %d bytes from device buffer\n", flushed_bytes);
            return(true);
        }

        sentinel_sleep(SENTINEL_LOOP_SLEEP_MS);

        i--;
    }

    return(false);
}

/**
 * send_sentinel_command: Handles sending of commands to the rebreather
 **/

bool send_sentinel_command(int fd, const void* command, size_t size) {
    printf("%s: Called\n", __func__);
    size_t nbytes = 0;
    char* buf = calloc(size + 1, sizeof(char));
    strncpy(buf, command, size);

    while (nbytes < size) {
        size_t n = write(fd, (const char*) command + nbytes, size - nbytes);

        if (n < 1) {
            printf("ERROR: write() of %lu bytes failed!\n", (size - nbytes));
            return(false);
        }

        nbytes += n;
    }

    free(buf);
    return(true);
}

/**
 * read_sentinel_response: Waits for the given start and then stores everything into the
 *                         buffer until the wait byte is encountered. Expects that a command
 *                         has already been sent
 **/

bool read_sentinel_response(int fd, char** buffer, const char start[], int start_len) {
    printf("%s: Called\n", __func__);
    char slide_buf[start_len];
    memset(slide_buf, 0, start_len);
    char buf[1] ={0};

    // Initialize the buffer
    *buffer = calloc(1, sizeof(char*));

    printf("%s: Start string (%d): %s\n", __func__, start_len, start);

    int i = 0;
    int n = 0;
    // Wait to receive the start packet for 20 cycles
    while (( n == 0) &&
           (i < 20)) {
        printf("Waiting (%d) to get data from device buffer\n", i);
        // TODO: Should we really read start_len worth of data here? What if the read data
        //       is a partial beginning of start-matcher?
        n = read(fd, slide_buf, start_len);
        sentinel_sleep(SENTINEL_LOOP_SLEEP_MS);
        i++;
    }

    while ((n > 0) &&
           (memcmp(slide_buf, start, start_len) != 0)) {
        n = read(fd, buf, sizeof(buf));
        int j = 0;
        while (j < (start_len - 1)) {
            slide_buf[j] = slide_buf[j + 1];
            j++;
        }

        slide_buf[j] = buf[0];
        sentinel_sleep(SENTINEL_LOOP_SLEEP_MS);
    }

    buf[0] = 0;
    i = 0;

    while (memcmp(SENTINEL_WAIT_BYTE, buf, sizeof(SENTINEL_WAIT_BYTE)) != 0) {
        n = read(fd, buf, sizeof(buf));

        *buffer = resize_string(*buffer, strlen(*buffer) + 1);
        if (*buffer == NULL) return(false);

        strncpy(*buffer + i, buf, 1);
        if (*buffer == NULL) return(false);

        sentinel_sleep(SENTINEL_LOOP_SLEEP_MS);
        i++;
    }

    printf("Read bytes: %d\n", i);

    if (buffer == NULL) {
        printf("%s: Buffer is empty\n", __func__);
        return(false);
    }

    return(true);
}

/**
 * disconnect_sentinel: Close the connection
 **/

bool disconnect_sentinel(int fd) {
    if(close(fd))
        return(true);
    else
        return(false);
}

/**
 * download_sentinel_header: Send command and read the raw output, storing it in the buffer variable
 *                           provided
 **/

bool download_sentinel_header(int fd, char** buffer) {
    printf("%s: Called\n", __func__);
    send_sentinel_command(fd, SENTINEL_LIST_CMD, sizeof(SENTINEL_LIST_CMD));
    if (!read_sentinel_response(fd, buffer, SENTINEL_HEADER_START, sizeof(SENTINEL_HEADER_START))) {
        printf("ERROR: Failed to read header from Sentinel\n");
        return(false);
    }

    if (*buffer == NULL) {
        printf("%s: Received NULL value as answer from device\n", __func__);
        return(false);
    }

    return(true);
}

/**
 * parse_sentinel_header: Parse a single dive header to the given dive header
 *                        struct from the given string buffer
 **/

bool parse_sentinel_header(sentinel_header_t** header_struct, char** buffer) {
    printf("%s: Called\n", __func__);
    char** h_lines = str_cut(buffer, SENTINEL_LINE_SEPARATOR); /* Cut it by lines */

    if (h_lines == NULL) {
        printf("ERROR: Received empty header string.\n");
        return(false);
    }

    // First we set the default values
    **header_struct = DEFAULT_HEADER;
    int line_idx = 0;

    while(h_lines[line_idx] != NULL) {
        if (strncmp(h_lines[line_idx], "ver=", 4) == 0) {
            (*header_struct)->version = resize_string((*header_struct)->version, strlen(h_lines[line_idx]) - 4);
            strncpy((*header_struct)->version, (h_lines[line_idx] + 4), (strlen(h_lines[line_idx]) - 4));
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Recint=", 7) == 0) {
            (*header_struct)->record_interval = atoi(h_lines[line_idx] + 7);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "SN=", 3) == 0) {
            (*header_struct)->serial_number = resize_string((*header_struct)->serial_number, strlen(h_lines[line_idx]) - 3);
            strncpy((*header_struct)->serial_number, (h_lines[line_idx] + 3), (strlen(h_lines[line_idx]) - 4));
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Mem ", 4) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->log_lines = atoi(fields[3]);
            free_string_array(fields);
            line_idx++;
            continue;
        }

        if (strncmp(h_lines[line_idx], "Start ", 6) == 0) {
            printf("It is a time start-field! Let's split it up by spaces\n");
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            printf("Create unix timestamp of: '%s'\n", fields[2]);
            (*header_struct)->start_s = sentinel_to_unix_timestamp(atoi(fields[2]));
            printf("Create datetime\n");
            (*header_struct)->start_time = sentinel_to_utc_datestring(atoi(fields[2]));
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Finish ", 7) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->end_s = sentinel_to_unix_timestamp(atoi(fields[2]));
            (*header_struct)->end_time = sentinel_to_utc_datestring(atoi(fields[2]));
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "MaxD ", 5) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->max_depth = atof(fields[2]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Status ", 7) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->status = atoi(fields[2]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "OTU ", 4) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->otu = atoi(fields[2]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DAtmos ", 7) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->atm = atoi(fields[2]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DStack ", 7) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->stack = atoi(fields[2]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DUsage ", 7) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->usage = atoi(fields[2]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DCNS ", 5) == 0) {
            // TODO: This gets rounded up to 2 decimals
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->cns = atof(fields[2]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DSafety ", 8) == 0) {
            // TODO: This gets rounded up to 2 decimals
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->safety = atof(fields[2]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Dexpert, ", 9) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->expert = atoi(fields[1]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Dtpm, ", 6) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->tpm = atoi(fields[1]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DDecoAlg ", 9) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->decoalg = resize_string((*header_struct)->decoalg, strlen(fields[1]));
            strncpy((*header_struct)->decoalg, fields[1], strlen(fields[1]));
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DVGMMaxDSafety ", 15) == 0) {
            (*header_struct)->vgm_max_safety = atof(h_lines[line_idx] + 15);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DVGMStopSafety ", 15) == 0) {
            (*header_struct)->vgm_stop_safety = atof(h_lines[line_idx] + 15);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "DVGMMidSafety ", 14) == 0) {
            (*header_struct)->vgm_mid_safety = atof(h_lines[line_idx] + 14);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Dfiltertype, ", 13) == 0) {
            (*header_struct)->filter_type = atoi(h_lines[line_idx] + 13);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Dcellhealth ", 12) == 0) {
            char* tmp_ptr = h_lines[line_idx] + 12;
            printf("%s: Looks like we found the health data for cell: %s\n", __func__, tmp_ptr);
            char** fields = str_cut(&tmp_ptr, ", ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            int cell_idx = atoi(fields[0]) - 1;
            (*header_struct)->cell_health[cell_idx] = atoi(fields[1]);
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Gas ", 4) == 0) {
            char* tmp_ptr = h_lines[line_idx] + 4;
            char** fields = str_cut(&tmp_ptr, ", ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            // TODO: This is not working
            int gas_idx = atoi(fields[0]) - 4010;
            (*header_struct)->gas[gas_idx].n2 = atoi(fields[1]);
            (*header_struct)->gas[gas_idx].he = atoi(fields[2]);
            (*header_struct)->gas[gas_idx].o2 = 100 - (*header_struct)->gas[gas_idx].n2 - (*header_struct)->gas[gas_idx].he;
            (*header_struct)->gas[gas_idx].max_depth = atoi(fields[3]);
            (*header_struct)->gas[gas_idx].enabled = atoi(fields[4]);
            free_string_array(fields);
            printf("Found the gas setting for gas# %d: %d/%d \n", gas_idx,
                   (*header_struct)->gas[gas_idx].o2, (*header_struct)->gas[gas_idx].he );
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Tissue ", 7) == 0) {
            char* tmp_ptr = h_lines[line_idx] + 7;
            char** fields = str_cut(&tmp_ptr, ", ");

            if (fields == NULL) {
                printf("ERROR: Received empty split list from: '%s'\n", h_lines[line_idx]);
                return(false);
            }

            // TODO: This is not working
            int tissue_idx = atoi(fields[0]) - 4020;
            (*header_struct)->tissue[tissue_idx].t1 = atoi(fields[1]);
            (*header_struct)->tissue[tissue_idx].t2 = atoi(fields[2]);
            free_string_array(fields);
            printf("Found the tissue value for tissue_idx# %d: %d-%d \n", tissue_idx,
                   (*header_struct)->tissue[tissue_idx].t1, (*header_struct)->tissue[tissue_idx].t2 );
            line_idx++;
            continue;
        }

        printf("Unknown field: '%s'\n", h_lines[line_idx]);
        line_idx++;
    }

    // Some additional computations
    // Calculate the length of the dive
    if ((*header_struct)->start_s < (*header_struct)->end_s &&
        (*header_struct)->start_s > 0) {
        (*header_struct)->length_s = (*header_struct)->end_s - (*header_struct)->start_s;
        (*header_struct)->length_time = seconds_to_hms((*header_struct)->length_s);
    }

    (*header_struct)->log = NULL;

    free_string_array(h_lines);

    return(true);
}

/**
 * parse_sentinel_log_line: Parse the single log line of a dive
 **/

bool parse_sentinel_log_line(int interval, sentinel_dive_log_line_t* line, char* linestr) {
    int buffer_size = strlen(linestr);
    dprint(true, "Line length: %d\n", buffer_size);
    char** log_field = str_cut(&linestr, ","); /* Cut it by comma */

    if (log_field == NULL) {
        printf("ERROR: Received empty split list from: '%s'\n", linestr);
        return(false);
    }

    int field_idx = 0;

    /* We presume that the first 15 fields are always in the same order
     * and represent the same thing */
    line->time_idx             = atoi(log_field[0] + 1);
    line->time_s               = line->time_idx * interval;
    line->time_string          = seconds_to_hms(line->time_s);
    /* This is the pressure measurement, not the actual depth, according to Martin Stanton,
     * who also provided the correct formula to convert to depth */
    line->depth                = (atoi(log_field[1]) * 6) / 64.0;
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
        sentinel_note_t** tmp = realloc(line->note, (note_idx + 1) * sizeof(sentinel_note_t));

        if (tmp != NULL) {
            line->note = tmp;
            // printf("%s: Reallocated to: (%d)\n", __func__, (int) strlen(*buffer));
        } else {
            printf("%s: Failed to reallocate buffer\n", __func__);
            return(false);
        }
        // line->note = realloc(line->note, (note_idx + 1) * sizeof(sentinel_note_t));
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

    free_string_array(log_field);

    return(true);
}

/**
 * get_sentinel_dive_list: Fetches the dive header data and populates the given header-struct list
 **/

bool get_sentinel_dive_list(int fd, sentinel_header_t*** header_list) {
    printf("%s: Called\n", __func__);
    /* Create and minimal allocation of the buffer */
    char* buffer;
    if (!download_sentinel_header(fd, &buffer)) {
        printf("ERROR: Failed to get the Sentinel header\n");
        free(buffer);
        return(false);
    }

    // TODO: This is not working, for some reason SENTINEL_HEADER_START is longer (5) and has 2 newlines
    // char** head_array = str_cut(buffer, SENTINEL_HEADER_START);
    char** head_array = str_cut(&buffer, "d\r\n");

    if (head_array == NULL) {
        printf("ERROR: Received empty head array from: '%s'\n", buffer);
        return(false);
    }

    /* We can now free the original buffer */
    free(buffer);

    int header_idx = 0;

    while (head_array[header_idx] != NULL) {
        *header_list = resize_sentinel_header_list(*header_list, header_idx + 1);

        if (*header_list == NULL) {
            printf("%s: ERROR Failed to reallocate header_list\n", __func__);
            return(false);
        }

        (*header_list)[header_idx] = alloc_sentinel_header();

        if ((*header_list)[header_idx] == NULL) {
            printf("%s: ERROR Could not allocate memory for header struct (%d)\n", __func__, header_idx);
            return(false);
        }

        printf("%s: Parsing header: %d\n", __func__, header_idx);
        if (!parse_sentinel_header(&(*header_list)[header_idx], &head_array[header_idx])) {
            printf("%s: ERROR: Failed parse the Sentinel header\n", __func__);
            return(false);
        }

        header_idx++;
    }

    free_string_array(head_array);
    /* TODO: Invert the list as it is now from the newest to the oldest */
    return(true);
}

/**
 * alloc_sentinel_header: Returns an allocated memory structure for header struct.
 *                        This will not allocate the member values
 **/

sentinel_header_t* alloc_sentinel_header(void) {
    sentinel_header_t *tmp = malloc(sizeof(sentinel_header_t));

    if (tmp == NULL)
        return(NULL);

    return(tmp);
}

/**
 * free_sentinel_header: Frees the memory of a given header struct. This will free
 *                      all the dynamically assigned member values too
 **/

void free_sentinel_header(sentinel_header_t* header) {
    if (header != NULL) {
        if (header->version       != NULL) free(header->version);
        if (header->decoalg       != NULL) free(header->decoalg);
        if (header->serial_number != NULL) free(header->serial_number);
        if (header->start_time    != NULL) free(header->start_time);
        if (header->end_time      != NULL) free(header->end_time);
        if (header->length_time   != NULL) free(header->length_time);
        if (header->log           != NULL) free(header->log);
        free(header);
    }
}

/**
 * print_sentinel_header: Prints out to std the header data, one item per line
 **/

void print_sentinel_header(sentinel_header_t* header) {
    if (header != NULL) {
        printf("version: %s\n", header->version);
        printf("record_interval: %d\n", header->record_interval);
        printf("serial_number: %s\n", header->serial_number);
        printf("log_lines: %d\n", header->log_lines);
        printf("start_s: %d\n", header->start_s);
        printf("end_s: %d\n", header->end_s);
        printf("start_time: %s\n", header->start_time);
        printf("end_time: %s\n", header->end_time);
        printf("max_depth: %.2lf\n", header->max_depth);
        printf("status: %d\n", header->status);
        printf("otu: %d\n", header->otu);
        printf("atm: %d\n", header->atm);
        printf("stack: %d\n", header->stack);
        printf("usage: %d\n", header->usage);
        printf("cns: %.2lf\n", header->cns);
        printf("safety: %.2lf\n", header->safety);
        printf("expert: %d\n", header->expert);
        printf("tpm: %d\n", header->tpm);
        printf("decoalg: %s\n", header->decoalg);
        printf("vgm_max_safety: %.2lf\n", header->vgm_max_safety);
        printf("vgm_stop_safety: %.2lf\n", header->vgm_stop_safety);
        printf("vgm_mid_safety: %.2lf\n", header->vgm_mid_safety);
        printf("filter_type: %d\n", header->filter_type);
        int i = 0;
        for (i = 0; i < 3; i++) {
            printf("cell_health[%d]: %d\n", i, header->cell_health[i]);
        }

        for (i = 0; i < 10; i++) {
            printf("gas[%2d]: N2: %2d He: %2d O2: %2d %3.2ld %d\n", i,
                   header->gas[i].n2, header->gas[i].he, header->gas[i].o2,
                   header->gas[i].max_depth, header->gas[i].enabled );
        }

        for (i = 0; i < 16; i++) {
            printf("tissue[%d]: %3d %3d\n", i, header->tissue[i].t1, header->tissue[i].t2 );
        }
    }
}

/**
 * short_print_sentinel_header: Prints out to std some data of the dive, on one line
 **/

void short_print_sentinel_header(int number, sentinel_header_t* header) {
    if (header != NULL) {
        printf("Dive#: %02d ", number);
        printf("start time: %s ", header->start_time);
        printf("end time: %s ", header->end_time);
        printf("length time: %s ", header->length_time);
        printf("max depth: %.2lf\n", header->max_depth);
    }
}

/**
 * resize_sentinel_header_list: Manages the resizing of an sentinel_header_t array
 **/

sentinel_header_t** resize_sentinel_header_list(sentinel_header_t** old_list, int list_size) {
    // Let's first get a count of the old list
    int i = 0;

    int old_size = 0;
    if (old_list != NULL) {
        while (old_list[i] != NULL) {
            i++;
        }

        old_size = i + 1;
    }

    int new_size = list_size + 1;
    printf("%s: Size of the old_list: %d\n", __func__, old_size);
    printf("%s: Size of the new_list: %d\n", __func__, new_size);

    sentinel_header_t** new_list = realloc(old_list, new_size * sizeof(sentinel_header_t*));

    if (new_list == NULL) {
        printf("%s: ERROR: Failed to reallocate header_list\n", __func__);
        int j = 0;

        while (old_list[j] != NULL) {
            free(old_list[j]);
        }

        free(old_list);
        return(NULL);
    }

    new_list[list_size] = NULL;

    return(new_list);
}

/**
 * free_sentinel_header_list: Goes through the given header list and frees each item
 * c                          separately
 **/

void free_sentinel_header_list(sentinel_header_t** old_list) {
    int i = 0;

    while (old_list[i] != NULL) {
        free_sentinel_header(old_list[i]);
        i++;
    }

    free(old_list);
}

/**
 * get_sentinel_note: Creates and returns a note struct on the given note-string
 **/

bool get_sentinel_note(char* note_str, sentinel_note_t* note) {
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
        printf("Warning: Unknown note: '%s'\n", note_str);
    }

    return(note);
}

/**
 * download_sentinel_dive: Fetches the given dive from the rebreather and populates the missing data
 *                         in the given dive construct
 **/

bool download_sentinel_dive(int fd, int dive_num, sentinel_header_t** header_item) {
    int cmd_size = (int) log10(dive_num) + 2;
    char command[cmd_size];
    char* buffer;

    sprintf(command, "D%d", dive_num);
    printf("%s: Send download command: '%s'\n", __func__, command);
    bool res = send_sentinel_command(fd, &command, sizeof(command));
    if (!res) return(false);

    if (!read_sentinel_response(fd, &buffer, SENTINEL_HEADER_START, sizeof(SENTINEL_HEADER_START))) {
        printf("%s: ERROR: Failed to read dive data from Sentinel\n", __func__);
        return(false);
    }

    printf("%s: ======================================================================\n", __func__);
    printf("%s\n", buffer);
    printf("%s: ======================================================================\n", __func__);

    free(buffer);
    return(res);
}
/*************************************************************************/
/* Minor helper functions used internally                                */
/*************************************************************************/

/**
 * str_cut: Homegrown disoptimized string cutter, takes a long string as well as the delimitator, and returns
 *          an array of strings with all the pieces
 **/

char** str_cut(char** orig_string, const char* delim) {
    if (orig_string == NULL) {
        printf("%s: Original string is null, return null\n", __func__);
        return(NULL);
    }

    if (delim == NULL || delim == 0) {
        printf("%s: Delimiter is null, return null\n", __func__);
        /* TODO: Should this actually return an array with each char is separated? */
        return(NULL);
    }

    char** str_array  = NULL; /* We store the splits here */
    int arr_idx = 0; /* Our index counter for str_array */
    char* start_ptr   = *orig_string;
    char* end_ptr     = *orig_string;
    const int win_len = strlen(delim); /* This is our moving window length */
    const long orig_len = strlen(*orig_string);

    /* We move the end_ptr one char at a time forward, and at each step we compare
     * whether the next win_len chars are equal to the delim. If this is the case,
     * then we know that the string between start and end ptr is to be stored in our
     * str_array after which we move the start_ptr and end_ptr win_len forward. If
     * we come across a delim while start and end ptr are equal, we skip since this
     * is an empty string. The last item in the array is a 0 so we know that this is
     * where it ends */

    // while ((end_ptr - *orig_string) <= (orig_len - win_len)) {
    while (end_ptr < *orig_string + orig_len) {
        if (strncmp(end_ptr, delim, win_len) == 0) {
            if (start_ptr < end_ptr) {
                str_array = resize_string_array(str_array, arr_idx + 1);

                if (str_array == NULL) return(NULL);

                int tmp_len = end_ptr - start_ptr;

                str_array[arr_idx] = resize_string(str_array[arr_idx], tmp_len);
                strncpy(str_array[arr_idx], start_ptr, tmp_len);
                arr_idx++;
            } /* Else we skip adding to the str_array as the string length is 0 */

            end_ptr += win_len; /* Jump over the delimiter string */
            start_ptr = end_ptr;
        } else {
            end_ptr++;
        }
    }

    /* We may have a residue string which needs to be stored */
    if (start_ptr < end_ptr) {
        str_array = resize_string_array(str_array, arr_idx + 1);
        if (str_array == NULL) return(NULL);
        int tmp_len = end_ptr + 1 - start_ptr;
        str_array[arr_idx] = resize_string(str_array[arr_idx], tmp_len);
        if (str_array[arr_idx] == NULL) return(NULL);
        strncpy(str_array[arr_idx], start_ptr, tmp_len);
        arr_idx++;
    }

    return(str_array);
}

/**
 * resize_string: Takes in a string, and resizes it to the given string length. This
 *                means that the actual lenght is +1 as it includes the terminating
 *                null. If the new string is longer, then it pads the end with
 *                null(s), if shorter, then the last character will be replaced with
 *                a null. Returns the new string back
 **/

char* resize_string(char* old_str, int string_length) {
    char* new_str = calloc(string_length + 1, sizeof(char));

    if (new_str == NULL) {
        printf("%s: ERROR: Unable to calloc string, return null\n", __func__);
        return(NULL);
    }

    if (old_str == NULL) {
        return(new_str);
    }

    char* tmp = strncpy(new_str, old_str, strlen(old_str));

    free(old_str);

    if (tmp == NULL) {
        printf("%s: ERROR: Unable to strncpy the old string to the new\n", __func__);
        free(new_str);
        return(NULL);
    }

    return(new_str);
}

/**
 * resize_string_array: Similar to resize_string except that it operates on arrays of
 *                      char-pointers, maintaining always a nullpointer as the last
 *                      item
 **/

char** resize_string_array(char** old_arr, int arr_size) {
    char** new_arr = calloc(arr_size + 1, sizeof(char**));

    if (old_arr == NULL && arr_size > 0) {
        int i = 0;

        for (i = 0; i < arr_size; i++) {
            new_arr[i] = calloc(1, sizeof(char));
        }

        return(new_arr);
    }

    if (arr_size == 0) {
        new_arr[0] = NULL;
        if (old_arr != NULL) free(old_arr);
        return(new_arr);
    }

    int i = 0;

    /* TODO: There is a flaw here as there may be a null pointer in the middle of the array.
     *       Maybe next consider storing these values in a linked list instead? */

    while (old_arr[i] != NULL) {
        new_arr[i] = calloc(strlen(old_arr[i]) + 1, sizeof(char));
        strncpy(new_arr[i], old_arr[i], strlen(old_arr[i]));
        i++;
    }

    new_arr[i] = NULL;
    free_string_array(old_arr);

    return(new_arr);
}

/**
 * free_string_array: Free up the memory reserved in the dynamically allocated array of strings
 **/

void free_string_array(char** str_arr) {
    int i = 0;

    if (str_arr == NULL) return;

    while (str_arr[i] != NULL) {
        free(str_arr[i]);
        i++;
    }

    free(str_arr);
}

/**
 * sentinel_to_unix_timestamp: Converts sentinel seconds to unixtime
 **/

int sentinel_to_unix_timestamp(const int sentinel_time) {
    return(sentinel_time + SENTINEL_TIME_START);
}

/**
 * sentinel_to_utc_datestring: Converts sentinel seconds to datetime string
 **/

// TODO: This is broken and does not return any strings
char* sentinel_to_utc_datestring(const int sentinel_time) {
    time_t t = sentinel_to_unix_timestamp(sentinel_time);
    const char* format = default_format;
    static int str_length = 60;

    char* outstr = calloc(str_length + 1, sizeof(char));
    struct tm lt;
    localtime_r(&t, &lt);

    if (strftime(outstr, str_length, format, &lt) == 0) {
        fprintf(stderr, "strftime returned 0 for %d\n", sentinel_time);
        return(0);
    }

    return(outstr);
}

/**
 * seconds_to_hms: Converts seconds to hh:mm:ss-string
 **/

char* seconds_to_hms(const int seconds) {
    static int str_length = 60;
    char* outstr = calloc(str_length, sizeof(char));
    int hours    = seconds / 3600;
    int mins     = (seconds - hours * 3600) / 60;
    int secs     = seconds % 60;
    sprintf(outstr, "%.2d:%.2d:%.02d", hours, mins, secs);
    return(outstr);
}

/**
 * sentinel_sleep: Sleeps for given milliseconds
 **/

void sentinel_sleep(const int msecs) {
    struct timespec ts;
    ts.tv_sec  = (msecs / 1000);
    ts.tv_nsec = (msecs % 1000) * 1000000;

    while (nanosleep (&ts, &ts) != 0) {
        int errcode = errno;
        if (errcode != EINTR ) {
            printf("%s: ERROR Something went wrong while nanosleeping\n", __func__);
        }
    }
}
