#ifndef DEFINES_H
#define DEFINES_H

#include <linux/module.h>
#include <linux/blkdev.h>

#define DRIVER_NAME "imgrement"
#define DEVICE_FILE_PATH "/dev/sdb"

#define SECTOR_SIZE  512

#define LOG_LABEL DRIVER_NAME

#endif /* DEFINES_H */
