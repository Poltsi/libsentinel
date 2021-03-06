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
        eprint("Could not open device: %s", device);
        return(0);
    }

    struct termios options;
    memset (&options, 0, sizeof (options));
    if (tcgetattr(fd, &options) != 0) {
        eprint("Unable to get attributes from fd: %d", fd);
        return(0);
    }

    /* Set baud rate */
    if (cfsetispeed(&options, B9600) != 0) {
        eprint("%s", "Could not set serial speed to 9600 for input");
        return(0);
    }

    if (cfsetospeed(&options, B9600) != 0) {
        eprint("%s", "Could not set serial speed to 9600 for output");
        return(0);
    }

    options.c_iflag &= ~(IGNBRK | BRKINT | ISTRIP | INLCR | IGNCR | ICRNL);
    options.c_oflag &= ~(OPOST);
    options.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 0;

    options.c_cflag &= ~CSIZE; /* Mask the character size bits */
    options.c_cflag &= ~(PARENB | PARODD);
    options.c_iflag &= ~(IGNPAR | PARMRK | INPCK);
    options.c_iflag |= IGNPAR;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    if (tcsetattr( fd, TCSANOW, &options ) == -1)
        eprint("%s", "Unable to set tcsetattr");

    fcntl(fd, F_SETFL, FNDELAY);

    int value = TIOCM_RTS;
    if (ioctl (fd, TIOCMBIS, &value) != 0) {
        eprint("%s", "Unable to set RTS line, are you running against the emulator?");
    }

    sentinel_sleep(500);
    return(fd);
}

/**
 * open_sentinel_device: Opens the given filedescriptor of serial device with the appropriate settings
 **/

int open_sentinel_device(char* device) {
    int fd; /* File descriptor for the port */

    fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd == -1) {
        eprint("Could not open device: %s", device);
        return(0);
    } else
        fcntl(fd, F_SETFL, FNDELAY);

    sentinel_sleep(500);
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
        } else {
            dprint(true, "Failed (%d) to read the serial device with return value: %d", i, m);
        }

        flushed_bytes += n;
        m = memcmp(store_byte, expected, sizeof(expected));

        if (m == 0) {
            if (flushed_bytes > 0) dprint(true, "Flushed %d bytes from device buffer", flushed_bytes);
            return(true);
        }

        sentinel_sleep(500);

        i--;
    }

    return(false);
}

/**
 * send_sentinel_command: Handles sending of commands to the rebreather
 **/

bool send_sentinel_command(int fd, const void* command, size_t size) {
    size_t nbytes = 0;
    char* buf = calloc(size + 1, sizeof(char));
    strncpy(buf, command, size);

    while (nbytes < size) {
        size_t n = write(fd, (const char*) command + nbytes, size - nbytes);

        if (n < 1) {
            eprint("write() of %lu bytes failed!\n", (size - nbytes));
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

bool read_sentinel_response(int fd, char** buffer, const char start[], int start_len, const char end[], int end_len) {
    char slide_start[start_len];
    memset(slide_start, 0, start_len);

    char slide_end[end_len];
    memset(slide_end, 0, end_len);

    char buf[1] ={0};

    char* end_str = restring(end, end_len);

    int i = 0;
    int n = 0;
    // Wait to receive the start packet for 20 cycles or as long as we get the wait byte
    while (( n == 0) &&
           (i < 20)) {
        // TODO: Should we really read start_len worth of data here? What if the read data
        //       is a partial beginning of start-matcher?
        n = read(fd, slide_start, start_len);
        sentinel_sleep(SENTINEL_LOOP_SLEEP_MS);

        if (strncmp("PPP", slide_start, 3) == 0) {
            i++;
        }
    }

    while ((n > 0) &&
           (memcmp(slide_start, start, start_len) != 0)) {
        n = read(fd, buf, sizeof(buf));
        int j = 0;

        while (j < (start_len - 1)) {
            slide_start[j] = slide_start[j + 1];
            j++;
        }

        slide_start[j] = buf[0];

        sentinel_sleep(SENTINEL_LOOP_SLEEP_MS);
    }

    // TODO: Make sure the buffer is not already initialized
    // Initialize the buffer so that it fits at least the end string
    *buffer = calloc(2, sizeof(char*));
    // Copy whatever we last read from the device to the buffer, otherwise we lose the first char
    strncpy(*buffer, buf, 1);
    *buffer = resize_string(*buffer, end_len + 1);
    // Then we read in end_len bytes from the device
    n = read(fd, *buffer, end_len);

    int m = memcmp(*buffer + strlen(*buffer) - end_len, end, end_len);

    // We need to advance the index, otherwise we will overwrite the beginning of the buffer
    i = strlen(*buffer);

    while (m != 0) {
        n = read(fd, buf, sizeof(buf));

        if (n < 0) {
            sentinel_sleep(50);
            continue;
        }

        *buffer = resize_string(*buffer, strlen(*buffer) + 1);
        if (*buffer == NULL) return(false);

        strncpy(*buffer + i, buf, 1);
        if (*buffer == NULL) return(false);

        m = memcmp(*buffer + strlen(*buffer) - end_len, end, end_len);
        // Let's do a sanity check, in case the transmission is not correct
        // if it starts to print out ,,, then we seem to be out of memory on the rebreather
        if (m != 0 && i > 3 &&
            ((memcmp(*buffer + strlen(*buffer) - 3, "PPP", 3) == 0) ||
             (memcmp(*buffer + strlen(*buffer) - 3, ",,,", 3) == 0))) {
            eprint("Somehow we missed the end string (%s) and see a lot of wait bytes or end-of-memory", end_str);
            break;
        }

        sentinel_sleep(SENTINEL_LOOP_SLEEP_MS);
        i++;
    }

    dprint(true, "Read bytes: %d", i);

    free(end_str);

    if (buffer == NULL) {
        eprint("%s", "Buffer is empty");
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
    send_sentinel_command(fd, SENTINEL_LIST_CMD, sizeof(SENTINEL_LIST_CMD));
    if (!read_sentinel_response(fd, buffer, SENTINEL_HEADER_START, sizeof(SENTINEL_HEADER_START),
                                SENTINEL_PROFILE_END, sizeof(SENTINEL_PROFILE_END))) {
        eprint("%s", "Failed to read header from Sentinel");
        return(false);
    }

    if (*buffer == NULL) {
        eprint("%s", "Received NULL value as answer from device");
        return(false);
    }

    return(true);
}

/**
 * parse_sentinel_header: Parse a single dive header to the given dive header
 *                        struct from the given string buffer
 **/

bool parse_sentinel_header(sentinel_header_t** header_struct, char** buffer) {
    char** h_lines = str_cut(buffer, "\r\n"); /* Cut it by lines */

    if (h_lines == NULL) {
        eprint("%s", "Received empty header string");
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->log_lines = atoi(fields[3]);
            free_string_array(fields);
            line_idx++;
            continue;
        }

        if (strncmp(h_lines[line_idx], "Start ", 6) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
                return(false);
            }

            (*header_struct)->start_s = sentinel_to_unix_timestamp(atoi(fields[2]));
            (*header_struct)->start_time = sentinel_to_utc_datestring(atoi(fields[2]));
            free_string_array(fields);
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Finish ", 7) == 0) {
            char** fields = str_cut(&h_lines[line_idx], " ");

            if (fields == NULL) {
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
            char** fields = str_cut(&tmp_ptr, ", ");

            if (fields == NULL) {
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
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
            line_idx++;
            continue;
        }
        if (strncmp(h_lines[line_idx], "Tissue ", 7) == 0) {
            char* tmp_ptr = h_lines[line_idx] + 7;
            char** fields = str_cut(&tmp_ptr, ", ");

            if (fields == NULL) {
                eprint("Received empty split list from: '%s'", h_lines[line_idx]);
                return(false);
            }

            // TODO: This is not working
            int tissue_idx = atoi(fields[0]) - 4020;
            (*header_struct)->tissue[tissue_idx].t1 = atoi(fields[1]);
            (*header_struct)->tissue[tissue_idx].t2 = atoi(fields[2]);
            free_string_array(fields);
            line_idx++;
            continue;
        }

        dprint(true, "Unknown field: '%s'", h_lines[line_idx]);
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
    char** log_field = str_cut(&linestr, ","); /* Cut it by comma */

    if (log_field == NULL) {
        eprint("Received empty split list from: '%s'", linestr);
        return(false);
    }

    // Print all the fields we splitted
    int i = 0;
    while (log_field[i] != NULL) {
        i++;
    }

    // Then again, with the shifted value
    i = 0;
    while (log_field[i] != NULL) {
        i++;
    }

    i = 0;

    // Default values for the line
    *line = DEFAULT_LOG_LINE;

    /* We presume that the first 15 fields are always in the same order
     * and represent the same thing */
    if (log_field[i] != NULL) line->time_idx             = atoi(log_field[i] + 1);
    if (log_field[i] != NULL) line->time_s               = line->time_idx * interval;
    if (log_field[i] != NULL) line->time_string          = seconds_to_hms(line->time_s);
    i++;
    /* This is the pressure measurement, not the actual depth, according to Martin Stanton,
     * who also provided the correct formula to convert to depth */
    if (log_field[i] != NULL) line->depth                = (atoi(log_field[i]) * 6) / 64.0;
    i += 2;
    if (log_field[i] != NULL) line->po2                  = atoi(log_field[i]) / 100.0;
    i += 2;
    if (log_field[i] != NULL) line->temperature          = atoi(log_field[i++] + 1);
    if (log_field[i] != NULL) line->scrubber_left        = atoi(log_field[i++] + 1) / 10.0;
    if (log_field[i] != NULL) line->primary_battery_V    = atoi(log_field[i++] + 1) / 100.0;
    if (log_field[i] != NULL) line->secondary_battery_V  = atoi(log_field[i++] + 1) / 100.0;
    if (log_field[i] != NULL) line->diluent_pressure     = atoi(log_field[i++] + 1);
    if (log_field[i] != NULL) line->o2_pressure          = atoi(log_field[i++] + 1);
    if (log_field[i] != NULL) line->cell_o2[0]           = atoi(log_field[i++] + 1) / 100.0;
    if (log_field[i] != NULL) line->cell_o2[1]           = atoi(log_field[i++] + 1) / 100.0;
    if (log_field[i] != NULL) line->cell_o2[2]           = atoi(log_field[i++] + 1) / 100.0;
    if (log_field[i] != NULL) line->setpoint             = atoi(log_field[i++] + 1) / 100.0;
    if (log_field[i] != NULL) line->ceiling              = atoi(log_field[i++] + 1);
    /* Now if the following field does not start with a S, then we have a note */
    int note_idx = 0;

    // There may be events recorded here
    // Allocate an empty array
    while (log_field[i] != NULL && strncmp(log_field[i], "S", 1) != 0) {
        line->note = resize_sentinel_note_list(line->note, note_idx + 1);
        line->note[note_idx] = alloc_sentinel_note();

        if (!get_sentinel_note(line->note[note_idx], log_field[i])) {
            eprint("Unable to add note: %s", log_field[i]);
        }

        note_idx++;
        i++;
    }

    int j = 0;
    // Do we have all the temp-stick fields?
    while (log_field[i] != NULL && j < 8) {
        line->tempstick_value[j]  = atoi(log_field[i] + 1) / 10.0;
        i++;
        j++;
    }

    if (log_field[i] != NULL) line->co2                 = atoi(log_field[i + 2] + 1);

    free_string_array(log_field);

    return(true);
}

/**
 *alloc_sentinel_note: Returns an allocated memory structure for note struct.
 *                     This will not allocate the member values 
 **/

sentinel_note_t* alloc_sentinel_note(void) {
    sentinel_note_t *tmp = malloc(sizeof(sentinel_note_t));

    if (tmp == NULL)
        return(NULL);

    return(tmp);
}

/**
 * resize_sentinel_note_list: Resizes and allocates memory for the new list of notes
 **/

sentinel_note_t** resize_sentinel_note_list(sentinel_note_t** old_list, int list_size) {
    // Let's first get a count of the old list
    int i = 0;

    if (old_list != NULL) {
        while (old_list[i] != NULL) {
            i++;
        }
    }

    int new_size = list_size + 1;

    sentinel_note_t** new_list = realloc(old_list, new_size * sizeof(sentinel_note_t*));

    if (new_list == NULL) {
        eprint("%s", "Failed to reallocate note list");
        int j = 0;

        while (old_list[j] != NULL) {
            free(old_list[j]);
            j++;
        }

        free(old_list);
        return(NULL);
    }

    new_list[list_size] = NULL;

    return(new_list);
}

/**
 * get_sentinel_dive_list: Fetches the dive header data and populates the given header-struct list
 **/

bool get_sentinel_dive_list(int fd, sentinel_header_t*** header_list) {
    /* Create and minimal allocation of the buffer */
    char* buffer;
    if (!download_sentinel_header(fd, &buffer)) {
        eprint("%s", "Failed to get the Sentinel header");
        free(buffer);
        return(false);
    }

    printf("========= List ==========\n");
    printf("%s", buffer);
    printf("========= List ==========\n");
    // TODO: This is not working, for some reason SENTINEL_HEADER_START is longer (5) and has 2 newlines
    // char** head_array = str_cut(buffer, SENTINEL_HEADER_START);
    char** head_array = str_cut(&buffer, "d\r\n");

    if (head_array == NULL) {
        eprint("Received empty head array from: '%s'", buffer);
        return(false);
    }

    /* We can now free the original buffer */
    free(buffer);

    int header_idx = 0;

    while (head_array[header_idx] != NULL) {
        *header_list = resize_sentinel_header_list(*header_list, header_idx + 1);

        if (*header_list == NULL) {
            eprint("%s", "Failed to reallocate header_list");
            return(false);
        }

        (*header_list)[header_idx] = alloc_sentinel_header();

        if ((*header_list)[header_idx] == NULL) {
            eprint("Could not allocate memory for header struct (%d)", header_idx);
            return(false);
        }

        if (!parse_sentinel_header(&(*header_list)[header_idx], &head_array[header_idx])) {
            eprint("%s", "Failed parse the Sentinel header");
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
        if (header->log           != NULL) free_sentinel_log_list(header->log);
        free(header);
    }
}

/**
 *
 *
 **/
void free_sentinel_note_list(sentinel_note_t** note) {
    if (note != NULL) {
        int i = 0;

        while (note[i] != NULL) {
            if (note[i]->note != NULL) free(note[i]->note);
            if (note[i]->description != NULL) free(note[i]->description);
            free(note[i]);
            i++;
        }

        free(note);
    }
}

/**
 * free_sentinel_log: Frees the memory of a given log struct. This will free
 *                      all the dynamically assigned member values too
 **/

void free_sentinel_log(sentinel_dive_log_line_t* log) {
    if (log != NULL) {
        if (log->time_string != NULL) free(log->time_string);
        free_sentinel_note_list(log->note);
        free(log);
    }
}

/**
 * free_sentinel_log_list: Frees the memory of a given log list
 **/

void free_sentinel_log_list(sentinel_dive_log_line_t** log) {
    if (log != NULL) {
        int i = 0;

        while (log[i] != NULL) {
            free_sentinel_log(log[i]);
            i++;
        }

        free(log);
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
            printf("gas[%2d]: N2: %2d He: %2d O2: %2d %3.2lf %d\n", i,
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
 * print_sentinel_log_line: Print some fields of the given log line
 **/

void print_sentinel_log_line(int number, sentinel_dive_log_line_t* line) {
    if (line != NULL) {
        printf("%04d ", number);
        printf("%s ", line->time_string);
        printf("%3.1lf ",line->depth);
        printf("%2d⁰C ", line->temperature);
        printf("%1.2lf ", line->po2);
        printf("%3d ", line->o2_pressure);
        printf("%3d ", line->diluent_pressure);
        printf("%1.2lf", line->setpoint);

        if (line->note != NULL) {
            int i = 0;
            while (line->note[i] != NULL) {
                if (line->note[i]->note != NULL) printf(" '%s'", line->note[i]->note);
                i++;
            }
        }

        printf("\n");
    }
}

/**
 * full_print_sentinel_dive: Print both header as well as choice parts of each log line
 **/

void full_print_sentinel_dive(sentinel_header_t* header) {
    if (header != NULL) {
        print_sentinel_header(header);

        if (header->log != NULL) {
            int i = 0;

            while (header->log[i] != NULL)  {
                print_sentinel_log_line(i, header->log[i]);
                i++;
            }
        }
    }
}
/**
 * resize_sentinel_log_list: Manages the resizing of an sentinel_dive_log_line_t array
 **/

sentinel_dive_log_line_t** resize_sentinel_log_list(sentinel_dive_log_line_t** old_list, int list_size) {
    // Let's first get a count of the old list
    int i = 0;

    if (old_list != NULL) {
        while (old_list[i] != NULL) {
            i++;
        }
    }

    int new_size = list_size + 1;

    sentinel_dive_log_line_t** new_list = realloc(old_list, new_size * sizeof(sentinel_dive_log_line_t*));

    if (new_list == NULL) {
        eprint("%s", "Failed to reallocate log list");
        int j = 0;

        while (old_list[j] != NULL) {
            free(old_list[j]);
            j++;
        }

        free(old_list);
        return(NULL);
    }

    new_list[list_size] = NULL;

    return(new_list);
}

/**
 * alloc_sentinel_dive_log_line: Returns an allocated memory structure for log line
 *                               This will not allocate the member values
 **/

sentinel_dive_log_line_t* alloc_sentinel_dive_log_line(void) {
    sentinel_dive_log_line_t* tmp = malloc(sizeof(sentinel_dive_log_line_t));

    if (tmp == NULL)
        return(NULL);

    return(tmp);
}



/**
 * free_sentinel_dive_log_list: Goes through the given log list and frees each item
 * c                            separately
 **/

void free_sentinel_dive_log_list(sentinel_dive_log_line_t** old_list) {
    int i = 0;

    while (old_list[i] != NULL) {
        free_sentinel_log(old_list[i]);
        i++;
    }

    free(old_list);
}


/**
 * resize_sentinel_header_list: Manages the resizing of an sentinel_header_t array
 **/

sentinel_header_t** resize_sentinel_header_list(sentinel_header_t** old_list, int list_size) {
    // Let's first get a count of the old list
    int i = 0;

    if (old_list != NULL) {
        while (old_list[i] != NULL) {
            i++;
        }
    }

    int new_size = list_size + 1;

    sentinel_header_t** new_list = realloc(old_list, new_size * sizeof(sentinel_header_t*));

    if (new_list == NULL) {
        eprint("%s", "Failed to reallocate header_list");

        free_sentinel_header_list(old_list);
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
 * get_sentinel_note: Creates and returns a note struct on the given note-string. The type is
 *                    taken from Subsurface
 **/

bool get_sentinel_note(sentinel_note_t* note, char* note_str) {
    note->note = calloc(1, sizeof(char));
    note->note = resize_string(note->note, (strlen(note_str) + 1));
    strncpy(note->note, note_str, strlen(note_str));

    if (strcmp(note_str, "ASCENT")) {
        note->type  = 3;
        note->description = strdup("Ascent");
    } else if (strcmp(note_str, "ASCENT FAST")) {
        note->type  = 3;
        note->description = strdup("High ascent rate");
    } else if (strcmp(note_str, "CELLmV ERROR")) {
        note->type  = 20;
        note->description = strdup("Cell voltage error");
    } else if (strcmp(note_str, "DECO ALARM")) {
        note->type  = 1;
        note->description = strdup("Deco alarm");
    } else if (strcmp(note_str, "FILTERREDDIFF")) {
        note->type  = 20;
        note->description = strdup("Filter reading difference");
    } else if (strcmp(note_str, "HPRATE HI")) {
        note->type  = 20;
        note->description = strdup("High pressure rate");
    } else if (strcmp(note_str, "PPO2 <HIGH")) {
        note->type  = 20;
        note->description = strdup("PO2 very high");
    } else if (strcmp(note_str, "PPO2 HIGH")) {
        note->type  = 20;
        note->description = strdup("PO2 high");
    } else if (strcmp(note_str, "PPO2 LOW")) {
        note->type  = 20;
        note->description = strdup("PO2 low");
    } else if (strcmp(note_str, "PPO2 mHIGH")) {
        note->type  = 20;
        note->description = strdup("PO2 medium high");
    } else if (strcmp(note_str, "PPO2 mLOW")) {
        note->type  = 20;
        note->description = strdup("PO2 medium low");
    } else if (strcmp(note_str, "PPO2 OFF")) {
        note->type  = 20;
        note->description = strdup("No pO2-reading");
    } else if (strcmp(note_str, "PPO2 SPINC")) {
        note->type  = 20;
        note->description = strdup("SP change");
    } else if (strcmp(note_str, "PPO2 VHIGH")) {
        note->type  = 20;
        note->description = strdup("PO2 very high");
    } else if (strcmp(note_str, "PREDIVE ABORT")) {
        note->type  = 20;
        note->description = strdup("No predive check done");
    } else if (strcmp(note_str, "VALVE")) {
        note->type  = 20;
        note->description = strdup("Valve issue detected");
    } else {
        eprint("Unknown note: '%s'", note_str);
    }

    return(note);
}

/**
 * download_sentinel_dive: Fetches the given dive from the rebreather and populates the missing data
 *                         in the given dive construct
 **/

bool download_sentinel_dive(int fd, int dive_num, sentinel_header_t** header_item) {
    int cmd_size = (int) log10(dive_num) + 2;
    bool res = true;

    if (dive_num == 0)
        cmd_size = 2;

    char command[cmd_size];
    char* buffer;
    /* TODO: This is not how the dive number is formed. It is actually just the ascii character,
     *       so eg. first dive is 0x30 (0) and the commad is D0, 12th dive is 0x3C (<) and the
     *       command is D< */
    sprintf(command, "D%d", dive_num);
    res = send_sentinel_command(fd, &command, sizeof(command));
    if (!res) return(false);

    if (!read_sentinel_response(fd, &buffer, SENTINEL_HEADER_START, sizeof(SENTINEL_HEADER_START),
                                SENTINEL_PROFILE_END, sizeof(SENTINEL_PROFILE_END))) {
        eprint("%s", "Failed to read dive data from Sentinel");
        res = false;
    } else {
        printf("========= Dive ==========\n");
        printf("%s", buffer);
        printf("========= Dive ==========\n");
        // Let's first separate the header from the profile
        char** header_and_profile = str_cut(&buffer, "Profile\r\n");

        // Let's repopulate the header, then we will also get the gas and tissues too
        free_sentinel_header(*header_item);
        *header_item = alloc_sentinel_header();
        **header_item = DEFAULT_HEADER;

        if (!parse_sentinel_header(header_item, &header_and_profile[0])) {
            eprint("%s", "Failed to re-parse header");
            res = false;
        } else {
            // Next we split the loglines
            char** log_lines = str_cut(&header_and_profile[1], "\r\n");
            int i = 0;

            // TODO: We know how many log lines there should be, from the Mem header line,
            //       this should be verified
            while (log_lines[i] != NULL && strncmp(log_lines[i], "End", 3) != 0) {
                (*header_item)->log = resize_sentinel_log_list((*header_item)->log, i + 1);
                (*header_item)->log[i] = alloc_sentinel_dive_log_line();

                if (!parse_sentinel_log_line((*header_item)->record_interval, (*header_item)->log[i], log_lines[i])) {
                    eprint("Unable to parse log line: %s", log_lines[i]);
                }

                i++;
            }

            free_string_array(log_lines);

        }

        free_string_array(header_and_profile);
    }

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
        dprint(true, "%s", "Original string is null, return null");
        return(NULL);
    }

    if (delim == NULL || delim == 0) {
        dprint(true, "%s", "Delimiter is null, return null");
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
        eprint("%s", "Unable to calloc string, return null");
        return(NULL);
    }

    if (old_str == NULL) {
        return(new_str);
    }

    char* tmp = strncpy(new_str, old_str, strlen(old_str));

    free(old_str);

    if (tmp == NULL) {
        eprint("%s", "Unable to strncpy the old string to the new");
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
        eprint("strftime returned 0 for %d", sentinel_time);
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
            eprint("%s", "Something went wrong while nanosleeping");
        }
    }
}

/**
 * restring: Takes a string and its length, and returns another string which has all \r \n and \t typed out
 **/

char* restring(const char* old_str, const int old_len) {
    // Just initialize the new string
    char* new_str = calloc(1, sizeof(char));

    int i = 0;
    int j = 0;

    while (i < old_len) {
        if (strncmp(old_str + i, "\n", 1) == 0) {
            new_str = resize_string(new_str, strlen(new_str) + 2);
            strncpy(new_str + j, "\\n", 2);
            j += 2;
        } else if (strncmp(old_str + i, "\r", 1) == 0) {
            new_str = resize_string(new_str, strlen(new_str) + 2);
            strncpy(new_str + j, "\\r", 2);
            j += 2;
        } else if (strncmp(old_str + i, "\t", 1) == 0) {
            new_str = resize_string(new_str, strlen(new_str) + 2);
            strncpy(new_str + j, "\\t", 2);
            j += 2;
        } else {
            new_str = resize_string(new_str, strlen(new_str) + 1);
            strncpy(new_str + j, old_str + i, 1);
            j++;
        }

        i++;
    }

    return(new_str);
}
