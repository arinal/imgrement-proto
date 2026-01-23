#ifndef INIT_H
#define INIT_H

#include "defines.h"

/* Forward declaration */
struct imgrement_device;

/**
 * imgrement_init_device - Initialize and hook the block device
 * @dev: Device structure to initialize
 *
 * Opens the target block device and installs I/O interception hooks.
 * Implementation is kernel version-specific.
 *
 * Return: 0 on success, negative error code on failure
 */
int imgrement_init_device(struct imgrement_device *dev);

/**
 * imgrement_cleanup_device - Clean up and unhook the block device
 * @dev: Device structure to clean up
 *
 * Removes I/O interception hooks and releases the block device.
 * Implementation is kernel version-specific.
 */
void imgrement_cleanup_device(struct imgrement_device *dev);

#endif /* INIT_H */