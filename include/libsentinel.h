#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifndef LIBSENTINEL_H
#define LIBSENTINEL_H

/* Commands */
static const char SENTINEL_LIST_CMD[]  = {0x4d}; // d command to list the dive headers
static const char SENTINEL_WAIT_BYTE[] = {0x50}; // P the rebreather prints this when it is waiting for a command

/* Macros */
#define dprint(verbose, ...) ( verbose ? printf(__VA_ARGS__) : 0) 
extern int connect_sentinel(void);
extern int open_sentinel_device(void);
extern bool send_sentinel_command(int fd, const char *command);
extern bool read_sentinel_data(int fd, char *buffer);
extern bool disconnect_sentinel(int fd);
extern bool get_sentinel_header(int fd, char *buffer);
#endif  // LIBSENTINEL_H
