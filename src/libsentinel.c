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

bool get_sentinel_header(int fd, char *buffer) {
    send_sentinel_command(fd, SENTINEL_LIST_CMD);
    read_sentinel_data(fd, buffer);
    return(false);
}
