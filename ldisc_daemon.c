#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define N_XBEE 17

int main(int argc, char **argv) {

    int err;
    int ldisc = N_XBEE;
    unsigned long baudrate;
    char port[80];
    char baud_command[80];

    // Check arguments
    if(argc != 3) {
        printf("usage: ldisc_daemon [serial port] [baud rate]\n");
        return -1;
    }

    if(sscanf(argv[2], "%li", &baudrate) != 1)
        perror("error reading port name\n");
    if(sscanf(argv[1], "%s", port) != 1)
        perror("error reading baud rate\n");

    printf("Setting %s to use N_XBEE with baud rate of %li\n", argv[1], baudrate);

    // Try to open serial port
    int fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(fd == -1) {
        printf("Couldn't open %s\n", argv[1]);
        return -1;
    }

    sprintf(baud_command, "stty -F %s %li", port, baudrate);
    if(err = system(baud_command)) {
        perror("Failed setting baud rate");
        return err;
    }

    // Set the line discipline
    if(err = ioctl(fd, TIOCSETD, &ldisc)) {
        perror("Setting line discipline failed");
        close(fd);
        return -err;
    }

    if(fork()) {
        printf("Line discipline attached, holding open tty");
        return 0;
    } else {
        while(1) {
            sleep(10);
        }
    }

    printf("Line discipline daemon shutting down...\n");


}

