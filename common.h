
#ifndef COMMON_H
#define COMMON_H

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

inline void ast_vfree(const void *ptr);
int find_word_count(const char *word, const char *data, const int len);

#endif /* COMMON_H */
