obj-m := imgrement.o
imgrement-objs := main.o ioa.o

KERNELVERSION = $(shell uname -r)
KDIR := /lib/modules/$(KERNELVERSION)/build
PWD := $(shell pwd)
EXTRA_CFLAGS := -g

default:
	$(MAKE) -I/usr/include -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	$(MAKE) -I/usr/include -C $(KDIR) SUBDIRS=$(PWD) clean

reload:
	rmmod imgrement
	dmesg -C
	sync
	echo 3 > /proc/sys/vm/drop_caches
	insmod imgrement.ko
