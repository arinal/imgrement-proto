# imgrement proto - A kernel daemon for tracing your disk's I/O requests
The source can only be compiled on Linux. It has already worked properly on Ubuntu.

Use the provided vagrant configuration if you want to use it outside Linux. After the VM is up, compile it inside the VM. Please make sure you've installed
appropriate Linux kernel headers and attached a formatted secondary hard disk named `sdb`. For simplicity this program only trace `/dev/sdb` device.
