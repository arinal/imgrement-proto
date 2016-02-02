#include <linux/module.h>
#include <linux/blkdev.h>

#define DRIVER_NAME "imgrement"

#define LOG(fmt, args...) printk(KERN_DEBUG DRIVER_NAME ": " fmt "\n", ## args)
#define LOG_VAR(msg) LOG("%s", msg)

/* assert goto */
#define _astgo(expr, msg, err, addr)           \
        if (!(expr)) {                          \
                err = msg;                      \
                goto addr;                      \
        }

struct imgrementDevice {
        int major;
        struct request_queue *baseQueue;
        struct block_device *baseDev;
        make_request_fn *origReqFn;
} *imgrementDevice;

static void traceRequestFn(struct request_queue *queue, struct bio *bio)
{
        LOG("Traced!");
        imgrementDevice->origReqFn(queue, bio);
}

static void imgrementExit(void)
{
        struct imgrementDevice *dev;

        LOG("Removing imgrement, rollback every changes");
        dev = imgrementDevice;
        if (dev == NULL) return;
        if (dev->baseQueue != NULL && dev->origReqFn != NULL) dev->baseQueue->make_request_fn = dev->origReqFn;
        unregister_blkdev(dev->major, DRIVER_NAME);
        kfree(imgrementDevice);
}
module_exit(imgrementExit);

static int __init imgrementInit(void)
{
        char* err = NULL;
        struct imgrementDevice *dev = NULL;

        LOG("Initializing..");

        imgrementDevice = kzalloc(sizeof(struct imgrementDevice), GFP_KERNEL);
        _astgo(imgrementDevice != NULL, "Error allocating", err, init_error);
        dev = imgrementDevice;

        dev->major = register_blkdev(0, DRIVER_NAME);
        _astgo(dev->major > 0, "Error register block device", err, init_error);

        dev->baseDev = blkdev_get_by_path("/dev/sdb", FMODE_READ, NULL);
        _astgo(dev->baseDev != NULL, "Error getting base block device", err, init_error);

        dev->baseQueue = bdev_get_queue(dev->baseDev);
        _astgo(dev->baseQueue != NULL, "Error getting queue", err, init_error);
        dev->origReqFn = dev->baseQueue->make_request_fn;
        dev->baseQueue->make_request_fn = traceRequestFn;

        LOG("Init succeeded");

        return 0;

init_error:
        LOG_VAR(err);
        imgrementExit();
        return -1;
}
module_init(imgrementInit);
