/**
 * defines.h - Configuration constants for the imgrement kernel module
 *
 * This header defines global constants used throughout the module for
 * device configuration, sector size, and logging identification.
 */

#ifndef DEFINES_H
#define DEFINES_H

#include <linux/module.h>
#include <linux/blkdev.h>

/**
 * DRIVER_NAME - Identifier for the kernel module
 *
 * Used in module registration, kernel logs, and system identification.
 * This name appears in lsmod output and dmesg messages.
 */
#define DRIVER_NAME "imgrement"

/**
 * DEVICE_FILE_PATH - Path to the block device to trace
 *
 * Specifies which block device the module should intercept I/O requests for.
 * Default is /dev/sdb. Change this to trace a different device.
 *
 * WARNING: Ensure this points to a test device with no important data.
 * The module will intercept all I/O to this device.
 *
 * Example: To trace /dev/sdc instead, change to "/dev/sdc"
 */
#define DEVICE_FILE_PATH "/dev/sdb"

/**
 * IMGREMENT_BLOCK_SIZE - Filesystem block size in bytes
 *
 * Defines the standard filesystem block size (1024 bytes).
 * Used for block-to-sector conversions and block calculations.
 *
 * Note: This is the typical block size for ext2/ext3/ext4 filesystems
 * with default formatting options. We use IMGREMENT_BLOCK_SIZE instead
 * of BLOCK_SIZE to avoid conflicts with kernel headers.
 */
#define IMGREMENT_BLOCK_SIZE 1024

/**
 * SECTOR_SIZE - Standard disk sector size in bytes
 *
 * The standard sector size is 512 bytes. Some newer kernels define this
 * in linux/blk_types.h, but older kernels (like 3.13) don't expose it.
 * We define it here to ensure compatibility across kernel versions.
 */
#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif

/**
 * LOG_LABEL - Prefix for kernel log messages
 *
 * Automatically prepended to all log messages output by the module.
 * This makes it easy to filter module output with: dmesg | grep imgrement
 */
#define LOG_LABEL DRIVER_NAME

#endif /* DEFINES_H */
