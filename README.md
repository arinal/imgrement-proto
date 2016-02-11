# imgrement proto - kernel daemon for tracing your disk's I/O requests
A simple kernel module to intercept all requests targeted at your disk device. Yeah.. it can literally catch all requests. This program map I/O information provided by kernel data structures into our own structures and show the information afterwards. The expected informations are sector number and data size. As you may have been thinking, we can show the information directly from kernel data structures without mapping it. The mapping part is meant to be a showcase only. 

## Getting started
This program can only be executed on Linux because it is a Linux kernel module. Even if you have different kernel version, it won't run unless you recompile it with matching version. Compilation has to be done on Linux as well. Well, if you know how to do it outside Linux, I'd love to hear it.

This program has already worked properly on Ubuntu. Windows or Mac users? Don't worry, you can still use virtual machines. To make your life easier, we've configured `Vagrantfile` which can create a VM with additional disk device to our tracer. The vagrant configuration only work with Virtual Box provider.

## Prerequisites
- Linux machine
- Latest GCC
- Linux headers

Or if you wanna run it in Vagrant:
- Vagrant
- Virtual Box

## Let's try it out with Vagrant
Clone repository from github and make it as current directory.
```
$ git clone https://github.com/arinal/imgrement-proto.git && cd imgrement-proto
```
Fire up vagrant to create new VM.
```
$ vagrant up
```

Login into the VM, change to root and go to our source.
```
$ vagrant ssh
vagrant@ubuntu:~$ sudo su
root@ubuntu:/home/vagrant# cd /vagrant
root@ubuntu:/vagrant#
```

Note that this folder is synced with our hosts machine e.g. your real PC, not virtual. Let's validate it by listing the files
```
root:/vagrant# ls
imgrement.c  Makefile  provision.sh  README.md  Vagrantfile
```

Validated, those files are our sources! Compile it using make.
```
root:/vagrant# make
```

After the compilation succeed, there will be several files generated
```
root@ubuntu:/vagrant# ls
imgrement.c      imgrement.mod.o  modules.order   README.md
imgrement.ko     imgrement.o      Module.symvers  Vagrantfile
imgrement.mod.c  Makefile         provision.sh
```

The most important file is `imgrement.ko`. It is the so called kernel module, and yes, `ko` is kernel object. We can install this module into the kernel by using `insmod` but before we use it, let's see what the vagrant file had configured for us. Let's see what disk already been attached.
```
root@ubuntu:/vagrant# lsblk
NAME   MAJ:MIN RM SIZE RO TYPE MOUNTPOINT
sda      8:0    0  40G  0 disk 
└─sda1   8:1    0  40G  0 part /
sdb      8:16   0   4M  0 disk /mnt
```

There is a device called `sdb` mounted on `/mnt`. For simplicity, `imgrement` is hard coded to trace a device named `sdb`. That being said, we have to make an I/O request to `sdb` to trigger our hook. Translated to human language, we have to perform read file operations (`ls`, `cat`) or write file operations (`rm`, `touch`, `echo`) inside `/mnt`.

Now let's clear kernel ring buffer and install our module.
```
root@ubuntu:/vagrant# dmesg -C
root@ubuntu:/vagrant# insmod imgrement.ko
```

To see the output from our module, use `dmesg`.
> All output from `printk` will be putted in kernel ring buffer and picked up by syslog user daemon. We can use `dmesg` to show syslog.

```
root@ubuntu:/vagrant# dmesg | tail
[ 4060.973987] imgrement: sdb trace initialization succeeded
```

Write some text to a file inside `/mnt` to initiate write I/O request.
```
# echo 'DAMNDAMNDAMN' >> /mnt/damn
# dmesg | tail
...
[ 4934.291668] imgrement: R, bytes to transfer: 1024, delta count: 1
[ 4934.291672] imgrement: bv #0: len 1024, offset 0, at (296, 297)
[ 4934.293533] imgrement: R, bytes to transfer: 1024, delta count: 1
[ 4934.293536] imgrement: bv #0: len 1024, offset 3072, at (38, 39)
[ 4934.293814] imgrement: R, bytes to transfer: 1024, delta count: 1
[ 4934.293816] imgrement: bv #0: len 1024, offset 2048, at (36, 37)
```

We can detect an `R` or read activity in `sdb`. Because `sdb` is formatted with 1024 bytes block size, every I/O request has 1024 size. Note that the number of sector is always two as in `(296, 297), (38, 39), (36, 37)`. It is because our disk sector size is 512 bytes so each request contains 2 sectors.

Why all of those activities are reads? Did we just write `DAMN` to a file? Kernel employs a delayed write, every write request will enter a cache first and will be flushed eventually to file at later time (like 10 seconds or so).

> Yes we can use `sync` to trigger flush but it will crash this program. I'll never fix this because this is only a demo :)

```
# dmesg | tail -20
[ 4964.326233] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4964.326238] imgrement: bv #0: len 1024, offset 1024, at (2, 3)
[ 4964.326296] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4964.326297] imgrement: bv #0: len 1024, offset 2048, at (4, 5)
[ 4964.326347] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4964.326349] imgrement: bv #0: len 1024, offset 2048, at (36, 37)
[ 4964.326395] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4964.326397] imgrement: bv #0: len 1024, offset 3072, at (38, 39)
[ 4964.326446] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4964.326447] imgrement: bv #0: len 1024, offset 0, at (40, 41)
[ 4964.326494] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4964.326496] imgrement: bv #0: len 1024, offset 1024, at (42, 43)
[ 4964.326544] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4964.326546] imgrement: bv #0: len 1024, offset 0, at (296, 297)
[ 4964.326678] imgrement: Found DAMN from page 3 times
[ 4964.326680] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4964.326681] imgrement: bv #0: len 1024, offset 0, at (1026, 1027)
[ 4964.326691] imgrement: Found DAMN from deltas 3 times

```

There, we have our write requests. Note that our program counts all `DAMN` words. All of requests targeted at small sector number is for updating metadata, and our `DAMN` text is located at sector 1026. Let's validate this by reading the file.

```
# cat /mnt/damn
DAMNDAMNDAMN
# dmesg | tail -4
[ 4964.326678] imgrement: Found DAMN from page 3 times
[ 4964.326680] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4964.326681] imgrement: bv #0: len 1024, offset 0, at (1026, 1027)
[ 4964.326691] imgrement: Found DAMN from deltas 3 times
```
This last 4 messages are from previous operations. We don't see any new read requests because our data is already in the cache. Discarding the cache and repeat the rad operation will surely trigger a read request.

```
# echo 3 > /proc/sys/vm/drop_caches
# cat /mnt/damn
DAMNDAMNDAMN
# dmesg | tail -4
...
[ 7326.931552] imgrement: R, bytes to transfer: 1024, delta count: 1
[ 7326.931556] imgrement: bv #0: len 1024, offset 0, at (1026, 1027)
```

See? By reading `/mnt/damn` we are creating a read request to sector 1026, the same sector with our last write request.

Last but not least, how many write request will be made if we write six (6) times?
```
# dmesg -C
# echo 'DAMNDAMNDAMN' >> /mnt/damn
# echo 'DAMNDAMNDAMN' >> /mnt/damn
# echo 'DAMNDAMNDAMN' >> /mnt/damn
# echo 'DAMNDAMNDAMN' >> /mnt/damn
# echo 'DAMNDAMNDAMN' >> /mnt/damn
# echo 'DAMNDAMNDAMN' >> /mnt/damn
# dmesg | tail
[ 8278.750985] imgrement: R, bytes to transfer: 1024, delta count: 1
[ 8278.750990] imgrement: bv #0: len 1024, offset 0, at (40, 41)
[ 8278.751094] imgrement: R, bytes to transfer: 1024, delta count: 1
....
....
[ 8278.755640] imgrement: bv #0: len 1024, offset 0, at (104, 105)
[ 8278.755698] imgrement: R, bytes to transfer: 1024, delta count: 1
[ 8278.755703] imgrement: bv #0: len 1024, offset 1024, at (42, 43)
```

No write request at all! Of course, wait until quite some time until the cache is flushed.
```
# dmesg | tail
....
....
[ 8278.755698] imgrement: R, bytes to transfer: 1024, delta count: 1
[ 8278.755703] imgrement: bv #0: len 1024, offset 1024, at (42, 43)
[ 8308.791450] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 8308.791454] imgrement: bv #0: len 1024, offset 1024, at (42, 43)
[ 8308.791589] imgrement: Found DAMN from page 21 times
[ 8308.791593] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 8308.791594] imgrement: bv #0: len 1024, offset 0, at (1026, 1027)
[ 8308.791604] imgrement: Found DAMN from deltas 21 times
```
Only 1 write and `DAMN` is found 21 times, the first 3 and a single batch of 18 damns.

Our last trial would be creating a large file, something like 12KB.
```
# dmesg -C
# dd if=/dev/urandom of=/mnt/random bs=1024 count=12
# La la la la la.. zzZZzz.. *waiting a kernel flush
La: command not found
# dmesg 
[  675.331523] imgrement: W, bytes to transfer: 1024, delta count: 1
[  675.331528] imgrement: bv #0: len 1024, offset 0, at (40, 41)
[  675.331589] imgrement: W, bytes to transfer: 1024, delta count: 1
[  675.331591] imgrement: bv #0: len 1024, offset 1024, at (42, 43)
[  675.331644] imgrement: W, bytes to transfer: 1024, delta count: 1
[  675.331646] imgrement: bv #0: len 1024, offset 0, at (296, 297)
[  675.331702] imgrement: W, bytes to transfer: 1024, delta count: 1
[  675.331704] imgrement: bv #0: len 1024, offset 2048, at (4, 5)
[  675.331756] imgrement: W, bytes to transfer: 1024, delta count: 1
[  675.331757] imgrement: bv #0: len 1024, offset 2048, at (36, 37)
[  675.331805] imgrement: W, bytes to transfer: 1024, delta count: 1
[  675.331806] imgrement: bv #0: len 1024, offset 3072, at (38, 39)
[  675.332183] imgrement: W, bytes to transfer: 12288, delta count: 3
[  675.332187] imgrement: bv #0: len 4096, offset 0, at (6154, 6161)
[  675.332222] imgrement: bv #1: len 4096, offset 0, at (6162, 6169)
[  675.332258] imgrement: bv #2: len 4096, offset 0, at (6170, 6177)
```
As usual, write request at low sector number is just updating metadata. The real meat happen at the last write request. It is consisted of 3 deltas with 4KB each.

## What next?
You can try by yourself, deleting a document with known sector numbers and bring it back again. If you want to learn further about all things Linux kernels, have a look at "Understanding Linux Kernel" and "Linux Device Driver". 
