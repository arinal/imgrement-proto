#include <linux/kthread.h>

#include "defines.h"
#include "common.h"
#include "ioa.h"
#include "device.h"
#include "init.h"

struct imgrement_device *imgrement_device;

static void log_ioa(const struct io_activity *ioa)
{
    int i;
    LOG("%s, bytes to transfer: %d, delta count: %d", ioa->rw ? "W" : "R",
        ioa->data_size, ioa->delta_count);

    for (i = 0; i < ioa->delta_count; ++i) {
        int count;
        struct block_delta *delta = &ioa->deltas[i];
        LOG("bv #%d: len %d, offset %d, %d sectors (%d..%d) or %d "
            "blocks (%d..%d)",
            i, delta->size, delta->offset, get_sectors_count(delta),
            delta->start_sector, delta->end_sector, get_blocks_count(delta),
            get_start_block(delta), get_end_block(delta));
        count = find_word_count("DAMN", delta->data, delta->size);
        if (count > 0)
            LOG("Found DAMN from deltas %d times", count);
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

void trace_request_fn(struct request_queue *queue, struct bio *bio)
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
    if (dev == NULL)
        return;

    imgrement_cleanup_device(dev);

    if (dev->major)
        unregister_blkdev(dev->major, DRIVER_NAME);
    kfree(dev);
}

static int __init imgrement_init(void)
{
    char *err;
    struct imgrement_device *dev;
    int ret;

    imgrement_device = kzalloc(sizeof(struct imgrement_device), GFP_KERNEL);
    _astgo(imgrement_device != NULL, "Error allocating", err, init_error);
    dev = imgrement_device;

    dev->block_count = 0;

    dev->major = register_blkdev(0, DRIVER_NAME);
    _astgo(dev->major > 0, "Error register block device", err, init_error);

    ret = imgrement_init_device(dev);
    if (ret < 0)
        goto init_error;

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
