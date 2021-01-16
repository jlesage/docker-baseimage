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

#include "utils.h"
#include "log.h"
#include "CException.h"

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
 * The maximum number of parameter a service's run program can have.
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
#define log_debug(fmt, arg...) do { \
    if (g_ctx.debug) { \
        log_stdout("[%-*s] " fmt "\n", g_ctx.log_prefix_length, g_ctx.progname, ##arg); \
    } \
} while (0)

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
    int stdout_pipe[2];
    int stderr_pipe[2];
    pthread_t logger;
    bool logger_started;
} service_t;

/** Context definition. */
typedef struct {
    char progname[255 + 1];               /**< Our program name. */
    char services_root[255 + 1];          /**< Root directory of services. */
    int log_prefix_length;                /**< Length of log prefixes. */
    bool debug;                           /**< Whether or not debug is enabled. */
    unsigned int services_gracetime;      /**< Services gracetimes (msec). */

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
    .services = {},
    .exit_code = 0,
};

static const char* const short_options = "dhr:g:p:";
static struct option long_options[] = {
    { "debug", no_argument, NULL, 'd' },
    { "progname", required_argument, NULL, 'p' },
    { "root-directory", required_argument, NULL, 'r' },
    { "services-gracetime", required_argument, NULL, 'g' },
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
    do_shutdown = true;
    log("SIGTINT received, shutting down...");
}

/**
 * Handler of the TERM signal.
 *
 * @param[in] sig Signal received.
 */
static void sigterm(int sig)
{
    do_shutdown = true;
    log("SIGTERM received, shutting down...");
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
 * Get the monotonic time since some unspecified starting point.
 *
 * @return Monotonic time, in milliseconds.
 */
static unsigned long get_time()
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        abort();
    }
    return (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
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
    int service = *((int*)p);
    char prefix[512] = "";

    ASSERT_VALID_SERVICE_INDEX(service);

    pthread_detach(pthread_self());

    // Build the prefix.
    snprintf(prefix, sizeof(prefix), "[%-*s] ", g_ctx.log_prefix_length, SRV(service).name);

    // Start the logger.
    log_prefixer(prefix, SRV(service).stdout_pipe[0], SRV(service).stderr_pipe[0]);

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

    // Initialize service's structure.
    memset(&SRV(sid), 0, sizeof(SRV(sid)));
    SRV(sid).umask = SERVICE_DEFAULT_UMASK;
    SRV(sid).ready_timeout = SERVICE_DEFAULT_READY_TIMEOUT;
    SRV(sid).min_running_time = SERVICE_DEFAULT_MIN_RUNNING_TIME;

    // Check if this is a service group.
    SRV(sid).is_service_group = (access("run", F_OK) != 0);

    // Check if service is disabled.
    load_value_as_bool("disabled", &SRV(sid).disabled);

    // Return now if nothing else to load.
    if (SRV(sid).is_service_group || SRV(sid).disabled) {
        strcpy(SRV(sid).name, service);
        return sid;
    }

    // Make sure the run file is executable if it exists.
    if (access("run", X_OK) != 0) {
        ThrowMessage("run file not executable");
    }

    // Get service configuration.
    {
        SRV(sid).run_abs_path = realpath("run", NULL);
        if (!SRV(sid).run_abs_path) {
            ThrowMessageWithErrno("could not get realpath of run program");
        }
    }
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

    // Create pipe for service's stdout.
    if (pipe(SRV(sid).stdout_pipe) < 0) {
        ThrowMessageWithErrno("could not create stdout pipe");
    }

    // Create pipe for service's stderr.
    if (pipe(SRV(sid).stderr_pipe) < 0) {
        ThrowMessageWithErrno("could not create stderr pipe");
    }

    // Set the service name at the end, when all validation is done.
    strcpy(SRV(sid).name, service);

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

    ASSERT_VALID_SERVICE_INDEX(service);
    
    switch (p = fork()) {
        case (pid_t)-1:
        {
            // Fork failed.
            return 0;
        }
        case 0:
        {
            // Child.

            // Set stdout and stderr.
            dup2(SRV(service).stdout_pipe[1], STDOUT_FILENO);
            dup2(SRV(service).stderr_pipe[1], STDERR_FILENO);

            // Read sides are not needed.
            close(SRV(service).stdout_pipe[0]);
            close(SRV(service).stderr_pipe[0]);

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
            if (SRV(service).umask != 0) {
                umask(SRV(service).umask);
            }

            // Set GID.
            if (SRV(service).gid > 0) {
               if (setgid(SRV(service).gid) < 0) {
                   err(50, "setgid(%i)", SRV(service).gid);
               }
            }

            // Set SGIDs.
            if (SRV(service).sgid_list_size > 0) {
                if (setgroups(SRV(service).sgid_list_size, SRV(service).sgid_list) < 0) {
                    err(50, "setgroups");
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
    else if (SRV(service).pid == 1) {
        // PID of 1 is used by non-respawning services.  Reset the PID so the
        // service can be re-started.
        SRV(service).pid = 0;
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
    // Close file descriptors.
    // NOTE: This will cause the logger threads to terminate.
    FOR_EACH_SERVICE(sid) {
        if (SRV(sid).stdout_pipe[0]) close(SRV(sid).stdout_pipe[0]);
        if (SRV(sid).stdout_pipe[1]) close(SRV(sid).stdout_pipe[1]);
        if (SRV(sid).stderr_pipe[0]) close(SRV(sid).stderr_pipe[0]);
        if (SRV(sid).stderr_pipe[1]) close(SRV(sid).stderr_pipe[1]);
    }

    // Free memory.
    FOR_EACH_SERVICE(sid) {
        for (unsigned int i = 0; i < SRV(sid).param_list_size; i++) {
            free(SRV(sid).param_list[i]);
        }
        SRV(sid).param_list_size = 0;

        for (unsigned int i = 0; i < SRV(sid).environment_size; i++) {
            free(SRV(sid).environment[i]);
        }
        SRV(sid).environment_size = 0;

        if (SRV(sid).run_abs_path) {
            free(SRV(sid).run_abs_path);
            SRV(sid).run_abs_path = NULL;
        }
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
        if (do_shutdown) {
            break;
        }

        Try {
            // Start the logger.
            if (pthread_create(&SRV(sid).logger, NULL, service_logger, (void*)&sid) == 0) {
                SRV(sid).logger_started = true;
            }
            else {
                ThrowMessage("could not create the logger thread");
            }

            // Start service.
            start_service(sid);

            // Check if we need to wait for the service to terminate.
            if (SRV(sid).sync) {
                log_debug("waiting for service '%s' to terminate...", SRV(sid).name);
                if (waitpid(SRV(sid).pid, NULL, 0) < 0) {
                    if (errno == EINTR) {
                        if (do_shutdown) {
                            ExitTry();
                        }
                    }

                    ThrowMessageWithErrno("could not wait for termination of service '%s'",
                            SRV(sid).name);
                }
                SRV(sid).pid = 1;

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
                else if (do_shutdown) {
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
                    else if (do_shutdown) {
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
 * @param[in] shutdown_in_progress Whether or not a shutdown is in progress.
 */
static void handle_killed(pid_t killed, int status, bool shutdown_in_progress)
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
                log("service '%s' exited (with status %d).", SRV(sid).name, WEXITSTATUS(status));
            }
        }
        else if (WIFSIGNALED(status)) {
            log("service '%s' exited (got signal %s).", SRV(sid).name, signal_to_str(WTERMSIG(status)));
        }
        else {
            log("service '%s' exited.", SRV(sid).name);
        }

        // Update service table.
        SRV(sid).pid = 0;

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

        // Check what we need to do with the service.
        if (shutdown_in_progress) {
            // We are shutting down all services.  Nothing to do.
        }
        else if (SRV(sid).shutdown_on_terminate) {
            // Termination of the service should cause a shutdown.
            log("service '%s' exited, shutting down...", SRV(sid).name);
            do_shutdown = true;

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
        else if (SRV(sid).respawn) {
            // Service needs to be restarted.
            if (get_time() - SRV(sid).start_time < SERVICE_RESTART_DELAY) {
                msleep(SERVICE_RESTART_DELAY);
            }
            log("restarting service '%s'.", SRV(sid).name);
            start_service(sid);
        }
        else {
            // Nothing to do when a one-shot service terminates.
            SRV(sid).pid = 1;
        }
    }
}

/**
 * Reap child processes that have died.
 *
 * @param[in] shutdown_in_progress Whether or not a shutdown is in progress.
 * @param[in] period The minimum amount of time (in milliseconds) the function
 *                   should perform reaping.  A value of -1 means until all
 *                   child processes have been reaped.
 * @param[in] service When set, the function will perform reaping until the
 *                    specified service gets reaped.
 *
 * @return True if all children have been reaped, false otherwise.
 */
static bool child_handler(bool shutdown_in_progress, int period, int service)
{
    time_t now = get_time();
    pid_t killed;

    while (true) {
        int status;

        do {
            killed = waitpid(-1, &status, WNOHANG);
            handle_killed(killed, status, shutdown_in_progress);
        } while (killed && killed != (pid_t)-1);

        if (killed == (pid_t)-1) {
            // All processes have terminated.
            log("all services exited.");
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

        Try {
            stop_service(sid);
        }
        Catch (e) {
            log_err("failed to stop service '%s': %s", SRV(sid).name, e.mMessage);
        }

        if (child_handler(true, 250, sid)) {
            // All processes have terminated.
            return;
        }
    }

    if (child_handler(true, 0, -1)) {
        // All processes have terminated.
        return;
    }

    // Send a SIGTERM to everyone.
    log("sending SIGTERM to all processes...");
    kill(-1, SIGTERM);

    // Allow some time (default 5 seconds) for remaining processes to gracefully
    // terminate.
    if (child_handler(true, g_ctx.services_gracetime, -1)) {
        // All processes have terminated.
        return;
    }

    // Now force-kill everyone.
    log("sending SIGKILL to all processes...");
    kill(-1, SIGKILL);

    // Wait until we reap all processes.
    child_handler(true, -1, -1);
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
    printf("  -d, --debug                      Enable debug logging.\n");
    printf("  -p, --progname <NAME>            Override the name that will be displayed in log messages to NAME.\n");
    printf("  -r, --root-directory <DIR>       Set the root directory to DIR.  Default is " SERVICES_DEFAULT_ROOT ".\n");
    printf("  -g, --services-gracetime <VALUE> Set the amount of time (in msec) allowed to\n"
           "                                   services to gracefully terminate before sending\n"
           "                                   the KILL signal to everyone.  Default is %d msec.\n", SERVICES_DEFAULT_GRACETIME);
    printf("  -h, --help                       Display this help and exit.\n");
}

int main(int argc, char *argv[])
{
    CEXCEPTION_T e;

    bool perform_shutdown = false;
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

        // Now that all services are known, update the log prefix log.
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
        do_shutdown = true;
    }

    // Start the main loop.
    while (true) {
        // Handle shutdown request.
        if (do_shutdown) {
            perform_shutdown = true;
            // Break the main loop.
            break;
        }

        // Process killed services.
        if (child_handler(false, 0, -1)) {
            // All processes have terminated.  No need to shutdown services.
            perform_shutdown = false;
            // Break the main loop.
            break;
        }

        // Process services that needs to run at regular interval.
        FOR_EACH_SERVICE(sid) {
            if (SRV(sid).interval > 0) {
                if ((get_time() - SRV(sid).start_time) >= SRV(sid).interval * 1000) {
                    // Check if service still running.
                    if (SRV(sid).pid > 1 && kill(SRV(sid).pid, 0) == 0) {
                        log_err("service '%s' didn't terminate before its next interval.", SRV(sid).name);
                        SRV(sid).start_time = get_time();
                        continue;
                    }

                    // Restart the service.
                    Try {
                        stop_service(sid);
                        start_service(sid);
                    }
                    Catch (e) {
                        log_err("failed to start service '%s': %s", SRV(sid).name, e.mMessage);
                    }
                }
            }
        }

        // Pauses for 1 second.
        msleep(1000);
    }

    if (exit_status == 0 && g_ctx.exit_code != 0) {
        exit_status = g_ctx.exit_code;
    }

    // Shutdown all services.
    if (perform_shutdown) {
        cinit_shutdown();
    }

    // Unload services.
    unload_services();

    // Exit.
    cinit_exit(exit_status);

    ASSERT_UNREACHABLE_POINT();
}
