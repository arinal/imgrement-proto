#ifndef HOOKS_H
#define HOOKS_H

#include <linux/blkdev.h>

/* Forward declarations for I/O interception hooks */

#ifndef KERNEL_6X_OR_NEWER
/**
 * trace_request_fn - I/O interception callback for kernel 3.x-5.x
 * @queue: Request queue
 * @bio: Block I/O request
 *
 * Intercepts I/O requests, logs them, and forwards to original handler.
 * Used with make_request_fn mechanism in older kernels.
 */
void trace_request_fn(struct request_queue *queue, struct bio *bio);
#endif

/* External reference to device instance needed by hooks */
extern struct imgrement_device *imgrement_device;

#endif /* HOOKS_H */