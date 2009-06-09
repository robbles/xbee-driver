#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define N_XBEE 18

int main(int argc, char **argv) {

    if(fork()) {
        printf("LDISC daemon started (holding open serial line)\n");
    }
    else {
        int fd = open("/dev/ttyS2", O_RDWR | O_NOCTTY | O_NONBLOCK);
        if(fd == -1) {
            printf("Couldn't open /dev/ttyS2!\n");
            return -1;
        } else {
            int i = N_XBEE;
            int result = ioctl(fd, TIOCSETD, &i);
            if(result) {
                printf("error setting line discipline...err # is %u", result);
                close(fd);
                return -1;
            }
            
            printf("successfully set line discipline to N_XBEE");
            while(1) {
                sleep(10);
            }
            return 0;

        }
    }

}

