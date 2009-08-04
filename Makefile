obj-m := n_xbee.o

KERNELDIR := ./kernel/

PWD := $(shell pwd)

CC := ${OE}/tmp/cross/armv7a/bin/arm-angstrom-linux-gnueabi-gcc

default: driver ldisc_daemon

clean:
	rm -f *.ko *.o *.mod.c Module.symvers modules.order

driver:
	@echo "Building driver"
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	
ldisc_daemon:
	@echo "Building line discipline daemon"
	$(CC) -o ldisc_daemon ldisc_daemon.c

