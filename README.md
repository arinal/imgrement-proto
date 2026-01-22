# imgrement - Linux Kernel I/O Tracer

A lightweight Linux kernel module for intercepting and analyzing disk I/O requests at the block device level.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
  - [Using Vagrant (Recommended for Testing)](#using-vagrant-recommended-for-testing)
  - [Native Linux Installation](#native-linux-installation)
- [Architecture](#architecture)
- [Configuration](#configuration)
- [Usage Examples](#usage-examples)
  - [Basic Write Operation](#basic-write-operation)
  - [Understanding Cache Behavior](#understanding-cache-behavior)
  - [Working with Large Files](#working-with-large-files)
  - [Multiple Write Operations](#multiple-write-operations)
- [Understanding the Output](#understanding-the-output)
- [Troubleshooting](#troubleshooting)
- [What Next?](#what-next)

## Overview

`imgrement` is a Linux kernel module that traces disk I/O operations by hooking into the kernel's block device request queue. It intercepts all read and write requests targeted at a specific block device (default: `/dev/sdb`), logs detailed information about each operation, and forwards the requests to the original handler.

This tool is designed for:
- **Learning kernel internals** - Understanding how the Linux kernel handles block I/O
- **Debugging I/O issues** - Analyzing disk access patterns and performance
- **Educational purposes** - Demonstrating kernel module development and bio structure handling
- **Research** - Studying filesystem and storage system behavior

The module extracts I/O metadata from kernel `bio` structures, including sector numbers, transfer sizes, and actual data content, then maps this information into custom data structures for logging and analysis.

## Features

- **Complete I/O interception** - Captures all read and write requests to the target device
- **Detailed logging** - Records sector ranges, block numbers, transfer sizes, and offsets
- **Bio vector analysis** - Shows how large I/O operations are split into segments
- **Data inspection** - Can examine actual data being written (includes word-counting demo)
- **Zero filesystem impact** - Transparent pass-through design maintains normal I/O behavior
- **Kernel version compatibility** - Conditional compilation for different kernel APIs
- **Educational codebase** - Well-commented, modular design ideal for learning

## Quick Start

**For testing with Vagrant:**

```bash
# Clone and start VM with pre-configured disk
git clone https://github.com/arinal/imgrement-proto.git
cd imgrement-proto
vagrant up

# SSH into VM
vagrant ssh
sudo su
cd /vagrant

# Build and load module
make
make reload

# Generate I/O activity
echo 'test data' > /mnt/testfile

# View traced I/O
dmesg | tail -20
```

**For native Linux (kernel 3.x - 6.x):**

```bash
# Check kernel version
uname -r  # Works on 3.x, 4.x, 5.x, and 6.x

# Build module
make

# Load module (requires root)
sudo insmod imgrement.ko

# Monitor kernel logs
dmesg | tail -f
```

**Note:** The module automatically detects your kernel version and uses the appropriate APIs.

## Prerequisites

### For Native Linux
- **Linux kernel 3.x - 6.x** (tested on 3.x, 4.x, 5.x, and 6.18.5)
- Linux kernel headers matching your running kernel
- GCC compiler
- Make build system
- Root/sudo access
- A secondary block device (the module traces `/dev/sdb` by default)

**Note:** The module uses conditional compilation to support both old (3.x-5.x) and modern (6.x+) kernel APIs automatically.

Install on Ubuntu/Debian:
```bash
sudo apt-get install build-essential linux-headers-$(uname -r)
```

Install on Fedora/RHEL:
```bash
sudo dnf install gcc make kernel-devel
```

### For Vagrant Setup
- [Vagrant](https://www.vagrantup.com/downloads)
- [VirtualBox](https://www.virtualbox.org/wiki/Downloads)

The Vagrant configuration automatically provisions a VM with an additional 4MB disk device at `/dev/sdb`.

## Installation

### Using Vagrant (Recommended for Testing)

This is the safest option as it provides an isolated environment with a dedicated test disk.

```bash
# Clone repository
git clone https://github.com/arinal/imgrement-proto.git
cd imgrement-proto

# Start VM (creates Ubuntu VM with /dev/sdb automatically)
vagrant up

# SSH into VM
vagrant ssh

# Switch to root
sudo su

# Navigate to synced folder
cd /vagrant

# Build module
make

# Load module (use 'make reload' to reload after changes)
make reload
```

The `/vagrant` directory is synced with your host machine, allowing you to edit code locally and compile in the VM.

### Native Linux Installation

**⚠️ Warning:** This module intercepts I/O to `/dev/sdb`. Ensure `/dev/sdb` is a test device with no important data.

```bash
# Clone repository
git clone https://github.com/arinal/imgrement-proto.git
cd imgrement-proto

# Build module
make

# Load module (requires root)
sudo insmod imgrement.ko

# Verify module loaded
lsmod | grep imgrement
dmesg | grep imgrement
```

To unload the module:
```bash
sudo rmmod imgrement
```

To reload after making changes:
```bash
make reload  # Removes old module, clears logs, reinstalls
```

## Architecture

### Hook-Based Interception

`imgrement` uses a function pointer replacement technique to intercept I/O requests:

```
┌─────────────────────────────────────────────────────────────────┐
│                         Kernel I/O Layer                        │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             │ I/O Request (struct bio)
                             ▼
                    ┌────────────────────┐
                    │  Request Queue     │
                    │  make_request_fn   │
                    └────────┬───────────┘
                             │
                ┌────────────┴──────────────┐
                │                           │
         HOOKED │                    NORMAL │
                ▼                           ▼
    ┌───────────────────────┐   ┌──────────────────────┐
    │  trace_request_fn     │   │  Original Handler    │
    │  (imgrement module)   │   │  (kernel default)    │
    └───────────┬───────────┘   └──────────────────────┘
                │
                ├── extract_ioa(bio)
                │   └── Parse bio into io_activity
                │
                ├── LOG(I/O details)
                │   └── Print to kernel ring buffer
                │
                └── Call original handler
                    └── Forward request to kernel
```

### Data Flow

1. **Initialization** (main.c:54-84):
   - Register as a block device driver
   - Open target device (`/dev/sdb`)
   - Save original `make_request_fn` pointer
   - Replace with `trace_request_fn`

2. **Interception** (main.c:35-41):
   - `trace_request_fn` receives every I/O request
   - Extract metadata using `extract_ioa()`
   - Log operation details
   - Forward to original handler

3. **Data Extraction** (ioa.c):
   - Parse `struct bio` into `struct io_activity`
   - Extract bio vectors (segments)
   - Copy data from page buffers
   - Calculate sector and block ranges

4. **Cleanup** (main.c:43-52):
   - Restore original `make_request_fn`
   - Unregister block device
   - Free allocated memory

### Key Data Structures

**`struct io_activity`** - Top-level I/O request representation:
```c
struct io_activity {
    char rw;                      // 0 = read, 1 = write
    int data_size;                // Total bytes to transfer
    int delta_count;              // Number of bio segments
    struct block_delta *deltas;   // Array of segments
};
```

**`struct block_delta`** - Single contiguous I/O segment:
```c
struct block_delta {
    int start_sector;   // Starting disk sector
    int end_sector;     // Ending disk sector
    int size;           // Size in bytes
    int offset;         // Offset within the page
    char *data;         // Copied data content
};
```

## Configuration

### Changing the Target Device

By default, `imgrement` traces `/dev/sdb`. To monitor a different device:

1. Open `defines.h` in your editor
2. Modify line 8:
   ```c
   #define DEVICE_FILE_PATH "/dev/sdb"  // Change this
   ```
3. Rebuild the module:
   ```bash
   make clean
   make
   sudo rmmod imgrement  # If already loaded
   sudo insmod imgrement.ko
   ```

**Example:** To trace `/dev/sdc`:
```c
#define DEVICE_FILE_PATH "/dev/sdc"
```

### Other Configuration Options

**Sector size** (defines.h:10):
```c
#define SECTOR_SIZE 512  // Standard sector size
```
Most modern systems use 512-byte sectors. Change only if your hardware requires it.

**Module name** (defines.h:7):
```c
#define DRIVER_NAME "imgrement"
```
Used in kernel logs and module identification.

## Usage Examples

All examples assume you're running in the VM (`vagrant ssh && sudo su && cd /vagrant`) or as root on native Linux.

### Basic Write Operation

Let's trace a simple file write:

```bash
# Clear kernel log and ensure module is loaded
dmesg -C
make reload

# Write to a file on the traced device
echo 'DAMNDAMNDAMN' > /mnt/damn

# View logs (you'll see READ operations)
dmesg | tail
```

Output:
```
[ 4934.291668] imgrement: R, bytes to transfer: 1024, delta count: 1
[ 4934.291672] imgrement: bv #0: len 1024, offset 0, 2 sectors (296..297) or 1 blocks (148..148)
```

**Why READ operations?** The kernel caches writes and performs reads for metadata updates. To see the actual write:

```bash
# Wait 10 seconds for kernel to flush, or use sync
sync

# Now check logs
dmesg | tail
```

Output:
```
[ 4964.326233] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4964.326238] imgrement: bv #0: len 1024, offset 1024, 2 sectors (2..3) or 1 blocks (1..1)
[ 4964.326678] imgrement: Found DAMN from page 3 times
```

### Understanding Cache Behavior

The kernel's page cache affects what I/O operations you see:

**Writes are delayed:**
```bash
dmesg -C
echo 'test' > /mnt/file1
dmesg | tail  # Shows reads (metadata), no write yet

sync          # Force flush
dmesg | tail  # Now shows write
```

**Reads are cached:**
```bash
dmesg -C
cat /mnt/file1            # First read - hits disk
dmesg | tail              # Shows read operation

cat /mnt/file1            # Second read - from cache
dmesg | tail              # No new entries!

# Drop cache to force disk read
echo 3 > /proc/sys/vm/drop_caches
cat /mnt/file1            # Reads from disk again
dmesg | tail              # Shows read operation
```

### Working with Large Files

Large files are split into multiple bio vectors (deltas):

```bash
dmesg -C

# Create 12KB file
dd if=/dev/urandom of=/mnt/random bs=1024 count=12

# Wait for flush
sync

# View traced I/O
dmesg | tail -20
```

Output shows multiple metadata writes followed by the actual data:
```
[  675.332183] imgrement: W, bytes to transfer: 12288, delta count: 3
[  675.332187] imgrement: bv #0: len 4096, offset 0, 8 sectors (6154..6161) or 4 blocks (3077..3080)
[  675.332222] imgrement: bv #1: len 4096, offset 0, 8 sectors (6162..6169) or 4 blocks (3081..3084)
[  675.332258] imgrement: bv #2: len 4096, offset 0, 8 sectors (6170..6177) or 4 blocks (3085..3088)
```

The 12KB write is split into 3 deltas of 4KB each.

### Multiple Write Operations

Multiple writes to the same file are often batched:

```bash
dmesg -C

# Write 6 times
echo 'DAMNDAMNDAMN' >> /mnt/damn
echo 'DAMNDAMNDAMN' >> /mnt/damn
echo 'DAMNDAMNDAMN' >> /mnt/damn
echo 'DAMNDAMNDAMN' >> /mnt/damn
echo 'DAMNDAMNDAMN' >> /mnt/damn
echo 'DAMNDAMNDAMN' >> /mnt/damn

dmesg | tail  # Shows only reads (metadata)

sync          # Flush writes

dmesg | tail
```

Output:
```
[ 8308.791450] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 8308.791589] imgrement: Found DAMN from page 21 times
[ 8308.791604] imgrement: Found DAMN from deltas 21 times
```

The 6 writes (18 "DAMN"s) plus previous 3 = 21 total, batched into a single I/O request.

## Understanding the Output

The module logs I/O operations to the kernel ring buffer (viewable with `dmesg`):

### Output Format

**Summary line:**
```
imgrement: R/W, bytes to transfer: <size>, delta count: <n>
```
- `R` = read, `W` = write
- `bytes to transfer` = total size of the I/O operation
- `delta count` = number of bio vectors (segments)

**Per-segment details:**
```
imgrement: bv #<i>: len <bytes>, offset <offset>, <sectors> sectors (<start>..<end>) or <blocks> blocks (<start>..<end>)
```
- `bv #i` = bio vector number (0-indexed)
- `len` = segment size in bytes
- `offset` = offset within the page (0-4095)
- `sectors` = number of 512-byte sectors
- `(start..end)` = sector or block range on disk

### Example Analysis

```
[ 4934.291668] imgrement: R, bytes to transfer: 1024, delta count: 1
[ 4934.291672] imgrement: bv #0: len 1024, offset 0, 2 sectors (296..297) or 1 blocks (148..148)
```

This indicates:
- **Operation:** Read (`R`)
- **Size:** 1024 bytes (1KB)
- **Segments:** 1 bio vector
- **Location:** Sectors 296-297 (2 sectors × 512 bytes = 1024 bytes)
- **Block:** Block 148 (1024-byte blocks, so sectors 296-297 = block 148)
- **Alignment:** offset 0 means page-aligned

```
[ 4964.326238] imgrement: bv #0: len 1024, offset 1024, 2 sectors (2..3) or 1 blocks (1..1)
```

This shows:
- **Operation:** Part of a write (W in summary line)
- **Offset:** 1024 bytes into the page (second 1KB of a page)
- **Location:** Sectors 2-3, block 1 (near start of disk = metadata)

## Troubleshooting

### Module fails to compile

**Symptom:** `make` returns errors about missing headers or incompatible types.

**Causes & Solutions:**

1. **Missing kernel headers:**
   ```bash
   # Check headers exist
   ls /lib/modules/$(uname -r)/build

   # Install if missing
   sudo apt-get install linux-headers-$(uname -r)  # Ubuntu/Debian
   sudo dnf install kernel-devel                   # Fedora/RHEL
   ```

2. **Kernel version mismatch:**
   - You must compile against the exact kernel version you're running
   - Check versions match:
     ```bash
     uname -r                                  # Running kernel
     ls /lib/modules/                          # Available headers
     ```

3. **Compiler version issues:**
   - Ensure GCC version is compatible with your kernel
   ```bash
   gcc --version
   ```

### Module fails to load (insmod error)

**Symptom:** `insmod imgrement.ko` returns an error.

**Causes & Solutions:**

1. **Target device not found:**
   ```bash
   # Check if /dev/sdb exists
   lsblk

   # If not, either:
   # - Add a disk to your system, or
   # - Change DEVICE_FILE_PATH in defines.h to an existing device
   ```

2. **Kernel version mismatch:**
   ```bash
   # Error: "Invalid module format" or "version magic"
   # Solution: Recompile against running kernel headers
   make clean
   make
   ```

3. **Permission denied:**
   ```bash
   # Must run as root
   sudo insmod imgrement.ko
   ```

4. **Module already loaded:**
   ```bash
   # Check if already loaded
   lsmod | grep imgrement

   # Remove if needed
   sudo rmmod imgrement
   ```

### No output in dmesg

**Symptom:** Module loads successfully but `dmesg` shows no I/O traces.

**Causes & Solutions:**

1. **Wrong device:**
   - Ensure you're performing I/O on the correct device
   - Default is `/dev/sdb` mounted at `/mnt`
   ```bash
   lsblk                    # Check device and mount point
   cat /proc/mounts         # Verify /dev/sdb is mounted
   ```

2. **Cache hiding I/O:**
   - Writes are delayed; reads come from cache
   ```bash
   # Force write flush
   sync

   # Force read from disk (drop cache)
   echo 3 > /proc/sys/vm/drop_caches
   ```

3. **Module not fully loaded:**
   ```bash
   # Check module status
   dmesg | grep imgrement

   # Should see: "imgrement: sdb trace initialization succeeded"
   # If not, check module_init ran successfully
   ```

4. **Log level filtering:**
   ```bash
   # Ensure kernel debug messages are visible
   dmesg -n 8              # Set console log level
   ```

### Module causes kernel crash or hang

**Symptom:** System becomes unresponsive after loading module.

**Prevention:**
- Always test in a VM first
- Ensure target device has no important data
- Use `make reload` for iteration (clears state)

**Recovery:**
- If in VM: restart VM (`vagrant reload`)
- If native: reboot system, module won't auto-load

**Debugging:**
- Check kernel logs after reboot: `journalctl -k | grep imgrement`
- Review code for null pointer dereferences or memory issues

### Compilation warnings

**Symptom:** Module compiles but shows warnings about deprecated functions.

**Solution:** This is normal for kernel modules due to evolving APIs. As long as the module loads and works, these can be safely ignored. The `HAVE_BVEC_ITER` macro handles most compatibility issues.

### Compilation succeeds but with warnings

**Symptom:** Module compiles successfully but shows some warnings.

**Common warnings:**
- `warning: no previous prototype for 'log_ioa'` - This is safe to ignore, log_ioa is an internal function
- `warning: format '%lu' expects argument of type 'long unsigned int'` - Minor format string mismatch, does not affect functionality

**Solution:** These warnings are cosmetic and do not affect the module's functionality. The module will work correctly despite these warnings.

### "Found DAMN from page" messages

**Symptom:** Seeing word count messages in logs.

**Explanation:** This is a demo feature showing data inspection capabilities. The module searches for the word "DAMN" in I/O data (see common.c:find_word_count). You can remove this feature or modify it to search for other patterns.

## What Next?

### Learning Resources

**Kernel development books:**
- "Linux Device Drivers" by Jonathan Corbet, Alessandro Rubini, and Greg Kroah-Hartman
- "Understanding the Linux Kernel" by Daniel P. Bovet and Marco Cesati
- "Linux Kernel Development" by Robert Love

**Online resources:**
- [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/)
- [The Linux Kernel Module Programming Guide](https://sysprog21.github.io/lkmpg/)
- [Block Layer Documentation](https://www.kernel.org/doc/html/latest/block/index.html)

### Project Extensions

**Easy:**
- Modify word search to find other patterns
- Add filtering by operation type (read-only or write-only mode)
- Change target device and experiment with different filesystems
- Log statistics (total bytes read/written, operation counts)

**Intermediate:**
- Add timestamps to I/O operations
- Track I/O latency (time between request and completion)
- Export statistics via `/proc` or `/sys` interface
- Add runtime configuration (change target device without recompiling)
- Implement filtering by sector range

**Advanced:**
- Support tracing multiple devices simultaneously
- Implement a userspace daemon to parse and visualize I/O patterns
- Add performance counters and profiling
- Integrate with tracing frameworks (ftrace, eBPF)
- Build a file recovery tool using sector information

### Contributing

Contributions are welcome! This is an educational project, so enhancements that improve learning value are especially appreciated.

### Experiments to Try

1. **Compare filesystem behaviors:**
   - Format `/dev/sdb` with different filesystems (ext4, xfs, btrfs)
   - Observe how each handles I/O differently

2. **Analyze access patterns:**
   - Create sequential vs random I/O workloads
   - Observe how the kernel merges and reorders requests

3. **Study cache effectiveness:**
   - Measure cache hit rates for different workloads
   - Experiment with different cache drop scenarios

4. **Sector-level forensics:**
   - Delete a file and note its sectors
   - Attempt to locate the data before it's overwritten
   - Study how filesystems reuse freed blocks

---

**License:** Check repository for license information.

**Repository:** https://github.com/arinal/imgrement-proto
