#!/usr/bin/env bash

echo y | mkfs -t ext2 /dev/sdb
echo `blkid /dev/sdb | awk '{print$2}' | sed -e 's/"//g'` /mnt   ext2   noatime,nobarrier   0   0 >> /etc/fstab
mount /dev/sdb /mnt
