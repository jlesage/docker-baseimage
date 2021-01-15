/* vi: set sw=4 ts=4 sts=4 et: */

/*
 * Simple log file monitor.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <linux/limits.h>
#include <assert.h>
#include <time.h>
#include <getopt.h>
#include <inttypes.h>

#define LM_SUCCESS 0
#define LM_ERROR 1

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#define DIM(a) (sizeof(a)/sizeof(a[0]))

#define DEFAULT_CONFIG_DIR "/etc/logmonitor"

#define MAX_NUM_MONITORED_FILES 16
#define MAX_NUM_NOTIFICATIONS 16
#define MAX_NUM_TARGETS 16

#define MAX_READ_FILE_SIZE (100 * 1024) /* 100KB */

#define STATUS_FILE_READ_INTERVAL 5 /* seconds */

#define DEBUG(...) do { if (debug_enabled) { printf(__VA_ARGS__); puts(""); } } while (0)
#define INFO(...)  do { printf(__VA_ARGS__); puts(""); } while (0)
#define ERROR(...) do { printf(__VA_ARGS__); puts(""); } while (0)

#define IS_SUCCESS(ret) ((ret) == LM_SUCCESS)
#define IS_ERROR(ret) (!IS_SUCCESS(ret))
#define SET_ERROR(ret, ...) do { (ret) = LM_ERROR; ERROR(__VA_ARGS__); } while (0)

#define FOR_EACH_MONITORED_FILE(c, f, idx) \
    lm_monitored_file_t *f = &(c)->monitored_files[0]; \
    for (int idx = 0; idx < (c)->num_monitored_files; idx++, (f) = &(c)->monitored_files[idx])

#define FOR_EACH_NOTIFICATION(c, n, idx) \
    lm_notification_t *n = (c)->notifications[0]; \
    for (int idx = 0; idx < (c)->num_notifications; idx++, (n) = (c)->notifications[idx])

#define FOR_EACH_TARGET(c, t, idx) \
    lm_target_t *t = (c)->targets[0]; \
    for (int idx = 0; idx < (c)->num_targets; idx++, (t) = (c)->targets[idx])

static const char* const short_options = "c:dh";
static struct option long_options[] = {
    { "configdir", no_argument, NULL, 'c' },
    { "debug", no_argument, NULL, 'd' },
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

static bool debug_enabled = false;

typedef struct {
    char *name;

    char *filter;

    char *title;
    bool is_title_exe;

    char *desc;
    bool is_desc_exe;

    char *level;
    bool is_level_exe;
} lm_notification_t;

typedef struct {
    char *name;
    char *send;
    int debouncing;

    time_t last_notif_sent[MAX_NUM_NOTIFICATIONS];
} lm_target_t;

typedef struct {
    char *path;
    int fd;
    bool is_status;
    time_t last_read;

    char *pending_buf;
} lm_monitored_file_t;

typedef struct {
    char *config_dir;

    unsigned char num_monitored_files;
    lm_monitored_file_t monitored_files[MAX_NUM_MONITORED_FILES];

    unsigned char num_notifications;
    lm_notification_t *notifications[MAX_NUM_NOTIFICATIONS];

    unsigned char num_targets;
    lm_target_t *targets[MAX_NUM_TARGETS];
} lm_context_t;

static bool str_ends_with(const char *str, const char c)
{
    return (str && strlen(str) > 0 && str[strlen(str) - 1] == c);
}

static bool str_starts_with(const char *str, const char c)
{
    return (str && strlen(str) > 0 && str[0] == 'c');
}

static char *join_path(const char *dir, const char *file)
{
    int size = strlen(dir) + strlen(file) + 2;
    char *buf = malloc(size * sizeof(char));

    if (buf) {
        strcpy(buf, dir);

        // Add '/' if necessary.
        if (!str_ends_with(dir, '/') && !str_starts_with(file, '/')) {
            strcat(buf, "/");
        }

        strcat(buf, file);
    }

    return buf;
}

static void terminate_str_at_first_eol(char *str)
{
    for (char *ptr = str; *ptr != '\0'; ptr++) {
        if (*ptr == '\n' || *ptr == '\r') {
            *ptr = '\0';
            break;
        }
    }
}

static ssize_t safe_read(int fd, void *buf, size_t count)
{
    ssize_t n;

    do {
        n = read(fd, buf, count);
    } while (n < 0 && errno == EINTR);

    return n;
}

/*
 * Read all of the supplied buffer from a file.
 * This does multiple reads as necessary.
 * Returns the amount read, or -1 on an error.
 * A short read is returned on an end of file.
 */
static ssize_t full_read(int fd, void *buf, size_t len)
{
    ssize_t cc;
    ssize_t total;

    total = 0;

    while (len) {
        cc = safe_read(fd, buf, len);

        if (cc < 0) {
            if (total) {
                /* we already have some! */
                /* user can do another read to know the error code */
                return total;
            }
            return cc; /* read() returns -1 on failure. */
        }
        if (cc == 0)
            break;
        buf = ((char *)buf) + cc;
        total += cc;
        len -= cc;
    }

    return total;
}

static ssize_t tail_read(int fd, char *buf, size_t count)
{
    ssize_t r;

    r = full_read(fd, buf, count);
    if (r < 0) {
        ERROR("read error");
    }

    return r;
}

static char *file2str(const char *filepath)
{
    int retval = LM_SUCCESS;
    char *buf = NULL;

    int fd = -1;
    struct stat st;

    // Get file stats.
    if (IS_SUCCESS(retval)) {
        if (stat(filepath, &st) < 0) {
            SET_ERROR(retval, "Failed to get stats of '%s'.", filepath);
        }
        else if (st.st_size > MAX_READ_FILE_SIZE) {
            SET_ERROR(retval, "File too big: '%s'.", filepath);
        }
    }

    // Allocate memory.
    if (IS_SUCCESS(retval)) {
        buf = malloc((st.st_size + 1) * sizeof(char));
        if (!buf) {
            SET_ERROR(retval, "Failed to allocated %jd bytes of memory to read file '%s'.",
                      (intmax_t)(st.st_size + 1),
                      filepath);
        }
    }

    // Open the file.
    if (IS_SUCCESS(retval)) {
        fd = open(filepath, O_RDONLY);
        if (fd < 0) {
            SET_ERROR(retval, "Failed to open '%s'.", filepath);
        }
    }

    // Read the file.
    if (IS_SUCCESS(retval)) {
        if (safe_read(fd, buf, st.st_size) < 0) {
            SET_ERROR(retval, "Failed to read '%s'.", filepath);
        }
        buf[st.st_size] = '\0';
    }

    // Close the file.
    if (fd >= 0) {
        close(fd);
    }

    // Free buffer if error occurred.
    if (retval != LM_SUCCESS && buf) {
        free(buf);
        buf = NULL;
    }

    return buf;
}

static int invoke_exec(const char *filter_exe, const char *args[], unsigned int num_args, int *exit_code, char *output, size_t outputsize)
{
    int retval = LM_SUCCESS;
    pid_t pid;
    int pipefds[2] = { -1, -1 };
    bool redirect_stdout = (output && outputsize > 0);

    // Create a pipe.
    if (IS_SUCCESS(retval) && redirect_stdout) {
        if (pipe(pipefds) == -1) {
            SET_ERROR(retval, "Pipe creation failed: %s.", strerror(errno));
        }
    }

    // Fork.
    if (IS_SUCCESS(retval)) {
        pid = fork();
        if (pid == -1) {
            SET_ERROR(retval, "Fork failed: %s.", strerror(errno));
        }
    }

    // Handle child.
    if (IS_SUCCESS(retval)) {
        if (pid == 0) {
            const char *all_args[num_args + 2];

            all_args[0] = filter_exe;
            for (int i = 1; i <= num_args; i++) {
                all_args[i] = args[i-1];
            }
            all_args[num_args + 1] = NULL;

            // Redirect stdout to our pipe.
            if (redirect_stdout) {
                while ((dup2(pipefds[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
                close(pipefds[1]);
                close(pipefds[0]);
            }

            if (execv(filter_exe, (char *const *)all_args) == -1) {
                exit(127);
            }
        }
    }

    if (pipefds[1] >= 0) {
        close(pipefds[1]);
    }

    // Handle output from child and wait for its termination.
    if (IS_SUCCESS(retval)) {
        pid_t ret;
        int status;

        assert(pid != 0);

        // Handle output from the child.
        if (output && outputsize > 0) {
            // Make sure to keep one byte for the null terminating character.
            ssize_t count = full_read(pipefds[0], output, outputsize - 1);
            if (count < 0) {
                output[0] = '\0';
                SET_ERROR(retval, "Failed to read output of child.");
            }
            else {
                output[count] = '\0';
            }
        }

        // Wait for the child to terminate.
        while ((ret = waitpid(pid, &status, 0)) == -1) {
            if (errno != EINTR) {
                SET_ERROR(retval, "wait_pid failure");
                break;
            }
        }

        if (ret == -1 || !WIFEXITED(status)) {
            retval = LM_ERROR;
        }
        else {
            *exit_code = WEXITSTATUS(status);
        }
    }

    if (pipefds[0] >= 0) {
        close(pipefds[0]);
    }

    return retval;
}

static int invoke_filter(const char *filter_exe, const char *line)
{
    int retval = LM_SUCCESS;
    const char *args[] = { line };
    int exit_code;

    retval = invoke_exec(filter_exe, args, DIM(args), &exit_code, NULL, 0);
    if (IS_SUCCESS(retval)) {
        return (exit_code == 0) ? LM_SUCCESS : LM_ERROR;
    }

    return retval;
}

static int invoke_target(const char *send_exe, const char *title, const char *desc, const char *level)
{
    int retval = LM_SUCCESS;
    const char *args[] = { title, desc, level };
    int exit_code;

    retval = invoke_exec(send_exe, args, DIM(args), &exit_code, NULL, 0);
    if (IS_SUCCESS(retval)) {
        return (exit_code == 0) ? LM_SUCCESS : LM_ERROR;
    }

    return retval;
}

static void handle_line(lm_context_t *ctx, char *buf)
{
    FOR_EACH_NOTIFICATION(ctx, notif, nidx) {
        DEBUG("Invoking filter for notification '%s'...", notif->name);
        if (invoke_filter(notif->filter, buf) == LM_SUCCESS) {
            // Filter indicated a match.
            DEBUG("Filter result: match.");

            FOR_EACH_TARGET(ctx, target, tidx) {
                if ((time(NULL) - target->last_notif_sent[nidx]) < target->debouncing) {
                    DEBUG("Ignoring target '%s': debouncing.", target->name);
                    continue;
                }

                // Send the target.
                DEBUG("Invoking target '%s'...", target->name);
                invoke_target(target->send, notif->title, notif->desc, notif->level);
                target->last_notif_sent[nidx] = time(NULL);
            }
        }
        else {
            DEBUG("Filter result: no match.");
        }
    }
}

static void handle_read(lm_context_t *ctx, unsigned int mfid, char *buf)
{
    lm_monitored_file_t *mf = &ctx->monitored_files[mfid];
    char *work_buf = NULL;
    char *eol_ptr = NULL;

    // Create the work buffer.
    if (mf->pending_buf) {
        int new_buf_size = strlen(mf->pending_buf) + strlen(buf) + 1;
        if (new_buf_size > (500 * 1024)) {
            ERROR("line too long");
            free(mf->pending_buf);
            mf->pending_buf = NULL;
            return;
        }

        work_buf = realloc(mf->pending_buf, new_buf_size);
        if (work_buf) {
            strcat(work_buf, buf);
            mf->pending_buf = NULL;
        }
        else {
            ERROR("memory reallocation error");
            free(mf->pending_buf);
            mf->pending_buf = NULL;
            return;
        }
    }
    else {
        work_buf = strdup(buf);
        if (!work_buf) {
            ERROR("string duplication error");
            return;
        }
    }

    eol_ptr = strchr(work_buf, '\n');

    if (eol_ptr) {
        char *next_line = eol_ptr + 1;

        // Remove the end of line characters.
        *eol_ptr = '\0';
        if (eol_ptr != work_buf && *(eol_ptr - 1) == '\r') {
            *(eol_ptr - 1) = '\0';
        }

        // Handle the line.
        if (strlen(work_buf) > 0) {
            handle_line(ctx, work_buf);
        }

        // Handle the next potential line.
        if (strlen(next_line) > 0) {
            handle_read(ctx, mfid, next_line);
        }

        free(work_buf);
        work_buf = NULL;
    }
    else {
        mf->pending_buf = work_buf;
    }
}

static void free_notification(lm_notification_t *notif) {
    if (notif) {
        if (notif->name) free(notif->name);
        if (notif->filter) free(notif->filter);
        if (notif->title) free(notif->title);
        if (notif->desc) free(notif->desc);
        if (notif->title) free(notif->title);

        memset(notif, 0, sizeof(*notif));
        free(notif);
    }
}

static lm_notification_t *alloc_notification(const char *notifications_dir, const char *name)
{
    int retval = LM_SUCCESS;
    lm_notification_t *notif = NULL;
    char *notification_dir = NULL;
    struct dirent *de = NULL;
    DIR *dr = NULL;

    // Build the notification directory path.
    if (IS_SUCCESS(retval)) {
        notification_dir = join_path(notifications_dir, name);
        if (!notification_dir) {
            SET_ERROR(retval, "Failed to build notification directory path.");
        }
    }

    // Alloc memory for notification structure.
    if (IS_SUCCESS(retval)) {
        notif = calloc(1, sizeof(*notif));
        if (!notif) {
            SET_ERROR(retval, "Failed to alloc memory for new notification.");
        }
    }

    // Set notification's name.
    if (IS_SUCCESS(retval)) {
        notif->name = strdup(name);
        if (!notif->name) {
            SET_ERROR(retval, "Failed to alloc memory for notification name.");
        }
    }

    // Open the directory.
    if (IS_SUCCESS(retval)) {
        dr = opendir(notification_dir);
        if (dr == NULL) {
            SET_ERROR(retval, "Notification config '%s' directory not found.", notification_dir);
        }
    }

    // Loop through all files of the directory.
    while (IS_SUCCESS(retval) && (de = readdir(dr)) != NULL) {
        // Handle regular files only.
        if (de->d_type != DT_REG) {
            continue;
        }

        char *filepath = join_path(notification_dir, de->d_name);
        if (!filepath) {
            SET_ERROR(retval, "Failed to build path.");
        }
        else if (strcmp(de->d_name, "filter") == 0) {
            notif->filter = filepath;
            filepath = NULL;
            if (access(notif->filter, X_OK) != 0) {
                SET_ERROR(retval, "Notification filter '%s' not executable.", notif->filter);
            }
        }
        else if (strcmp(de->d_name, "title") == 0) {
            if (access(filepath, X_OK) == 0) {
                notif->title = filepath;
                notif->is_title_exe = true;
                filepath = NULL;
            }
            else {
                notif->title = file2str(filepath);
                if (notif->title) {
                    terminate_str_at_first_eol(notif->title);
                }
                else {
                    SET_ERROR(retval, "Failed to read notification title.");
                }
            }
        }
        else if (strcmp(de->d_name, "desc") == 0) {
            if (access(filepath, X_OK) == 0) {
                notif->desc = filepath;
                notif->is_desc_exe = true;
                filepath = NULL;
            }
            else {
                notif->desc = file2str(filepath);
                if (notif->desc) {
                    terminate_str_at_first_eol(notif->desc);
                }
                else {
                    SET_ERROR(retval, "Failed to read notification description.");
                }
            }
        }
        else if (strcmp(de->d_name, "level") == 0) {
            if (access(filepath, X_OK) == 0) {
                notif->level = filepath;
                notif->is_level_exe = true;
                filepath = NULL;
            }
            else {
                notif->level = file2str(filepath);
                if (notif->level) {
                    terminate_str_at_first_eol(notif->level);
                    if (strcmp(notif->level, "ERROR") &&
                        strcmp(notif->level, "WARNING") &&
                        strcmp(notif->level, "INFO")) {
                        SET_ERROR(retval, "Invalid level '%s'.", notif->level);
                    }
                }
                else {
                    SET_ERROR(retval, "Failed to read notification level.");
                }
            }
        }

        if (filepath) {
            free(filepath);
            filepath = NULL;
        }
    }

    if (dr) {
        closedir(dr);
        dr = NULL;
    }

    // Validate config.
    if (IS_SUCCESS(retval)) {
        if (strlen(notif->filter) == 0) {
            SET_ERROR(retval, "Filter executable missing for notification defined at '%s'", notification_dir);
        }
        else if (strlen(notif->title) == 0) {
            SET_ERROR(retval, "Title missing for notification defined at '%s'", notification_dir);
        }
        else if (strlen(notif->desc) == 0) {
            SET_ERROR(retval, "Description missing for notification defined at '%s'", notification_dir);
        }
        else if (strlen(notif->level) == 0) {
            SET_ERROR(retval, "Level missing for notification defined at '%s'", notification_dir);
        }
    }

    if (notification_dir) {
        free(notification_dir);
        notification_dir = NULL;
    }

    if (IS_ERROR(retval) && notif) {
        free_notification(notif);
        notif = NULL;
    }

    return notif;
}

static int load_config_notifications(lm_context_t *ctx, const char *notifications_dir)
{
    int retval = LM_SUCCESS;

    DIR *dr = opendir(notifications_dir);
    if (dr) {
        struct dirent *de = NULL;

        while (IS_SUCCESS(retval) && (de = readdir(dr)) != NULL) {
            // Handle directories only.
            if (de->d_type != DT_DIR) {
                continue;
            }

            // Skip '.' and '..' directories.
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
                continue;
            }

            // Load the current notification config.
            if (ctx->num_notifications < DIM(ctx->notifications)) {
                ctx->notifications[ctx->num_notifications] = 
                    alloc_notification(notifications_dir, de->d_name);

                if (ctx->notifications[ctx->num_notifications]) {
                    ctx->num_notifications++;
                }
                else {
                    SET_ERROR(retval, "Failed to load notification '%s'.", de->d_name);
                }
            }
            else {
                SET_ERROR(retval, "Too many notifications defined.");
            }
        }

        closedir(dr);
    }
    else {
        SET_ERROR(retval, "Config directory '%s' not found.", notifications_dir);
    }

    return retval;
}

static void free_target(lm_target_t *target) {
    if (target) {
        if (target->name) free(target->name);
        if (target->send) free(target->send);

        memset(target, 0, sizeof(*target));
        free(target);
    }
}

static lm_target_t *alloc_target(const char *targets_dir, const char *name)
{
    int retval = LM_SUCCESS;
    lm_target_t *target = NULL;
    char *target_dir = NULL;
    struct dirent *de = NULL;
    DIR *dr = NULL;

    // Build the target directory path.
    if (IS_SUCCESS(retval)) {
        target_dir = join_path(targets_dir, name);
        if (!target_dir) {
            SET_ERROR(retval, "Failed to build target directory path.");
        }
    }

    // Alloc memory for target structure.
    if (IS_SUCCESS(retval)) {
        target = calloc(1, sizeof(*target));
        if (!target) {
            SET_ERROR(retval, "Failed to alloc memory for new target.");
        }
    }

    // Set target's name.
    if (IS_SUCCESS(retval)) {
        target->name = strdup(name);
        if (!target->name) {
            SET_ERROR(retval, "Failed to alloc memory for target name.");
        }
    }

    // Open the directory.
    if (IS_SUCCESS(retval)) {
        dr = opendir(target_dir);
        if (dr == NULL) {
            SET_ERROR(retval, "Config directory '%s' not found.", target_dir);
        }
    }

    // Loop through all files of the directory.
    while (IS_SUCCESS(retval) && (de = readdir(dr)) != NULL) {
        // Handle regular files only.
        if (de->d_type != DT_REG) {
            continue;
        }

        char *filepath = join_path(target_dir, de->d_name);
        if (!filepath) {
            SET_ERROR(retval, "Failed to build path.");
        }
        // Handle the send script.
        else if (strcmp(de->d_name, "send") == 0) {
            target->send = filepath;
            filepath = NULL;
            if (access(target->send, X_OK) != 0) {
                SET_ERROR(retval, "Target send '%s' not executable.", target->send);
            }
        }
        else if (strcmp(de->d_name, "debouncing") == 0) {
            char *debouncing_str = NULL;

            // Get the debouncing value.
            debouncing_str = file2str(filepath);
            if (debouncing_str) {
                terminate_str_at_first_eol(debouncing_str);
            }
            else {
                SET_ERROR(retval, "Failed to read target debouncing.");
            }

            // Convert the debouncing value.
            if (IS_SUCCESS(retval)) {
                char *end;
                target->debouncing = strtol(debouncing_str, &end, 10);
                if (end == debouncing_str
                    || *end != '\0'
                    || ((LONG_MIN == target->debouncing || LONG_MAX == target->debouncing) && ERANGE == errno)
                    || target->debouncing > INT_MAX
                    || target->debouncing < INT_MIN) {
                    SET_ERROR(retval, "Invalid debouncing value '%s' defined in %s.", debouncing_str, filepath);
                }
            }

            if (debouncing_str) {
                free(debouncing_str);
                debouncing_str = NULL;
            }
        }

        if (filepath) {
            free(filepath);
            filepath = NULL;
        }
    }

    if (dr) {
        closedir(dr);
    }

    // Validate config.
    if (IS_SUCCESS(retval)) {
        if (strlen(target->send) == 0) {
            SET_ERROR(retval, "Missing send executable for target defined at '%s'.", target_dir);
        }
    }

    if (target_dir) {
        free(target_dir);
        target_dir = NULL;
    }

    if (IS_ERROR(retval) && target) {
        free_target(target);
        target = NULL;
    }

    return target;
}

static int load_config_targets(lm_context_t *ctx, const char *targets_dir)
{
    int retval = LM_SUCCESS;

    DIR *dr = opendir(targets_dir);
    if (dr) {
        struct dirent *de = NULL;

        while (IS_SUCCESS(retval) && (de = readdir(dr)) != NULL) {
            // Handle directories only.
            if (de->d_type != DT_DIR) {
                continue;
            }

            // Skip '.' and '..' directories.
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
                continue;
            }

            // Load the current target config.
            if (ctx->num_targets < DIM(ctx->targets)) {
                ctx->targets[ctx->num_targets] = alloc_target(targets_dir, de->d_name);

                if (ctx->targets[ctx->num_targets]) {
                    ctx->num_targets++;
                }
                else {
                    SET_ERROR(retval, "Failed to load target '%s'.", de->d_name);
                }
            }
            else {
                SET_ERROR(retval, "Too many targets defined.");
            }
        }

        closedir(dr);
    }
    else {
        SET_ERROR(retval, "Config directory not found: %s.", targets_dir);
    }

    return retval;
}

static void destroy_context(lm_context_t *ctx)
{
    if (ctx) {
        if (ctx->config_dir) {
            free(ctx->config_dir);
            ctx->config_dir = NULL;
        }

        // Free notifications.
        FOR_EACH_NOTIFICATION(ctx, notif, nidx) {
            free_notification(notif);
            ctx->notifications[nidx] = NULL;
        }
        ctx->num_notifications = 0;

        // Free targets.
        FOR_EACH_TARGET(ctx, target, tidx) {
            free_target(target);
            ctx->targets[tidx] = NULL;
        }
        ctx->num_targets = 0;

        // Free monitored files.
        FOR_EACH_MONITORED_FILE(ctx, mf, mfidx) {
            if (mf->path) {
                free(mf->path);
                mf->path = NULL;
            }
            if (mf->pending_buf) {
                free(mf->pending_buf);
                mf->pending_buf = NULL;
            }
        }

        memset(ctx, 0, sizeof(*ctx));
        free(ctx);
        ctx = NULL;
    }
}

static lm_context_t *create_context(const char *cfgdir, char **monitored_files, unsigned short num_monitored_files)
{
    int retval = LM_SUCCESS;
    lm_context_t *ctx = NULL;

    // Alloc memory for context structure.
    if (IS_SUCCESS(retval)) {
        ctx = calloc(1, sizeof(*ctx));
        if (!ctx) {
            SET_ERROR(retval, "Failed to alloc memory for context.");
        }
    }

    // Set configuration directory.
    if (IS_SUCCESS(retval)) {
        ctx->config_dir = strdup(cfgdir);
        if (!ctx->config_dir) {
            SET_ERROR(retval, "Failed to set context configuration directory.");
        }
    }

    // Load notifications.
    if (IS_SUCCESS(retval)) {
        char *path = join_path(ctx->config_dir, "notifications.d");
        if (path) {
            retval = load_config_notifications(ctx, path);
            free(path);
        }
        else {
            SET_ERROR(retval, "Failed to build notifications directory path.");
        }
    }

    // Load targets.
    if (IS_SUCCESS(retval)) {
        char *path = join_path(ctx->config_dir, "targets.d");
        if (path) {
            retval = load_config_targets(ctx, path);
            free(path);
        }
        else {
            SET_ERROR(retval, "Failed to build targets directory path.");
        }
    }

    if (IS_SUCCESS(retval)) {
        if (num_monitored_files > DIM(ctx->monitored_files)) {
            SET_ERROR(retval, "Too many files to monitor.");
        }
    }

    // Set monitored files.
    if (IS_SUCCESS(retval)) {
        for (int i = 0; i < num_monitored_files; ++i) {
            const char *fpath = monitored_files[i];

            // Determine if it's a status file.
            if (fpath[0] == 's' && fpath[1] == ':') {
                fpath += 2;
                ctx->monitored_files[i].is_status = true;
            }
            else {
                ctx->monitored_files[i].is_status = false;
            }

            // Set path.
            ctx->monitored_files[i].path = strdup(fpath);
            if (!ctx->monitored_files[i].path) {
                SET_ERROR(retval, "Failed to alloc memory for monitored file path.");
                break;
            }

            // Init fd;
            ctx->monitored_files[i].fd = -1;
        }
        ctx->num_monitored_files = num_monitored_files;
    }

    if (IS_ERROR(retval)) {
        destroy_context(ctx);
        ctx = NULL;
    }

    return ctx;
}

static void usage(void)
{
    fprintf(stderr, "Usage: logmonitor [OPTIONS...] FILE [FILE...]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  FILE                    Path to the file(s) to be monitored. Prefix\n");
    fprintf(stderr, "                          with 's:' for status files.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -c, --configdir         Directory where configuration is stored (default: "DEFAULT_CONFIG_DIR").\n");
    fprintf(stderr, "  -d, --debug             Enable debug logging.\n");
    fprintf(stderr, "  -h, --help              Display this help and exit.\n");
}

int main(int argc, char **argv)
{
    int retval = LM_SUCCESS;
    lm_context_t *ctx = NULL;
    unsigned sleep_period = 1;
    const char *cfgdir = DEFAULT_CONFIG_DIR;

    char *tailbuf;
    int n;

    // Parse options.
    if (IS_SUCCESS(retval)) {
        while (n >= 0) {
            n = getopt_long(argc, argv, short_options, long_options, NULL);
            if (n < 0) {
                 continue;
            }
            switch(n) {
                case 'c':
                    cfgdir = optarg;
                    break;
                case 'd':
                    debug_enabled = true;
                    break;
                case 'h':
                case '?':
                    retval = LM_ERROR;
                    usage();
            }
        }
    }

    // Create context;
    if (IS_SUCCESS(retval)) {
        ctx = create_context(cfgdir, argv+optind, argc - optind);
        if (!ctx) {
            SET_ERROR(retval, "Context creation failed.");
        }
    }

    // Validate config.
    if (IS_SUCCESS(retval)) {
        if (ctx->num_monitored_files == 0) {
            SET_ERROR(retval, "At least of file to monitor must be specified.");
        }
        else if (ctx->num_notifications == 0) {
            SET_ERROR(retval, "No notification configured.");
        }
        else if (ctx->num_targets == 0) {
            SET_ERROR(retval, "No target configured.");
        }
    }

    // Open all files to be monitored.
    if (IS_SUCCESS(retval)) {
        FOR_EACH_MONITORED_FILE(ctx, mf, mfidx) {
            const char *filename = mf->path;
            int fd = open(filename, O_RDONLY);

            if (fd >= 0 && !mf->is_status) {
                lseek(fd, 0, SEEK_END);
            }

            mf->fd = fd;
        }
    }

    // Allocate the memory for the main buffer.
    if (IS_SUCCESS(retval)) {
        tailbuf = malloc(BUFSIZ + 1);
        if (tailbuf == NULL) {
            SET_ERROR(retval, "Buffer memory allocation failed.");
        }
    }

    // Tail the files.
    while (IS_SUCCESS(retval)) {
        int i = 0 ;
        sleep(sleep_period);

        do {
            int nread;
            const char *filename = ctx->monitored_files[i].path;
            int fd = ctx->monitored_files[i].fd;

            /* skip reading status file if we just read it */
            if (ctx->monitored_files[i].is_status) {
                if (time(NULL) - ctx->monitored_files[i].last_read < STATUS_FILE_READ_INTERVAL) {
                    continue;
                }
            }

            /* re-open the file if needed */
            {
                struct stat sbuf, fsbuf;

                if (fd < 0
                 || fstat(fd, &fsbuf) < 0
                 || stat(filename, &sbuf) < 0
                 || fsbuf.st_dev != sbuf.st_dev
                 || fsbuf.st_ino != sbuf.st_ino
                ) {
                    int new_fd;

                    if (fd >= 0)
                        close(fd);
                    new_fd = open(filename, O_RDONLY);
                    if (new_fd >= 0) {
                        DEBUG("%s has %s; following end of new file.",
                            filename, (fd < 0) ? "appeared" : "been replaced"
                        );
                    } else if (fd >= 0) {
                        DEBUG("%s has become inaccessible.", filename);
                    }
                    ctx->monitored_files[i].fd = fd = new_fd;
                }
            }
            if (fd < 0)
                 continue;

            /* make sure status files are read from the beginning */
            if (ctx->monitored_files[i].is_status) {
                lseek(fd, 0, SEEK_SET);
            }

            for (;;) {
                /* tail -f keeps following files even if they are truncated */
                struct stat sbuf;
                /* /proc files report zero st_size, don't lseek them */
                if (fstat(fd, &sbuf) == 0 && sbuf.st_size > 0) {
                    off_t current = lseek(fd, 0, SEEK_CUR);
                    if (sbuf.st_size < current)
                        lseek(fd, 0, SEEK_SET);
                }

                nread = tail_read(fd, tailbuf, BUFSIZ);
                if (nread <= 0)
                    break;
                tailbuf[nread] = '\0';
                handle_read(ctx, i, tailbuf);
            }

            ctx->monitored_files[i].last_read = time(NULL);
        } while (++i < ctx->num_monitored_files);
    } /* while (1) */

    if (tailbuf) {
        free(tailbuf);
    }

    destroy_context(ctx);
    ctx = NULL;

    return (IS_SUCCESS(retval)) ? EXIT_SUCCESS : EXIT_FAILURE;
}
