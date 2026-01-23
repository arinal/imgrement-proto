# Linux Kernel Block I/O Basics

This document explains the fundamental concepts of the Linux kernel block layer and how the `imgrement` module intercepts I/O operations.

## Table of Contents

- [Introduction](#introduction)
- [The Block Layer](#the-block-layer)
- [Core Data Structures](#core-data-structures)
  - [struct bio](#struct-bio)
  - [Bio Vectors (bvec)](#bio-vectors-bvec)
  - [Request Queue](#request-queue)
- [Sectors and Blocks](#sectors-and-blocks)
- [I/O Request Flow](#io-request-flow)
- [Kernel API Evolution](#kernel-api-evolution)
  - [make_request_fn (Kernel 3.x - 5.x)](#make_request_fn-kernel-3x---5x)
  - [submit_bio (Kernel 6.x+)](#submit_bio-kernel-6x)
- [How imgrement Intercepts I/O](#how-imgrement-intercepts-io)
- [Code Examples](#code-examples)
- [References](#references)

## Introduction

The Linux kernel block layer is a subsystem that manages all block I/O operations between the filesystem and physical storage devices. It provides a unified interface for different types of block devices (hard disks, SSDs, USB drives, etc.) and handles optimization tasks like request merging, scheduling, and caching.

Understanding the block layer is essential for working with the `imgrement` module, as it intercepts I/O requests at the block layer level before they reach the actual storage device.

## The Block Layer

The block layer sits between:
- **Upper layers**: Filesystems (ext4, btrfs, etc.) and applications
- **Lower layers**: Device drivers (SATA, NVMe, SCSI, etc.)

Its responsibilities include:
- Converting filesystem operations into block I/O requests
- Merging adjacent requests for efficiency
- Scheduling I/O operations (I/O schedulers like mq-deadline, BFQ)
- Managing the page cache for read/write caching
- Providing a generic interface for block devices

## Core Data Structures

### struct bio

The `struct bio` (block I/O) is the fundamental data structure representing a single I/O request in the kernel. It contains:

```c
struct bio {
    struct bio *bi_next;           // Next bio in request
    struct block_device *bi_bdev;  // Target block device
    unsigned int bi_opf;           // Operation and flags (READ/WRITE)

    // Kernel 3.14+ uses bi_iter
    struct bvec_iter bi_iter;      // Iterator for bio vectors
        // bi_iter.bi_sector      // Starting sector number
        // bi_iter.bi_size        // Total bytes in this bio
        // bi_iter.bi_idx         // Current bio vector index

    // Pre-3.14 kernels accessed these directly
    // sector_t bi_sector;
    // unsigned int bi_size;

    unsigned short bi_vcnt;        // Number of bio vectors
    struct bio_vec *bi_io_vec;     // Array of bio vectors
    // ... other fields ...
};
```

**Key fields:**
- `bi_bdev`: The target block device (e.g., /dev/sdb)
- `bi_opf`: Operation flags (READ, WRITE, SYNC, etc.)
- `bi_iter.bi_sector`: Starting sector number (512-byte units)
- `bi_iter.bi_size`: Total bytes to transfer
- `bi_vcnt`: Number of segments (bio vectors) in this I/O
- `bi_io_vec`: Array of bio vectors describing memory locations

### Bio Vectors (bvec)

A bio vector (`struct bio_vec`) represents a single contiguous memory segment:

```c
struct bio_vec {
    struct page *bv_page;    // Physical page containing data
    unsigned int bv_len;     // Length in bytes
    unsigned int bv_offset;  // Offset within the page
};
```

**Why multiple bio vectors?**
- Large I/O operations may span non-contiguous physical memory pages
- The kernel uses scatter-gather I/O to efficiently handle fragmented memory
- Each bio vector points to a different physical page

**Example:** A 12KB write might have 3 bio vectors:
- Vector 0: Page A, 4KB at offset 0
- Vector 1: Page B, 4KB at offset 0
- Vector 2: Page C, 4KB at offset 0

### Request Queue

Each block device has a request queue (`struct request_queue`) that manages I/O operations:

```c
struct request_queue {
    make_request_fn *make_request_fn;  // Old API (pre-6.0)
    // or
    const struct block_device_operations *ops;  // New API (6.0+)

    // ... scheduling, merging, statistics ...
};
```

**The request queue is responsible for:**
- Receiving bio requests from upper layers
- Merging adjacent requests for efficiency
- Scheduling I/O operations (elevator algorithms)
- Invoking the actual device driver

## Sectors and Blocks

### Sector
- **Size**: 512 bytes (historically, though modern drives may use 4KB Advanced Format)
- **Defined by**: Hardware/kernel constant
- **Purpose**: Smallest addressable unit on the disk
- **Used in**: `struct bio` (bi_sector field)

### Block
- **Size**: Variable, typically 1024, 2048, or 4096 bytes
- **Defined by**: Filesystem at mount time
- **Purpose**: Smallest allocation unit for the filesystem
- **Used in**: Filesystem operations

**Relationship:**
```
1 Block (1024 bytes) = 2 Sectors (512 bytes each)
1 Block (4096 bytes) = 8 Sectors (512 bytes each)
```

**Converting between sectors and blocks:**
```c
// From imgrement module (src/ioa.h)
static inline int get_block_sector_ratio(void) {
    return BLOCK_SIZE / SECTOR_SIZE;  // e.g., 1024 / 512 = 2
}

static inline int get_start_block(const struct block_delta *delta) {
    return delta->start_sector / get_block_sector_ratio();
}
```

## I/O Request Flow

Here's how an I/O request flows through the kernel:

```
┌─────────────────┐
│  Application    │ (write() syscall)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  VFS Layer      │ (Virtual File System)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Filesystem     │ (ext4, btrfs, etc.)
│  (ext4, btrfs)  │ Creates bio structures
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Block Layer    │ <-- imgrement hooks here
│  (Request Queue)│
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  I/O Scheduler  │ (mq-deadline, BFQ)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Device Driver  │ (SATA, NVMe, SCSI)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Hardware       │ (Physical disk)
└─────────────────┘
```

**imgrement intercepts at the block layer** by replacing the request queue's function pointer.

## Kernel API Evolution

The kernel's block I/O API has evolved across versions. The `imgrement` module supports both old and new APIs using conditional compilation.

### make_request_fn (Kernel 3.x - 5.x)

**Mechanism:** Direct function pointer in `struct request_queue`

```c
// Request queue structure (simplified)
struct request_queue {
    make_request_fn *make_request_fn;  // Function pointer
    // ...
};

// Function signature
typedef blk_qc_t (make_request_fn)(struct request_queue *q, struct bio *bio);
```

**How imgrement uses it:**
```c
// Save original function pointer
static make_request_fn *orig_mrfn = NULL;

// Custom interception function
static blk_qc_t trace_request_fn(struct request_queue *q, struct bio *bio) {
    // Extract and log I/O information
    struct io_activity *ioa = extract_ioa(bio);
    log_ioa(ioa);
    free_ioa(ioa);

    // Forward to original handler
    return orig_mrfn(q, bio);
}

// Module initialization
orig_mrfn = queue->make_request_fn;  // Save original
queue->make_request_fn = trace_request_fn;  // Replace with ours
```

### submit_bio (Kernel 6.x+)

**Mechanism:** Callback in `struct block_device_operations`

Starting in kernel 6.0, the direct `make_request_fn` pointer was removed in favor of a more structured approach using `block_device_operations`.

```c
// Block device operations structure
struct block_device_operations {
    void (*submit_bio)(struct bio *bio);
    // ... other operations ...
};
```

**How imgrement uses it (kernel 6.x+):**
```c
// Define custom operations with our submit_bio callback
static const struct block_device_operations imgrement_ops = {
    .submit_bio = trace_submit_bio,
    .owner = THIS_MODULE,
};

// Custom callback
static void trace_submit_bio(struct bio *bio) {
    // Extract and log I/O information
    struct io_activity *ioa = extract_ioa(bio);
    log_ioa(ioa);
    free_ioa(ioa);

    // Forward to kernel's generic handler
    submit_bio_noacct(bio);
}
```

**Key differences:**
- Kernel 6.x uses `submit_bio_noacct()` to forward requests instead of calling the original function pointer
- Device access uses `bdev_file_open_by_path()` returning `struct file *` instead of `blkdev_get_by_path()`
- Mode flags changed from `FMODE_READ` to `BLK_OPEN_READ`

## How imgrement Intercepts I/O

The `imgrement` module works by intercepting I/O requests at the request queue level:

**Step 1: Module Initialization** (src/main.c)
```c
static int __init tracer_init(void) {
    // Open the target block device
    bdev = blkdev_get_by_path(DEVICE_FILE_PATH, FMODE_READ, NULL);

    // Get the request queue
    queue = bdev_get_queue(bdev);

    #ifdef KERNEL_6X_OR_NEWER
        // Kernel 6.x: Register block_device_operations
        queue->queue_flags |= QUEUE_FLAG_TRACE;
    #else
        // Kernel 3.x-5.x: Replace make_request_fn pointer
        orig_mrfn = queue->make_request_fn;
        queue->make_request_fn = trace_request_fn;
    #endif
}
```

**Step 2: I/O Interception**
```c
static blk_qc_t trace_request_fn(struct request_queue *q, struct bio *bio) {
    // Extract I/O metadata from bio structure
    struct io_activity *ioa = extract_ioa(bio);

    // Log to kernel ring buffer (visible with dmesg)
    if (ioa) {
        LOG("R/W, bytes to transfer: %d, delta count: %d",
            ioa->data_size, ioa->delta_count);

        // Log each bio vector
        for (int i = 0; i < ioa->delta_count; i++) {
            struct block_delta *delta = &ioa->deltas[i];
            LOG("  bv #%d: %d sectors (%d..%d)",
                i, get_sectors_count(delta),
                delta->start_sector, delta->end_sector);
        }

        free_ioa(ioa);
    }

    // Forward request to original handler for actual I/O
    return orig_mrfn(q, bio);
}
```

**Step 3: Cleanup**
```c
static void __exit tracer_cleanup(void) {
    // Restore original function pointer
    if (queue && orig_mrfn) {
        queue->make_request_fn = orig_mrfn;
    }

    // Release block device
    blkdev_put(bdev, FMODE_READ);
}
```

**This approach allows imgrement to:**
- See all I/O operations before they execute
- Extract detailed information (sectors, sizes, data)
- Log operations for debugging/analysis
- Forward requests unchanged so normal I/O continues

## Code Examples

### Iterating Bio Vectors

The code must handle both old and new kernel APIs:

```c
// From src/ioa.c
#ifdef HAVE_BVEC_ITER
    // Kernel 3.14+ with bi_iter
    #define bio_sector(bio) ((bio)->bi_iter.bi_sector)
    #define bio_size(bio) ((bio)->bi_iter.bi_size)
    #define bio_idx(bio) ((bio)->bi_iter.bi_idx)
#else
    // Pre-3.14 kernels
    #define bio_sector(bio) ((bio)->bi_sector)
    #define bio_size(bio) ((bio)->bi_size)
    #define bio_idx(bio) ((bio)->bi_idx)
#endif

// Iterate through bio vectors
struct bio_vec bvec;
struct bvec_iter iter;

bio_for_each_segment(bvec, bio, iter) {
    struct page *page = bvec.bv_page;
    unsigned int len = bvec.bv_len;
    unsigned int offset = bvec.bv_offset;

    // Access data
    char *data = kmap(page) + offset;
    // ... process data ...
    kunmap(page);
}
```

### Extracting I/O Information

From `src/ioa.c`, the `extract_ioa()` function:

```c
struct io_activity *extract_ioa(struct bio *bio) {
    struct io_activity *ioa = create_ioa(bio);
    if (!ioa) return NULL;

    // Extract basic info
    ioa->rw = bio_data_dir(bio);  // 0=READ, 1=WRITE
    ioa->data_size = bio_size(bio);
    ioa->delta_count = bio->bi_vcnt;

    // Extract each bio vector
    int i = 0;
    struct bio_vec bvec;
    struct bvec_iter iter;

    bio_for_each_segment(bvec, bio, iter) {
        fill_delta(&ioa->deltas[i], &bvec, bio, i);
        i++;
    }

    return ioa;
}
```

## References

### Kernel Documentation
- [Linux Kernel Block Layer](https://www.kernel.org/doc/html/latest/block/index.html)
- [Bio and Request Structures](https://www.kernel.org/doc/html/latest/block/biodoc.html)
- [I/O Schedulers](https://www.kernel.org/doc/html/latest/block/io-schedulers.html)

### Books
- "Linux Kernel Development" by Robert Love (Chapter 14: The Block I/O Layer)
- "Linux Device Drivers" by Corbet, Rubini, and Kroah-Hartman (Chapter 16: Block Drivers)

### Online Resources
- [LWN.net: The block I/O layer](https://lwn.net/Articles/736534/) - Coverage of kernel 6.x changes
- [Kernel source: include/linux/blk_types.h](https://github.com/torvalds/linux/blob/master/include/linux/blk_types.h) - Bio structure definitions
- [Kernel source: block/blk-core.c](https://github.com/torvalds/linux/blob/master/block/blk-core.c) - Core block layer implementation

### imgrement Module Files
- `src/main.c` - Module initialization and I/O hooking
- `src/ioa.c` - Bio extraction and parsing
- `src/ioa.h` - I/O activity data structures
- `src/defines.h` - Constants (SECTOR_SIZE, BLOCK_SIZE, etc.)

---

**Note:** This documentation is based on Linux kernels 3.x through 6.x. Always consult the kernel documentation for your specific kernel version, as APIs continue to evolve.
