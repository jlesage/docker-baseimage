#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "libc_wraps.h"
#include "test_helpers.h"

#ifndef CINIT_TESTING
#define CINIT_TESTING
#endif
#include "cinit.c"

void setUp(void)
{
    wraps_reset();
    test_helpers_set_up();
    optind = 1;
    optarg = NULL;
    opterr = 0;
    do_shutdown = false;

    memset(&g_ctx, 0, sizeof(g_ctx));
    snprintf(g_ctx.progname, sizeof(g_ctx.progname), "%s", DEFAULT_PROGRAM_NAME);
    snprintf(g_ctx.services_root, sizeof(g_ctx.services_root), "%s", SERVICES_DEFAULT_ROOT);
    g_ctx.log_prefix_length = (int)strlen(DEFAULT_PROGRAM_NAME);
    g_ctx.services_gracetime = SERVICES_DEFAULT_GRACETIME;
    g_ctx.default_srv_ready_timeout = SERVICE_DEFAULT_READY_TIMEOUT;
    g_ctx.default_srv_uid = SERVICE_DEFAULT_UID;
    g_ctx.default_srv_gid = SERVICE_DEFAULT_GID;
    g_ctx.default_srv_umask = SERVICE_DEFAULT_UMASK;
    for (int i = 0; i < (int)DIM(g_ctx.start_order); i++) {
        g_ctx.start_order[i] = -1;
    }
}

void tearDown(void)
{
    unload_services();
    wraps_reset();
    test_helpers_tear_down();
}

static char *make_root(void)
{
    char *root = test_mkdtemp();
    TEST_ASSERT_TRUE(strlen(root) < sizeof(g_ctx.services_root));
    snprintf(g_ctx.services_root, sizeof(g_ctx.services_root), "%s", root);
    return root;
}

static char *svc_path(const char *root, const char *name, const char *file)
{
    char *dir = test_join(root, name);
    if (file == NULL) {
        return dir;
    }
    char *path = test_join(dir, file);
    free(dir);
    return path;
}

static void make_svc_dir(const char *root, const char *name)
{
    char *dir = svc_path(root, name, NULL);
    test_mkdir(dir);
    free(dir);
}

static void make_run(const char *root, const char *name)
{
    make_svc_dir(root, name);
    char *run = svc_path(root, name, "run");
    test_write_exec(run, "#!/bin/sh\nexit 0\n");
    free(run);
}

static void write_svc(const char *root, const char *name, const char *file, const char *contents)
{
    make_svc_dir(root, name);
    char *path = svc_path(root, name, file);
    test_write_file(path, contents);
    free(path);
}

static void parse2(char *arg1, char *arg2)
{
    char arg0[] = "cinit";
    char *argv[4];
    int argc = 1;
    argv[0] = arg0;
    if (arg1) {
        argv[argc++] = arg1;
    }
    if (arg2) {
        argv[argc++] = arg2;
    }
    argv[argc] = NULL;
    optind = 1;
    parse_args(argc, argv);
}

void test_ends_with(void)
{
    TEST_ASSERT_TRUE(ends_with("foo.dep", ".dep"));
    TEST_ASSERT_TRUE(ends_with(".dep", ".dep"));
    TEST_ASSERT_FALSE(ends_with("dep", ".dep"));
    TEST_ASSERT_FALSE(ends_with("foo.dep", ".txt"));
    TEST_ASSERT_TRUE(ends_with("foo", ""));
    TEST_ASSERT_FALSE(ends_with("", "x"));
}

void test_signal_to_str(void)
{
    TEST_ASSERT_EQUAL_STRING("SIGTERM", signal_to_str(SIGTERM));
    TEST_ASSERT_EQUAL_STRING("SIGINT", signal_to_str(SIGINT));
    TEST_ASSERT_EQUAL_STRING("SIGKILL", signal_to_str(SIGKILL));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", signal_to_str(9999));
}

void test_close_fd(void)
{
    close_fd(NULL);
    int fd = -1;
    close_fd(&fd);
    TEST_ASSERT_EQUAL(-1, fd);

    int p[2];
    TEST_ASSERT_EQUAL(0, pipe(p));
    close(p[1]);
    int rd = p[0];
    close_fd(&rd);
    TEST_ASSERT_EQUAL(-1, rd);
}

void test_service_table_lookup(void)
{
    TEST_ASSERT_EQUAL(0, alloc_service_index());
    strcpy(SRV(0).name, "app");
    SRV(0).pid = 42;
    strcpy(SRV(1).name, "log");
    SRV(1).pid = 43;

    TEST_ASSERT_EQUAL(0, find_service("app"));
    TEST_ASSERT_EQUAL(1, find_service("log"));
    TEST_ASSERT_EQUAL(-1, find_service("missing"));
    TEST_ASSERT_EQUAL(0, find_service_by_pid(42));
    TEST_ASSERT_EQUAL(-1, find_service_by_pid(99));
    TEST_ASSERT_TRUE(is_service_started(0));
    TEST_ASSERT_TRUE(is_service_started(1));
    SRV(1).pid = 0;
    TEST_ASSERT_FALSE(is_service_started(1));
    TEST_ASSERT_EQUAL(2, alloc_service_index());

    for (int i = 0; i < MAX_NUM_SERVICES; i++) {
        snprintf(SRV(i).name, sizeof(SRV(i).name), "s%d", i);
    }
    TEST_ASSERT_EQUAL(-1, alloc_service_index());
}

void test_add_to_start_order(void)
{
    add_to_start_order(1, -1);
    TEST_ASSERT_EQUAL(1, g_ctx.start_order[0]);
    add_to_start_order(2, -1);
    TEST_ASSERT_EQUAL(2, g_ctx.start_order[1]);
    add_to_start_order(0, 1);
    TEST_ASSERT_EQUAL(0, g_ctx.start_order[0]);
    TEST_ASSERT_EQUAL(1, g_ctx.start_order[1]);
    TEST_ASSERT_EQUAL(2, g_ctx.start_order[2]);
}

void test_parse_args_defaults_and_flags(void)
{
    char none[] = "cinit";
    char *argv[] = { none, NULL };
    optind = 1;
    TEST_ASSERT_NO_THROW(parse_args(1, argv));
    TEST_ASSERT_FALSE(g_ctx.debug);
    TEST_ASSERT_EQUAL_STRING(DEFAULT_PROGRAM_NAME, g_ctx.progname);
    TEST_ASSERT_EQUAL_STRING(SERVICES_DEFAULT_ROOT, g_ctx.services_root);

    char d[] = "-d";
    parse2(d, NULL);
    TEST_ASSERT_TRUE(g_ctx.debug);

    char p[] = "-p";
    char pname[] = "init";
    parse2(p, pname);
    TEST_ASSERT_EQUAL_STRING("init", g_ctx.progname);

    char r[] = "-r";
    char root[] = "/etc/services.d";
    parse2(r, root);
    TEST_ASSERT_EQUAL_STRING("/etc/services.d", g_ctx.services_root);

    char g[] = "-g";
    char gv[] = "1234";
    parse2(g, gv);
    TEST_ASSERT_EQUAL(1234, g_ctx.services_gracetime);

    char t[] = "-t";
    char tv[] = "50";
    parse2(t, tv);
    TEST_ASSERT_EQUAL(50, g_ctx.default_srv_ready_timeout);

    char u[] = "-u";
    char uv[] = "0";
    parse2(u, uv);
    TEST_ASSERT_EQUAL(0, g_ctx.default_srv_uid);

    char i[] = "-i";
    char iv[] = "0";
    parse2(i, iv);
    TEST_ASSERT_EQUAL(0, g_ctx.default_srv_gid);

    char s[] = "-s";
    char sv[] = "0,0";
    parse2(s, sv);
    TEST_ASSERT_EQUAL(2, g_ctx.default_srv_sgid_list_size);
    TEST_ASSERT_EQUAL(0, g_ctx.default_srv_sgid_list[0]);
    TEST_ASSERT_EQUAL(0, g_ctx.default_srv_sgid_list[1]);

    char m[] = "-m";
    char mv[] = "077";
    parse2(m, mv);
    TEST_ASSERT_EQUAL(077, g_ctx.default_srv_umask);
}

void test_parse_args_rejects_invalid(void)
{
    char r[] = "-r";
    char rel[] = "relative";
    TEST_ASSERT_THROWS("must be absolute", parse2(r, rel));

    char r2[] = "-r";
    char longpath[300];
    memset(longpath, 'a', sizeof(longpath));
    longpath[0] = '/';
    longpath[sizeof(longpath) - 1] = '\0';
    TEST_ASSERT_THROWS("too long", parse2(r2, longpath));

    char p[] = "-p";
    char longname[300];
    memset(longname, 'n', sizeof(longname) - 1);
    longname[sizeof(longname) - 1] = '\0';
    TEST_ASSERT_THROWS("too long", parse2(p, longname));

    char g[] = "-g";
    char badg[] = "nope";
    TEST_ASSERT_THROWS("Invalid gracetime", parse2(g, badg));

    char h[] = "-h";
    TEST_ASSERT_THROWS("help", parse2(h, NULL));

    char unk[] = "-z";
    TEST_ASSERT_THROWS("help", parse2(unk, NULL));

    char a0[] = "cinit";
    char extra[] = "leftover";
    char *argv[] = { a0, extra, NULL };
    optind = 1;
    TEST_ASSERT_THROWS("Unexpected argument", parse_args(2, argv));
}

void test_load_service_group_and_disabled(void)
{
    char *root = make_root();
    make_svc_dir(root, "group");
    int sid = -1;
    TEST_ASSERT_NO_THROW(sid = load_service("group"));
    TEST_ASSERT_TRUE(sid >= 0);
    TEST_ASSERT_TRUE(SRV(sid).is_service_group);
    TEST_ASSERT_EQUAL_STRING("group", SRV(sid).name);

    write_svc(root, "off", "disabled", "");
    make_run(root, "off");
    TEST_ASSERT_NO_THROW(sid = load_service("off"));
    TEST_ASSERT_TRUE(SRV(sid).disabled);
    TEST_ASSERT_EQUAL_STRING("off", SRV(sid).name);
}

void test_load_service_run_not_executable(void)
{
    char *root = make_root();
    make_svc_dir(root, "app");
    char *run = svc_path(root, "app", "run");
    test_write_file(run, "#!/bin/sh\nexit 0\n");
    free(run);
    TEST_ASSERT_THROWS("run file not executable", load_service("app"));
}

void test_load_service_params_env_and_defaults(void)
{
    char *root = make_root();
    make_run(root, "app");
    write_svc(root, "app", "params", "one\r\n\ntwo\n");
    write_svc(root, "app", "environment", "FOO=bar\nBADNAME\n");
    /* invalid env should throw */
    TEST_ASSERT_THROWS("invalid environment variable format", load_service("app"));

    /* Fresh table after failed load (unload in Catch). */
    write_svc(root, "app", "environment", "FOO=bar\n_BAZ=1\n");
    write_svc(root, "app", "environment_extra", "EXTRA=1\n");
    write_svc(root, "app", "uid", "0");
    write_svc(root, "app", "gid", "0");
    write_svc(root, "app", "sgid", "0\n");
    write_svc(root, "app", "umask", "077");
    write_svc(root, "app", "priority", "-5");
    write_svc(root, "app", "workdir", "/tmp\n");
    write_svc(root, "app", "min_running_time", "10");
    write_svc(root, "app", "ready_timeout", "1");
    write_svc(root, "app", "ignore_failure", "");

    int sid = -1;
    TEST_ASSERT_NO_THROW(sid = load_service("app"));
    TEST_ASSERT_EQUAL(2, SRV(sid).param_list_size);
    TEST_ASSERT_EQUAL_STRING("one", SRV(sid).param_list[0]);
    TEST_ASSERT_EQUAL_STRING("two", SRV(sid).param_list[1]);
    TEST_ASSERT_EQUAL(2, SRV(sid).environment_size);
    TEST_ASSERT_EQUAL_STRING("FOO=bar", SRV(sid).environment[0]);
    TEST_ASSERT_EQUAL(1, SRV(sid).environment_extra_size);
    TEST_ASSERT_EQUAL(0, SRV(sid).uid);
    TEST_ASSERT_EQUAL(0, SRV(sid).gid);
    TEST_ASSERT_EQUAL(1, SRV(sid).sgid_list_size);
    TEST_ASSERT_EQUAL(077, SRV(sid).umask);
    TEST_ASSERT_EQUAL(-5, SRV(sid).priority);
    TEST_ASSERT_EQUAL_STRING("/tmp", SRV(sid).working_directory);
    TEST_ASSERT_EQUAL(10, SRV(sid).min_running_time);
    TEST_ASSERT_TRUE(SRV(sid).ignore_failure);
    /* Service ready_timeout is raised to at least the default. */
    TEST_ASSERT_EQUAL(SERVICE_DEFAULT_READY_TIMEOUT, SRV(sid).ready_timeout);
    TEST_ASSERT_NOT_NULL(SRV(sid).run_abs_path);
    TEST_ASSERT_EQUAL(0, SRV(sid).pid);

    /* Already loaded is a no-op. */
    TEST_ASSERT_EQUAL(sid, load_service("app"));

    unload_service(sid);
    TEST_ASSERT_EQUAL_STRING("", SRV(sid).name);
    TEST_ASSERT_EQUAL(0, SRV(sid).param_list_size);
    TEST_ASSERT_NULL(SRV(sid).run_abs_path);
}

void test_load_service_empty_environment_file(void)
{
    char *root = make_root();
    make_run(root, "app");
    write_svc(root, "app", "environment", "");
    int sid = -1;
    TEST_ASSERT_NO_THROW(sid = load_service("app"));
    TEST_ASSERT_EQUAL(1, SRV(sid).environment_size);
    TEST_ASSERT_NULL(SRV(sid).environment[0]);
}

void test_load_service_exclusive_flags(void)
{
    char *root = make_root();
    make_run(root, "app");
    write_svc(root, "app", "respawn", "");
    write_svc(root, "app", "sync", "");
    TEST_ASSERT_THROWS("exclusive", load_service("app"));

    write_svc(root, "app2", "respawn", "");
    write_svc(root, "app2", "interval", "10");
    make_run(root, "app2");
    TEST_ASSERT_THROWS("interval cannot be used with respawned service", load_service("app2"));
}

void test_load_service_sgid_appends_defaults(void)
{
    g_ctx.default_srv_sgid_list[0] = 1;
    g_ctx.default_srv_sgid_list_size = 1;
    char *root = make_root();
    make_run(root, "app");
    write_svc(root, "app", "sgid", "0\n");
    int sid = -1;
    TEST_ASSERT_NO_THROW(sid = load_service("app"));
    TEST_ASSERT_EQUAL(2, SRV(sid).sgid_list_size);
    TEST_ASSERT_EQUAL(1, SRV(sid).sgid_list[0]);
    TEST_ASSERT_EQUAL(0, SRV(sid).sgid_list[1]);
}

void test_load_service_ready_timeout_uses_max_of_default(void)
{
    g_ctx.default_srv_ready_timeout = 50;
    char *root = make_root();
    make_run(root, "low");
    write_svc(root, "low", "ready_timeout", "10");
    int sid = -1;
    TEST_ASSERT_NO_THROW(sid = load_service("low"));
    TEST_ASSERT_EQUAL(50, SRV(sid).ready_timeout);

    make_run(root, "high");
    write_svc(root, "high", "ready_timeout", "80");
    TEST_ASSERT_NO_THROW(sid = load_service("high"));
    TEST_ASSERT_EQUAL(80, SRV(sid).ready_timeout);
}

void test_load_service_env_name_validation(void)
{
    char *root = make_root();
    make_run(root, "app");
    write_svc(root, "app", "environment", "1FOO=bar\n");
    TEST_ASSERT_THROWS("invalid environment variable name", load_service("app"));

    make_run(root, "app2");
    write_svc(root, "app2", "environment", "FOO-BAR=1\n");
    TEST_ASSERT_THROWS("invalid environment variable name", load_service("app2"));
}

void test_load_service_too_many_params(void)
{
    char *root = make_root();
    make_run(root, "app");
    char params[1024] = { 0 };
    for (int i = 0; i < MAX_NUM_SERVICE_RUN_PARAMS + 1; i++) {
        char line[16];
        snprintf(line, sizeof(line), "p%d\n", i);
        strcat(params, line);
    }
    write_svc(root, "app", "params", params);
    TEST_ASSERT_THROWS("too much parameters", load_service("app"));
}

void test_load_service_name_too_long(void)
{
    make_root();
    char name[300];
    memset(name, 'a', 256);
    name[256] = '\0';
    TEST_ASSERT_THROWS("name too long", load_service(name));
}

void test_load_service_with_deps(void)
{
    CEXCEPTION_T e;
    int threw = 0;
    char msg[256] = "";
    char *root = make_root();
    make_svc_dir(root, "default");
    write_svc(root, "default", "app.dep", "");
    write_svc(root, "default", "skipped.dep", "0");
    make_run(root, "app");
    make_run(root, "skipped");

    test_fd_capture_t cap_out, cap_err;
    test_fd_capture_start(&cap_out, STDOUT_FILENO);
    test_fd_capture_start(&cap_err, STDERR_FILENO);
    Try { load_service_with_deps("default", -1); load_service_with_deps("default", -1); }
    Catch (e) { threw = 1; snprintf(msg, sizeof(msg), "%s", e.mMessage); }
    free(test_fd_capture_stop(&cap_err));
    free(test_fd_capture_stop(&cap_out));
    TEST_ASSERT_FALSE_MESSAGE(threw, msg);

    TEST_ASSERT_TRUE(find_service("default") >= 0);
    TEST_ASSERT_TRUE(find_service("app") >= 0);
    TEST_ASSERT_EQUAL(-1, find_service("skipped"));
    TEST_ASSERT_EQUAL(find_service("app"), g_ctx.start_order[0]);
    TEST_ASSERT_EQUAL(find_service("default"), g_ctx.start_order[1]);
}

void test_load_service_with_deps_skips_disabled_in_start_order(void)
{
    CEXCEPTION_T e;
    int threw = 0;
    char msg[256] = "";
    char *root = make_root();
    make_run(root, "off");
    write_svc(root, "off", "disabled", "");

    test_fd_capture_t cap_out, cap_err;
    test_fd_capture_start(&cap_out, STDOUT_FILENO);
    test_fd_capture_start(&cap_err, STDERR_FILENO);
    Try { load_service_with_deps("off", -1); }
    Catch (e) { threw = 1; snprintf(msg, sizeof(msg), "%s", e.mMessage); }
    free(test_fd_capture_stop(&cap_err));
    free(test_fd_capture_stop(&cap_out));
    TEST_ASSERT_FALSE_MESSAGE(threw, msg);

    TEST_ASSERT_TRUE(find_service("off") >= 0);
    TEST_ASSERT_TRUE(SRV(find_service("off")).disabled);
    TEST_ASSERT_EQUAL(-1, g_ctx.start_order[0]);
}

void test_handle_killed(void)
{
    handle_killed(0, 0);
    handle_killed(99999, 0);

    char *root = make_root();
    make_svc_dir(root, "app");

    pid_t pid = fork();
    TEST_ASSERT_TRUE(pid >= 0);
    if (pid == 0) {
        _exit(42);
    }
    int status = 0;
    TEST_ASSERT_EQUAL(pid, waitpid(pid, &status, 0));

    strcpy(SRV(0).name, "app");
    SRV(0).pid = pid;
    SRV(0).logger_started = false;
#ifdef SINGLE_CHILD_STDOUT_STDERR_STREAM
    SRV(0).output_fd = -1;
#else
    SRV(0).stdout_fd = -1;
    SRV(0).stderr_fd = -1;
#endif
    SRV(0).shutdown_on_terminate = true;

    test_fd_capture_t cap_out, cap_err;
    test_fd_capture_start(&cap_out, STDOUT_FILENO);
    test_fd_capture_start(&cap_err, STDERR_FILENO);
    handle_killed(pid, status);
    free(test_fd_capture_stop(&cap_err));
    free(test_fd_capture_stop(&cap_out));

    TEST_ASSERT_EQUAL(0, SRV(0).pid);
    TEST_ASSERT_TRUE(do_shutdown);
    TEST_ASSERT_EQUAL(42, g_ctx.exit_code);
}

void test_handle_killed_restart_requested_does_not_shutdown(void)
{
    char *root = make_root();
    make_svc_dir(root, "app");

    pid_t pid = fork();
    TEST_ASSERT_TRUE(pid >= 0);
    if (pid == 0) {
        _exit(1);
    }
    int status = 0;
    TEST_ASSERT_EQUAL(pid, waitpid(pid, &status, 0));

    strcpy(SRV(0).name, "app");
    SRV(0).pid = pid;
    SRV(0).logger_started = false;
#ifdef SINGLE_CHILD_STDOUT_STDERR_STREAM
    SRV(0).output_fd = -1;
#else
    SRV(0).stdout_fd = -1;
    SRV(0).stderr_fd = -1;
#endif
    SRV(0).shutdown_on_terminate = true;
    SRV(0).restart_requested = true;

    test_fd_capture_t cap_out, cap_err;
    test_fd_capture_start(&cap_out, STDOUT_FILENO);
    test_fd_capture_start(&cap_err, STDERR_FILENO);
    handle_killed(pid, status);
    free(test_fd_capture_stop(&cap_err));
    free(test_fd_capture_stop(&cap_out));

    TEST_ASSERT_EQUAL(0, SRV(0).pid);
    TEST_ASSERT_FALSE(do_shutdown);
}

static pid_t forkpty_parent(int *amaster, char *name, const struct termios *termp, const struct winsize *winp)
{
    (void)name;
    (void)termp;
    (void)winp;
    int p[2];
    if (pipe(p) != 0) {
        return -1;
    }
    close(p[1]);
    if (amaster) {
        *amaster = p[0];
    }
    return 4242;
}

static pid_t forkpty_fail(int *amaster, char *name, const struct termios *termp, const struct winsize *winp)
{
    (void)amaster;
    (void)name;
    (void)termp;
    (void)winp;
    errno = EAGAIN;
    return -1;
}

static pid_t forkpty_as_child(int *amaster, char *name, const struct termios *termp, const struct winsize *winp)
{
    (void)name;
    (void)termp;
    (void)winp;
    if (amaster) {
        *amaster = -1;
    }
    return 0;
}

static pid_t waitpid_running(pid_t pid, int *status, int options)
{
    (void)pid;
    (void)options;
    if (status) {
        *status = 0;
    }
    return 0;
}

static pid_t waitpid_echild(pid_t pid, int *status, int options)
{
    (void)pid;
    (void)status;
    (void)options;
    errno = ECHILD;
    return -1;
}

static pid_t waitpid_running_until_kill_all(pid_t pid, int *status, int options)
{
    (void)pid;
    (void)options;
    /* stop_service SIGTERM, then kill(-1, SIGTERM), then kill(-1, SIGKILL). */
    if (mock_kill_fake.call_count >= 3) {
        errno = ECHILD;
        return -1;
    }
    if (status) {
        *status = 0;
    }
    return 0;
}

static pid_t waitpid_reap_once_then_echild(pid_t pid, int *status, int options)
{
    (void)pid;
    (void)options;
    if (mock_waitpid_fake.call_count <= 1) {
        if (status) {
            *status = 0x0100; /* WIFEXITED, exit code 1 */
        }
        return 4242;
    }
    errno = ECHILD;
    return -1;
}

static void silence_logs_start(test_fd_capture_t *out, test_fd_capture_t *err)
{
    test_fd_capture_start(out, STDOUT_FILENO);
    test_fd_capture_start(err, STDERR_FILENO);
}

static void silence_logs_stop(test_fd_capture_t *out, test_fd_capture_t *err)
{
    free(test_fd_capture_stop(err));
    free(test_fd_capture_stop(out));
}

static int load_named_service(const char *name)
{
    char *root = make_root();
    make_run(root, name);
    int sid = -1;
    TEST_ASSERT_NO_THROW(sid = load_service(name));
    TEST_ASSERT_TRUE(sid >= 0);
    return sid;
}

void test_fork_and_exec_parent_sets_pid_and_pty(void)
{
    int sid = load_named_service("app");
    wrap_use_mock_forkpty = 1;
    mock_forkpty_fake.custom_fake = forkpty_parent;

    pid_t pid = fork_and_exec(sid);
    TEST_ASSERT_EQUAL(4242, pid);
    TEST_ASSERT_TRUE(SRV(sid).output_fd >= 0);
    close_fd(&SRV(sid).output_fd);
}

void test_fork_and_exec_fork_failure_returns_zero(void)
{
    int sid = load_named_service("app");
    wrap_use_mock_forkpty = 1;
    mock_forkpty_fake.custom_fake = forkpty_fail;
    TEST_ASSERT_EQUAL(0, fork_and_exec(sid));
}

void test_fork_and_exec_child_builds_argv_and_privs(void)
{
    int sid = load_named_service("app");
    SRV(sid).param_list[0] = strdup("--flag");
    SRV(sid).param_list_size = 1;
    SRV(sid).uid = 1000;
    SRV(sid).gid = 1000;
    SRV(sid).priority = 5;
    SRV(sid).umask = 0077;

    wrap_use_mock_forkpty = 1;
    mock_forkpty_fake.custom_fake = forkpty_as_child;
    wrap_use_mock_creds = 1;
    wrap_use_mock_chdir = 1;
    mock_chdir_fake.return_val = 0;
    wrap_use_mock_execve = 1;
    wrap_execve_succeeds = 1;

    wrap_jmp_armed = 1;
    int jumped = setjmp(wrap_jmp);
    if (jumped == 0) {
        fork_and_exec(sid);
        wrap_jmp_armed = 0;
        TEST_FAIL_MESSAGE("child path did not exec");
    }
    wrap_jmp_armed = 0;

    TEST_ASSERT_EQUAL(2, jumped);
    TEST_ASSERT_NOT_NULL(strstr(wrap_execve_path, "/run"));
    TEST_ASSERT_TRUE(wrap_execve_argc >= 2);
    TEST_ASSERT_EQUAL_STRING("run", wrap_execve_argv[0]);
    TEST_ASSERT_EQUAL_STRING("--flag", wrap_execve_argv[1]);
    TEST_ASSERT_EQUAL(1, mock_setuid_fake.call_count);
    TEST_ASSERT_EQUAL(1000, mock_setuid_fake.arg0_val);
    TEST_ASSERT_EQUAL(1, mock_setgid_fake.call_count);
    TEST_ASSERT_EQUAL(1000, mock_setgid_fake.arg0_val);
    TEST_ASSERT_EQUAL(1, mock_setpriority_fake.call_count);
    TEST_ASSERT_EQUAL(5, mock_setpriority_fake.arg2_val);
    TEST_ASSERT_EQUAL(1, mock_umask_fake.call_count);
    TEST_ASSERT_EQUAL(0077, mock_umask_fake.arg0_val);
}

void test_start_service_noop_if_running(void)
{
    strcpy(SRV(0).name, "app");
    SRV(0).pid = 99;
    wrap_use_mock_forkpty = 1;
    mock_forkpty_fake.custom_fake = forkpty_parent;
    start_service(0);
    TEST_ASSERT_EQUAL(99, SRV(0).pid);
    TEST_ASSERT_EQUAL(0, mock_forkpty_fake.call_count);
}

void test_start_service_starts_logger(void)
{
    int sid = load_named_service("app");
    wrap_use_mock_forkpty = 1;
    mock_forkpty_fake.custom_fake = forkpty_parent;
    wrap_use_mock_pthread = 1;
    wrap_use_mock_time = 1;
    wrap_mono_msec = 1234;

    test_fd_capture_t cap_out, cap_err;
    silence_logs_start(&cap_out, &cap_err);
    TEST_ASSERT_NO_THROW(start_service(sid));
    silence_logs_stop(&cap_out, &cap_err);

    TEST_ASSERT_EQUAL(4242, SRV(sid).pid);
    TEST_ASSERT_TRUE(SRV(sid).logger_started);
    TEST_ASSERT_EQUAL(1, wrap_pthread_create_count);
    TEST_ASSERT_EQUAL(1234, SRV(sid).start_time);
    close_fd(&SRV(sid).output_fd);
}

void test_start_service_throws_after_fork_retries(void)
{
    int sid = load_named_service("app");
    wrap_use_mock_forkpty = 1;
    mock_forkpty_fake.custom_fake = forkpty_fail;
    wrap_use_mock_time = 1;

    test_fd_capture_t cap_out, cap_err;
    silence_logs_start(&cap_out, &cap_err);
    TEST_ASSERT_THROWS("could not fork", start_service(sid));
    silence_logs_stop(&cap_out, &cap_err);
    TEST_ASSERT_EQUAL(4, mock_forkpty_fake.call_count);
}

void test_stop_service_sends_sigterm(void)
{
    char *root = make_root();
    make_run(root, "app");
    int sid = -1;
    TEST_ASSERT_NO_THROW(sid = load_service("app"));
    SRV(sid).pid = 4242;
    wrap_use_mock_kill = 1;
    mock_kill_fake.return_val = 0;

    test_fd_capture_t cap_out, cap_err;
    silence_logs_start(&cap_out, &cap_err);
    TEST_ASSERT_NO_THROW(stop_service(sid));
    silence_logs_stop(&cap_out, &cap_err);
    TEST_ASSERT_EQUAL(1, mock_kill_fake.call_count);
    TEST_ASSERT_EQUAL(4242, mock_kill_fake.arg0_val);
    TEST_ASSERT_EQUAL(SIGTERM, mock_kill_fake.arg1_val);
}

void test_stop_service_noop_if_not_running(void)
{
    strcpy(SRV(0).name, "app");
    SRV(0).pid = 0;
    wrap_use_mock_kill = 1;
    stop_service(0);
    TEST_ASSERT_EQUAL(0, mock_kill_fake.call_count);
}

void test_child_handler_all_reaped(void)
{
    wrap_use_mock_waitpid = 1;
    mock_waitpid_fake.custom_fake = waitpid_echild;
    TEST_ASSERT_TRUE(child_handler(0, -1));
}

void test_child_handler_period_zero_still_running(void)
{
    wrap_use_mock_waitpid = 1;
    mock_waitpid_fake.custom_fake = waitpid_running;
    TEST_ASSERT_FALSE(child_handler(0, -1));
}

void test_cinit_shutdown_reaps_without_kill_all(void)
{
    char *root = make_root();
    make_run(root, "app");
    int sid = -1;
    TEST_ASSERT_NO_THROW(sid = load_service("app"));
    SRV(sid).pid = 4242;
    add_to_start_order(sid, -1);

    wrap_use_mock_kill = 1;
    mock_kill_fake.return_val = 0;
    wrap_use_mock_waitpid = 1;
    mock_waitpid_fake.custom_fake = waitpid_reap_once_then_echild;
    wrap_use_mock_time = 1;

    test_fd_capture_t cap_out, cap_err;
    silence_logs_start(&cap_out, &cap_err);
    cinit_shutdown();
    silence_logs_stop(&cap_out, &cap_err);

    TEST_ASSERT_EQUAL(0, SRV(sid).pid);
    TEST_ASSERT_EQUAL(1, mock_kill_fake.call_count);
    TEST_ASSERT_EQUAL(4242, mock_kill_fake.arg0_val);
    TEST_ASSERT_EQUAL(SIGTERM, mock_kill_fake.arg1_val);
}

void test_cinit_shutdown_broadcasts_if_children_remain(void)
{
    char *root = make_root();
    make_run(root, "app");
    int sid = -1;
    TEST_ASSERT_NO_THROW(sid = load_service("app"));
    SRV(sid).pid = 4242;
    add_to_start_order(sid, -1);
    g_ctx.services_gracetime = 10;

    wrap_use_mock_kill = 1;
    mock_kill_fake.return_val = 0;
    wrap_use_mock_waitpid = 1;
    mock_waitpid_fake.custom_fake = waitpid_running_until_kill_all;
    wrap_use_mock_time = 1;

    test_fd_capture_t cap_out, cap_err;
    silence_logs_start(&cap_out, &cap_err);
    cinit_shutdown();
    silence_logs_stop(&cap_out, &cap_err);

    TEST_ASSERT_TRUE(mock_kill_fake.call_count >= 3);
    TEST_ASSERT_EQUAL(-1, mock_kill_fake.arg0_history[mock_kill_fake.call_count - 1]);
    TEST_ASSERT_EQUAL(SIGKILL, mock_kill_fake.arg1_history[mock_kill_fake.call_count - 1]);
}

void test_start_services_waits_min_uptime(void)
{
    int sid = load_named_service("app");
    add_to_start_order(sid, -1);
    SRV(sid).min_running_time = 500;

    wrap_use_mock_forkpty = 1;
    mock_forkpty_fake.custom_fake = forkpty_parent;
    wrap_use_mock_pthread = 1;
    wrap_use_mock_time = 1;
    wrap_use_mock_waitpid = 1;
    mock_waitpid_fake.custom_fake = waitpid_running;

    test_fd_capture_t cap_out, cap_err;
    silence_logs_start(&cap_out, &cap_err);
    TEST_ASSERT_NO_THROW(start_services());
    silence_logs_stop(&cap_out, &cap_err);

    TEST_ASSERT_EQUAL(4242, SRV(sid).pid);
    TEST_ASSERT_TRUE(SRV(sid).logger_started);
    close_fd(&SRV(sid).output_fd);
}

void test_start_services_sync_nonzero_status_throws(void)
{
    int sid = load_named_service("app");
    add_to_start_order(sid, -1);
    SRV(sid).sync = true;

    wrap_use_mock_forkpty = 1;
    mock_forkpty_fake.custom_fake = forkpty_parent;
    wrap_use_mock_pthread = 1;
    wrap_use_mock_time = 1;
    wrap_use_mock_waitpid = 1;
    mock_waitpid_fake.custom_fake = waitpid_reap_once_then_echild;

    test_fd_capture_t cap_out, cap_err;
    silence_logs_start(&cap_out, &cap_err);
    TEST_ASSERT_THROWS("failed to be started", start_services());
    silence_logs_stop(&cap_out, &cap_err);
}

void test_start_services_ignore_failure(void)
{
    int sid = load_named_service("app");
    add_to_start_order(sid, -1);
    SRV(sid).sync = true;
    SRV(sid).ignore_failure = true;

    wrap_use_mock_forkpty = 1;
    mock_forkpty_fake.custom_fake = forkpty_parent;
    wrap_use_mock_pthread = 1;
    wrap_use_mock_time = 1;
    wrap_use_mock_waitpid = 1;
    mock_waitpid_fake.custom_fake = waitpid_reap_once_then_echild;

    test_fd_capture_t cap_out, cap_err;
    silence_logs_start(&cap_out, &cap_err);
    TEST_ASSERT_NO_THROW(start_services());
    silence_logs_stop(&cap_out, &cap_err);
}

void test_cinit_exit_without_script(void)
{
    make_root();
    wrap_jmp_armed = 1;
    int jumped = setjmp(wrap_jmp);
    if (jumped == 0) {
        cinit_exit(7);
        wrap_jmp_armed = 0;
        TEST_FAIL_MESSAGE("cinit_exit did not _exit");
    }
    wrap_jmp_armed = 0;
    TEST_ASSERT_EQUAL(7, wrap_exit_status);
    TEST_ASSERT_EQUAL(0, wrap_execve_argc);
}

void test_cinit_exit_execs_script(void)
{
    char *root = make_root();
    char *exit_path = test_join(root, "exit");
    test_write_exec(exit_path, "#!/bin/sh\nexit 0\n");
    free(exit_path);

    wrap_use_mock_execve = 1;
    wrap_execve_succeeds = 1;
    wrap_jmp_armed = 1;
    int jumped = setjmp(wrap_jmp);
    if (jumped == 0) {
        cinit_exit(9);
        wrap_jmp_armed = 0;
        TEST_FAIL_MESSAGE("cinit_exit did not exec");
    }
    wrap_jmp_armed = 0;
    TEST_ASSERT_EQUAL(2, jumped);
    TEST_ASSERT_EQUAL_STRING("./exit", wrap_execve_path);
    TEST_ASSERT_EQUAL_STRING("exit", wrap_execve_argv[0]);
    TEST_ASSERT_EQUAL_STRING("9", wrap_execve_argv[1]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ends_with);
    RUN_TEST(test_signal_to_str);
    RUN_TEST(test_close_fd);
    RUN_TEST(test_service_table_lookup);
    RUN_TEST(test_add_to_start_order);
    RUN_TEST(test_parse_args_defaults_and_flags);
    RUN_TEST(test_parse_args_rejects_invalid);
    RUN_TEST(test_load_service_group_and_disabled);
    RUN_TEST(test_load_service_run_not_executable);
    RUN_TEST(test_load_service_params_env_and_defaults);
    RUN_TEST(test_load_service_empty_environment_file);
    RUN_TEST(test_load_service_exclusive_flags);
    RUN_TEST(test_load_service_sgid_appends_defaults);
    RUN_TEST(test_load_service_ready_timeout_uses_max_of_default);
    RUN_TEST(test_load_service_env_name_validation);
    RUN_TEST(test_load_service_too_many_params);
    RUN_TEST(test_load_service_name_too_long);
    RUN_TEST(test_load_service_with_deps);
    RUN_TEST(test_load_service_with_deps_skips_disabled_in_start_order);
    RUN_TEST(test_handle_killed);
    RUN_TEST(test_handle_killed_restart_requested_does_not_shutdown);
    RUN_TEST(test_fork_and_exec_parent_sets_pid_and_pty);
    RUN_TEST(test_fork_and_exec_fork_failure_returns_zero);
    RUN_TEST(test_fork_and_exec_child_builds_argv_and_privs);
    RUN_TEST(test_start_service_noop_if_running);
    RUN_TEST(test_start_service_starts_logger);
    RUN_TEST(test_start_service_throws_after_fork_retries);
    RUN_TEST(test_stop_service_sends_sigterm);
    RUN_TEST(test_stop_service_noop_if_not_running);
    RUN_TEST(test_child_handler_all_reaped);
    RUN_TEST(test_child_handler_period_zero_still_running);
    RUN_TEST(test_cinit_shutdown_reaps_without_kill_all);
    RUN_TEST(test_cinit_shutdown_broadcasts_if_children_remain);
    RUN_TEST(test_start_services_waits_min_uptime);
    RUN_TEST(test_start_services_sync_nonzero_status_throws);
    RUN_TEST(test_start_services_ignore_failure);
    RUN_TEST(test_cinit_exit_without_script);
    RUN_TEST(test_cinit_exit_execs_script);
    return UNITY_END();
}
