#include "device.h"
#include "common.h"
#include "init.h"
#include "hooks.h"

/**
 * imgrement_init_device - Initialize device for kernel 3.x-5.x
 * @dev: Device structure to initialize
 *
 * Opens the block device using kernel 3.x-5.x API (blkdev_get_by_path)
 * and installs I/O interception by replacing the make_request_fn.
 *
 * Return: 0 on success, negative error code on failure
 */
int imgrement_init_device(struct imgrement_device *dev)
{
    char *err;

    /* Open block device using old API */
    dev->base_dev = blkdev_get_by_path(DEVICE_FILE_PATH, FMODE_READ, NULL);
    _astgo(dev->base_dev != NULL && !IS_ERR(dev->base_dev),
           "Error getting base block device", err, init_error);

    dev->base_queue = bdev_get_queue(dev->base_dev);
    _astgo(dev->base_queue != NULL, "Error getting queue", err, init_error);

    /* Hook by replacing make_request_fn */
    dev->orig_req_fn = dev->base_queue->make_request_fn;
    dev->base_queue->make_request_fn = trace_request_fn;

    LOG("Kernel 3.x-5.x: Using make_request_fn hook for I/O interception");
    LOG("%s trace initialization succeeded", dev->base_dev->bd_disk->disk_name);
    return 0;

init_error:
    LOG_VAR(err);
    return -1;
}

/**
 * imgrement_cleanup_device - Clean up device for kernel 3.x-5.x
 * @dev: Device structure to clean up
 *
 * Restores the original make_request_fn and releases the block device.
 */
void imgrement_cleanup_device(struct imgrement_device *dev)
{
    /* Restore original make_request_fn */
    if (dev->base_queue != NULL && dev->orig_req_fn != NULL)
        dev->base_queue->make_request_fn = dev->orig_req_fn;

    if (dev->base_dev != NULL)
        blkdev_put(dev->base_dev, FMODE_READ);
}