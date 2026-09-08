#ifndef CINIT_TEST_HELPERS_H
#define CINIT_TEST_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "CException.h"
#include "unity.h"

void test_helpers_set_up(void);
void test_helpers_tear_down(void);

char *test_mkdtemp(void);
void test_rm_rf(const char *path);
char *test_join(const char *dir, const char *name);
void test_mkdir(const char *path);
void test_write_file(const char *path, const char *contents);
void test_write_exec(const char *path, const char *script);

typedef struct {
    int target_fd;
    int saved_fd;
    int read_fd;
} test_fd_capture_t;

void test_fd_capture_start(test_fd_capture_t *c, int fd);
char *test_fd_capture_stop(test_fd_capture_t *c);

#define TEST_ASSERT_THROWS(expected_substr, stmt) do { \
    CEXCEPTION_T _e; \
    int _threw = 0; \
    char _msg[256]; \
    _msg[0] = '\0'; \
    Try { stmt; } \
    Catch (_e) { \
        _threw = 1; \
        snprintf(_msg, sizeof(_msg), "%s", _e.mMessage); \
    } \
    TEST_ASSERT_TRUE_MESSAGE(_threw, "expected exception, none thrown"); \
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(_msg, (expected_substr)), _msg); \
} while (0)

#define TEST_ASSERT_NO_THROW(stmt) do { \
    CEXCEPTION_T _e; \
    int _threw = 0; \
    char _msg[256]; \
    _msg[0] = '\0'; \
    Try { stmt; } \
    Catch (_e) { \
        _threw = 1; \
        snprintf(_msg, sizeof(_msg), "%s", _e.mMessage); \
    } \
    TEST_ASSERT_FALSE_MESSAGE(_threw, _msg); \
} while (0)

#endif /* CINIT_TEST_HELPERS_H */
