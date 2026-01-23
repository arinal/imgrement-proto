/**
 * common.h - Common utilities and logging macros
 *
 * This header provides shared utility functions and logging infrastructure
 * used throughout the imgrement kernel module.
 */

#ifndef COMMON_H
#define COMMON_H

#include "defines.h"

/**
 * LOG_LABEL and LOG_LABEL_FMT - Logging prefix configuration
 *
 * These macros configure the prefix that appears before all log messages.
 * If LOG_LABEL is defined (typically as DRIVER_NAME), messages will be
 * prefixed with "imgrement: ". This makes filtering module output easy:
 *
 *   dmesg | grep imgrement
 */
#ifndef LOG_LABEL
#define LOG_LABEL
#define LOG_LABEL_FMT
#else
#define LOG_LABEL_FMT LOG_LABEL ": "
#endif

/**
 * LOG - Log a formatted message to kernel ring buffer
 * @fmt: Printf-style format string
 * @args: Variable arguments for format string
 *
 * Wrapper around printk that automatically adds the module name prefix
 * and uses KERN_DEBUG log level. Messages can be viewed with dmesg.
 *
 * Log levels (from most to least severe):
 *   KERN_EMERG, KERN_ALERT, KERN_CRIT, KERN_ERR,
 *   KERN_WARNING, KERN_NOTICE, KERN_INFO, KERN_DEBUG
 *
 * Note: KERN_DEBUG messages may be filtered by default. Use "dmesg -n 8"
 * to ensure debug messages are visible.
 *
 * Example:
 *   LOG("Processing %d bytes at sector %d", size, sector);
 *   // Output: "imgrement: Processing 1024 bytes at sector 42"
 */
#define LOG(fmt, args...) printk(KERN_DEBUG LOG_LABEL_FMT fmt "\n", ##args)

/**
 * LOG_VAR - Log a simple string variable
 * @msg: String to log
 *
 * Convenience macro for logging a string without format specifiers.
 *
 * Example:
 *   LOG_VAR("Module initialized");
 */
#define LOG_VAR(msg) LOG("%s", msg)

/**
 * _astgo - Assert with goto error handling
 * @expr: Expression to evaluate (condition that should be true)
 * @msg: Error message to set if assertion fails
 * @err: Variable to store error message
 * @addr: Label to jump to on failure
 *
 * This is an error handling pattern used in the module. If the expression
 * evaluates to false, it sets the error message and jumps to the specified
 * label (typically for cleanup code).
 *
 * Example usage:
 *   char *err = NULL;
 *   struct resource *res = allocate_resource();
 *   _astgo(res != NULL, "Allocation failed", err, cleanup);
 *
 *   // ... use resource ...
 *
 * cleanup:
 *   if (err) LOG("Error: %s", err);
 *   free_resource(res);
 *
 * Note: This is an internal macro. Consider using standard kernel error
 * handling patterns (IS_ERR, PTR_ERR, etc.) for new code.
 */
#define _astgo(expr, msg, err, addr)                                           \
    if (!(expr)) {                                                             \
        err = msg;                                                             \
        goto addr;                                                             \
    }

/**
 * ast_vfree - Safe wrapper around vfree
 * @ptr: Pointer to memory allocated with vmalloc
 *
 * Safely frees memory allocated with vmalloc, checking for NULL first.
 * This prevents crashes from accidental double-free or NULL pointer scenarios.
 *
 * Context: Can sleep (vmalloc/vfree may sleep). Do not call from atomic context.
 *
 * Example:
 *   char *buffer = vmalloc(SIZE);
 *   // ... use buffer ...
 *   ast_vfree(buffer);        // Safe even if buffer is NULL
 *   ast_vfree(buffer);        // Second call is safe (no-op)
 */
inline void ast_vfree(const void *ptr);

/**
 * find_word_count - Count occurrences of a word in a buffer
 * @word: Null-terminated string to search for
 * @data: Buffer to search within
 * @len: Length of buffer in bytes
 *
 * Searches through a data buffer and counts how many times a specific
 * word appears. This is a demonstration feature showing how to inspect
 * I/O data content.
 *
 * The function performs simple substring matching. For the current
 * implementation, it counts overlapping occurrences. For example,
 * "AAA" searched in "AAAA" would count as 2 matches.
 *
 * Return: Number of occurrences found (0 if word not found)
 *
 * Example usage:
 *   char data[] = "DAMNDAMNDAMN";
 *   int count = find_word_count("DAMN", data, sizeof(data) - 1);
 *   LOG("Found DAMN %d times", count);  // Output: "Found DAMN 3 times"
 *
 * Note: This is a demo feature. In production code, you might want to:
 * - Search for binary patterns instead of strings
 * - Use more efficient search algorithms (Boyer-Moore, KMP)
 * - Make the search term configurable at runtime
 * - Search for multiple patterns simultaneously
 *
 * Security consideration: Avoid logging or processing sensitive data
 * patterns that might expose passwords, keys, or private information.
 */
int find_word_count(const char *word, const char *data, const int len);

#endif /* COMMON_H */
