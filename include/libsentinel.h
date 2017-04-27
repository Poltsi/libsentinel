#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <stdbool.h>

#ifndef libsentinel_h__
#define libsentinel_h__

extern int connect_sentinel(void);
extern int open_sentinel_device(void);
extern bool send_sentinel_command(int fd, char *command);
extern bool read_sentinel_data(int fd, char *buffer);
extern bool disconnect_sentinel(int fd);
#endif  // libsentinel_h__
