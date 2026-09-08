#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "test_helpers.h"

void setUp(void)
{
    test_helpers_set_up();
}

void tearDown(void)
{
    test_helpers_tear_down();
}

void test_log_stdout_format(void)
{
    test_fd_capture_t cap;
    test_fd_capture_start(&cap, STDOUT_FILENO);
    log_stdout("hello %d %s", 7, "x");
    char *out = test_fd_capture_stop(&cap);
    TEST_ASSERT_EQUAL_STRING("hello 7 x", out);
    free(out);
}

void test_log_stderr_format(void)
{
    test_fd_capture_t cap;
    test_fd_capture_start(&cap, STDERR_FILENO);
    log_stderr("err %s", "msg");
    char *out = test_fd_capture_stop(&cap);
    TEST_ASSERT_EQUAL_STRING("err msg", out);
    free(out);
}

void test_log_prefixer_applies_prefix_and_routes_streams(void)
{
    int outp[2], errp[2];
    TEST_ASSERT_EQUAL(0, pipe(outp));
    TEST_ASSERT_EQUAL(0, pipe(errp));
    TEST_ASSERT_EQUAL(9, write(outp[1], "out-line\n", 9));
    TEST_ASSERT_EQUAL(9, write(errp[1], "err-line\n", 9));
    close(outp[1]);
    close(errp[1]);

    test_fd_capture_t cap_out, cap_err;
    test_fd_capture_start(&cap_out, STDOUT_FILENO);
    test_fd_capture_start(&cap_err, STDERR_FILENO);
    TEST_ASSERT_EQUAL(0, log_prefixer("[p] ", outp[0], errp[0], NULL));
    char *stdout_data = test_fd_capture_stop(&cap_out);
    char *stderr_data = test_fd_capture_stop(&cap_err);
    close(outp[0]);
    close(errp[0]);

    TEST_ASSERT_EQUAL_STRING("[p] out-line\n", stdout_data);
    TEST_ASSERT_EQUAL_STRING("[p] err-line\n", stderr_data);
    free(stdout_data);
    free(stderr_data);
}

void test_log_prefixer_triple_colon_skips_prefix(void)
{
    int outp[2];
    TEST_ASSERT_EQUAL(0, pipe(outp));
    TEST_ASSERT_EQUAL(11, write(outp[1], ":::already\n", 11));
    close(outp[1]);

    test_fd_capture_t cap;
    test_fd_capture_start(&cap, STDOUT_FILENO);
    TEST_ASSERT_EQUAL(0, log_prefixer("[p] ", outp[0], -1, NULL));
    char *out = test_fd_capture_stop(&cap);
    close(outp[0]);
    TEST_ASSERT_EQUAL_STRING("already\n", out);
    free(out);
}

void test_log_prefixer_time_to_exit(void)
{
    int outp[2];
    TEST_ASSERT_EQUAL(0, pipe(outp));
    atomic_bool stop = true;
    test_fd_capture_t cap;
    test_fd_capture_start(&cap, STDOUT_FILENO);
    TEST_ASSERT_EQUAL(0, log_prefixer("[p] ", outp[0], -1, &stop));
    char *out = test_fd_capture_stop(&cap);
    close(outp[0]);
    close(outp[1]);
    TEST_ASSERT_EQUAL_STRING("", out);
    free(out);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_log_stdout_format);
    RUN_TEST(test_log_stderr_format);
    RUN_TEST(test_log_prefixer_applies_prefix_and_routes_streams);
    RUN_TEST(test_log_prefixer_triple_colon_skips_prefix);
    RUN_TEST(test_log_prefixer_time_to_exit);
    return UNITY_END();
}
