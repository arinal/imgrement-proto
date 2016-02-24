#include "common.h"

inline void ast_vfree(const void *ptr)
{
        if (ptr != NULL) vfree(ptr);
}

int find_word_count(const char *word, const char *data, const int len)
{
        int i = 0, count = 0;
        for (; i < len; ++i) count += (strnstr(data + i, word, strlen(word)) == NULL)? 0 : 1;
        return count;
}
