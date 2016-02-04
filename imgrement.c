#include <linux/module.h>
#include <linux/blkdev.h>

#define DRIVER_NAME "imgrement"
#define SECTOR_SIZE 512

#define LOG(fmt, args...) printk(KERN_DEBUG DRIVER_NAME ": " fmt "\n", ## args)
#define LOG_VAR(msg) LOG("%s", msg)

/* assert goto */
#define _astgo(expr, msg, err, addr)            \
        if (!(expr)) {                          \
                err = msg;                      \
                goto addr;                      \
        }

#ifndef HAVE_BVEC_ITER
typedef int bvecIter;
typedef struct bio_vec *bioVec;
#define bio_iter_len(bio, iter) ((bio)->bi_io_vec[(iter)].bv_len)
#define bio_iter_offset(bio, iter) ((bio)->bi_io_vec[(iter)].bv_offset)
#define bio_iter_page(bio, iter) ((bio)->bi_io_vec[(iter)].bv_page)
#define bio_iter_idx(iter) (iter)
#define bio_sector(bio) (bio)->bi_sector
#define bio_size(bio) (bio)->bi_size
#define bio_idx(bio) (bio)->bi_idx
#else
typedef struct bvecIter bvecIter;
typedef struct bioVec bioVec;
#define bio_iter_idx(iter) (iter.bi_idx)
#define bio_sector(bio) (bio)->bi_iter.bi_sector
#define bio_size(bio) (bio)->bi_iter.bi_size
#define bio_idx(bio) (bio)->bi_iter.bi_idx
#endif

struct imgrementDevice {
        int major;
        struct request_queue *baseQueue;
        struct block_device *baseDev;
        make_request_fn *origReqFn;
} *imgrementDevice;

static void traceRequestFn(struct request_queue *queue, struct bio *bio)
{
        int i, offset, size;
        bvecIter iter;
        bioVec bvec;
        char *data;

        sector_t startSect, endSect = bio_sector(bio);
        int wholeSize = bio_size(bio);
        int write = bio_data_dir(bio);

        LOG("%s sector: %d, bytes to transfer: %d",
            write? "W" : "R",
            endSect,
            wholeSize);

        bio_for_each_segment(bvec, bio, iter) {
                startSect = endSect;
                size = bio_iter_len(bio, iter);
                endSect = startSect + size / SECTOR_SIZE - 1;

                LOG("bv #%d: len %d, offset %d, at (%d, %d)",
                    bio_iter_idx(iter),
                    bio_iter_len(bio, iter),
                    bio_iter_offset(bio, iter),
                    startSect, endSect);

                data = page_address(bio_iter_page(bio, iter));

                /* ret = cow_write_current(dev->sd_cow, startSect, data); */
                offset = bio_iter_offset(bio, iter);
                for (i = offset; i < size; ++i)
                        if (data[i] == 'D' && data[i + 1] == 'A' && data[i + 2] == 'M' && data[i + 3] == 'N')
                                LOG("%s at %d", data + i, i);
                endSect++;
                /* kunmap(bio_iter_page(bio, iter)); */
        }

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

        LOG("%s trace initialization succeeded", dev->baseDev->bd_disk->disk_name);

        return 0;

init_error:
        LOG_VAR(err);
        imgrementExit();
        return -1;
}

module_init(imgrementInit);
module_exit(imgrementExit);
