#ifndef IOA_H
#define IOA_H

struct block_delta {
        int start_sector;
        int end_sector;
        int size;
        int offset;
        char *data;
};

inline int get_block_sector_ratio(const struct block_delta *delta);
inline int get_start_block(const struct block_delta *delta);
inline int get_end_block(const struct block_delta *delta);
inline int get_next_sector(const struct block_delta *delta);
inline int get_blocks_count(const struct block_delta *delta);
inline int get_sectors_count(const struct block_delta *delta);

struct io_activity {
        char rw;
        int data_size;
        int delta_count;
        struct block_delta *deltas;
};

struct bio;
struct io_activity *create_ioa(struct bio *bio);
struct io_activity *extract_ioa(struct bio *bio);
void free_ioa(const struct io_activity *ioa);

#endif /* IOA_H */
