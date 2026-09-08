#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_helpers.h"
#include "utils.h"

void setUp(void)
{
    test_helpers_set_up();
}

void tearDown(void)
{
    test_helpers_tear_down();
}

static const char *first_existing(const char *a, const char *b)
{
    if (access(a, X_OK) == 0) {
        return a;
    }
    return b;
}

typedef struct {
    char stdout_lines[8][128];
    char stderr_lines[8][128];
    int n_out;
    int n_err;
} cb_acc_t;

static void line_cb(const char *line, std_output_t src, void *data)
{
    cb_acc_t *acc = data;
    if (src == STDOUT) {
        TEST_ASSERT_TRUE(acc->n_out < 8);
        snprintf(acc->stdout_lines[acc->n_out++], sizeof(acc->stdout_lines[0]), "%s", line);
    } else {
        TEST_ASSERT_TRUE(acc->n_err < 8);
        snprintf(acc->stderr_lines[acc->n_err++], sizeof(acc->stderr_lines[0]), "%s", line);
    }
}

void test_exec_cmd_true_false_missing(void)
{
    const char *true_path = first_existing("/bin/true", "/usr/bin/true");
    const char *false_path = first_existing("/bin/false", "/usr/bin/false");

    TEST_ASSERT_EQUAL(0, exec_cmd(true, NULL, true_path, "true", NULL));
    TEST_ASSERT_EQUAL(1, exec_cmd(true, NULL, false_path, "false", NULL));
    TEST_ASSERT_EQUAL(126, exec_cmd(true, NULL, "/no/such/cinit-cmd", "cmd", NULL));
}

void test_exec_cmd_prefix_on_output(void)
{
    const char *echo_path = first_existing("/bin/echo", "/usr/bin/echo");
    test_fd_capture_t cap;
    test_fd_capture_start(&cap, STDOUT_FILENO);
    TEST_ASSERT_EQUAL(0, exec_cmd(false, "PRE:", echo_path, "echo", "hello", NULL));
    char *out = test_fd_capture_stop(&cap);
    TEST_ASSERT_NOT_NULL(strstr(out, "PRE:hello"));
    free(out);
}

void test_exec_cmd_with_line_callback(void)
{
    const char *echo_path = first_existing("/bin/echo", "/usr/bin/echo");
    cb_acc_t acc = { 0 };
    TEST_ASSERT_EQUAL(0, exec_cmd_with_line_callback(line_cb, &acc, echo_path, "echo", "cb-line", NULL));
    TEST_ASSERT_EQUAL(1, acc.n_out);
    TEST_ASSERT_EQUAL_STRING("cb-line", acc.stdout_lines[0]);
}

void test_exec_cmd_signaled_child(void)
{
    const char *sh = first_existing("/bin/sh", "/usr/bin/sh");
    int rc = exec_cmd(true, NULL, sh, "sh", "-c", "kill -s TERM $$", NULL);
    TEST_ASSERT_EQUAL(128 + SIGTERM, rc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exec_cmd_true_false_missing);
    RUN_TEST(test_exec_cmd_prefix_on_output);
    RUN_TEST(test_exec_cmd_with_line_callback);
    RUN_TEST(test_exec_cmd_signaled_child);
    return UNITY_END();
}
