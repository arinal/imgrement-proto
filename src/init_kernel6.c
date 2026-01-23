#include "device.h"
#include "common.h"
#include "init.h"

/**
 * imgrement_init_device - Initialize device for kernel 6.x
 * @dev: Device structure to initialize
 *
 * Opens the block device using kernel 6.x API (bdev_file_open_by_path)
 * and sets up necessary structures. Note: Kernel 6.x hooking is done
 * via submit_bio callback in the main module.
 *
 * Return: 0 on success, negative error code on failure
 */
int imgrement_init_device(struct imgrement_device *dev)
{
    char *err;

    /* Open block device file using new API */
    dev->base_file = bdev_file_open_by_path(DEVICE_FILE_PATH, BLK_OPEN_READ,
                                            THIS_MODULE, NULL);
    _astgo(dev->base_file != NULL && !IS_ERR(dev->base_file),
           "Error opening block device file", err, init_error);

    /* Get the block_device from the file */
    dev->base_dev = file_bdev(dev->base_file);
    _astgo(dev->base_dev != NULL, "Error getting block device from file", err,
           init_error);

    dev->base_queue = bdev_get_queue(dev->base_dev);
    _astgo(dev->base_queue != NULL, "Error getting queue", err, init_error);

    LOG("Kernel 6.x: Using submit_bio callback for I/O interception");
    LOG("%s trace initialization succeeded", dev->base_dev->bd_disk->disk_name);
    return 0;

init_error:
    LOG_VAR(err);
    return -1;
}

/**
 * imgrement_cleanup_device - Clean up device for kernel 6.x
 * @dev: Device structure to clean up
 *
 * Releases the file handle obtained from bdev_file_open_by_path.
 */
void imgrement_cleanup_device(struct imgrement_device *dev)
{
    if (dev->base_file != NULL)
        fput(dev->base_file);
}