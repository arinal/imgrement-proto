obj-m := imgrement.o

KERNELVERSION = $(shell uname -r)
KDIR := /lib/modules/$(KERNELVERSION)/build
PWD := $(shell pwd)
EXTRA_CFLAGS := -g -I$(PWD)/src

# Determine kernel version and include appropriate init file
KERNEL_MAJOR := $(shell echo $(KERNELVERSION) | cut -d. -f1)
ifeq ($(shell test $(KERNEL_MAJOR) -ge 6 && echo yes),yes)
    imgrement-objs := src/main.o src/ioa.o src/common.o src/init_kernel6.o
else
    imgrement-objs := src/main.o src/ioa.o src/common.o src/init_kernel3.o
endif

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

reload:
	rmmod imgrement 2>/dev/null || :
	dmesg -C && sync && echo 3 > /proc/sys/vm/drop_caches && insmod imgrement.ko

format:
	clang-format -i src/*.c src/*.h

compile_commands.json:
	bear -- $(MAKE)

.PHONY: default clean reload format
