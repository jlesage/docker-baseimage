#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <poll.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "log.h"
#include "utils.h"

#define STDOUT_IDX 0
#define STDERR_IDX 1

typedef struct {
    int fds[2];
    const char *prefix;
} log_prefixer_ctx_t;

static pthread_mutex_t g_stdout_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_stderr_mutex = PTHREAD_MUTEX_INITIALIZER;

static void log_prefixer_callback(int fd, const char *line, void *data)
{
    log_prefixer_ctx_t *ctx = (log_prefixer_ctx_t *)data;

    if (fd == ctx->fds[STDOUT_IDX]) {
        log_stdout("%s%s\n",
                ctx->prefix ? ctx->prefix : "",
                line);
    }
    else if (fd == ctx->fds[STDERR_IDX]) {
        log_stderr("%s%s\n",
                ctx->prefix ? ctx->prefix : "",
                line);
    }
    else {
        assert(!"Unexpected file descriptor.");
    }
}

void log_stdout(const char *format, ...)
{
    pthread_mutex_lock(&g_stdout_mutex);

    va_list args;
    va_start(args, format);
    vdprintf(STDOUT_FILENO, format, args);
    va_end(args);

    pthread_mutex_unlock(&g_stdout_mutex);
}

void log_stderr(const char *format, ...)
{
    pthread_mutex_lock(&g_stderr_mutex);

    va_list args;
    va_start(args, format);
    vdprintf(STDERR_FILENO, format, args);
    va_end(args);

    pthread_mutex_unlock(&g_stderr_mutex);
}

int log_prefixer(const char *prefix, int stdout_fd, int stderr_fd)
{
    log_prefixer_ctx_t ctx = {
        { stdout_fd, stderr_fd },
        prefix,
    };

    return read_lines(ctx.fds, DIM(ctx.fds), log_prefixer_callback, &ctx);
}
