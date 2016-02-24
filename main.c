#include <linux/kthread.h>

#include "defines.h"
#include "common.h"
#include "ioa.h"

struct imgrement_device {
        int major;
        int block_count;

        struct request_queue *base_queue;
        struct block_device *base_dev;
        make_request_fn *orig_req_fn;
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

static void trace_request_fn(struct request_queue *queue, struct bio *bio)
{
        struct io_activity *ioa;

        ioa = extract_ioa(bio);
        log_ioa(ioa);
        free_ioa(ioa);

        imgrement_device->orig_req_fn(imgrement_device->base_queue, bio);
}

static void imgrement_exit(void)
{
        struct imgrement_device *dev;

        LOG("Removing imgrement, rollback every changes");
        dev = imgrement_device;

        if (dev == NULL) return;
        if (dev->base_queue != NULL && dev->orig_req_fn != NULL) dev->base_queue->make_request_fn = dev->orig_req_fn;
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

        dev->base_dev = blkdev_get_by_path(DEVICE_FILE_PATH, FMODE_READ, NULL);
        _astgo(dev->base_dev != NULL, "Error getting base block device", err, init_error);

        dev->base_queue = bdev_get_queue(dev->base_dev);
        _astgo(dev->base_queue != NULL, "Error getting queue", err, init_error);
        dev->orig_req_fn = dev->base_queue->make_request_fn;

        dev->base_queue->make_request_fn = trace_request_fn;

        LOG("%s trace initialization succeeded", dev->base_dev->bd_disk->disk_name);
        return 0;

init_error:
        LOG_VAR(err);
        imgrement_exit();
        return -1;
}

module_init(imgrement_init);
module_exit(imgrement_exit);
