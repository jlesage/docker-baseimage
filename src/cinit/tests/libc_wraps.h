#ifndef CINIT_LIBC_WRAPS_H
#define CINIT_LIBC_WRAPS_H

#include <pty.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "fff.h"

DECLARE_FAKE_VALUE_FUNC(pid_t, mock_forkpty, int *, char *, const struct termios *, const struct winsize *);
DECLARE_FAKE_VALUE_FUNC(pid_t, mock_waitpid, pid_t, int *, int);
DECLARE_FAKE_VALUE_FUNC(int, mock_kill, pid_t, int);
DECLARE_FAKE_VALUE_FUNC(int, mock_execve, const char *, char * const *, char * const *);
DECLARE_FAKE_VALUE_FUNC(int, mock_setuid, uid_t);
DECLARE_FAKE_VALUE_FUNC(int, mock_setgid, gid_t);
DECLARE_FAKE_VALUE_FUNC(int, mock_setgroups, size_t, const gid_t *);
DECLARE_FAKE_VALUE_FUNC(int, mock_setpriority, int, id_t, int);
DECLARE_FAKE_VALUE_FUNC(int, mock_setpgid, pid_t, pid_t);
DECLARE_FAKE_VALUE_FUNC(mode_t, mock_umask, mode_t);
DECLARE_FAKE_VALUE_FUNC(int, mock_chdir, const char *);
DECLARE_FAKE_VALUE_FUNC(int, mock_access, const char *, int);
DECLARE_FAKE_VALUE_FUNC(int, mock_nanosleep, const struct timespec *, struct timespec *);
DECLARE_FAKE_VALUE_FUNC(int, mock_clock_gettime, clockid_t, struct timespec *);

extern int wrap_use_mock_forkpty;
extern int wrap_use_mock_waitpid;
extern int wrap_use_mock_kill;
extern int wrap_use_mock_execve;
extern int wrap_use_mock_creds;
extern int wrap_use_mock_pthread;
extern int wrap_use_mock_time;
extern int wrap_use_mock_chdir;
extern int wrap_use_mock_access;
extern int wrap_execve_succeeds;
extern int wrap_pthread_create_rc;
extern int wrap_pthread_create_count;
extern int wrap_exit_status;
extern unsigned long wrap_mono_msec;
extern jmp_buf wrap_jmp;
extern int wrap_jmp_armed;
extern char wrap_execve_path[512];
extern char wrap_execve_argv[16][256];
extern int wrap_execve_argc;
extern char wrap_execve_env[16][256];
extern int wrap_execve_envc;

void wraps_reset(void);

#endif /* CINIT_LIBC_WRAPS_H */
