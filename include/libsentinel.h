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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#ifndef LIBSENTINEL_H
#define LIBSENTINEL_H

/* Constants */
#define SENTINEL_TIME_START 694137600
static const char default_format[] = "%F %T %Z%z";
static const int SENTINEL_LOOP_SLEEP_MS = 5;
/* Commands */
static const char SENTINEL_LIST_CMD[1]  = {0x4d}; // d command to list the dive headers
static const char SENTINEL_WAIT_BYTE[1] = {0x50}; // P the rebreather prints this when it is waiting for a command
static const char SENTINEL_HEADER_START[3] = {0x64,0x0d, 0x0a}; // d\r\n
static const char SENTINEL_LINE_SEPARATOR[2] = {0x0d, 0x0a}; // \r\n
static const char SENTINEL_PROFILE_END[5] = {0x45, 0x6e, 0x64, 0x0d, 0x0a}; // End\r\n

/* Colors for printing */

#define RESET "\x1B[0m"
#define BOLD  "\x1B[1m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

/* Macros */
#define dprint(verbose, format, ...) ( verbose ? printf(BOLD KYEL "DEBUG: %s: %s: %d:" RESET " " format "\n", __FILE__, __func__, __LINE__, __VA_ARGS__) : 0)

#define eprint(format, ...) printf(BOLD KRED "ERROR: %s: %s: %d:" RESET " " format "\n", __FILE__, __func__, __LINE__, __VA_ARGS__)

/* Structs */
typedef struct sentinel_gas {
    int n2; /* Nitrogen percentage */
    int he; /* Helium percentage */
    int o2; /* O2 percentage, we calculate this from the previous two */
    double max_depth; /* Configured max depth for gas, originally given in decimeter */
    int enabled; /* Whether the gas is enabled */
} sentinel_gas_t;

typedef struct sentinel_tissue {
    int t1; /* Unknown */
    int t2; /* Unknown */
} sentinel_tissue_t;

typedef struct sentinel_note {
    char* note; /* Original note */
    int type; /* Type is taken from Subsurface */
    char* description; /* Longer description */
} sentinel_note_t;

typedef struct sentinel_dive_log_line {
    int time_idx; /* Time index */
    int time_s; /* Seconds since start of dive, equal to time_idx * record_interval */
    char* time_string; /* Beautified time string in the format hh:mm:ss */
    double depth; /* Converted to meter from decimeter */
    double po2; /* Converted from hectobar */
    int temperature;
    double scrubber_left; /* Converted from promille to percentage. This is broken in the newer version */
    double primary_battery_V; /* Converted from hectovolt, value for primary handset */
    double secondary_battery_V; /* Converted from hectovolt, value for secondary handset */
    int diluent_pressure; /* Diluent cylinder pressure */
    int o2_pressure; /* Oxygen cylinder pressure */
    double cell_o2[3]; /* pO2-reading for each cell */
    double setpoint; /* Converted from hectobar to bar */
    int ceiling; /* Decompression ceiling, m*/
    sentinel_note_t** note; /* Info, warning and alerts, can be max 3 per log line, last in array is always null */
    double tempstick_value[8]; /* Converted from decicelsius, there are 8 sensors along the tempstick */
    double co2; /* Converted from millibar to bar */
} sentinel_dive_log_line_t;

const sentinel_dive_log_line_t DEFAULT_LOG_LINE = {
    0,
    0,
    NULL,
    0.0,
    0.0,
    0,
    0.0,
    0.0,
    0.0,
    0,
    0,
    {0.0,0.0,0.0},
    0.0,
    0,
    NULL,
    {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0},
    0.0
};

typedef struct sentinel_dive_header {
    char* version;
    int record_interval;
    char* serial_number;
    int log_lines; /* Last number of Mem */
    int start_s; /* Original value converted to unixtime */
    int end_s; /* Original value converted to unixtime */
    int length_s; /* Length of the dive, in seconds */
    char* start_time; /* Derived string representation from the start_s */
    char* end_time; /* Derived string representation from the end_s */
    char* length_time; /* Derived string representation from the length_s */
    double max_depth;
    int status;
    int otu;
    int atm; /* Atmospheric pressure in mbar */
    int stack; /* No information what this is, could have something to do with the scrubber */
    int usage; /* Unknown */
    double cns;
    double safety; /* Unknown */
    int expert; /* Unknown */
    int tpm; /* Is the TPM enabled */
    char* decoalg; /* Decompression algorithm */
    double vgm_max_safety; /* Unknown */
    double vgm_stop_safety; /* Unknown */
    double vgm_mid_safety; /* Unknown */
    int filter_type;  /* Unknown */
    int cell_health[3]; /* Cell health for cell 1, 2 and 3 */
    sentinel_gas_t gas[10]; /* Configured gasses */
    sentinel_tissue_t tissue[16]; /* Not yet clear what these are */
    sentinel_dive_log_line_t** log; /* Allocate this based on the log_lines */
} sentinel_header_t;

const sentinel_header_t DEFAULT_HEADER = {
    NULL,
    0,
    NULL,
    0,
    0,
    0,
    0,
    NULL,
    NULL,
    NULL,
    0.0,
    0,
    0,
    0,
    0,
    0,
    0.0,
    0.0,
    0,
    0,
    NULL,
    0.0,
    0.0,
    0.0,
    0,
    {0,0,0},
    {{0,0,0,0.0,0},{0,0,0,0.0,0},{0,0,0,0.0,0},{0,0,0,0.0,0},{0,0,0,0.0,0},{0,0,0,0.0,0},{0,0,0,0.0,0},{0,0,0,0.0,0},{0,0,0,0.0,0},{0,0,0,0.0,0}},
    {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
    NULL
};

/* External functions */
extern int connect_sentinel(char* devicex);
extern int open_sentinel_device(char* device);
extern bool is_sentinel_idle(int fd, const int tries);
extern bool send_sentinel_command(int fd, const void* command, size_t size);
extern bool read_sentinel_response(int fd, char** buffer, const char* start, int start_len, const char end[], int end_len);
extern bool disconnect_sentinel(int fd);
extern bool download_sentinel_header(int fd, char** buffer);
extern bool parse_sentinel_header(sentinel_header_t** header_struct, char** buffer);
extern bool parse_sentinel_log_line(int interval, sentinel_dive_log_line_t* line, char* linestr);
extern sentinel_note_t** resize_sentinel_note_list(sentinel_note_t** old_list, int list_size);
extern sentinel_note_t* alloc_sentinel_note(void);
extern bool get_sentinel_dive_list(int fd, sentinel_header_t*** header_list);
extern sentinel_header_t* alloc_sentinel_header(void);
extern void free_sentinel_header(sentinel_header_t* header);
extern void free_sentinel_log(sentinel_dive_log_line_t* log);
extern void free_sentinel_log_list(sentinel_dive_log_line_t** log);
extern void print_sentinel_header(sentinel_header_t* header);
extern void short_print_sentinel_header(int number, sentinel_header_t* header);
extern void print_sentinel_log_line(int number, sentinel_dive_log_line_t* line);
extern void full_print_sentinel_dive(sentinel_header_t* header);
extern sentinel_dive_log_line_t** resize_sentinel_log_list(sentinel_dive_log_line_t** old_list, int list_size);
extern sentinel_dive_log_line_t* alloc_sentinel_dive_log_line(void);
extern void free_sentinel_dive_log_list(sentinel_dive_log_line_t** old_list);
extern sentinel_header_t** resize_sentinel_header_list(sentinel_header_t** old_list, int list_size);
extern void free_sentinel_header_list(sentinel_header_t** h_list);
extern bool get_sentinel_note(sentinel_note_t* note, char* note_str);
extern bool download_sentinel_dive(int device, int dive_num, sentinel_header_t** header_item);

/* Internal functions */
char** str_cut(char** orig_string, const char* delim);
int sentinel_to_unix_timestamp(int sentinel_time);
char* sentinel_to_utc_datestring(const int sentinel_time);
char* seconds_to_hms(const int seconds);
void sentinel_sleep(const int msecs);
char* resize_string(char* old_str, int len);
char** resize_string_array(char** old_arr, int len);
void free_string_array(char** str_arr);
char* restring(const char* old_str, const int old_len);
#endif  // LIBSENTINEL_H
