#ifndef __CINIT_LOG_H__
#define __CINIT_LOG_H__

#include <stdatomic.h>

/**
 * Log to stdout.
 *
 * This function protects concurrent accesses to stdout with mutex.
 *
 * @param[in] format Format of the message to be logged.
 * @param[in] ... Argument(s) for the message.
 */
void log_stdout(const char *format, ...);

/**
 * Log to stderr.
 *
 * This function protects concurrent accesses to stderr with mutex.
 *
 * @param[in] format Format of the message to be logged.
 * @param[in] ... Argument(s) for the message.
 */
void log_stderr(const char *format, ...);

/**
 * Read from file descriptors and append prefix before logging to stdout/stderr.
 *
 * @param[in] prefix Prefix to be added.
 * @param[in] stdout_fd File descriptor associated to stdout.
 * @param[in] stderr_fd File descriptor associated to stderr.
 * @param[in] time_to_exit Pointer to boolean indicating if it's time to stop.
 *
 * @return -1 if an error occurred, 0 otherwise.
 */
int log_prefixer(const char *prefix, int stdout_fd, int stderr_fd, atomic_bool *time_to_exit);

#endif // __CINIT_LOG_H__
