obj-m := imgrement.o
imgrement-objs := src/main.o src/ioa.o src/common.o

KERNELVERSION = $(shell uname -r)
KDIR := /lib/modules/$(KERNELVERSION)/build
PWD := $(shell pwd)
EXTRA_CFLAGS := -g -I$(PWD)/src

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

reload:
	rmmod imgrement 2>/dev/null || :
	dmesg -C && sync && echo 3 > /proc/sys/vm/drop_caches && insmod imgrement.ko

format:
	clang-format -i src/*.c src/*.h

.PHONY: default clean reload format
