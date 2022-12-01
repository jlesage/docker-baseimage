#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <grp.h>
#include <dirent.h>
#include <assert.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <ctype.h>
#include <pty.h>

#include "utils.h"
#include "log.h"
#include "CException.h"

#if ATOMIC_BOOL_LOCK_FREE != 2
#error "Atomic bool type is not lock free."
#endif

/**
 * Our defaut program name.
 */
#ifndef DEFAULT_PROGRAM_NAME
#define DEFAULT_PROGRAM_NAME "cinit"
#endif

/**
 * The default root directory where services are defined.
 */ 
#ifndef SERVICES_DEFAULT_ROOT
#define SERVICES_DEFAULT_ROOT "/etc/services.d"
#endif

/**
 * The maximum number of supported services.
 */
#ifndef MAX_NUM_SERVICES
#define MAX_NUM_SERVICES 64
#endif

/**
 * Minimum log prefix length.
 */
#define MIN_LOG_PREFIX_LENGTH 12

/**
 * The maximum number of parameters a service's run program can have.
 */
#define MAX_NUM_SERVICE_RUN_PARAMS 32

/**
 * The maximum number of environment variables that can be defined for a
 * service.
 */
#define MAX_NUM_SERVICE_ENV_VARS 32

/**
 * The amount of time (in msec) allowed to services to gracefully terminate
 * before sending the KILL signal to everyone.
 */
#define SERVICES_DEFAULT_GRACETIME 5000

/**
 * Amount of time (in msec) to wait between service readiness checks.
 */
#define SERVICE_READINESS_CHECK_INTERVAL 250

/**
 * Minimum number of time (in msec) between restarts of a service.
 */
#define SERVICE_RESTART_DELAY 500

/**
 * Maximum time (in msec) to wait for a service to be ready.
 */
#define SERVICE_DEFAULT_READY_TIMEOUT 5000

/**
 * Minimum amount of time (in msec) a service must be running before considering
 * it as ready.
 */
#define SERVICE_DEFAULT_MIN_RUNNING_TIME 500

/**
 * Default UID to use for services.
 */
#define SERVICE_DEFAULT_UID 1000

/**
 * Default UID to use for services.
 */
#define SERVICE_DEFAULT_GID 1000

/**
 * Default service's umask value.
 */
#define SERVICE_DEFAULT_UMASK 0022

/**
 * The maximum number of supplementary groups a service can have.
 */
#define SERVICE_SGID_LIST_SIZE 32

#define FMT_LONG 41 /* enough space to hold -2^127 in decimal, plus \0 */

#define MEMBER_SIZE(t, f) (sizeof(((t*)0)->f))

#define FOR_EACH_SERVICE(s) for (int s = 0; s < DIM(g_ctx.services) && g_ctx.services[s].name[0] != '\0'; s++)

#define SRV(i) g_ctx.services[i]

#define SRV_ROOT() g_ctx.services_root

#define MAX(a, b) ((a)>=(b)?(a):(b))

#define ASSERT_LOG(a, ...) do { if (!(a)) { log_stdout("ASSERT: " __VA_ARGS__); log_stdout("\n"); assert(a); } } while(0)
#define ASSERT_VALID_SERVICE_NAME(service) assert(service != NULL && service[0] != '\0')
#define ASSERT_VALID_SERVICE_INDEX(sid) ASSERT_LOG(sid >= 0 && sid < DIM(g_ctx.services), "Invalid service ID %d.", sid)
#define ASSERT_UNREACHABLE_POINT() assert(!"Unreachable point reached.")

#define log(fmt, arg...) log_stdout("[%-*s] " fmt "\n", g_ctx.log_prefix_length, g_ctx.progname, ##arg)
#define log_err(fmt, arg...) log_stderr("[%-*s] ERROR: " fmt "\n", g_ctx.log_prefix_length, g_ctx.progname, ##arg)
#define log_fatal(fmt, arg...) log_stderr("[%-*s] FATAL: " fmt "\n", g_ctx.log_prefix_length, g_ctx.progname, ##arg)
#define log_debug(fmt, arg...) do { \
    if (g_ctx.debug) { \
        log_stdout("[%-*s] " fmt "\n", g_ctx.log_prefix_length, g_ctx.progname, ##arg); \
    } \
} while (0)

#define SHUTDOWN_REQUESTED() (do_shutdown == true)
#define REQUEST_SHUTDOWN() do { do_shutdown = true; } while(0);
#define BREAK_IF_SHUTDOWN_REQUESTED() if (SHUTDOWN_REQUESTED()) break

/** Definition of a service. */
typedef struct {
    char name[255 + 1];

    bool disabled;
    bool is_service_group;

    char *run_abs_path;
    char *param_list[MAX_NUM_SERVICE_RUN_PARAMS];
    size_t param_list_size;
    char *environment[MAX_NUM_SERVICE_ENV_VARS];
    size_t environment_size;
    uid_t uid;
    gid_t gid;
    gid_t sgid_list[SERVICE_SGID_LIST_SIZE];
    size_t sgid_list_size;
    mode_t umask;
    int priority;
    char working_directory[255 + 1];
    bool respawn;
    bool sync;
    bool ignore_failure;
    bool shutdown_on_terminate;
    unsigned int min_running_time;
    unsigned int ready_timeout;
    unsigned int interval;

    pid_t pid;
    unsigned long start_time;
#ifdef SINGLE_CHILD_STDOUT_STDERR_STREAM
    int output_fd;
#else
    int stdout_fd;
    int stderr_fd;
#endif
    pthread_t logger;
    atomic_bool logger_exit;
    bool logger_started;
} service_t;

/** Context definition. */
typedef struct {
    char progname[255 + 1];               /**< Our program name. */
    char services_root[255 + 1];          /**< Root directory of services. */
    int log_prefix_length;                /**< Length of log prefixes. */
    bool debug;                           /**< Whether or not debug is enabled. */
    unsigned int services_gracetime;      /**< Services gracetimes (msec). */

    uid_t default_srv_uid;                /**< Default UID of services. */
    gid_t default_srv_gid;                /**< Default UID of services. */
    gid_t default_srv_sgid_list[SERVICE_SGID_LIST_SIZE]; /**< Default supplementary group list of services. */
    size_t default_srv_sgid_list_size;    /**< Size of the default supplementary group list. */
    mode_t default_srv_umask;             /**< Default umask value of services. */

    service_t services[MAX_NUM_SERVICES]; /**< Table of services. */
    int start_order[MAX_NUM_SERVICES];    /**< Start order of services. */
    int exit_code;                        /**< Exit code to use when exiting. */
} context_t;

extern char **environ;

static volatile bool do_shutdown = false;

/* Declare the global context. */
static context_t g_ctx = {
    .progname = DEFAULT_PROGRAM_NAME,
    .services_root = SERVICES_DEFAULT_ROOT,
    .log_prefix_length = strlen(DEFAULT_PROGRAM_NAME),
    .debug = false,
    .services_gracetime = SERVICES_DEFAULT_GRACETIME,
    .default_srv_uid = SERVICE_DEFAULT_UID,
    .default_srv_gid = SERVICE_DEFAULT_GID,
    .default_srv_sgid_list = { 0 },
    .default_srv_sgid_list_size = 0,
    .default_srv_umask = SERVICE_DEFAULT_UMASK,
    .services = {},
    .exit_code = 0,
};

static const char* const short_options = "dhr:g:p:u:i:m:s:";
static struct option long_options[] = {
    { "debug", no_argument, NULL, 'd' },
    { "progname", required_argument, NULL, 'p' },
    { "root-directory", required_argument, NULL, 'r' },
    { "services-gracetime", required_argument, NULL, 'g' },
    { "default-service-uid", required_argument, NULL, 'u' },
    { "default-service-gid", required_argument, NULL, 'i' },
    { "default-service-sgid-list", required_argument, NULL, 's' },
    { "default-service-umask", required_argument, NULL, 'm' },
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

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
 * Handler of the CHLD signal.
 *
 * @param[in] sig Signal received.
 */
static void sigchild(int sig)
{
    // Nothing to do.
}

/**
 * Handler of the INT signal.
 *
 * @param[in] sig Signal received.
 */
static void sigint(int sig)
{
    log("SIGTINT received, shutting down...");
    REQUEST_SHUTDOWN();
}

/**
 * Handler of the TERM signal.
 *
 * @param[in] sig Signal received.
 */
static void sigterm(int sig)
{
    log("SIGTERM received, shutting down...");
    REQUEST_SHUTDOWN();
}

/**
 * Get string representation of a signal.
 *
 * @param[in] sig Signal number.
 *
 * @return String representation of the signal.
 */
static const char *signal_to_str(int sig)
{
    switch (sig) {
        case SIGHUP:    return "SIGHUP";
        case SIGINT:    return "SIGINT";
        case SIGQUIT:   return "SIGINT";
        case SIGILL:    return "SIGILL";
        case SIGTRAP:   return "SIGILL";
        case SIGABRT:   return "SIGABRT";
        case SIGBUS:    return "SIGBUS";
        case SIGFPE:    return "SIGFPE";
        case SIGKILL:   return "SIGKILL";
        case SIGUSR1:   return "SIGUSR1";
        case SIGSEGV:   return "SIGSEGV";
        case SIGUSR2:   return "SIGUSR2";
        case SIGPIPE:   return "SIGPIPE";
        case SIGALRM:   return "SIGALRM";
        case SIGTERM:   return "SIGTERM";
        case SIGSTKFLT: return "SIGSTKFLT";
        case SIGCHLD:   return "SIGCHLD";
        case SIGCONT:   return "SIGCONT";
        case SIGSTOP:   return "SIGSTOP";
        case SIGTSTP:   return "SIGTSTP";
        case SIGTTIN:   return "SIGTTIN";
        case SIGTTOU:   return "SIGTTOU";
        case SIGURG:    return "SIGURG";
        case SIGXCPU:   return "SIGXCPU";
        case SIGXFSZ:   return "SIGXFSZ";
        case SIGVTALRM: return "SIGVTALRM";
        case SIGPROF:   return "SIGPROF";
        case SIGWINCH:  return "SIGWINCH";
        case SIGPOLL:   return "SIGPOLL";
        case SIGPWR:    return "SIGPWR";
        default:        return "UNKNOWN";
    }
}

/**
 * Close a file descriptor if needed.
 *
 * @param fd Pointer to the file descriptor.
 */
static void close_fd(int *fd)
{
    if (fd && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

/**
 * Get the monotonic time since some unspecified starting point.
 *
 * @return Monotonic time, in milliseconds.
 */
static unsigned long get_time()
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
        return (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
    }
    else {
        // Check if we failed to get time because operation is not permitted.
        // This could happen for example on Raspberry Pi running with an old
        // version of libseccomp2 (e.g. with distros based on Debian 10).
        if (errno != EPERM) {
            log_fatal("Could not get time: %s.",
                    strerror(errno));
            abort();
        }
    }

    // Get time via /proc/uptime as a fallback.
    {
        double uptime;
        FILE *f = fopen("/proc/uptime", "r");
        if (f == NULL) {
            log_fatal("Could not get time: %s.", strerror(errno));
            abort();
        }

        if (fscanf(f, "%lf", &uptime) != 1) {
            log_fatal("Could not get time: parse error.");
            abort();
        }
        fclose(f);

        return uptime * 1000;
    }
}   

/**
 * Sleep for the specified amount of milliseonds.
 *
 * @param[in] msec Number of milliseconds to sleep.
 *
 * @return 0 on success, -1 on error.
 */
static int msleep(unsigned long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0) {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

/**
 * Check if a string ends with the provided suffix.
 *
 * @param[in] str Input string.
 * @param[in] suffix Suffix to check.
 *
 * @return true if string ends with suffix, else otherwise.
 */
static bool ends_with(const char *str, const char *suffix)
{
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    return (str_len >= suffix_len) &&
           (!memcmp(str + str_len - suffix_len, suffix, suffix_len));
}

/*
 * Execute a program of the service's directory.
 *
 * Output of the command will be silenced, unless debug is enabled.
 *
 * @param[in] service Service index.
 * @param[in] cmd Name of the program.
 * @param[in] cmd_path Path of the program.
 * @param[in] arg Argument od the program.
 *
 * @return Program's exit code or -1 on error.
 */
static int exec_service_cmd(int service, const char *cmd, const char *cmd_path, const char *arg)
{
    ASSERT_VALID_SERVICE_INDEX(service);

    if (g_ctx.debug) {
        char prefix[512];
        snprintf(prefix, sizeof(prefix), "[%-*s] %s: ", g_ctx.log_prefix_length, SRV(service).name, cmd);
        return exec_cmd(false, prefix, cmd_path, cmd, arg, NULL);
    }
    else {
        return exec_cmd(true, NULL, cmd_path, cmd, arg, NULL);
    }
}

/**
 * Service logger.
 *
 * This function is intented to be run into a thread.  It handles data written
 * to both stdin and stderr of a service, by prefixing the name of the service
 * to every lines.
 *
 * @param[in] p Parameter to be casted as a service index.
 *
 * @return NULL;
 */
static void *service_logger(void *p)
{
    int service = -1;
    char prefix[512] = "";

    // Service ID is passed as argument.
    ASSERT_LOG(p != NULL, "No argument passed to logger thread.");
    service = *((int*)p);
    ASSERT_VALID_SERVICE_INDEX(service);

    // Free argument.
    free(p);

    // Build the prefix.
    snprintf(prefix, sizeof(prefix), "[%-*s] ", g_ctx.log_prefix_length, SRV(service).name);

    // Start the logger.
#ifdef SINGLE_CHILD_STDOUT_STDERR_STREAM
    log_prefixer(prefix, SRV(service).output_fd, -1, &SRV(service).logger_exit);
#else
    log_prefixer(prefix, SRV(service).stdout_fd, SRV(service).stderr_fd, &SRV(service).logger_exit);
#endif

    return NULL;
}

/**
 * Add a service to the start order table.
 *
 * @param[in] service Service to be added.
 * @param[in] before_service Service should be started before this one.
 */
static void add_to_start_order(int service, int before_service)
{
    for (int i = 0; i < DIM(g_ctx.start_order); i++) {
        if (g_ctx.start_order[i] == -1) {
            // The "before service" not added yet.
            g_ctx.start_order[i] = service;
            break;
        }
        else if (g_ctx.start_order[i] == before_service) {
            // Move all services to the right by one position.
            for (int j = DIM(g_ctx.start_order) - 1; j > i; j--) {
                g_ctx.start_order[j] = g_ctx.start_order[j - 1];
            }
            // Insert the service.
            g_ctx.start_order[i] = service;
            break;
        }
    }
}

/**
 * Change the working directory to the service directory.
 *
 * @param[in] service Name of the service.
 */
static void chdir_to_service(const char *service)
{
    ASSERT_VALID_SERVICE_NAME(service);

    if (chdir(SRV_ROOT()) || chdir(service)) {
        switch (errno) {
            case ENOENT:
                ThrowMessage("service directory not found");
                break;
            case EACCES:
                ThrowMessage("permission denied on service directory");
                break;
            default:
                ThrowMessageWithErrno("could not access service directory");
        }
    }
}

/**
 * Get a new, unused, service index.
 *
 * @return The service index or -1 if maximum number of services reached.
 */ 
static int alloc_service_index()
{
    for (int sid = 0; sid <= DIM(g_ctx.services); ++sid) {
        if (strlen(SRV(sid).name) == 0) {
            return sid;
        }
    }
    return -1;
}

/**
 * Find the index of a service.
 *
 * @param[in] service Name of the service.
 *
 * @return Index of service in table or -1 if not found.
 */
static int find_service(const char *service)
{
    ASSERT_VALID_SERVICE_NAME(service);

    FOR_EACH_SERVICE(sid) {
        if (!strcmp(SRV(sid).name, service)) {
            return sid;
        }
    }
    return -1;
}

/**
 * Find the index of a service by PID.
 *
 * @param[in] pid PID of the service.
 *
 * @return Index of service in table or -1 if not found.
 */
static int find_service_by_pid(pid_t pid)
{
    FOR_EACH_SERVICE(sid) {
        if (SRV(sid).pid == pid) {
            return sid;
        }
    }
    return -1;
}

/**
 * Check if a service is started.
 *
 * @param[in] service Index of the service.
 *
 * @return True if service is started, false otherwise.
 */
static bool is_service_started(int service)
{
    ASSERT_VALID_SERVICE_INDEX(service);
    
    return (SRV(service).pid != 0);
}

/**
 * Unload a service from the service table.
 *
 * All allocated memory associated to the service is freed.
 *
 * @param[in] service Index of the service.
 */
static void unload_service(int service)
{
    ASSERT_VALID_SERVICE_INDEX(service);

    for (unsigned int i = 0; i < SRV(service).param_list_size; i++) {
        free(SRV(service).param_list[i]);
    }
    SRV(service).param_list_size = 0;

    for (unsigned int i = 0; i < SRV(service).environment_size; i++) {
        free(SRV(service).environment[i]);
    }
    SRV(service).environment_size = 0;

    if (SRV(service).run_abs_path) {
        free(SRV(service).run_abs_path);
        SRV(service).run_abs_path = NULL;
    }

    memset(&SRV(service), 0, sizeof(SRV(service)));
}

/**
 * Load a service in service table.
 *
 * @param[in] service Name of the service to be added.
 *
 * @return Index of the added service in table or -1 on error.
 */
static int load_service(const char *service)
{
    CEXCEPTION_T e;

    int sid = -1;

    ASSERT_VALID_SERVICE_NAME(service);

    // Check if the service already exists.
    sid = find_service(service);
    if (sid >= 0) {
        return sid;
    }

    // Validate length of the service name.
    if (strlen(service) >= MEMBER_SIZE(service_t, name)) {
        ThrowMessage("name too long");
    }

    // Change the working directory to the service directory.
    chdir_to_service(service);

    // Get a free index for the service.
    sid = alloc_service_index();
    if (sid < 0) {
        ThrowMessage("maximum number of services reached");
    }

    // Now fill the entry.
    Try {
        // Initialize service's structure.
        memset(&SRV(sid), 0, sizeof(SRV(sid)));
#ifdef SINGLE_CHILD_STDOUT_STDERR_STREAM
        SRV(sid).output_fd = -1;
#else
        SRV(sid).stdout_fd = -1;
        SRV(sid).stderr_fd = -1;
#endif
        SRV(sid).uid = g_ctx.default_srv_uid;
        SRV(sid).gid = g_ctx.default_srv_gid;
        memcpy(SRV(sid).sgid_list, g_ctx.default_srv_sgid_list, sizeof(SRV(sid).sgid_list));
        SRV(sid).sgid_list_size = g_ctx.default_srv_sgid_list_size;
        SRV(sid).umask = g_ctx.default_srv_umask;
        SRV(sid).ready_timeout = SERVICE_DEFAULT_READY_TIMEOUT;
        SRV(sid).min_running_time = SERVICE_DEFAULT_MIN_RUNNING_TIME;

        // Check if this is a service group.
        SRV(sid).is_service_group = (access("run", F_OK) != 0);

        // Check if service is disabled.
        load_value_as_bool("disabled", &SRV(sid).disabled);

        // Return now if nothing else to load.
        if (SRV(sid).is_service_group || SRV(sid).disabled) {
            strcpy(SRV(sid).name, service);
            ExitTry();
        }

        // Make sure the run file is executable.
        if (access("run", X_OK) != 0) {
            ThrowMessage("run file not executable");
        }

        // Save the absolute path of the run program.
        {
            SRV(sid).run_abs_path = realpath("run", NULL);
            if (!SRV(sid).run_abs_path) {
                ThrowMessageWithErrno("could not get realpath of run program");
            }
        }

        // Get service configuration settings.
        {
            char *buf = NULL;

            load_value_as_string("params", &buf, 0);
            if (buf) {
                size_t param_list_size;
                char **param_list = NULL;

                // Before spliting on line-endings, make sure there is no
                // Windows-style line-endings ('\r\n').
                remove_all_char(buf, '\r');

                param_list = split(trim(buf), '\n', &param_list_size, 0, 0);

                Try {
                    if (!param_list) {
                        ThrowMessage("out of memory");
                    }
                    else if (param_list_size > DIM(SRV(sid).param_list)) {
                        ThrowMessage("too much parameters");
                    }
                    for (int i = 0; i < param_list_size; i++) {
                        if (strlen(param_list[i]) > 0) {
                            SRV(sid).param_list[i] = strdup(param_list[i]);
                            if (SRV(sid).param_list[i] == NULL) {
                                ThrowMessage("out of memory");
                            }
                            SRV(sid).param_list_size++;
                        }
                    }
                    free(param_list);
                    free(buf);
                }
                Catch (e) {
                    if (param_list) {
                        free(param_list);
                    }
                    free(buf);
                    ThrowMessage("could not load 'params': %s", e.mMessage);
                }
            }
        }
        {
            char *buf = NULL;

            load_value_as_string("environment", &buf, 0);
            if (buf) {
                size_t environment_size;
                char **environment = NULL;

                // Before spliting on line-endings, make sure there is no
                // Windows-style line-endings ('\r\n').
                remove_all_char(buf, '\r');

                environment = split(trim(buf), '\n', &environment_size, 0, 0);

                Try {
                    if (!environment) {
                        ThrowMessage("out of memory");
                    }
                    else if (environment_size > DIM(SRV(sid).environment)) {
                        ThrowMessage("too much environment variables");
                    }
                    for (int i = 0; i < environment_size; i++) {
                        if (strlen(environment[i]) > 0) {
                            char *p = strchr(environment[i], '=');
                            if (!p) {
                                ThrowMessage("invalid environment variable format");
                            }
                            else if (isdigit(environment[i][0])) {
                                ThrowMessage("invalid environment variable name");
                            }
                            else {
                                for (unsigned int j = 0; ; j++) {
                                    char c = environment[i][j];
                                    if (c == '=') {
                                        break;
                                    }
                                    else if (!isalnum(c) && c != '_') {
                                        ThrowMessage("invalid environment variable name");
                                    }
                                }
                            }
                            SRV(sid).environment[i] = strdup(environment[i]);
                            if (SRV(sid).param_list[i] == NULL) {
                                ThrowMessage("out of memory");
                            }
                            SRV(sid).environment_size++;
                        }
                    }
                    free(environment);
                    free(buf);

                    // An empty file means no environment variables to pass.
                    if (SRV(sid).environment_size == 0) {
                        SRV(sid).environment[0] = NULL;
                        SRV(sid).environment_size = 1;
                    }
                }
                Catch (e) {
                    if (environment) {
                        free(environment);
                    }
                    free(buf);
                    ThrowMessage("could not load 'params': %s", e.mMessage);
                }
            }
        }
        load_value_as_uid("uid", &SRV(sid).uid);
        load_value_as_gid("gid", &SRV(sid).gid);
        {
            char *buf = NULL;

            load_value_as_string("sgid", &buf, 0);
            if (buf) {
                size_t sgid_list_size;
                char **sgid_list = NULL;

                // Before spliting on line-endings, make sure there is no
                // Windows-style line-endings ('\r\n').
                remove_all_char(buf, '\r');

                sgid_list = split(trim(buf), '\n', &sgid_list_size, 0, 0);

                Try {
                    if (!sgid_list) {
                        ThrowMessage("out of memory");
                    }
                    else if (sgid_list_size > DIM(SRV(sid).sgid_list)) {
                        ThrowMessage("too much supplementary groups");
                    }
                    for (int i = 0; i < sgid_list_size; i++) {
                        if (strlen(sgid_list[i]) > 0) {
                            string_to_gid(sgid_list[i], &SRV(sid).sgid_list[i]);
                            SRV(sid).sgid_list_size++;
                        }
                    }
                    free(sgid_list);
                    free(buf);
                }
                Catch (e) {
                    if (sgid_list) {
                        free(sgid_list);
                    }
                    free(buf);
                    ThrowMessage("could not load 'sgid': %s", e.mMessage);
                }
            }
        }
        load_value_as_mode("umask", &SRV(sid).umask);
        load_value_as_int("priority", &SRV(sid).priority);
        {
            char *ptr = SRV(sid).working_directory;
            load_value_as_string("workdir", &ptr, sizeof(SRV(sid).working_directory));
            terminate_at_first_eol(SRV(sid).working_directory);
        }
        load_value_as_bool("respawn", &SRV(sid).respawn);
        load_value_as_bool("sync", &SRV(sid).sync);
        load_value_as_bool("ignore_failure", &SRV(sid).ignore_failure);
        load_value_as_bool("shutdown_on_terminate", &SRV(sid).shutdown_on_terminate);
        load_value_as_uint("min_running_time", &SRV(sid).min_running_time);
        load_value_as_uint("ready_timeout", &SRV(sid).ready_timeout);
        load_value_as_interval("interval", &SRV(sid).interval);

        // Do some validations.
        if (SRV(sid).respawn && SRV(sid).sync) {
            ThrowMessage("'respawn' and 'sync' flags are exclusive");
        }
        else if (SRV(sid).respawn && SRV(sid).interval > 0) {
            ThrowMessage("interval cannot be used with respawned service");
        }

        // PID of 0 means service not running.
        SRV(sid).pid = 0;

        // Set the service name at the end, when all validation is done.
        strcpy(SRV(sid).name, service);
    }
    Catch (e) {
        unload_service(sid);
        ThrowMessage("%s", e.mMessage);
    }

    return sid;
}

/**
 * Fork and exec into a service.
 *
 * NOTE: Must be called from inside the service directory.
 *
 * @param[in] service Index of the service.
 *
 * @return PID of the service or 0 on error.
 */
static pid_t fork_and_exec(int service)
{
    pid_t p;
#ifdef SINGLE_CHILD_STDOUT_STDERR_STREAM
    int pty_master;
#else
    int pty_stdout[2] = { -1, -1 };
    int pty_stderr[2] = { -1, -1 };
#endif

    ASSERT_VALID_SERVICE_INDEX(service);

#ifndef SINGLE_CHILD_STDOUT_STDERR_STREAM
    // Create pseudo-terminals for stdout and stderr.  The stdout and stderr
    // of the child process will be connected to 2 different pseudo-terminals.
    // Pseudo-terminals are needed to disable buffering on the child side. Also,
    // 2 differents pseudo-terminals are needed because we want to be able to
    // differentiate the 2 streams (i.e. we want to tell is a message comes from
    // stdout or stderr).
    // https://reviews.llvm.org/D15073
    // https://github.com/microsoft/node-pty/issues/71
    // https://stackoverflow.com/questions/4057985
    if (openpty(&pty_stdout[0], &pty_stdout[1], NULL, NULL, NULL) < 0) {
        //ThrowMessageWithErrno("could not create pseudo-terminal for stdout";
        return 0;
    }
    if (openpty(&pty_stderr[0], &pty_stderr[1], NULL, NULL, NULL) < 0) {
        //ThrowMessageWithErrno("could not create pseudo-terminal for stderr";
        return 0;
        REQUEST_SHUTDOWN();
    }
#endif

#ifdef SINGLE_CHILD_STDOUT_STDERR_STREAM
    // Use forkpty() to disable buffering on child side.  forkpty() redirects
    // stdout and stderr into a single stream.
    switch (p = forkpty(&pty_master, NULL, NULL, NULL)) {
#else
    switch (p = fork()) {
#endif
        case (pid_t)-1:
        {
            // Fork failed.
#ifndef SINGLE_CHILD_STDOUT_STDERR_STREAM
            close_fd(&pty_stdout[0]);
            close_fd(&pty_stdout[1]);
            close_fd(&pty_stderr[0]);
            close_fd(&pty_stderr[1]);
#endif
            //ThrowMessageWithErrno("fork failed");
            return 0;
        }
        case 0:
        {
            // Child.

#ifndef SINGLE_CHILD_STDOUT_STDERR_STREAM
            // Map stdout and stderr of the child to the pseudo-terminals
            // slave file descriptors.
            dup2(pty_stdout[1], STDOUT_FILENO);
            dup2(pty_stderr[1], STDERR_FILENO);

            // Master file descriptors of pseudo-terminals are not used.
            close_fd(&pty_stdout[0]);
            close_fd(&pty_stderr[0]);
#endif

#if 0
            ioctl(0, TIOCNOTTY, 0);
            setsid();
            tcsetpgrp(0, getpgrp());
#else
            setpgrp();
#endif

            // Get the canonical, absolute path of the program to run.
            char *argv0 = SRV(service).run_abs_path;
            if (!argv0) {
                err(50, "realpath(\"run\")");
            }

            // Set the list of arguments.
            size_t argv_size = SRV(service).param_list_size + 2;
            char *argv[argv_size];

            for (unsigned int i = 0; i < argv_size; i++) {
                if (i == 0) {
                    // The first argument is the program name.
                    argv[i] = strrchr(argv0, '/');
                    if (argv[i]) {
                        argv[i]++;
                    }
                    else {
                        argv[i] = argv0;
                    }
                }
                else if (i == argv_size - 1) {
                    // The last argument should be NULL;
                    argv[i] = NULL;
                }
                else {
                    // Standard argument.
                    argv[i] = SRV(service).param_list[i - 1];
                }
            }

            // Set the environment.
            size_t environment_size = SRV(service).environment_size + 1;
            char *environment[environment_size];
            char **env_p = NULL;
            if (environment_size > 1) {
                for (unsigned int i = 0; i < environment_size; i++) {
                    environment[i] = SRV(service).environment[i];
                }
                environment[environment_size - 1] = NULL;
                env_p = environment;
            }
            else {
                env_p = environ;
            }

            // Set priority (niceness).
            if (SRV(service).priority != 0) {
                if (setpriority(0, PRIO_PROCESS, SRV(service).priority) < 0) {
                    err(50, "setpriority(%d)", SRV(service).priority);
                }
            }

            // Set umask.
            umask(SRV(service).umask);

            // Set SGIDs.
            if (setgroups(SRV(service).sgid_list_size, SRV(service).sgid_list) < 0) {
                err(50, "setgroups");
            }

            // Set GID.
            if (SRV(service).gid > 0) {
               if (setgid(SRV(service).gid) < 0) {
                   err(50, "setgid(%i)", SRV(service).gid);
               }
            }

            // Set UID.
            if (SRV(service).uid > 0) {
                if (setuid(SRV(service).uid) < 0) {
                    err(50, "setuid(%i)", SRV(service).uid);
                }
            }

            // Set working directory.
            if (strlen(SRV(service).working_directory) > 0) {
                if (chdir(SRV(service).working_directory)) {
                    err(50, "chdir(%s)", SRV(service).working_directory);
                }
            }

            // Execute.
            execve(argv0, argv, env_p);
            err(126, "execve(%s)", argv0);
        }
        default:
        {
            // Parent.
#ifdef SINGLE_CHILD_STDOUT_STDERR_STREAM
            // Keep the master file descriptor.
            SRV(service).output_fd = pty_master;
#else
            // Keep master file descriptors of pseudo-terminals.
            SRV(service).stdout_fd = pty_stdout[0];
            SRV(service).stderr_fd = pty_stderr[0];

            // Slave file descriptors of pseudo-terminals are not needed.  We
            // are not sending anything to child process.
            close_fd(&pty_stdout[1]);
            close_fd(&pty_stderr[1]);
#endif
            return p;
        }
    }
}

/**
 * Start a service.
 *
 * @param[in] service Index of the service.
 */
static void start_service(int service)
{
    ASSERT_VALID_SERVICE_INDEX(service);
    
    // Check if the process is already up.
    if (is_service_started(service)) {
        return;
    }

    if (SRV(service).interval > 0 || g_ctx.debug) {
        log_debug("starting service '%s'...", SRV(service).name);
    }
    else {
        log("starting service '%s'...", SRV(service).name);
    }

    // Change the working directory to the service directory.
    chdir_to_service(SRV(service).name);

    // Fork and exec service, put PID in data structure.
    for (int count = 0; count < 4; count++) {
        SRV(service).pid = fork_and_exec(service);
        if (SRV(service).pid > 0) {
            log_debug("started service '%s'.", SRV(service).name);
            SRV(service).start_time = get_time();

            // Service has been successfully started.  Now create its logger
            // thread.

            ASSERT_LOG(!SRV(service).logger_started,
                    "Logger thread already started for service '%s'.",
                    SRV(service).name);

            // Create and start the logger thread.
            int *service_arg = malloc(sizeof(int));
            *service_arg = service;
            atomic_store(&SRV(service).logger_exit, false);
            int rc = pthread_create(&SRV(service).logger, NULL, service_logger, service_arg);
            ASSERT_LOG(rc == 0, "Failed to create logger thread of service '%s': %s.",
                    SRV(service).name,
                    strerror(errno));

            SRV(service).logger_started = true;
            return;
        }
        msleep(500);
    }
    ThrowMessageWithErrno("could not fork");
}

/**
 * Stop a service.
 *
 * @param[in] service Index of the service.
 */
static void stop_service(int service)
{
    ASSERT_VALID_SERVICE_INDEX(service);
    
    if (SRV(service).pid == 0) {
        // Service not running.
        return;
    }

    log("stopping service '%s'...", SRV(service).name);

    // Change the working directory to the service directory.
    chdir_to_service(SRV(service).name);

    // Run the service's (optional) kill program.
    if (access("kill", X_OK) == 0) {
        char tmp[FMT_LONG];
        snprintf(tmp, sizeof(tmp), "%d", SRV(service).pid);
        exec_service_cmd(service, "./kill", "kill", tmp);
    }

    /* Send SIGTERM signal. */
    kill(SRV(service).pid, SIGTERM);
}

/**
 * Load a service and its dependencies.
 *
 * @param[in] service Name of the service to start.
 * @param[in] dependent Index of the dependent service, if any.
 */
static void load_service_with_deps(const char *service, int dependent)
{
    CEXCEPTION_T e;

    int sid = -1;

    ASSERT_VALID_SERVICE_NAME(service);

    // Check if service is already loaded.
    if (find_service(service) >= 0) {
        return;
    }

    // Load the service.
    log("loading service '%s'...", service);
    Try {
        sid = load_service(service);
    }
    Catch (e) {
        ThrowMessage("could not load service '%s': %s", service, e.mMessage);
    }

    // Return now if service disabled.
    if (SRV(sid).disabled) {
        log("service '%s' is disabled.", service);
        return;
    }

    // Update the start order.
    add_to_start_order(sid, dependent);

    // Load dependencies.
    {
        struct dirent *dir;
        DIR *dirstream = opendir(".");
        if (!dirstream) {
            ThrowMessageWithErrno("could not load service '%s': "
                                  "could not open stream for service directory",
                                  service);
        }

        while ((dir = readdir(dirstream)) != NULL) {
            bool depends;

            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }
            if (dir->d_type != DT_REG) {
                continue;
            }
            if (!ends_with(dir->d_name, ".dep")) {
                continue;
            }

            // Check the dependency state.
            load_value_as_bool(dir->d_name, &depends);
            if (!depends) {
                continue;
            }

            // Remove the '.dep' suffix.
            char *p = strrchr(dir->d_name, '.');
            assert(p);
            *p = '\0';

            // Load service.
            load_service_with_deps(dir->d_name, sid);
        }
        closedir(dirstream);
    }
}

/**
 * Load all services.
 */
#ifdef LOAD_ALL_DEFINED_SERVICES
static void load_services()
{
    DIR *dirstream = opendir(SRV_ROOT());
    if (!dirstream) {
        ThrowMessageWithErrno("could not open stream for root directory");
    }

    struct dirent *dir;
    while ((dir = readdir(dirstream)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }
        if (dir->d_type != DT_DIR) {
            continue;
        }
        load_service_with_deps(dir->d_name, -1);
    }

    closedir(dirstream);
}
#endif

/**
 * Unload all services.
 */
static void unload_services()
{
    FOR_EACH_SERVICE(sid) {
        unload_service(sid);
    }
}

/**
 * Start all services, in order.
 */
static void start_services()
{
    CEXCEPTION_T e;

    for (int i = 0; i < DIM(g_ctx.start_order); i++) {
        int sid = g_ctx.start_order[i];
        if (sid < 0) {
            break;
        }
        else if (SRV(sid).is_service_group) {
            continue;
        }

        // We may have received a shutdown request during the startup.
        BREAK_IF_SHUTDOWN_REQUESTED();

        Try {
            // Start service.
            start_service(sid);

            // Check if we need to wait for the service to terminate.
            if (SRV(sid).sync) {
                log_debug("waiting for service '%s' to terminate...", SRV(sid).name);
                while (waitpid(SRV(sid).pid, NULL, 0) < 0) {
                    if (errno == EINTR) {
                        if (SHUTDOWN_REQUESTED()) {
                            ExitTry();
                        }
                        continue;
                    }

                    ThrowMessageWithErrno("could not wait for termination of service '%s'",
                            SRV(sid).name);
                }
                SRV(sid).pid = 0;

                // Skip to next service.
                ExitTry();
            }

            // Check that the service ran for a minimum amount of time before
            // considering it as ready/up.
            while (true) {
                if (get_time() - SRV(sid).start_time >= SRV(sid).min_running_time) {
                    // Minimum uptime met.
                    break;
                }
                else if (kill(SRV(sid).pid, 0) != 0) {
                    // Service died.
                    ThrowMessage("minimum uptime not met");
                }
                else if (SHUTDOWN_REQUESTED()) {
                    ExitTry();
                }
                msleep(500);
            }

            // Change the working directory to the service directory.
            chdir_to_service(SRV(sid).name);

            // Wait for the service to be ready.
            if (access("is_ready", X_OK) == 0) {
                char arg[FMT_LONG];
                snprintf(arg, sizeof(arg), "%d", SRV(sid).pid);

                log_debug("waiting for service '%s' to be ready...", SRV(sid).name);
                while (true) {
                    if (get_time() - SRV(sid).start_time >= SRV(sid).ready_timeout) {
                        ThrowMessage("not ready after %d msec, giving up", SRV(sid).ready_timeout);
                    }
                    else if (kill(SRV(sid).pid, 0) != 0) {
                        // Service died, stop waiting.
                        ThrowMessage("terminated before being ready");
                    }
                    else if (exec_service_cmd(sid, "./is_ready", "is_ready", arg) == 0) {
                        // Service is ready, stop waiting.
                        break;
                    }
                    else if (SHUTDOWN_REQUESTED()) {
                        ExitTry();
                    }

                    msleep(SERVICE_READINESS_CHECK_INTERVAL);
                }
            }
        }
        Catch (e) {
            if (SRV(sid).ignore_failure) {
                log_err("service '%s' failed to be started: %s.", SRV(sid).name, e.mMessage);
            }
            else {
                ThrowMessage("service '%s' failed to be started: %s.", SRV(sid).name, e.mMessage);
            }
        }
    }
}

/**
 * Handle a killed service.
 *
 * @param[in] pid PID of the killed service.
 * @param[in] status Status information of the killed service.
 */
static void handle_killed(pid_t killed, int status)
{
    CEXCEPTION_T e;

    int sid;

    if (killed == (pid_t)-1) {
        // All processes terminated.
    }
    else if (killed == 0) {
        // No service killed.
        return;
    }
    else if ((sid = find_service_by_pid(killed)) >= 0) {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0 || SRV(sid).interval == 0 || g_ctx.debug) {
                log("service '%s' exited (with status %d).",
                        SRV(sid).name,
                        WEXITSTATUS(status));
            }
        }
        else if (WIFSIGNALED(status)) {
            log("service '%s' exited (got signal %s).",
                    SRV(sid).name,
                    signal_to_str(WTERMSIG(status)));
        }
        else {
            log("service '%s' exited.", SRV(sid).name);
        }

        // Update service table.
        SRV(sid).pid = 0;

        // Join the logger thread.
        log_debug("waiting termination of logger thread of service '%s'...",
                SRV(sid).name);
        atomic_store(&SRV(sid).logger_exit, true);
        int rc = pthread_join(SRV(sid).logger, NULL);
        ASSERT_LOG(rc == 0, "Failed to join logger thread of service '%s': %s.",
                SRV(sid).name, strerror(rc));
        log_debug("logger thread of service '%s' successfully terminated.",
                SRV(sid).name);

        // Close file descriptors.
#ifdef SINGLE_CHILD_STDOUT_STDERR_STREAM
        close_fd(&SRV(sid).output_fd);
#else
        close_fd(&SRV(sid).stdout_fd);
        close_fd(&SRV(sid).stderr_fd);
#endif

        SRV(sid).logger_started = false;

        // Run the service's finish script.
        Try {
            chdir_to_service(SRV(sid).name);

            if (access("finish", X_OK) == 0) {
                char arg[FMT_LONG] = "126";
                if (WIFEXITED(status)) {
                    snprintf(arg, sizeof(arg), "%d", WEXITSTATUS(status));
                }
                else if (WIFSIGNALED(status)) {
                    // https://tldp.org/LDP/abs/html/exitcodes.html
                    snprintf(arg, sizeof(arg), "%d", 128 + WTERMSIG(status));
                }
                exec_service_cmd(sid, "./finish", "finish", arg);
            }
        }
        Catch (e) {
            log_err("could execute finish script of service '%s': %s.",
                    SRV(sid).name, e.mMessage);
        }

        // Check if termination of this service should trigger a shutdown.
        if (!SHUTDOWN_REQUESTED() && SRV(sid).shutdown_on_terminate) {
            // Termination of the service should cause a shutdown.
            log("service '%s' exited, shutting down...", SRV(sid).name);
            REQUEST_SHUTDOWN();

            // We should exit with the same code as the service.
            if (WIFEXITED(status)) {
                g_ctx.exit_code = WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status)) {
                // https://tldp.org/LDP/abs/html/exitcodes.html
                g_ctx.exit_code = 128 + WTERMSIG(status);
            }
            else {
                g_ctx.exit_code = 1;
            }
        }
    }
}

/**
 * Reap child processes that have died.
 *
 * @param[in] period The minimum amount of time (in milliseconds) the function
 *                   should perform reaping.  A value of -1 means until all
 *                   child processes have been reaped.
 * @param[in] service When set, the function will perform reaping until the
 *                    specified service gets reaped.
 *
 * @return True if *all* children have been reaped, false otherwise.
 */
static bool child_handler(int period, int service)
{
    time_t now = get_time();
    pid_t killed;

    while (true) {
        int status;

        do {
            killed = waitpid(-1, &status, WNOHANG);
            handle_killed(killed, status);
        } while (killed && killed != (pid_t)-1);

        if (killed == (pid_t)-1) {
            // All processes have terminated.
            break;
        }

        // Check if it's time to stop because of the specified period.
        if (period == 0 || (period > 0 && get_time() - now >= period)) {
            break;
        }

        // Check if it't time to stop beause of the specified service.
        if (service >= 0 && killed != 0) {
            ASSERT_VALID_SERVICE_INDEX(service);
            if (SRV(service).pid == 0) {
                break;
            }
        }

        msleep(50);
    }

    return (killed == (pid_t)-1);
}

/**
 * Proceed with the container shutdown.
 *
 * The shutdown sequence consists in the following steps:
 *
 *   1) All services are stopped in reverse order.  We wait for a maximum of
 *      250msec before proceeding to the next service.
 *   2) If some processes are still alive, a TERM signal is sent to everyone.
 *   3) Processes are reaped for a maximum of 5 seconds.
 *   3) If some processes are still alive, a KILL signal is sent to everyone.
 *   4) Wait until all processes are reaped.
 */ 
static void cinit_shutdown()
{
    CEXCEPTION_T e;

    // Stop all services in reverse order.
    for (int i = DIM(g_ctx.start_order) - 1; i >= 0; i--) {
        int sid = g_ctx.start_order[i];
        if (sid < 0) {
            continue;
        }
        else if (SRV(sid).is_service_group) {
            continue;
        }
        else if (SRV(sid).pid == 0) {
            continue;
        }

        Try {
            stop_service(sid);
        }
        Catch (e) {
            log_err("failed to stop service '%s': %s", SRV(sid).name, e.mMessage);
        }

        if (child_handler(250, sid)) {
            // All processes have terminated.
            return;
        }
    }

    if (child_handler(0, -1)) {
        // All processes have terminated.
        return;
    }

    // Send a SIGTERM to everyone.
    log("sending SIGTERM to all processes...");
    kill(-1, SIGTERM);

    // Allow some time (default 5 seconds) for remaining processes to gracefully
    // terminate.
    if (child_handler(g_ctx.services_gracetime, -1)) {
        // All processes have terminated.
        return;
    }

    // Now force-kill everyone.
    log("sending SIGKILL to all processes...");
    kill(-1, SIGKILL);

    // Wait until we reap all processes.
    child_handler(-1, -1);
}

static void cinit_exit(int status)
{
    // Replace ourself with the exit script, if it exists.
    if (chdir(SRV_ROOT()) == 0 && access("exit", X_OK) == 0) {
        char arg[FMT_LONG];
        char *argv[] = { "exit", arg, NULL };
        snprintf(arg, sizeof(arg), "%d", status);
        execve("./exit", argv, environ);
    }
    _exit(status);
}

static void parse_args(int argc, char *argv[])
{
    CEXCEPTION_T e;

    int n = 0;
    while (n >= 0) {
        n = getopt_long(argc, argv, short_options, long_options, NULL);
        if (n < 0) {
             continue;
        }
        switch (n) {
            case 'd':
                g_ctx.debug = true;
                break;
              case 'p':
                if (strlen(optarg) >= sizeof(g_ctx.progname)) {
                    ThrowMessage("Programme namne too long.");
                }
                else {
                    strcpy(g_ctx.progname, optarg);
                }
                break;
            case 'r':
                if (strlen(optarg) >= sizeof(SRV_ROOT())) {
                    ThrowMessage("Root directory path too long.");
                }
                else if (optarg[0] != '/') {
                    ThrowMessage("Root directory path must be absolute.");
                }
                else {
                    strcpy(SRV_ROOT(), optarg);
                }
                break;
            case 'g':
                Try {
                    string_to_uint(optarg, &g_ctx.services_gracetime);
                }
                Catch (e) {
                    ThrowMessage("Invalid gracetime value '%s': %s.",
                            optarg, e.mMessage);
                }
                break;
            case 'u':
                Try {
                    string_to_uid(optarg, &g_ctx.default_srv_uid);
                }
                Catch (e) {
                    ThrowMessage("Invalid default service UID value '%s': %s.",
                            optarg, e.mMessage);
                }
                break;
            case 'i':
                Try {
                    string_to_gid(optarg, &g_ctx.default_srv_gid);
                }
                Catch (e) {
                    ThrowMessage("Invalid default service GID value '%s': %s.",
                            optarg, e.mMessage);
                }
                break;
            case 's':
            {
                size_t sgid_list_size;
                char **sgid_list = NULL;

                sgid_list = split(trim(optarg), ',', &sgid_list_size, 0, 0);

                Try {
                    if (!sgid_list) {
                        ThrowMessage("failed to process list");
                    }
                    else if (sgid_list_size > DIM(g_ctx.default_srv_sgid_list)) {
                        ThrowMessage("too much groups");
                    }
                    for (int i = 0; i < sgid_list_size; i++) {
                        if (strlen(sgid_list[i]) > 0) {
                            Try {
                                string_to_gid(sgid_list[i], &g_ctx.default_srv_sgid_list[i]);
                            }
                            Catch (e) {
                                ThrowMessage("invalid GID '%s': %s", sgid_list[i], e.mMessage);
                            }
                            g_ctx.default_srv_sgid_list_size++;
                        }
                    }
                    free(sgid_list);
                }
                Catch (e) {
                    if (sgid_list) {
                        free(sgid_list);
                    }
                    ThrowMessage("Invalid default service supplementary group list '%s': %s",
                            optarg, e.mMessage);
                }
                break;
            }
            case 'm':
                Try {
                    string_to_mode(optarg, &g_ctx.default_srv_umask);
                }
                Catch (e) {
                    ThrowMessage("Invalid default service umask value '%s': %s.",
                            optarg, e.mMessage);
                }
                break;
            case 'h':
            case '?':
                ThrowMessage("help");
        }
    }

    if (optind < argc) {
        ThrowMessage("Unexpected argument: '%s'.", argv[optind]);
    }
}

static void usage(const char *progname)
{
    printf("Usage: %s [OPTIONS...]\n", progname);
    printf("\n");
    printf("Options:\n");
    printf("  -d, --debug                            Enable debug logging.\n");
    printf("  -p, --progname <NAME>                  Override the name that will be displayed in log messages to NAME.\n");
    printf("  -r, --root-directory <DIR>             Set the root directory to DIR.  Default is " SERVICES_DEFAULT_ROOT ".\n");
    printf("  -g, --services-gracetime <VALUE>       Set the amount of time (in msec) allowed to\n"
           "                                         services to gracefully terminate before sending\n"
           "                                         the KILL signal to everyone.  Default is %d msec.\n", SERVICES_DEFAULT_GRACETIME);
    printf("  -u, --default-service-uid <VALUE>      UID to apply when not set in service's definition directory.\n");
    printf("                                         Default is %d.\n", SERVICE_DEFAULT_UID);
    printf("  -i, --default-service-gid <VALUE>      GID to apply when not set in service's definition directory.\n");
    printf("                                         Default is %d.\n", SERVICE_DEFAULT_GID);
    printf("  -s, --default-service-sgid-list <LIST> Comma-separated list of supplementary groups to apply when not\n");
    printf("                                         set in service's definition directory.  No group by default.\n");
    printf("  -m, --default-service-umask <VALUE>    Umask value (in octal notation) to apply when not set in service's\n");
    printf("                                         definition directory.  Default is 0022.\n");
    printf("  -h, --help                             Display this help and exit.\n");
}

int main(int argc, char *argv[])
{
    CEXCEPTION_T e;

    int exit_status = 0;

    // Get the program name.
    const char *progname = strrchr(argv[0], '/');
    if (progname) {
        progname++;
    }
    else {
        progname = argv[0];
    }
    snprintf(g_ctx.progname, sizeof(g_ctx.progname), "%s", progname);

    // Parse and validate arguments.
    Try {
        // Parse.
        parse_args(argc, argv);

        // Validate.
        if (chdir(SRV_ROOT()) < 0) {
            ThrowMessage("Root directory not found: %s", SRV_ROOT());
        }
    }
    Catch (e) {
        if (strcmp(e.mMessage, "help") == 0) {
            usage(g_ctx.progname);
        }
        else {
            printf("%s\n", e.mMessage);
        }
        return EXIT_FAILURE;
    }

    // Update the log prefix length.
    g_ctx.log_prefix_length = MAX(MIN_LOG_PREFIX_LENGTH, strlen(g_ctx.progname));

    // Initialize start order of services.
    for (int i = 0; i < DIM(g_ctx.start_order); ++i) {
        g_ctx.start_order[i] = -1;
    }

    // Setup signals.
    {
        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = 0;
        sa.sa_flags = 0;

        sa.sa_handler=sigint; sigaction(SIGINT, &sa, 0);
        sa.sa_handler=sigterm; sigaction(SIGTERM, &sa, 0);
        
        // SIGCHLD has different behavior (flags).
        sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
        sa.sa_handler = sigchild; sigaction(SIGCHLD, &sa, 0);
    }

    // Bring up services.
    Try {
        // Load services.
        log("loading services...");
#ifdef LOAD_ALL_DEFINED_SERVICES
        load_services();
#else
        load_service_with_deps("default", -1);
#endif
        log("all services loaded.");

        // Now that all services are known, update the log prefix length.
        FOR_EACH_SERVICE(sid) {
            if (SRV(sid).disabled || SRV(sid).is_service_group) {
                continue;
            }
            if (strlen(SRV(sid).name) > g_ctx.log_prefix_length) {
                g_ctx.log_prefix_length = strlen(SRV(sid).name);
            }
        }

        // Start services.
        log("starting services...");
        start_services();
        log("all services started.");
    }
    Catch (e) {
        log("%s", e.mMessage);
        exit_status = 1;
        REQUEST_SHUTDOWN();
    }

    // Start the main loop.
    while (true) {
        // Check if shutdown has been requested.  We may have received a
        // termination signal.
        BREAK_IF_SHUTDOWN_REQUESTED();

        // Process killed services.
        bool all_children_terminated = child_handler(0, -1);
 
        // Check if shutdown has been requested.  A terminated service may
        // have triggered a shutdown.
        BREAK_IF_SHUTDOWN_REQUESTED();

        // If all processes are terminated and no service is scheduled to be
        // restarted, trigger a shutdown.
        if (all_children_terminated) {
            bool services_to_be_restarted = false;

            FOR_EACH_SERVICE(sid) {
                if (SRV(sid).respawn && SRV(sid).pid == 0) {
                    services_to_be_restarted = true;
                    break;
                }
            }

            if (!services_to_be_restarted) {
                log("all services exited, shutting down..");
                REQUEST_SHUTDOWN();
                BREAK_IF_SHUTDOWN_REQUESTED();
            }
        }

        // Process services that need to run at regular interval.
        FOR_EACH_SERVICE(sid) {
            if (SRV(sid).interval > 0) {
                if ((get_time() - SRV(sid).start_time) >= SRV(sid).interval * 1000) {
                    // Check if service still running.
                    if (SRV(sid).pid > 0) {
                        log_err("service '%s' didn't terminate within "
                                "its defined interval of %d seconds.",
                                SRV(sid).name,
                                SRV(sid).interval);
                        SRV(sid).start_time = get_time();
                        continue;
                    }

                    // Start the service again.
                    Try {
                        start_service(sid);
                    }
                    Catch (e) {
                        log_err("failed to start service '%s': %s",
                                SRV(sid).name,
                                e.mMessage);
                    }
                }
            }
        }

        // Process services that needs to be restarted.
        FOR_EACH_SERVICE(sid) {
            if (SRV(sid).respawn && SRV(sid).pid == 0) {
                if (get_time() - SRV(sid).start_time > SERVICE_RESTART_DELAY) {
                    log("restarting service '%s'.", SRV(sid).name);
                    Try {
                        start_service(sid);
                    }
                    Catch (e) {
                        log_err("failed to restart service '%s': %s",
                                SRV(sid).name, e.mMessage);
                    }
                }
            }
        }

        // Pause for 1 second.
        msleep(1000);
    }

    if (exit_status == 0 && g_ctx.exit_code != 0) {
        exit_status = g_ctx.exit_code;
    }

    // Shutdown all services.
    ASSERT_LOG(SHUTDOWN_REQUESTED(), "Performing shutdown without request.");
    cinit_shutdown();

    // Unload services.
    unload_services();

    // Exit.
    cinit_exit(exit_status);

    ASSERT_UNREACHABLE_POINT();
}
