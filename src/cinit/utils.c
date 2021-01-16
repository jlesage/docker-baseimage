#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <poll.h>

#include "utils.h"
#include "CException.h"

#define MAX(a, b) ((a)>=(b)?(a):(b))

/** Do not read file bigger than 1MB. */
#define MAX_FILE_READ_SIZE 1048576

/** Do not alloc more than 1MB of memory for command output. */
#define MAX_MEMORY_FOR_CMD_OUTPUT 1048576

typedef struct {
    char buf[4096];
    size_t used;
    bool eof;
} output_read_state_t;

typedef struct {
    int err;
    unsigned int num_lines_added;
    bool allocated_buf;
    char *buf;
    size_t bufsize;
} process_cmd_output_ctx_t;

/**
 * Function invoked for each line of the command's output.
 *
 * @param[in] line The line to process.
 * @param[in] src Whether the line is coming from stdout or stderr.
 * @param[in] data Pointer to our process_cmd_output_ctx_t context.
 */
static void process_line(const char *line, std_output_t src, void *data)
{
    process_cmd_output_ctx_t *ctx = (process_cmd_output_ctx_t *)data;

    if (src != STDOUT) {
        // Only handle stdout.
        return;
    }

    if (ctx->err != 0) {
        return;
    }

    if (ctx->num_lines_added == 0 && ctx->buf) {
        ctx->buf[0] = '\0';
    }

    size_t line_len = strlen(line);
    size_t buf_len = ctx->buf ? strlen(ctx->buf) : 0;
    int nl_char_needed = (ctx->num_lines_added > 0);

    if ((buf_len + line_len + nl_char_needed) >= ctx->bufsize) {
        if (ctx->allocated_buf) {
            ctx->bufsize = MAX(ctx->bufsize + 1024, ctx->bufsize + line_len + 1);
            if (ctx->bufsize > MAX_MEMORY_FOR_CMD_OUTPUT) {
                // Output is too big.
                ctx->err = E2BIG;
                return;
            }

            // Re-alloc memory.
            char *newBuf = realloc(ctx->buf, ctx->bufsize);
            if (!newBuf) {
                // Memory allocation failure.
                ctx->err = ENOMEM;
                return;
            }
            if (ctx->buf == NULL) {
                newBuf[0] = '\0';
            }
            ctx->buf = newBuf;
        }
        else {
            // Buffer too small to contains the output.
            ctx->err = ENOBUFS;
            return;
        }
    }

    if (nl_char_needed) {
        strcat(ctx->buf, "\n");
    }
    strcat(ctx->buf, line);
    ctx->num_lines_added++;
}

/**
 * Determine if the string contains digits only.
 *
 * @param[in] str The input string.
 *
 * @return true if the string contains digits only, false otherwise.
 */
static bool digits_only(const char *str)
{
    if (!str || strlen(str) == 0) {
        return false;
    }

    for (int i = 0; i < strlen(str); i++) {
        if (!isdigit(str[i])) {
            return false;
        }
    }
    return true;
}

char *trim_char(char *s, char c)
{
    char *p = s;
    int l = strlen(p);

    // Trim the end.
    while(p[l - 1] == c) p[--l] = 0;

    // Trim the beginning.
    while(*p && *p == c) ++p, --l;
    memmove(s, p, l + 1);

    return s;
}

char *trim(char *s)
{
    char *p = s;
    int l = strlen(p);

    // Trim the end.
    while(isspace(p[l - 1])) p[--l] = 0;

    // Trim the beginning.
    while(*p && isspace(* p)) ++p, --l;
    memmove(s, p, l + 1);

    return s;
}

void remove_duplicated_char(char *s, char c)
{
    if (s[0] == '\0') {
        return;
    }
    else if (s[0] == s[1] && s[0] == c) {
        int i = 0;
        while (s[i] != '\0') {
            s[i] = s[i  + 1];
            i++;
        }
        remove_duplicated_char(s, c);
    }
    remove_duplicated_char(s + 1, c);
}

void remove_all_char(char *s, char c)
{
    if (s[0] == '\0') {
        return;
    }
    else if (s[0] == c) {
        int i = 0;
        while (s[i] != '\0') {
            s[i] = s[i  + 1];
            i++;
        }
        remove_all_char(s, c);
    }
    remove_all_char(s + 1, c);
}

void terminate_at_first_eol(char *s)
{
    for (char *ptr = s; *ptr != '\0'; ptr++) {
        if (*ptr == '\n' || *ptr == '\r') {
            *ptr = '\0';
            break;
        }
    }
}

char **split(char *buf, int c, size_t *len, int plus, int ofs)
{
    int num_tokens = 1;
    char **vector = NULL;
    char **w;

    // Remove any consecutive delimiters from the string.
    remove_duplicated_char(buf, c);

    // Count tokens.
    for (char *s = buf; *s; s++) {
        if (*s == c) num_tokens++;
    }

    // Allocate the vector.
    vector = (char **)malloc((num_tokens + plus) * sizeof(char*));
    if (!vector) return NULL;

    // Skip the first entries.
    w = vector + ofs;

    // Fill the vector.
    *w++ = buf;
    for (char *s = buf; ; s++) {
        // Get the next token.
        while (*s && *s != c) s++;

        // Check if we reached the end of the buffer.
        if (*s == 0) {
            break;
        }

        // Mark the end of the token..
        *s = 0;

        // Make sure the next token is not empty and add it to the vector.
        if (*(s + 1) == 0) {
            break;
        }
        *w++ = s + 1;
    }

    // Set the length of the vector.
    *len = w - vector;

    // Return the vector.
    return vector;
}

int read_lines(int *fds, size_t num_fds, line_callback_t callback, void *callback_data)
{
    int retval = 0;
    bool done = false;
    output_read_state_t *read_states = NULL;
    struct pollfd pfds[num_fds];

    // Fill the file descriptors for poll function.
    for (unsigned int i = 0; i < num_fds; i++) {
        pfds[i].fd = fds[i];
        pfds[i].events = POLLIN;
    }

    // Alloc memory for read states.
    read_states = calloc(num_fds, sizeof(output_read_state_t));
    if (!read_states) {
        retval = -1;
    }

    // Read file descriptors.
    while (retval == 0 && !done) {
        // Poll.
        if (poll(pfds, num_fds, -1) < 0) {
            if (errno == EINTR) {
                continue;
            }
            else {
                retval = -1;
                break;
            }
        }

        // Process file descriptors.
        for (unsigned int i = 0; i < num_fds; i++) {
            output_read_state_t *rstate = &read_states[i];

            if (pfds[i].revents & POLLIN) {
                // Ok, data available to read.
            }
            else if (pfds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                // The other end of the pipe has been closed.
                rstate->eof = true;
                rstate->buf[rstate->used] = '\0';
                rstate->used = 0;
                if (rstate->buf[0] != '\0') {
                    callback(pfds[i].fd, rstate->buf, callback_data);
                }
                continue;
            }
            else {
                // Current fd not readable yet.
                continue;
            }

            // Get the maximum number of bytes to read.
            size_t max_to_read = sizeof(rstate->buf) - rstate->used - 1;

            // If there is nothing to read, line too big to fit in buffer.  We
            // need to flush the buffer.
            if (max_to_read == 0) {
                rstate->buf[rstate->used] = '\0';
                rstate->used = 0;
                callback(pfds[i].fd, rstate->buf, callback_data);
                continue;
            }

            // Read data.
            ssize_t bytes_read = read(pfds[i].fd, rstate->buf + rstate->used, max_to_read);
            if (bytes_read < 0) {
                if (errno == EINTR) {
                    continue;
                }
                // Read error.
                retval = -1;
                break;
            }
            else if (bytes_read == 0) {
                // EOF.  Line is complete.
                rstate->eof = true;
                rstate->buf[rstate->used] = '\0';
                rstate->used = 0;
                if (rstate->buf[0] != '\0') {
                    callback(pfds[i].fd, rstate->buf, callback_data);
                }
                continue;
            }
            else {
                rstate->used += bytes_read;
            }

            // Check if we have a complete line.
            while (true) {
                bool complete_line = false;
                for (unsigned int j = 0; j < rstate->used; j++) {
                    if (rstate->buf[j] == '\n' || rstate->buf[j] == '\r') {
                        rstate->buf[j] = '\0';

                        // We have a complete line.
                        complete_line = true;
                        if (j != 0) {
                            // Invoke the callback.
                            callback(pfds[i].fd, rstate->buf, callback_data);
                        }

                        // Adjust the buffer.
                        rstate->used -= (j + 1);
                        if (rstate->used > 0) {
                            memmove(rstate->buf, rstate->buf + j + 1, rstate->used);
                        }
                        break;
                    }
                }

                // Continue until we don't find complete lines.
                if (!complete_line) {
                    break;
                }
            }
        }

        // Check if all file descriptor are done.
        done = true;
        for (unsigned int i = 0; i < num_fds; i++) {
            if (!read_states[i].eof) {
                done = false;
                break;
            }
        }
    }

    // Free read states.
    if (read_states) {
        free(read_states);
        read_states = NULL;
    }

    return retval;
}

void read_file(const char *filepath, char **buf, size_t bufsize)
{
    bool alloc_buf = (*buf == NULL);

    // If a buffer is provided, its size should not be 0.
    assert(*buf == NULL || bufsize > 0);

    // Open the file.
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        ThrowMessageWithErrno("could not open file");
    }

    // Allocate buffer if no one provided.
    if (alloc_buf) {
        // Get the size of the file.
        off_t flen = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        bufsize = flen + 1;

        // Protect against loading big files into memory.
        if (bufsize > MAX_FILE_READ_SIZE) {
            close(fd);
            ThrowMessage("file too big");
        }

        // Allocate memory.
        *buf = (char*)malloc(bufsize);
        if (!*buf) {
            close(fd);
            ThrowMessage("memory allocation failed");
        }
    }

    // Read the file.
    ssize_t len = read(fd, *buf, bufsize - 1);
    if (len < 0) {
        close(fd);
        ThrowMessageWithErrno("read failed");
    }

    // NULL-terminate the buffer.
    (*buf)[len] = 0;

    if (!alloc_buf && len == bufsize - 1) {
        // Check if we reached the EOF.  If not, the passed buffer is too small.
        char b;
        len = read(fd, &b, sizeof(b));
        if (len < 0) {
            close(fd);
            ThrowMessageWithErrno("read failed");
        }
        else if (len == 0) {
            // Ok, we got an EOF.
        }
        else {
            // We were able to read additonal data.
            close(fd);
            ThrowMessage("buffer too small");
        }
    }

    close(fd);
}

void string_to_bool(const char *str, bool *result)
{
    if (strcmp(str, "1") == 0 ||
        strcasecmp(str, "true") == 0 ||
        strcasecmp(str, "on") == 0 ||
        strcasecmp(str, "yes") == 0 ||
        strcasecmp(str, "enable") == 0 ||
        strcasecmp(str, "enabled") == 0) {
        // True value.
        *result = true;
    }
    else if (strcmp(str, "0") == 0 ||
        strcasecmp(str, "false") == 0 ||
        strcasecmp(str, "off") == 0 ||
        strcasecmp(str, "no") == 0 ||
        strcasecmp(str, "disable") == 0 ||
        strcasecmp(str, "disabled") == 0) {
        // False value.
        *result = false;
    }
    else {
        ThrowMessage("not a boolean value");
    }
}

void string_to_int(const char *str, int *result)
{
    char *endptr;
    long int val;

    errno = 0;
    val = strtol(str, &endptr, 10);
    if (endptr == str) {
        // Not a  number.
        ThrowMessage("not a number");
    }
    else if ('\0' != *endptr) {
        // Extra characters at end of input.
        ThrowMessage("not a number");
    }
    else if ((LONG_MIN == val || LONG_MAX == val) && ERANGE == errno) {
        // Out of range of type long int.
        ThrowMessage("out of range");
    }
    else if (val > INT_MAX || val < INT_MIN) {
        // Out of range of type int.
        ThrowMessage("out of range");
    }

    *result = (int)val;
}

void string_to_uint(const char *str, unsigned int *result)
{
    char *endptr;
    unsigned long int val;

    errno = 0;
    val = strtoul(str, &endptr, 10);
    if (endptr == str) {
        // Not a number.
        ThrowMessage("not a number");
    }
    else if ('\0' != *endptr) {
        // Extra characters at end of input.
        ThrowMessage("not a number");
    }
    else if (val == ULLONG_MAX && ERANGE == errno) {
        // Out of range of type unsinged long int.
        ThrowMessage("out of range");
    }
    else if (val > UINT_MAX) {
        // Out of range of type unsigned int.
        ThrowMessage("out of range");
    }

    *result = (unsigned int)val;
}

void string_to_interval(const char *str, unsigned int *result)
{
    if (strcmp(str, "yearly") == 0) {
        *result = 60 * 60 * 24 * 7 * 365;
    }
    else if (strcmp(str, "monthly") == 0) {
        *result = 60 * 60 * 24 * 7 * 30;
    }
    else if (strcmp(str, "weekly") == 0) {
        *result = 60 * 60 * 24 * 7;
    }
    else if (strcmp(str, "daily") == 0) {
        *result = 60 * 60 * 24;
    }
    else if (strcmp(str, "hourly") == 0) {
        *result = 60 * 60;
    }

    // Convert to unsigned int.
    string_to_uint(str, result);
}

void string_to_uid(const char *str, uid_t *result)
{
    if (digits_only(str)) {
        unsigned int val;
        string_to_uint(str, &val);
        *result = (uid_t)val;
    }
    else {
        struct passwd *pwd = getpwnam(str);
        if (pwd == NULL) {
            ThrowMessage("unknown user '%s'", str);
        }
        *result = pwd->pw_uid;
    }
}

void string_to_gid(const char *str, gid_t *result)
{
    if (digits_only(str)) {
        unsigned int val;
        string_to_uint(str, &val);
        *result = (gid_t)val;
    }
    else {
        struct group *grp = getgrnam(str);
        if (grp == NULL) {
            ThrowMessage("unknown group '%s'", str);
        }
        *result = grp->gr_gid;
    }
}

void string_to_mode(const char *str, mode_t *result)
{
    char *endptr;
    unsigned long int val;

    errno = 0;
    val = strtoul(str, &endptr, 8);
    if (endptr == str) {
        // Not a number.
        ThrowMessage("not a number");
    }
    else if ('\0' != *endptr) {
        // Extra characters at end of input.
        ThrowMessage("not a number");
    }
    else if (val == ULLONG_MAX && ERANGE == errno) {
        // Out of range of type unsinged long int.
        ThrowMessage("out of range");
    }
    else if (val > UINT_MAX) {
        // Out of range of type unsigned int.
        ThrowMessage("out of range");
    }

    *result = val;
}

bool load_value_as_string(const char *filepath, char **buf, size_t bufsize)
{
    // Check if the file exists and is executable.
    if (access(filepath, X_OK) == 0) {
        // Extract the programe name.
        const char *progname = rindex(filepath, '/');
        if (progname) {
            progname++;
        }
        else {
            progname = filepath;
        }

        process_cmd_output_ctx_t ctx = {
            .err = 0,
            .num_lines_added = 0,
            .allocated_buf = (*buf == NULL),
            .buf = *buf,
            .bufsize = (*buf == NULL) ? 0 : bufsize,
        };

        // Run the program.
        int rc = exec_cmd_with_line_callback(process_line, &ctx, filepath, progname, NULL);
        if (rc != 0) {
            if (ctx.allocated_buf && ctx.buf) {
                free(ctx.buf);
            }
            ThrowMessage("could not load '%s': command failed (%d)", filepath, rc);
        }
        else if (ctx.err != 0) {
            if (ctx.allocated_buf && ctx.buf) {
                free(ctx.buf);
            }
            ThrowMessage("could not load '%s': command output handling failed: %s",
                    filepath,
                    strerror(ctx.err));
        }
        else if (ctx.allocated_buf) {
            *buf = ctx.buf;
        }
        return true;
    }
    // Check if the file exists.
    else if (access(filepath, F_OK) == 0) {
        CEXCEPTION_T e;

        Try {
            read_file(filepath, buf, bufsize);
        }
        Catch (e) {
            ThrowMessage("could not load '%s': %s", filepath, e.mMessage);
        }
        return true;
    }
    else {
        // Config value not set.
        return false;
    }
}

bool load_value_as_bool(const char *filepath, bool *result)
{
    CEXCEPTION_T e;

    char buf[32];
    char *bufptr = buf;

    // Get value from file.
    if (!load_value_as_string(filepath, &bufptr, sizeof(buf))) {
        return false;
    }

    terminate_at_first_eol(buf);
    trim(buf);

    // Check if the file has been "touched".
    if (strlen(buf) == 0) {
        *result = true;
        return true;
    }

    // Convert.
    Try {
        string_to_bool(buf, result);
    }
    Catch (e) {
        ThrowMessage("could not load '%s': %s", filepath, e.mMessage);
    }

    return true;
}

bool load_value_as_int(const char *filepath, int *result)
{
    CEXCEPTION_T e;

    char buf[128];
    char *bufptr = buf;

    // Get value from file.
    if (!load_value_as_string(filepath, &bufptr, sizeof(buf))) {
        return false;
    }

    terminate_at_first_eol(buf);
    trim(buf);

    // Convert.
    Try {
        string_to_int(buf, result);
    }
    Catch (e) {
        ThrowMessage("could not load '%s': %s", filepath, e.mMessage);
    }

    return true;
}

bool load_value_as_uint(const char *filepath, unsigned int *result)
{
    CEXCEPTION_T e;

    char buf[128];
    char *bufptr = buf;

    // Get value from file.
    if (!load_value_as_string(filepath, &bufptr, sizeof(buf))) {
        return false;
    }

    terminate_at_first_eol(buf);
    trim(buf);

    // Convert.
    Try {
        string_to_uint(buf, result);
    }
    Catch (e) {
        ThrowMessage("could not load '%s': %s", filepath, e.mMessage);
    }

    return true;
}

bool load_value_as_interval(const char *filepath, unsigned int *result)
{
    CEXCEPTION_T e;

    char buf[128];
    char *bufptr = buf;

    // Get value from file.
    if (!load_value_as_string(filepath, &bufptr, sizeof(buf))) {
        return false;
    }

    terminate_at_first_eol(buf);
    trim(buf);

    // Convert.
    Try {
        string_to_interval(buf, result);
    }
    Catch (e) {
        ThrowMessage("could not load '%s': %s", filepath, e.mMessage);
    }

    return true;
}

bool load_value_as_uid(const char *filepath, uid_t *result)
{
    CEXCEPTION_T e;

    char buf[256];
    char *bufptr = buf;

    // Get value from file.
    if (!load_value_as_string(filepath, &bufptr, sizeof(buf))) {
        return false;
    }

    terminate_at_first_eol(buf);
    trim(buf);

    // Convert.
    Try {
        string_to_uid(buf, result);
    }
    Catch (e) {
        ThrowMessage("could not load '%s': %s", filepath, e.mMessage);
    }

    return true;
}

bool load_value_as_gid(const char *filepath, gid_t *result)
{
    CEXCEPTION_T e;

    char buf[256];
    char *bufptr = buf;

    // Get value from file.
    if (!load_value_as_string(filepath, &bufptr, sizeof(buf))) {
        return false;
    }

    terminate_at_first_eol(buf);
    trim(buf);

    // Convert.
    Try {
        string_to_gid(buf, result);
    }
    Catch (e) {
        ThrowMessage("could not load '%s': %s", filepath, e.mMessage);
    }

    return true;
}

bool load_value_as_mode(const char *filepath, mode_t *result)
{
    CEXCEPTION_T e;

    char buf[256];
    char *bufptr = buf;

    // Get value from file.
    if (!load_value_as_string(filepath, &bufptr, sizeof(buf))) {
        return false;
    }

    terminate_at_first_eol(buf);
    trim(buf);

    // Convert.
    Try {
        string_to_mode(buf, result);
    }
    Catch (e) {
        ThrowMessage("could not load '%s': %s", filepath, e.mMessage);
    }

    return true;
}
