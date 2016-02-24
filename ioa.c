#include "common.h"
#include "ioa.h"

#include <linux/module.h>
#include <linux/blkdev.h>

#ifndef HAVE_BVEC_ITER
typedef int bvec_iter;
typedef struct bio_vec *bio_vec;
#define bio_iter_len(bio, iter) ((bio)->bi_io_vec[(iter)].bv_len)
#define bio_iter_offset(bio, iter) ((bio)->bi_io_vec[(iter)].bv_offset)
#define bio_iter_page(bio, iter) ((bio)->bi_io_vec[(iter)].bv_page)
#define bio_iter_idx(iter) (iter)
#define bio_sector(bio) (bio)->bi_sector
#define bio_size(bio) (bio)->bi_size
#define bio_idx(bio) (bio)->bi_idx
#define bio_cnt(bio) (bio)->bi_vcnt
#else
typedef struct bvec_iter bvec_iter;
typedef struct bio_vec bio_vec;
#define bio_iter_idx(iter) (iter.bi_idx)
#define bio_sector(bio) (bio)->bi_iter.bi_sector
#define bio_size(bio) (bio)->bi_iter.bi_size
#define bio_idx(bio) (bio)->bi_iter.bi_idx
#endif

inline int get_block_sector_ratio(const struct block_delta *delta)
{
        return BLOCK_SIZE / SECTOR_SIZE;
}

inline int get_start_block(const struct block_delta *delta)
{
        return delta->start_sector / get_block_sector_ratio(delta);
}

inline int get_end_block(const struct block_delta *delta)
{
        return delta->end_sector / get_block_sector_ratio(delta);
}

inline int get_blocks_count(const struct block_delta *delta)
{
        return get_end_block(delta) - get_start_block(delta) + 1;
}

inline int get_sectors_count(const struct block_delta *delta)
{
        return delta->end_sector - delta->start_sector + 1;
}

inline int get_next_sector(const struct block_delta *delta)
{
        return delta->start_sector + delta->size / SECTOR_SIZE;
}

static int fill_delta(struct block_delta *delta, const struct bio *bio, bvec_iter iter, const int start_sector)
{
        int count;
        int next_sector;
        char *data;

        delta->start_sector = start_sector;
        delta->size = bio_iter_len(bio, iter);
        next_sector = get_next_sector(delta);
        delta->end_sector = next_sector - 1;
        delta->offset = bio_iter_offset(bio, iter);

        data = kmap(bio_iter_page(bio, iter));
        count = find_word_count("DAMN", data, PAGE_SIZE);
        if (count > 0) LOG("Found DAMN from page %d times", count);
        delta->data = vmalloc(delta->size);
        if (delta->data != NULL) memcpy(delta->data, data + delta->offset, delta->size);

        kunmap(bio_iter_page(bio, iter));
        return (delta->data != NULL)? next_sector : -1;
}

struct io_activity *create_ioa(struct bio *bio)
{
        char *err;
        struct io_activity *ioa = vmalloc(sizeof(struct io_activity));
        _astgo(ioa != NULL, "Error allocating io_activity", err, create_ioa_error);

        ioa->rw = bio_data_dir(bio);
        ioa->data_size = bio_size(bio);
        ioa->delta_count = bio_cnt(bio);

        if (ioa->delta_count != 0) {
                ioa->deltas = vmalloc(ioa->delta_count * sizeof(struct block_delta));
                _astgo(ioa->deltas != NULL, "Error allocating deltas", err, create_ioa_error);
        } else ioa->deltas = NULL;

        return ioa;

create_ioa_error:
        LOG_VAR(err);
        LOG("Error creating ioa from bio with: %lu sector, %d size", bio_sector(bio), bio_size(bio));
        ast_vfree(ioa->deltas);
        ast_vfree(ioa);
        return NULL;
}

struct io_activity *extract_ioa(struct bio *bio)
{
        int next_sector;
        bvec_iter iter;
        bio_vec bvec;
        char *err;
        struct block_delta *delta;

        struct io_activity *ioa = create_ioa(bio);
        _astgo(ioa != NULL, "Error creating io_activity", err, extract_error);
        next_sector = bio_sector(bio);
        delta = ioa->deltas;

        bio_for_each_segment(bvec, bio, iter) {
                next_sector = fill_delta(delta, bio, iter, next_sector);
                _astgo(next_sector >= 0, "Error filling delta->data", err, extract_error);
                delta++;
        }

        return ioa;

extract_error:
        LOG_VAR(err);
        return NULL;
}

void free_ioa(const struct io_activity *ioa)
{
        int i;
        for (i = 0; i < ioa->delta_count; ++i) ast_vfree(ioa->deltas[i].data);
        ast_vfree(ioa->deltas);
        ast_vfree(ioa);
}
