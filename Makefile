obj-m := n_turk.o

KERNELDIR := /home/rob/linux-2.6.25.10/

PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	
ldiscdaemon:
	avr32-linux-gcc -o ldisc_daemon ldisc_daemon.c

injector:
	avr32-linux-gcc -o injector injector.c
	
listener:
	avr32-linux-gcc -o udp_listener udp_listener.c
	
