#include "common.h"
#include "ioa.h"

int find_word_count(const char *word, const char *data, const int len)
{
        int i = 0, count = 0;
        for (; i < len; ++i)
               count += (strnstr(data + i, word, strlen(word)) == NULL)? 0 : 1;
        return count;
}

inline int get_next_sector(const int start, const int size)
{
        return start + size / SECTOR_SIZE;
}

int fill_delta(struct block_delta *delta, struct bio *bio, bvec_iter iter, const int start_sector)
{
        int count;
        int next_sector;
        char *data;

        delta->start = start_sector;
        delta->size = bio_iter_len(bio, iter);
        next_sector = get_next_sector(delta->start, delta->size);
        delta->end = next_sector - 1;
        delta->offset = bio_iter_offset(bio, iter);

        data = kmap(bio_iter_page(bio, iter));
        count = find_word_count("DAMN", data, PAGE_SIZE);
        if (count > 0) LOG("Found DAMN from page %d times", count);
        delta->data = vmalloc(delta->size);
        if (delta->data == NULL) return -1;
        memcpy(delta->data, data + delta->offset, delta->size);
        kunmap(bio_iter_page(bio, iter));

        return next_sector;
}

struct io_activity *create_ioa(struct bio *bio)
{
        struct io_activity *ioa = vmalloc(sizeof(struct io_activity));
        if (ioa == NULL) return NULL;

        ioa->rw = bio_data_dir(bio);
        ioa->data_size = bio_size(bio);
        ioa->delta_count = bio_cnt(bio);

        ioa->deltas = vmalloc(ioa->delta_count * sizeof(struct block_delta));
        if (ioa->deltas == NULL) return NULL;

        return ioa;
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
                _astgo(next_sector >= 0, "Error allocating delta->data", err, extract_error);
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
        for (i = 0; i < ioa->delta_count; ++i) vfree(ioa->deltas[i].data);
        vfree(ioa->deltas);
        vfree(ioa);
}

void log_ioa(const struct io_activity *ioa)
{
        int i;
        LOG("%s, bytes to transfer: %d, delta count: %d", ioa->rw? "W" : "R", ioa->data_size, ioa->delta_count);

        for (i = 0; i < ioa->delta_count; ++i) {
                int count;
                struct block_delta *delta = &ioa->deltas[i];
                LOG("bv #%d: len %d, offset %d, at (%d, %d)", i, delta->size, delta->offset, delta->start, delta->end);
                count = find_word_count("DAMN", delta->data, delta->size);
                if (count > 0) LOG("Found DAMN from deltas %d times", count);
        }
}
