/**
 * ioa.h - I/O Activity data structures and API
 *
 * This header defines the data structures used to represent I/O operations
 * and provides functions to extract I/O information from kernel bio structures.
 *
 * The core abstraction maps kernel's struct bio (which uses bio vectors)
 * into a more accessible io_activity structure that contains all I/O metadata.
 */

#ifndef IOA_H
#define IOA_H

/**
 * struct block_delta - Represents a single contiguous I/O segment
 *
 * A block_delta corresponds to one bio vector in the kernel's bio structure.
 * Large I/O operations are often split into multiple segments (deltas), each
 * representing a contiguous range of sectors on disk.
 *
 * @start_sector: First 512-byte sector on disk for this segment
 * @end_sector: Last 512-byte sector on disk for this segment (inclusive)
 * @size: Total size of this segment in bytes
 * @offset: Byte offset within the 4KB page where data starts (0-4095)
 * @data: Pointer to copied data content (NULL if data copying is disabled)
 *
 * Memory management: The data field is allocated separately with vmalloc
 * and must be freed when the block_delta is no longer needed.
 */
struct block_delta {
    int start_sector;
    int end_sector;
    int size;
    int offset;
    char *data;
};

/**
 * get_block_sector_ratio - Calculate sectors per block
 *
 * Returns the ratio of block size to sector size. For standard configurations
 * with 1KB blocks and 512-byte sectors, this returns 2.
 * This is a constant value determined by IMGREMENT_BLOCK_SIZE and SECTOR_SIZE.
 *
 * Return: Number of sectors that make up one block
 */
inline int get_block_sector_ratio(void);

/**
 * get_start_block - Get starting block number
 * @delta: Pointer to block_delta structure
 *
 * Converts the start_sector to a block number based on the block size.
 *
 * Return: Starting block number for this segment
 */
inline int get_start_block(const struct block_delta *delta);

/**
 * get_end_block - Get ending block number
 * @delta: Pointer to block_delta structure
 *
 * Converts the end_sector to a block number based on the block size.
 *
 * Return: Ending block number for this segment
 */
inline int get_end_block(const struct block_delta *delta);

/**
 * get_next_sector - Get the sector following this segment
 * @delta: Pointer to block_delta structure
 *
 * Return: Sector number immediately after end_sector
 */
inline int get_next_sector(const struct block_delta *delta);

/**
 * get_blocks_count - Count blocks in this segment
 * @delta: Pointer to block_delta structure
 *
 * Return: Number of blocks spanned by this segment
 */
inline int get_blocks_count(const struct block_delta *delta);

/**
 * get_sectors_count - Count sectors in this segment
 * @delta: Pointer to block_delta structure
 *
 * Return: Number of 512-byte sectors in this segment
 */
inline int get_sectors_count(const struct block_delta *delta);

/**
 * struct io_activity - Top-level representation of an I/O request
 *
 * This structure aggregates all information about a single I/O operation,
 * including its direction (read/write), size, and constituent segments.
 *
 * @rw: Operation type - 0 for read, 1 for write (from bio_data_dir)
 * @data_size: Total bytes to transfer across all deltas
 * @delta_count: Number of bio vector segments (size of deltas array)
 * @deltas: Array of block_delta structures representing each segment
 *
 * Memory management: This entire structure (including the deltas array
 * and each delta's data field) is allocated with vmalloc. Use free_ioa()
 * to properly deallocate all associated memory.
 *
 * Example:
 *   struct io_activity *ioa = extract_ioa(bio);
 *   if (ioa) {
 *       // Process I/O information
 *       for (int i = 0; i < ioa->delta_count; i++) {
 *           process_delta(&ioa->deltas[i]);
 *       }
 *       free_ioa(ioa);  // Clean up when done
 *   }
 */
struct io_activity {
    char rw;
    int data_size;
    int delta_count;
    struct block_delta *deltas;
};

/**
 * struct bio - Forward declaration
 *
 * Kernel's bio structure defined in linux/bio.h. We forward-declare it
 * here to avoid including the entire header in this interface file.
 */
struct bio;

/**
 * create_ioa - Allocate an empty io_activity structure
 * @bio: Pointer to kernel bio structure
 *
 * Allocates memory for an io_activity and its deltas array based on the
 * number of bio vectors in the provided bio. Does NOT populate the structure
 * with actual data - use extract_ioa() for that.
 *
 * Memory is allocated using vmalloc to handle potentially large I/O operations.
 *
 * Return: Pointer to allocated io_activity, or NULL on allocation failure
 *
 * Note: This is typically used internally by extract_ioa(). Most callers
 * should use extract_ioa() directly instead.
 */
struct io_activity *create_ioa(struct bio *bio);

/**
 * extract_ioa - Extract I/O activity information from a bio
 * @bio: Pointer to kernel bio structure to parse
 *
 * This is the main function for converting a kernel bio structure into
 * an io_activity structure. It:
 * - Allocates memory for io_activity and all deltas
 * - Extracts read/write direction
 * - Calculates total data size
 * - Iterates through bio vectors and populates each delta
 * - Copies actual data content from bio pages
 *
 * The function handles kernel version differences automatically via
 * conditional compilation (HAVE_BVEC_ITER macro).
 *
 * Return: Pointer to populated io_activity structure, or NULL on failure
 *
 * Memory management: Caller MUST call free_ioa() when done with the
 * returned structure to avoid memory leaks.
 *
 * Thread safety: Not thread-safe. Assumes bio remains valid during extraction.
 *
 * Context: Can be called from interrupt context if using GFP_ATOMIC allocations
 * (current implementation uses vmalloc which may sleep - see Performance
 * Considerations in CLAUDE.md for alternatives).
 *
 * Example usage:
 *   struct io_activity *ioa = extract_ioa(bio);
 *   if (!ioa) {
 *       printk(KERN_ERR "Failed to extract I/O activity\n");
 *       return;
 *   }
 *
 *   // Use the extracted information
 *   printk(KERN_INFO "%c operation: %d bytes in %d segments\n",
 *          ioa->rw ? 'W' : 'R', ioa->data_size, ioa->delta_count);
 *
 *   // Clean up
 *   free_ioa(ioa);
 */
struct io_activity *extract_ioa(struct bio *bio);

/**
 * free_ioa - Free an io_activity structure
 * @ioa: Pointer to io_activity to deallocate
 *
 * Properly deallocates all memory associated with an io_activity structure:
 * - Frees data buffers in each delta (if allocated)
 * - Frees the deltas array
 * - Frees the io_activity structure itself
 *
 * Handles NULL pointers gracefully (no-op if ioa is NULL).
 *
 * IMPORTANT: Always call this function when done with an io_activity
 * returned by extract_ioa() to prevent memory leaks.
 *
 * Example:
 *   struct io_activity *ioa = extract_ioa(bio);
 *   if (ioa) {
 *       // ... use ioa ...
 *       free_ioa(ioa);  // Always free when done
 *       ioa = NULL;     // Good practice: prevent use-after-free
 *   }
 */
void free_ioa(const struct io_activity *ioa);

#endif /* IOA_H */
