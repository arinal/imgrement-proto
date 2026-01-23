# imgrement - Linux Kernel I/O Tracer

A lightweight Linux kernel module for intercepting and analyzing disk I/O requests at the block device level.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start) - See [docs/getting-started.md](docs/getting-started.md) for detailed guide
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
- [Documentation](#documentation)
- [Development Setup](#development-setup)
  - [LSP Support](#lsp-support-clangd-ccls)
  - [Code Formatting](#code-formatting)
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

⚠️ **Safety First:** This module intercepts I/O on a real block device. See **[docs/getting-started.md](docs/getting-started.md)** for safe testing methods.

**Safest method - Loopback device (virtual disk):**

```bash
# Create 100MB virtual disk
dd if=/dev/zero of=/tmp/test_disk.img bs=1M count=100
sudo losetup -fP /tmp/test_disk.img

# Configure module to trace it (edit src/defines.h line 8)
# Change DEVICE_FILE_PATH to "/dev/loop0"

# Build and test
make
sudo insmod imgrement.ko
echo 'test' | sudo tee /mnt/test.txt
sync && dmesg
```

**Alternative - Vagrant VM (completely isolated):**

```bash
vagrant up && vagrant ssh
cd /vagrant
make reload
echo 'test data' > /mnt/testfile
dmesg | tail -20
```

📖 **See [docs/getting-started.md](docs/getting-started.md) for complete step-by-step instructions including:**
- Vagrant VM setup (safest)
- Loopback device method (safe virtual disk)
- Native installation (requires spare disk)
- Understanding output and troubleshooting

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

Install on Arch Linux:
```bash
sudo pacman -S base-devel linux-headers
```

### For Vagrant Setup
- [Vagrant](https://www.vagrantup.com/downloads)
- [VirtualBox](https://www.virtualbox.org/wiki/Downloads)

The Vagrant configuration automatically provisions a VM with an additional 4MB disk device at `/dev/sdb`.

📖 **For detailed installation instructions, see [docs/getting-started.md](docs/getting-started.md)**

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

1. **Initialization** (src/main.c:54-84):
   - Register as a block device driver
   - Open target device (`/dev/sdb`)
   - Save original `make_request_fn` pointer
   - Replace with `trace_request_fn`

2. **Interception** (src/main.c:35-41):
   - `trace_request_fn` receives every I/O request
   - Extract metadata using `extract_ioa()`
   - Log operation details
   - Forward to original handler

3. **Data Extraction** (src/ioa.c):
   - Parse `struct bio` into `struct io_activity`
   - Extract bio vectors (segments)
   - Copy data from page buffers
   - Calculate sector and block ranges

4. **Cleanup** (src/main.c:43-52):
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

1. Open `src/defines.h` in your editor
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

**Sector size** (src/defines.h:10):
```c
#define SECTOR_SIZE 512  // Standard sector size
```
Most modern systems use 512-byte sectors. Change only if your hardware requires it.

**Module name** (src/defines.h:7):
```c
#define DRIVER_NAME "imgrement"
```
Used in kernel logs and module identification.

## Usage Examples

Basic I/O tracing:

```bash
# Write a file and trace it
echo 'test data' > /mnt/testfile
sync
dmesg | tail
```

Example output:
```
[ 1234.567] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 1234.568] imgrement: bv #0: len 1024, offset 0, 2 sectors (100..101) or 1 blocks (50..50)
```

📖 **See [docs/getting-started.md](docs/getting-started.md) for detailed examples including cache behavior, large files, and advanced usage.**

## Understanding the Output

Output format:
```
imgrement: R/W, bytes to transfer: <size>, delta count: <n>
imgrement: bv #<i>: len <bytes>, offset <offset>, <sectors> sectors (<start>..<end>) or <blocks> blocks (<start>..<end>)
```

- **R/W**: Read or Write operation
- **bytes to transfer**: Total I/O size
- **delta count**: Number of bio vector segments
- **bv #i**: Bio vector index
- **sectors/blocks**: Disk location

📖 **See [docs/getting-started.md](docs/getting-started.md) for detailed output analysis and examples.**

## Documentation

- **[docs/getting-started.md](docs/getting-started.md)** - Step-by-step guide for safe testing, troubleshooting, and detailed examples
- **[docs/block-io-basics.md](docs/block-io-basics.md)** - Linux kernel block I/O fundamentals and how imgrement works

## Development Setup

### LSP Support (clangd, ccls)

For code navigation, autocomplete, and diagnostics in your editor:

```bash
# Install bear (Arch Linux)
sudo pacman -S bear

# Generate compile_commands.json
make compile_commands.json
```

Your LSP server (clangd, ccls) will automatically pick up `compile_commands.json` and provide:
- Go to definition
- Code completion
- Real-time error checking
- Symbol search

**Supported editors:** VSCode, Neovim, Emacs, Vim, Sublime Text, and any editor with LSP support.

### Code Formatting

Automatically format all C source files:

```bash
make format
```

Uses `clang-format` with 4-space indentation. Configuration in `.clang-format`.

## What Next?

**Extend the module:**
- Add filtering by operation type (read/write only)
- Track I/O statistics and latency
- Export data via `/proc` or `/sys`
- Support multiple devices simultaneously

**Learn more:**
- Read [docs/block-io-basics.md](docs/block-io-basics.md) for kernel internals
- Experiment with different filesystems (ext4, xfs, btrfs)
- Study cache behavior and access patterns
- Check CLAUDE.md for developer guide and API reference

**Recommended books:**
- "Linux Device Drivers" by Corbet, Rubini, Kroah-Hartman
- "Linux Kernel Development" by Robert Love

---

**License:** Check repository for license information.

**Repository:** https://github.com/arinal/imgrement-proto
