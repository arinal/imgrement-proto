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
#define bio_cnt(bio) (bio)->bi_vcnt
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

struct blockDelta {
        int start;
        int end;
        int size;
        int offset;
        char *data;
};

struct ioActivity {
        char rw;
        int dataSize;
        int deltaCount;
        struct blockDelta *deltas;
};

static int findWordCount(char *word, char *data, int len)
{
        int i = 0, count = 0;
        for (; i < len; ++i)
                count += (strnstr(data + i, word, strlen(word)) == NULL)? 0 : 1;
        return count;
}

static struct ioActivity *extractActivity(struct bio *bio)
{
        int nextSect;
        bvecIter iter;
        bioVec bvec;
        char *data;
        char *err = NULL;

        struct blockDelta *delta;
        struct ioActivity *ioActivity;

        ioActivity = vmalloc(sizeof(struct ioActivity));
        _astgo(ioActivity != NULL, "Error allocating ioActivity", err, extract_error);
        ioActivity->rw = bio_data_dir(bio);
        ioActivity->dataSize = bio_size(bio);
        ioActivity->deltaCount = bio_cnt(bio);

        ioActivity->deltas = vmalloc(ioActivity->deltaCount * sizeof(struct blockDelta));
        _astgo(ioActivity->deltas != NULL, "Error allocating ioActivity->deltas", err, extract_error);
        delta = ioActivity->deltas;
        nextSect = bio_sector(bio);

        bio_for_each_segment(bvec, bio, iter) {
                int count;

                delta->start = nextSect;
                delta->size = bio_iter_len(bio, iter);
                nextSect = delta->start + delta->size / SECTOR_SIZE;
                delta->end = nextSect - 1;
                delta->offset = bio_iter_offset(bio, iter);

                data = kmap(bio_iter_page(bio, iter));
                count = findWordCount("DAMN", data, PAGE_SIZE);
                if (count > 0) LOG("Found DAMN from page %d times", count);
                delta->data = vmalloc(delta->size);
                _astgo(delta->data != NULL, "Error allocating delta->data", err, extract_error);
                memcpy(delta->data, data + delta->offset, delta->size);
                kunmap(bio_iter_page(bio, iter));

                delta++;
        }

        return ioActivity;

extract_error:
        LOG_VAR(err);
        return NULL;
}

static void freeIoa(struct ioActivity *ioa)
{
        int i;
        for (i = 0; i < ioa->deltaCount; ++i) vfree(ioa->deltas[i].data);
        vfree(ioa->deltas);
        vfree(ioa);
}

static void logActivity(struct ioActivity *ioa)
{
        int i;
        LOG("%s, bytes to transfer: %d, delta count: %d", ioa->rw? "W" : "R", ioa->dataSize, ioa->deltaCount);

        for (i = 0; i < ioa->deltaCount; ++i) {
                int count;
                struct blockDelta *delta = &ioa->deltas[i];
                LOG("bv #%d: len %d, offset %d, at (%d, %d)", i, delta->size, delta->offset, delta->start, delta->end);
                count = findWordCount("DAMN", delta->data, delta->size);
                if (count > 0) LOG("Found DAMN from deltas %d times", count);
        }
}

static void traceRequestFn(struct request_queue *queue, struct bio *bio)
{
        struct ioActivity *ioa;

        ioa = extractActivity(bio);
        logActivity(ioa);
        freeIoa(ioa);

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
        char* err;
        struct imgrementDevice *dev;

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
