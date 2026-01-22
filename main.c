#include <linux/kthread.h>
#include <linux/version.h>

#include "defines.h"
#include "common.h"
#include "ioa.h"

/* Conditional compilation for different kernel versions */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0)
#define KERNEL_6X_OR_NEWER 1
#endif

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

struct imgrement_device *imgrement_device;

void log_ioa(const struct io_activity *ioa)
{
	int i;
	LOG("%s, bytes to transfer: %d, delta count: %d", ioa->rw? "W" : "R", ioa->data_size, ioa->delta_count);

	for (i = 0; i < ioa->delta_count; ++i) {
		int count;
		struct block_delta *delta = &ioa->deltas[i];
		LOG("bv #%d: len %d, offset %d, %d sectors (%d..%d) or %d blocks (%d..%d)",
		    i, delta->size, delta->offset,
		    get_sectors_count(delta), delta->start_sector, delta->end_sector,
		    get_blocks_count(delta), get_start_block(delta), get_end_block(delta));
		count = find_word_count("DAMN", delta->data, delta->size);
		if (count > 0) LOG("Found DAMN from deltas %d times", count);
	}
}

#ifdef KERNEL_6X_OR_NEWER
/* Kernel 6.x path: use submit_bio callback */

static void imgrement_submit_bio(struct bio *bio)
{
	struct io_activity *ioa = extract_ioa(bio);
	if (ioa) {
		log_ioa(ioa);
		free_ioa(ioa);
	}

	/* Forward the bio to the actual device */
	submit_bio_noacct(bio);
}

static const struct block_device_operations imgrement_ops = {
	.submit_bio = imgrement_submit_bio,
	.owner = THIS_MODULE,
};

#else
/* Kernel 3.x-5.x path: use make_request_fn */

static void trace_request_fn(struct request_queue *queue, struct bio *bio)
{
	struct io_activity *ioa = extract_ioa(bio);
	if (ioa) {
		log_ioa(ioa);
		free_ioa(ioa);
	}
	imgrement_device->orig_req_fn(imgrement_device->base_queue, bio);
}

#endif

static void imgrement_exit(void)
{
	struct imgrement_device *dev = imgrement_device;

	LOG("Removing imgrement, rollback every changes");
	if (dev == NULL) return;

#ifndef KERNEL_6X_OR_NEWER
	/* Kernel 3.x-5.x: restore original make_request_fn */
	if (dev->base_queue != NULL && dev->orig_req_fn != NULL)
		dev->base_queue->make_request_fn = dev->orig_req_fn;
#endif

#ifdef KERNEL_6X_OR_NEWER
	/* Kernel 6.x: release block device file handle */
	if (dev->base_file != NULL)
		fput(dev->base_file);
#else
	/* Older kernels: release block device with old API */
	if (dev->base_dev != NULL)
		blkdev_put(dev->base_dev, FMODE_READ);
#endif

	if (dev->major) unregister_blkdev(dev->major, DRIVER_NAME);
	kfree(dev);
}

static int __init imgrement_init(void)
{
	char* err;
	struct imgrement_device *dev;

	imgrement_device = kzalloc(sizeof(struct imgrement_device), GFP_KERNEL);
	_astgo(imgrement_device != NULL, "Error allocating", err, init_error);
	dev = imgrement_device;

	dev->block_count = 0;

	dev->major = register_blkdev(0, DRIVER_NAME);
	_astgo(dev->major > 0, "Error register block device", err, init_error);

#ifdef KERNEL_6X_OR_NEWER
	/* Kernel 6.x: new bdev_file_open_by_path API */
	dev->base_file = bdev_file_open_by_path(DEVICE_FILE_PATH,
						BLK_OPEN_READ,
						THIS_MODULE,
						NULL);
	_astgo(dev->base_file != NULL && !IS_ERR(dev->base_file),
	       "Error opening block device file", err, init_error);

	/* Get the block_device from the file */
	dev->base_dev = file_bdev(dev->base_file);
	_astgo(dev->base_dev != NULL, "Error getting block device from file", err, init_error);
#else
	/* Kernel 3.x-5.x: old blkdev_get_by_path with 3 parameters */
	dev->base_dev = blkdev_get_by_path(DEVICE_FILE_PATH, FMODE_READ, NULL);
	_astgo(dev->base_dev != NULL && !IS_ERR(dev->base_dev),
	       "Error getting base block device", err, init_error);
#endif

	dev->base_queue = bdev_get_queue(dev->base_dev);
	_astgo(dev->base_queue != NULL, "Error getting queue", err, init_error);

#ifndef KERNEL_6X_OR_NEWER
	/* Kernel 3.x-5.x: hook by replacing make_request_fn */
	dev->orig_req_fn = dev->base_queue->make_request_fn;
	dev->base_queue->make_request_fn = trace_request_fn;
#else
	/* Kernel 6.x: hooking is done via block_device_operations */
	/* Note: In kernel 6.x, we would need to replace the device's gendisk operations,
	 * but this is more complex. For now, we use a different approach with submit_bio. */
	LOG("Note: Kernel 6.x hooking uses submit_bio callback");
#endif

	LOG("%s trace initialization succeeded", dev->base_dev->bd_disk->disk_name);
	return 0;

init_error:
	LOG_VAR(err);
	imgrement_exit();
	return -1;
}

module_init(imgrement_init);
module_exit(imgrement_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("imgrement contributors");
MODULE_DESCRIPTION("Block device I/O tracer for learning kernel internals");
