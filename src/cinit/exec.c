#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include "utils.h"
#include "log.h"

extern char **environ;

#define MAX_ARGS 128

#define DIM(a) (sizeof(a)/sizeof(a[0]))

typedef struct {
    int fds[2];
    exec_cmd_line_callback_t callback;
    void *callback_data;
} process_line_callback_ctx_t;

/*
 * Print to the standard error output a message followed by the last errno
 * description and then exit.
 *
 * NOTE: This function is intended to be called by the child after a fork.
 *       Mutex is not locked before printing to the standard error output.
 *
 * @param[in] eval The value to exit with.
 * @param[in] fmt The format of the message.
 * @param[in] ... Arguments of the message.
 */
static void err(int eval, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, ": %s\n", strerror(errno));
    _exit(eval);
}

/**
 * Function invoked for each line of the command's output.
 *
 * @param[in] fd File descriptor from which the line comes from.
 * @param[in] line The line.
 * @param[in] data Pointer to our process_line_callback_ctx_t context.
 */
static void process_line(int fd, const char *line, void *data)
{
    // Get our context.
    process_line_callback_ctx_t *ctx = (process_line_callback_ctx_t *)data;

    if (ctx->fds[STDOUT] == fd) {
        // Line from the command's stdout.
        ctx->callback(line, STDOUT, ctx->callback_data);
    }
    else if (ctx->fds[STDERR] == fd) {
        // Line from the command's stderr.
        ctx->callback(line, STDERR, ctx->callback_data);
    }
    else {
        assert(!"Unexpected file descriptor.");
    }
}

int exec_cmd_with_line_callback(exec_cmd_line_callback_t callback, void *callback_data, const char *cmd, ...)
{
    va_list arguments;
    int stdout_link[2];
    int stderr_link[2];

    char *argv[MAX_ARGS + 1];
    memset(argv, 0, DIM(argv) * sizeof(char *));

    // Build the array of arguments.
    va_start(arguments, cmd);
    for (unsigned int i = 0; i < DIM(argv) - 1; i++) {
        argv[i] = va_arg(arguments, char *);
        if (argv[i] == NULL) {
            break;
        }
    }
    va_end(arguments);

    // Create the pipe.
    if (pipe(stdout_link) != 0) {
        return -1;
    }
    else if (pipe(stderr_link) != 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        // Fork failed.
        close(stdout_link[0]);
        close(stdout_link[1]);
        close(stderr_link[0]);
        close(stderr_link[1]);
        return -1;
    }
    else if (pid > 0) {
        // Parent.
        int status;
        int retval = 0;

        // Write side not needed.
        close(stdout_link[1]);
        close(stderr_link[1]);

        // Read child's output.
        process_line_callback_ctx_t ctx = {
            { stdout_link[0], stderr_link[0] },
            callback,
            callback_data,
        };
        retval = read_lines(ctx.fds, DIM(ctx.fds), process_line, &ctx);

        close(stdout_link[0]);
        close(stderr_link[0]);

        // Wait for the child to terminate.
        if (waitpid(pid, &status, 0) < 0) {
            retval = -1;
        }
        else if (retval == 0 && WIFEXITED(status)) {
            retval = WEXITSTATUS(status);
        }
        else if (retval == 0 && WIFSIGNALED(status)) {
            // https://tldp.org/LDP/abs/html/exitcodes.html
            retval = 128 + WTERMSIG(status);
        }
        else {
            retval = -1;
        }

        return retval;
    }
    else {
        // Child.
        if (dup2(stdout_link[1], STDOUT_FILENO) < 0) {
            err(126, "dup2(STDOUT_FILENO)");
        }
        else if (dup2(stderr_link[1], STDERR_FILENO) < 0) {
            err(126, "dup2(STDERR_FILENO)");
        }
        close(stdout_link[0]);
        close(stdout_link[1]);
        close(stderr_link[0]);
        close(stderr_link[1]);

        // Execute program.
        execve(cmd, argv, environ);
        err(126, "execve(%s)", argv[0]);
    }
    return -1;
}

#if 0
int exec_cmd_with_output(char **buf, size_t *bufsize, const char *cmd, ...)
{
    //The maximum amount of memory that can be allocated to store the output of
    // a command.
    #define MAX_MEMORY_FOR_CMD_OUTPUT 1048576

    int i;
    char *argv[MAX_ARGS + 1];
    va_list arguments;
    pid_t pid;
    int link[2];
    bool alloc_buf = (*buf == NULL);

    if (*buf != NULL && *bufsize == 0) {
        // Buffer provided, but size is 0.
        return -1;
    }
    else if (*buf == NULL) {
        *bufsize = 0;
    }

    // Create the pipe.
    if (pipe(link) != 0) {
        return -1;
    }

    // Build the array of arguments.
    va_start(arguments, cmd);
    for (i = 0; i < DIM(argv) - 1; i++) {
        argv[i] = va_arg(arguments, char *);
        if (argv[i] == NULL) {
            break;
        }
    }
    argv[i] = NULL;
    va_end(arguments);

    pid = fork();
    if (pid < 0) {
        // Fork failed.
        close(link[0]);
        close(link[1]);
        return -1;
    }
    else if (pid > 0) {
        // Parent.
        int status;
        int retval = 0;
        size_t total_bytes_read = 0;

        // Write side not needed.
        close(link[1]);

        // Read child's output.
        while (true) {
            if (alloc_buf) {
                *bufsize += 1024;

                // Protect against output being too big.
                if (*bufsize > MAX_MEMORY_FOR_CMD_OUTPUT) {
                    retval = -1;
                    break;
                }

                char *new_buf = realloc(*buf, *bufsize);
                if (!new_buf) {
                    // Error reallocating memory.
                    retval = -1;
                    break;
                }
                *buf = new_buf;
            }

            size_t bytes_to_read = *bufsize - total_bytes_read - 1;
            ssize_t bytes_read = read(link[0], *buf + total_bytes_read, bytes_to_read);
            if (bytes_read < 0) {
                if (errno == EINTR) {
                    continue;
                }
                // Read error.
                retval = -1;
                break;
            }
            else if (bytes_read == 0) {
                (*buf)[total_bytes_read] = '\0';
                break;
            }
            else {
                total_bytes_read += bytes_read;
                if (!alloc_buf && bytes_read == bytes_to_read) {
                    // Check if we reached the EOF.  If not, the passed buffer
                    // is too small.
                    char b;
                    if (read(link[0], &b, sizeof(b)) != 0) {
                        // We were able to read additional data, or we got a
                        // read error.
                        retval = -1;
                        break;
                    }
                }
            }
        }

        // Make sure the output is NULL-terminated.
        if (retval == 0) {
            (*buf)[total_bytes_read] = '\0';
        }

        close(link[0]);

        // Wait for the child to terminate.
        if (waitpid(pid, &status, 0) < 0) {
            retval = -1;
        }
        else if (retval == 0 && WIFEXITED(status)) {
            retval = WEXITSTATUS(status);
        }
        else if (retval == 0 && WIFSIGNALED(status)) {
            // https://tldp.org/LDP/abs/html/exitcodes.html
            retval = 128 + WTERMSIG(status);
        }
        else {
            retval = -1;
        }

        if (retval < 0 && alloc_buf) {
            if (*buf) {
                free(*buf);
                *buf = NULL;
            }
        }
        return retval;
    }
    else {
        // Child.
        dup2(link[1], STDOUT_FILENO);
        close(link[0]);
        close(link[1]);
        execve(cmd, argv, environ);
        err(126, "execve(%s)", argv[0]);
    }
    return -1;
}
#endif

int exec_cmd(bool disable_output, const char *output_prefix, const char *cmd, ...)
{
    va_list arguments;
    int stdout_link[2];
    int stderr_link[2];

    char *argv[MAX_ARGS + 1];
    memset(argv, 0, DIM(argv) * sizeof(char *));

    // Build the array of arguments.
    va_start(arguments, cmd);
    for (unsigned int i = 0; i < DIM(argv) - 1; i++) {
        argv[i] = va_arg(arguments, char *);
        if (argv[i] == NULL) {
            break;
        }
    }
    va_end(arguments);

    // Create the pipe.
    if (!disable_output) {
        if (pipe(stdout_link) != 0) {
            return -1;
        }
        else if (pipe(stderr_link) != 0) {
            return -1;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        // Fork failed.
        close(stdout_link[0]);
        close(stdout_link[1]);
        close(stderr_link[0]);
        close(stderr_link[1]);
        return -1;
    }
    else if (pid > 0) {
        // Parent.
        int status;
        int retval = 0;

        if (!disable_output) {
            // Write side not needed.
            close(stdout_link[1]);
            close(stderr_link[1]);

            // Read child's output.
            retval = log_prefixer(output_prefix, stdout_link[0], stderr_link[0]);
        }

        close(stdout_link[0]);
        close(stderr_link[0]);

        // Wait for the child to terminate.
        if (waitpid(pid, &status, 0) < 0) {
            retval = -1;
        }
        else if (retval == 0 && WIFEXITED(status)) {
            retval = WEXITSTATUS(status);
        }
        else if (retval == 0 && WIFSIGNALED(status)) {
            // https://tldp.org/LDP/abs/html/exitcodes.html
            retval = 128 + WTERMSIG(status);
        }
        else {
            retval = -1;
        }

        return retval;
    }
    else {
        // Child.
        if (disable_output) {
            int fd = open("/dev/null", O_WRONLY);
            if (fd < 0) {
                err(126, "open(/dev/null)");
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                err(126, "dup2(STDOUT_FILENO)");
            }
            if (dup2(fd, STDERR_FILENO) < 0) {
                err(126, "dup2(STDERR_FILENO)");
            }
            close(fd);
        }
        else {
            if (dup2(stdout_link[1], STDOUT_FILENO) < 0) {
                err(126, "dup2(STDOUT_FILENO)");
            }
            else if (dup2(stderr_link[1], STDERR_FILENO) < 0) {
                err(126, "dup2(STDERR_FILENO)");
            }
            close(stdout_link[0]);
            close(stdout_link[1]);
            close(stderr_link[0]);
            close(stderr_link[1]);
        }

        // Execute program.
        execve(cmd, argv, environ);
        err(126, "execve(%s)", argv[0]);
    }
    return -1;
}

