#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifndef LIBSENTINEL_H
#define LIBSENTINEL_H

/* Constants */
#define SENTINEL_TIME_START 694137600
static const char default_format[] = "%F %T %Z%z";

/* Commands */
static const char SENTINEL_LIST_CMD[]  = {0x4d}; // d command to list the dive headers
static const char SENTINEL_WAIT_BYTE[] = {0x50}; // P the rebreather prints this when it is waiting for a command

/* Macros */
#define dprint(verbose, ...) ( verbose ? printf(__VA_ARGS__) : 0)

/* Structs */
typedef struct sentinel_gas {
    int n2; /* Nitrogen percentage */
    int he; /* Helium percentage */
    int o2; /* O2 percentage, we calculate this from the previous two */
    long max_depth; /* Configured max depth for gas, originally given in decimeter */
    int enabled; /* Whether the gas is enabledx */
} sentinel_gas_t;

typedef struct sentinel_tissue {
    int t1; /* Unknown */
    int t2; /* Unknown */
} sentinel_tissue_t;

typedef struct sentinel_note {
    char *note; /* Original note */
    int type; /* Type of note, 0 == info, 1 == warning, 2 == error */
} sentinel_note_t;

typedef struct sentinel_dive_log_line {
    int time_idx; /* Time index */
    int time_s; /* Seconds since start of dive, equal to time_idx * record_interval */
    char *time_string; /* Beautified time string in the format hh:mm:ss */
    long depth; /* Converted to meter from decimeter */
    long po2; /* Converted from hectobar */
    int temperature;
    long scrubber_left; /* Converted from promille to percentage. This is broken in the newer version */
    long primary_battery_V; /* Converted from hectovolt, value for primary handset */
    long secondary_battery_V; /* Converted from hectovolt, value for secondary handset */
    int diluent_pressure; /* Diluent cylinder pressure */
    int o2_pressure; /* Oxygen cylinder pressure */
    long cell_o2[3]; /* pO2-reading for each cell */
    long setpoint; /* Converted from hectobar to bar */
    int ceiling; /* Decompression ceiling, m*/
    sentinel_note_t note[3]; /* Info, warning and alerts, can be max 3 per log line */
    long tempstick_value[8]; /* Converted from decicelsius, there are 8 sensors along the tempstick */
    long co2; /* Converted from millibar to bar */
} sentinel_dive_log_line_t;

typedef struct sentinel_dive_header {
    char *version;
    int record_interval;
    char *serial_number;
    int log_lines; /* Last number of Mem */
    int start_s; /* Original value converted to unixtime */
    int end_s; /* Original value converted to unixtime */
    char *start_time; /* Derived string representation from the start_s */
    char *end_time; /* Derived string representation from the end_s */
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
    char *decoalg; /* Decompression algorithm */
    double vgm_max_safety; /* Unknown */
    double vgm_stop_safety; /* Unknown */
    double vgm_mid_safety; /* Unknown */
    int filter_type;  /* Unknown */
    int cell_health[3]; /* Cell health for cell 1, 2 and 3*/
    sentinel_gas_t gas[10]; /* Configured gasses */
    sentinel_tissue_t tissue[16]; /* Not yet clear what these are */
    sentinel_dive_log_line_t **log; /* Allocate this based on the log_lines */
} sentinel_header_t;

/* External functions */
extern int connect_sentinel(char *devicex);
extern int open_sentinel_device(char *device);
extern bool send_sentinel_command(int fd, const char *command);
extern bool read_sentinel_data(int fd, char *buffer);
extern bool disconnect_sentinel(int fd);
extern bool download_sentinel_header(int fd, char *buffer);
extern bool parse_sentinel_header(sentinel_header_t *header_struct, char *buffer);
extern bool get_sentinel_dive_list(int fd, char *buffer);

/* Internal functions */
char **str_cut(char *orig_string, const char *delim);
int sentinel_to_unix_timestamp(int sentinel_time);
char *sentinel_to_utc_datestring(const int sentinel_time);
#endif  // LIBSENTINEL_H
