# Getting Started with imgrement - Safe Testing Guide

This guide shows you how to safely test the imgrement kernel module using two methods: Vagrant (safest) or native Arch Linux (requires a spare disk).

## Table of Contents
- [Prerequisites Check](#prerequisites-check)
- [Method 1: Vagrant VM (Safest - Recommended)](#method-1-vagrant-vm-safest---recommended)
- [Method 2: Native Arch Linux (Requires Spare Disk)](#method-2-native-arch-linux-requires-spare-disk)
- [Understanding the Output](#understanding-the-output)
- [Common Issues](#common-issues)
- [Next Steps](#next-steps)

---

## Prerequisites Check

Before starting, verify you have the required tools:

```bash
# Check kernel version (should be 3.x - 6.x)
uname -r

# Check if you have build tools
which gcc make

# Check if kernel headers are installed
ls /lib/modules/$(uname -r)/build
```

If kernel headers are missing:
```bash
# Arch Linux
sudo pacman -S base-devel linux-headers
```

---

## Method 1: Vagrant VM (Safest - Recommended)

**Why this is safest:** Creates an isolated Ubuntu VM with a dedicated 4MB test disk. Nothing on your host system is affected.

### Step 1: Install Vagrant and VirtualBox

```bash
# Arch Linux
sudo pacman -S vagrant virtualbox

# Enable and start VirtualBox kernel modules
sudo modprobe vboxdrv
```

### Step 2: Start the Test Environment

```bash
# In the imgrement-proto directory
vagrant up
```

**What happens:**
- Creates Ubuntu VM
- Adds a dedicated `/dev/sdb` disk (4MB)
- Formats it as ext2
- Mounts it at `/mnt`
- Syncs your code to `/vagrant` inside VM

**This takes ~5 minutes on first run.**

### Step 3: SSH into VM

```bash
vagrant ssh

# Switch to root (module loading requires root)
sudo su

# Navigate to synced code
cd /vagrant
```

### Step 4: Build and Load Module

```bash
# Build the module
make

# You should see:
# CC [M]  src/main.o
# CC [M]  src/ioa.o
# CC [M]  src/common.o
# LD [M]  imgrement.ko

# Load the module
make reload
```

**Expected output:**
```
imgrement: sdb trace initialization succeeded
```

### Step 5: Test I/O Tracing

```bash
# Clear kernel logs for clean output
dmesg -C

# Write a small file
echo 'Hello from imgrement!' > /mnt/test.txt

# Flush writes to disk
sync

# View traced I/O
dmesg
```

**You should see output like:**
```
[ 1234.567890] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 1234.567891] imgrement: bv #0: len 1024, offset 0, 2 sectors (100..101) or 1 blocks (50..50)
```

### Step 6: Experiment Safely

Try different I/O operations inside the VM:

```bash
# Become root for easier testing
sudo su

# Test write operations
dmesg -C
echo 'Hello imgrement!' > /mnt/test.txt
sync
dmesg | tail

# Test read operations
dmesg -C
cat /mnt/test.txt
dmesg | tail

# Test larger files
dmesg -C
dd if=/dev/urandom of=/mnt/random bs=1024 count=12
sync
dmesg | tail

# Test cache behavior
echo 3 > /proc/sys/vm/drop_caches
cat /mnt/test.txt
dmesg | tail
```

**Note:** If you get "Permission denied" when writing to `/mnt`, either:
- Use `sudo su` to become root, OR
- Run `sudo chmod 777 /mnt` to give yourself write access

### Step 7: When Done

```bash
# Exit root shell
exit

# Exit VM
exit

# Stop VM (from host)
vagrant halt

# Or destroy VM completely
vagrant destroy
```

**Benefits of Vagrant method:**
- ✅ Completely isolated from your host system
- ✅ Can destroy and recreate VM anytime
- ✅ Dedicated test disk included
- ✅ No risk to your data

---

## Method 2: Native Arch Linux (Requires Spare Disk)

⚠️ **WARNING:** This intercepts I/O on a real block device. **DO NOT** use `/dev/sda` or any disk with important data!

### Safety Requirements

Before proceeding, you **MUST** have:
1. A spare disk (USB drive, old hard drive, or loopback device)
2. Nothing important on that disk
3. Verified it's not your system disk

### Step 1: Identify a Safe Test Device

```bash
# List all block devices
lsblk

# Example output:
# NAME   MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
# sda      8:0    0 500G  0 disk              <- SYSTEM DISK - DO NOT USE
# ├─sda1   8:1    0 512M  0 part /boot
# └─sda2   8:2    0 499G  0 part /
# sdb      8:16   1  16G  0 disk              <- USB DRIVE - SAFE TO USE
```

**Options for test devices:**

**Option A: USB Flash Drive (Recommended)**
```bash
# Insert USB drive
# Identify it (usually /dev/sdb or /dev/sdc)
lsblk

# Use this device for testing
# We'll configure it in Step 2
```

**Option B: Create Loopback Device (No Physical Device Needed)**
```bash
# Create a 100MB file
dd if=/dev/zero of=/tmp/test_disk.img bs=1M count=100

# Create loopback device
sudo losetup -fP /tmp/test_disk.img

# Find the loopback device created
losetup -a
# Example: /dev/loop0: []: (/tmp/test_disk.img)

# Use /dev/loop0 as your test device
```

### Step 2: Prepare the Test Device

**⚠️ WARNING:** This will **ERASE ALL DATA** on the device!

```bash
# Replace /dev/sdb with your actual test device
TEST_DEVICE="/dev/sdb"  # CHANGE THIS TO YOUR DEVICE!

# Format as ext2
sudo mkfs.ext2 $TEST_DEVICE

# Create mount point
sudo mkdir -p /mnt/imgrement_test

# Mount it
sudo mount $TEST_DEVICE /mnt/imgrement_test

# Verify mount
df -h | grep imgrement_test
```

### Step 3: Update Module Configuration

The module currently traces `/dev/sdb` by default. If your test device is different, update the configuration:

```bash
# Edit the configuration
nano src/defines.h

# Change line 8 to your test device:
# #define DEVICE_FILE_PATH "/dev/sdb"
# to
# #define DEVICE_FILE_PATH "/dev/loop0"  (or your device)

# Save and exit (Ctrl+O, Enter, Ctrl+X)
```

### Step 4: Build and Load Module

```bash
# Build (no sudo needed for compilation)
make

# Load module (requires root)
sudo insmod imgrement.ko

# Verify module loaded
lsmod | grep imgrement

# Check kernel logs
dmesg | tail
# Should see: "imgrement: sdb trace initialization succeeded"
```

### Step 5: Test I/O Tracing

```bash
# Clear logs
sudo dmesg -C

# Write test file
echo 'Testing imgrement on Arch Linux!' | sudo tee /mnt/imgrement_test/test.txt

# Flush to disk
sync

# View traced I/O
dmesg
```

**Expected output:**
```
[ 4567.123456] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 4567.123457] imgrement: bv #0: len 1024, offset 512, 2 sectors (200..201) or 1 blocks (100..100)
```

### Step 6: Experiment

```bash
# Become root for easier testing
sudo su

# Test different operations
dmesg -C

# Write multiple files
for i in {1..5}; do
    echo "File $i" > /mnt/imgrement_test/file$i.txt
done
sync
dmesg | tail

# Test cache behavior
dmesg -C
cat /mnt/imgrement_test/test.txt  # From cache - no I/O
dmesg  # Should be empty

echo 3 > /proc/sys/vm/drop_caches  # Drop cache
cat /mnt/imgrement_test/test.txt  # From disk - I/O traced
dmesg | tail  # Should show READ operation
```

**Note:** If you get "Permission denied" when writing to `/mnt/imgrement_test`:
- Use `sudo su` to become root (simplest), OR
- Use `sudo tee`: `echo 'test' | sudo tee /mnt/file.txt`, OR
- Fix permissions: `sudo chmod 777 /mnt/imgrement_test`

### Step 7: Cleanup

```bash
# Unload module
sudo rmmod imgrement

# Unmount test device
sudo umount /mnt/imgrement_test

# If using loopback device:
sudo losetup -d /dev/loop0
rm /tmp/test_disk.img
```

---

## Understanding the Output

### Log Format

```
imgrement: R/W, bytes to transfer: <size>, delta count: <n>
imgrement: bv #<i>: len <bytes>, offset <offset>, <sectors> sectors (<start>..<end>) or <blocks> blocks (<start>..<end>)
```

**Example:**
```
[ 1234.567890] imgrement: W, bytes to transfer: 1024, delta count: 1
[ 1234.567891] imgrement: bv #0: len 1024, offset 0, 2 sectors (100..101) or 1 blocks (50..50)
```

**Explanation:**
- `W` = Write operation (R = Read)
- `1024` bytes = 1KB total transfer
- `delta count: 1` = One bio vector (segment)
- `bv #0` = First bio vector
- `len 1024` = 1KB in this segment
- `offset 0` = At page boundary
- `2 sectors (100..101)` = Sectors 100-101 on disk (512 bytes each)
- `1 blocks (50..50)` = Block 50 (1024-byte blocks)

### Why You See Reads After Writes

When you write a file:
```bash
echo 'test' > /mnt/test.txt
```

You might see **READ** operations first! This is normal:
- The kernel reads filesystem metadata (inodes, directories)
- Then buffers the write in page cache
- The actual **WRITE** appears after ~10 seconds or when you run `sync`

---

## Detailed Usage Examples

### Basic Write and Read

```bash
# Clear logs for clean output
sudo dmesg -C

# Write a file
echo 'Hello imgrement!' | sudo tee /mnt/imgrement_test/test.txt

# Force write to disk
sync

# View traces I/O
dmesg | grep imgrement
```

**Expected output:**
```
imgrement: W, bytes to transfer: 1024, delta count: 1
imgrement: bv #0: len 1024, offset 0, 2 sectors (100..101) or 1 blocks (50..50)
```

### Cache Behavior

**Test write caching:**
```bash
sudo dmesg -C
echo 'test' > /mnt/file.txt
dmesg              # Shows reads (metadata), no write yet

sync               # Force flush
dmesg              # Now shows write
```

**Test read caching:**
```bash
sudo dmesg -C
cat /mnt/file.txt  # First read - from disk
dmesg              # Shows READ operation

cat /mnt/file.txt  # Second read - from cache
dmesg              # No new entries!

# Drop cache
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
cat /mnt/file.txt  # From disk again
dmesg              # Shows READ operation
```

### Large Files (Multiple Bio Vectors)

```bash
sudo dmesg -C

# Create 12KB file
dd if=/dev/urandom of=/mnt/large.dat bs=1024 count=12
sync

dmesg | grep imgrement
```

**Output shows split into multiple 4KB segments:**
```
imgrement: W, bytes to transfer: 12288, delta count: 3
imgrement: bv #0: len 4096, offset 0, 8 sectors (200..207) or 4 blocks (100..103)
imgrement: bv #1: len 4096, offset 0, 8 sectors (208..215) or 4 blocks (104..107)
imgrement: bv #2: len 4096, offset 0, 8 sectors (216..223) or 4 blocks (108..111)
```

The 12KB write is split into 3 deltas of 4KB each.

### Multiple Writes (Batching)

```bash
sudo dmesg -C

# Write same file multiple times
for i in {1..5}; do
    echo "Line $i" >> /mnt/batch.txt
done

sync
dmesg | grep imgrement
```

The kernel often batches multiple writes into a single I/O request for efficiency.

### Random vs Sequential I/O

```bash
# Sequential write
sudo dmesg -C
dd if=/dev/zero of=/mnt/sequential bs=1M count=1
sync
dmesg | grep "delta count"   # Note: likely 1-2 large segments

# Many small random writes
sudo dmesg -C
for i in {1..10}; do
    echo $i > /mnt/file$i.txt
done
sync
dmesg | grep "delta count"    # Many small segments
```

---

## Common Issues & Troubleshooting

### Vagrant/VirtualBox Issues

**VT-x conflict with KVM:**

**Symptom:**
```
VBoxManage: error: VT-x is being used by another hypervisor (VERR_VMX_IN_VMX_ROOT_MODE)
VBoxManage: error: VirtualBox can't operate in VMX root mode
```

**Cause:** VirtualBox and KVM cannot run simultaneously - both need exclusive hardware virtualization access.

**Solutions:**

**Option A - Use loopback device instead (Recommended):**
```bash
# No VM needed! See "Method 2: Native Arch Linux" section above
# This is safer and simpler than dealing with hypervisor conflicts
```

**Option B - Unload KVM temporarily:**
```bash
# Check which KVM modules are loaded
lsmod | grep kvm

# Unload KVM (use kvm_amd on AMD processors)
sudo modprobe -r kvm_intel
sudo modprobe -r kvm

# Now vagrant up will work
vagrant up

# To re-enable KVM later:
sudo modprobe kvm_intel
```

**Option C - Use libvirt instead of VirtualBox:**
```bash
# Install libvirt provider
sudo pacman -S vagrant libvirt qemu

# Add user to libvirt group
sudo usermod -a -G libvirt $USER
newgrp libvirt

# Use libvirt provider
vagrant up --provider=libvirt
```

### Module Compilation Errors

**SECTOR_SIZE undefined (older kernels):**

**Symptom:**
```
error: 'SECTOR_SIZE' undeclared (first use in this function)
```

**Cause:** Ubuntu 14.04 (kernel 3.13) and some older kernels don't expose `SECTOR_SIZE` in their headers.

**Solution:** This was fixed in commit adding `SECTOR_SIZE` definition to `src/defines.h`. If you see this error:
```bash
# Pull latest code or manually add to src/defines.h:
#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif

# Rebuild
make clean && make
```

**Missing kernel headers:**

**Symptom:** `make` fails with "linux/module.h: No such file or directory"

**Solution:**
```bash
# Check headers exist
ls /lib/modules/$(uname -r)/build

# Install based on your distribution
sudo apt-get install linux-headers-$(uname -r)  # Ubuntu/Debian
sudo dnf install kernel-devel                   # Fedora/RHEL
sudo pacman -S linux-headers                    # Arch Linux

# Rebuild
make clean && make
```

**Kernel version mismatch:**
```bash
# Must compile for running kernel
uname -r                    # Running kernel
ls /lib/modules/            # Available headers

# Install matching headers or reboot to correct kernel
```

**Common warnings (safe to ignore):**
- `warning: no previous prototype for 'log_ioa'` - Internal function
- `warning: format '%lu' expects...` - Harmless format mismatch across kernel versions

### Module Loading Errors

**Device not found:**
```bash
# Error: "No such device"
lsblk                          # Check devices
grep DEVICE_FILE_PATH src/defines.h  # Verify config
# Fix: Change src/defines.h to match your device, rebuild
```

**Permission denied:**
```bash
# Must use sudo/root
sudo insmod imgrement.ko
```

**Module already loaded:**
```bash
lsmod | grep imgrement         # Check if loaded
sudo rmmod imgrement          # Remove first
# Or: sudo make reload
```

**Invalid module format:**
```bash
# Kernel version mismatch
make clean && make             # Rebuild for current kernel
```

### Permission Denied Writing to /mnt

**Symptom:**
```bash
echo 'test' > /mnt/test.txt
# bash: /mnt/test.txt: Permission denied

sudo echo 'test' > /mnt/test.txt
# bash: /mnt/test.txt: Permission denied  (still fails!)
```

**Cause:** `/mnt` is owned by root. Even `sudo echo > file` fails because the shell (running as your user) does the redirection before sudo runs.

**Solutions:**

**Option A - Become root:**
```bash
sudo su                        # Become root
echo 'test' > /mnt/test.txt   # Now works
sync && dmesg | tail
exit                          # Leave root shell
```

**Option B - Use tee:**
```bash
echo 'test' | sudo tee /mnt/test.txt
sync && dmesg | tail
```

**Option C - Fix permissions once:**
```bash
sudo chmod 777 /mnt           # Give everyone write access
echo 'test' > /mnt/test.txt   # Now works as regular user
sync && dmesg | tail
```

### No I/O Traced in dmesg

**Wrong device:**
```bash
lsblk && df -h                 # Check mounts
# Ensure writing to the traced device (/dev/sdb by default)
```

**Cache hiding I/O:**
```bash
# Writes buffered:
sync                           # Force flush

# Reads cached:
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
cat /mnt/test.txt              # Re-read from disk
dmesg | tail
```

**Module not initialized:**
```bash
dmesg | grep imgrement         # Look for "initialization succeeded"
# If not found, check for error messages during insmod
```

**Log level too low:**
```bash
dmesg -n 8                     # Enable debug messages
```

**dmesg permission denied:**

**Symptom:** `dmesg: klogctl failed: Operation not permitted`

**Solution:**
```bash
# Must use sudo to read kernel logs
sudo dmesg | tail

# Or in Vagrant VM, become root
sudo su
dmesg | tail
```

### System Crashes or Hangs

**Prevention:**
- Test in VM or with loopback device first
- Never use your system disk
- Keep backups

**Recovery:**
```bash
# VM: vagrant reload or vagrant destroy
# Native: hard reboot (module won't auto-load)
# Loopback: reboot clears /tmp files automatically
```

**Post-crash debugging:**
```bash
# Check previous boot logs
sudo journalctl -k -b -1 | grep -i "imgrement\|panic\|oops"
```

### Understanding Cache Behavior

**Writes appear delayed:**
- Kernel buffers writes in page cache (~10 seconds)
- Use `sync` to force immediate flush
- Metadata reads may appear first

**Reads don't appear:**
- Data served from cache (no disk I/O)
- Drop caches: `echo 3 > /proc/sys/vm/drop_caches`
- Then read again to hit disk

**Example:**
```bash
# Write file - may only see metadata reads
echo 'test' > /mnt/file.txt
dmesg | tail                   # Might show R (reads) only

sync                           # Force write
dmesg | tail                   # Now shows W (write)
```

### "Found DAMN from page" Messages

**What it is:** Demo feature counting word "DAMN" in I/O data (see `src/common.c`).

**To remove:**
```bash
# Edit src/main.c, comment out word counting in trace_request_fn
# Rebuild: make clean && make
```

---

## Next Steps

### Learn More

1. **Read the documentation:**
   - `README.md` - Full project documentation
   - `docs/block-io-basics.md` - Linux block I/O fundamentals

2. **Experiment:**
   - Try different filesystems (ext4, xfs, btrfs)
   - Test sequential vs random I/O
   - Observe cache hit/miss patterns
   - Trace different block devices

3. **Modify the code:**
   - Add filtering by operation type (read-only mode)
   - Track I/O statistics
   - Add timestamps
   - Export data via /proc interface

### Safety Reminders

✅ **Safe:**
- Using Vagrant VM
- Using a USB flash drive you can erase
- Using a loopback device
- Testing on a dedicated disk partition with no important data

❌ **NEVER:**
- Use your system disk (/dev/sda or wherever your / is mounted)
- Use a disk with important data
- Load the module in production
- Trace a disk you can't afford to lose

### Getting Help

If you encounter issues:
1. Check `dmesg` for error messages
2. Review the detailed troubleshooting section above
4. Verify kernel version: `uname -r` (must be 3.x - 6.x)
5. Check module info: `modinfo imgrement.ko`

---

**Happy tracing! 🚀**
