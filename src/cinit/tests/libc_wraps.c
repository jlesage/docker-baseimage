#include "libc_wraps.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

DEFINE_FFF_GLOBALS;

DEFINE_FAKE_VALUE_FUNC(pid_t, mock_forkpty, int *, char *, const struct termios *, const struct winsize *);
DEFINE_FAKE_VALUE_FUNC(pid_t, mock_waitpid, pid_t, int *, int);
DEFINE_FAKE_VALUE_FUNC(int, mock_kill, pid_t, int);
DEFINE_FAKE_VALUE_FUNC(int, mock_execve, const char *, char * const *, char * const *);
DEFINE_FAKE_VALUE_FUNC(int, mock_setuid, uid_t);
DEFINE_FAKE_VALUE_FUNC(int, mock_setgid, gid_t);
DEFINE_FAKE_VALUE_FUNC(int, mock_setgroups, size_t, const gid_t *);
DEFINE_FAKE_VALUE_FUNC(int, mock_setpriority, int, id_t, int);
DEFINE_FAKE_VALUE_FUNC(int, mock_setpgid, pid_t, pid_t);
DEFINE_FAKE_VALUE_FUNC(mode_t, mock_umask, mode_t);
DEFINE_FAKE_VALUE_FUNC(int, mock_chdir, const char *);
DEFINE_FAKE_VALUE_FUNC(int, mock_access, const char *, int);
DEFINE_FAKE_VALUE_FUNC(int, mock_nanosleep, const struct timespec *, struct timespec *);
DEFINE_FAKE_VALUE_FUNC(int, mock_clock_gettime, clockid_t, struct timespec *);

int wrap_use_mock_forkpty;
int wrap_use_mock_waitpid;
int wrap_use_mock_kill;
int wrap_use_mock_execve;
int wrap_use_mock_creds;
int wrap_use_mock_pthread;
int wrap_use_mock_time;
int wrap_use_mock_chdir;
int wrap_use_mock_access;
int wrap_execve_succeeds;
int wrap_pthread_create_rc;
int wrap_pthread_create_count;
int wrap_exit_status;
unsigned long wrap_mono_msec;
jmp_buf wrap_jmp;
int wrap_jmp_armed;
char wrap_execve_path[512];
char wrap_execve_argv[16][256];
int wrap_execve_argc;
char wrap_execve_env[16][256];
int wrap_execve_envc;

pid_t __real_forkpty(int *amaster, char *name, const struct termios *termp, const struct winsize *winp);
pid_t __real_waitpid(pid_t pid, int *status, int options);
int __real_kill(pid_t pid, int sig);
int __real_execve(const char *pathname, char *const argv[], char *const envp[]);
void __real__exit(int status);
int __real_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start)(void *), void *arg);
int __real_pthread_join(pthread_t thread, void **retval);
int __real_nanosleep(const struct timespec *req, struct timespec *rem);
int __real_clock_gettime(clockid_t clk_id, struct timespec *tp);
int __real_setuid(uid_t uid);
int __real_setgid(gid_t gid);
int __real_setgroups(size_t size, const gid_t *list);
int __real_setpriority(int which, id_t who, int prio);
int __real_setpgid(pid_t pid, pid_t pgid);
mode_t __real_umask(mode_t mask);
int __real_chdir(const char *path);
int __real_access(const char *pathname, int mode);

void wraps_reset(void)
{
    wrap_use_mock_forkpty = 0;
    wrap_use_mock_waitpid = 0;
    wrap_use_mock_kill = 0;
    wrap_use_mock_execve = 0;
    wrap_use_mock_creds = 0;
    wrap_use_mock_pthread = 0;
    wrap_use_mock_time = 0;
    wrap_use_mock_chdir = 0;
    wrap_use_mock_access = 0;
    wrap_execve_succeeds = 0;
    wrap_pthread_create_rc = 0;
    wrap_pthread_create_count = 0;
    wrap_exit_status = -1;
    wrap_mono_msec = 0;
    wrap_jmp_armed = 0;
    wrap_execve_path[0] = '\0';
    wrap_execve_argc = 0;
    wrap_execve_envc = 0;

    RESET_FAKE(mock_forkpty);
    RESET_FAKE(mock_waitpid);
    RESET_FAKE(mock_kill);
    RESET_FAKE(mock_execve);
    RESET_FAKE(mock_setuid);
    RESET_FAKE(mock_setgid);
    RESET_FAKE(mock_setgroups);
    RESET_FAKE(mock_setpriority);
    RESET_FAKE(mock_setpgid);
    RESET_FAKE(mock_umask);
    RESET_FAKE(mock_chdir);
    RESET_FAKE(mock_access);
    RESET_FAKE(mock_nanosleep);
    RESET_FAKE(mock_clock_gettime);
}

pid_t __wrap_forkpty(int *amaster, char *name, const struct termios *termp, const struct winsize *winp)
{
    if (!wrap_use_mock_forkpty) {
        return __real_forkpty(amaster, name, termp, winp);
    }
    return mock_forkpty(amaster, name, termp, winp);
}

pid_t __wrap_waitpid(pid_t pid, int *status, int options)
{
    if (!wrap_use_mock_waitpid) {
        return __real_waitpid(pid, status, options);
    }
    return mock_waitpid(pid, status, options);
}

int __wrap_kill(pid_t pid, int sig)
{
    if (!wrap_use_mock_kill) {
        return __real_kill(pid, sig);
    }
    return mock_kill(pid, sig);
}

int __wrap_execve(const char *pathname, char *const argv[], char *const envp[])
{
    if (!wrap_use_mock_execve) {
        return __real_execve(pathname, argv, envp);
    }

    wrap_execve_path[0] = '\0';
    if (pathname) {
        snprintf(wrap_execve_path, sizeof(wrap_execve_path), "%s", pathname);
    }
    wrap_execve_argc = 0;
    if (argv) {
        for (; argv[wrap_execve_argc] && wrap_execve_argc < 16; wrap_execve_argc++) {
            snprintf(wrap_execve_argv[wrap_execve_argc], sizeof(wrap_execve_argv[0]),
                     "%s", argv[wrap_execve_argc]);
        }
    }
    wrap_execve_envc = 0;
    if (envp) {
        for (; envp[wrap_execve_envc] && wrap_execve_envc < 16; wrap_execve_envc++) {
            snprintf(wrap_execve_env[wrap_execve_envc], sizeof(wrap_execve_env[0]),
                     "%s", envp[wrap_execve_envc]);
        }
    }

    int rc = mock_execve(pathname, argv, envp);
    if (wrap_execve_succeeds && wrap_jmp_armed) {
        wrap_exit_status = 0;
        longjmp(wrap_jmp, 2);
    }
    return rc;
}

void __wrap__exit(int status)
{
    if (wrap_jmp_armed) {
        wrap_exit_status = status;
        longjmp(wrap_jmp, 1);
    }
    __real__exit(status);
}

int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start)(void *), void *arg)
{
    if (!wrap_use_mock_pthread) {
        return __real_pthread_create(thread, attr, start, arg);
    }
    wrap_pthread_create_count++;
    if (wrap_pthread_create_rc != 0) {
        return wrap_pthread_create_rc;
    }
    if (thread) {
        *thread = (pthread_t)1;
    }
    /* start_service mallocs the argument; the real logger thread would free it. */
    free(arg);
    (void)attr;
    (void)start;
    return 0;
}

int __wrap_pthread_join(pthread_t thread, void **retval)
{
    if (!wrap_use_mock_pthread) {
        return __real_pthread_join(thread, retval);
    }
    (void)thread;
    (void)retval;
    return 0;
}

int __wrap_nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (!wrap_use_mock_time) {
        return __real_nanosleep(req, rem);
    }
    if (req) {
        wrap_mono_msec += (unsigned long)req->tv_sec * 1000UL + (unsigned long)req->tv_nsec / 1000000UL;
    }
    return mock_nanosleep(req, rem);
}

int __wrap_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    if (!wrap_use_mock_time) {
        return __real_clock_gettime(clk_id, tp);
    }
    mock_clock_gettime(clk_id, tp);
    if (tp) {
        tp->tv_sec = (time_t)(wrap_mono_msec / 1000UL);
        tp->tv_nsec = (long)(wrap_mono_msec % 1000UL) * 1000000L;
    }
    return 0;
}

int __wrap_setuid(uid_t uid)
{
    if (!wrap_use_mock_creds) {
        return __real_setuid(uid);
    }
    return mock_setuid(uid);
}

int __wrap_setgid(gid_t gid)
{
    if (!wrap_use_mock_creds) {
        return __real_setgid(gid);
    }
    return mock_setgid(gid);
}

int __wrap_setgroups(size_t size, const gid_t *list)
{
    if (!wrap_use_mock_creds) {
        return __real_setgroups(size, list);
    }
    return mock_setgroups(size, list);
}

int __wrap_setpriority(int which, id_t who, int prio)
{
    if (!wrap_use_mock_creds) {
        return __real_setpriority(which, who, prio);
    }
    return mock_setpriority(which, who, prio);
}

int __wrap_setpgid(pid_t pid, pid_t pgid)
{
    if (!wrap_use_mock_creds) {
        return __real_setpgid(pid, pgid);
    }
    return mock_setpgid(pid, pgid);
}

mode_t __wrap_umask(mode_t mask)
{
    if (!wrap_use_mock_creds) {
        return __real_umask(mask);
    }
    return mock_umask(mask);
}

int __wrap_chdir(const char *path)
{
    if (!wrap_use_mock_chdir) {
        return __real_chdir(path);
    }
    return mock_chdir(path);
}

int __wrap_access(const char *pathname, int mode)
{
    if (!wrap_use_mock_access) {
        return __real_access(pathname, mode);
    }
    return mock_access(pathname, mode);
}
