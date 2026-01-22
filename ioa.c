#include "common.h"
#include "ioa.h"

#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include <linux/version.h>

/* Conditional compilation for bio structure changes */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0)
#undef HAVE_BVEC_ITER
#else
#define HAVE_BVEC_ITER
#endif

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
/* bio_cnt is bio_segments() in newer kernels */
#define bio_cnt(bio) bio_segments(bio)
#endif

inline int get_block_sector_ratio(const struct block_delta *delta)
{
	return IMGREMENT_BLOCK_SIZE / SECTOR_SIZE;
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

#ifdef HAVE_BVEC_ITER
/* Kernel 3.14+ uses bio_for_each_segment with bvec_iter */
static int fill_delta(struct block_delta *delta, const struct bio *bio, struct bio_vec bvec)
{
	int count;
	int next_sector;
	char *data;

	delta->start_sector = bio_sector(bio);
	delta->size = bvec.bv_len;
	delta->offset = bvec.bv_offset;
	next_sector = delta->start_sector + delta->size / SECTOR_SIZE;
	delta->end_sector = next_sector - 1;

	data = kmap(bvec.bv_page);
	count = find_word_count("DAMN", data, PAGE_SIZE);
	if (count > 0) LOG("Found DAMN from page %d times", count);
	delta->data = vmalloc(delta->size);
	if (delta->data != NULL) memcpy(delta->data, data + delta->offset, delta->size);

	kunmap(bvec.bv_page);
	return (delta->data != NULL)? next_sector : -1;
}
#else
/* Older kernels */
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
#endif

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
#ifdef HAVE_BVEC_ITER
	struct bvec_iter iter;
	struct bio_vec bvec;
#else
	bvec_iter iter;
	bio_vec bvec;
	int next_sector;
#endif
	char *err;
	struct block_delta *delta;

	struct io_activity *ioa = create_ioa(bio);
	_astgo(ioa != NULL, "Error creating io_activity", err, extract_error);

	delta = ioa->deltas;

#ifdef HAVE_BVEC_ITER
	/* Modern kernel bio iteration */
	bio_for_each_segment(bvec, bio, iter) {
		if (fill_delta(delta, bio, bvec) < 0) {
			err = "Error filling delta->data";
			goto extract_error;
		}
		delta++;
	}
#else
	/* Old kernel bio iteration */
	next_sector = bio_sector(bio);
	bio_for_each_segment(bvec, bio, iter) {
		next_sector = fill_delta(delta, bio, iter, next_sector);
		_astgo(next_sector >= 0, "Error filling delta->data", err, extract_error);
		delta++;
	}
#endif

	return ioa;

extract_error:
	LOG_VAR(err);
	return NULL;
}

void free_ioa(const struct io_activity *ioa)
{
	int i;
	if (ioa == NULL) return;
	for (i = 0; i < ioa->delta_count; ++i) ast_vfree(ioa->deltas[i].data);
	ast_vfree(ioa->deltas);
	ast_vfree(ioa);
}
