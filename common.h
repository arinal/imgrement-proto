
#ifndef COMMON_H
#define COMMON_H

#include <linux/module.h>
#include <linux/blkdev.h>
#include "defines.h"

#ifndef LOG_LABEL
#define LOG_LABEL
#define LOG_LABEL_FMT
#else
#define LOG_LABEL_FMT LOG_LABEL ": "
#endif

#define LOG(fmt, args...) printk(KERN_DEBUG LOG_LABEL_FMT fmt "\n", ## args)
#define LOG_VAR(msg) LOG("%s", msg)

/* assert goto */
#define _astgo(expr, msg, err, addr)            \
  if (!(expr)) {                                \
    err = msg;                                  \
    goto addr;                                  \
  }

#endif /* COMMON_H */
