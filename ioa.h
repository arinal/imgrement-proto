#ifndef IOA_H
#define IOA_H

#include <linux/module.h>
#include <linux/blkdev.h>

struct block_delta {
        int start;
        int end;
        int size;
        int offset;
        char *data;
};

struct io_activity {
        char rw;
        int data_size;
        int delta_count;
        struct block_delta *deltas;
};

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

int find_word_count(const char *word, const char *data, const int len);
inline int get_next_sector(const int start, const int size);

struct io_activity *create_ioa(struct bio *bio);
struct io_activity *extract_ioa(struct bio *bio);
void free_ioa(const struct io_activity *ioa);
void log_ioa(const struct io_activity *ioa);

int fill_delta(struct block_delta *delta, struct bio *bio, bvec_iter iter, const int start_sector);

#endif /* IOA_H */
