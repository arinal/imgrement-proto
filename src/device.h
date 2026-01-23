#ifndef DEVICE_H
#define DEVICE_H

#include <linux/version.h>

/* Conditional compilation for different kernel versions */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
#define KERNEL_6X_OR_NEWER 1
#endif

/**
 * struct imgrement_device - Main device state structure
 * @major: Major device number registered with the kernel
 * @block_count: Counter for tracked blocks (currently unused)
 * @base_queue: Request queue of the target block device
 * @base_dev: Pointer to the target block device
 * @base_file: File handle for block device (kernel 6.x only)
 * @orig_req_fn: Original make_request_fn pointer (kernel 3.x-5.x only)
 */
struct imgrement_device {
    int major;
    int block_count;

    struct request_queue *base_queue;
    struct block_device *base_dev;
#ifdef KERNEL_6X_OR_NEWER
    struct file *base_file; /* Kernel 6.x uses file handles */
#else
    make_request_fn *orig_req_fn;
#endif
};

#endif /* DEVICE_H */