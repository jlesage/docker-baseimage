#include <limits.h>
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

void test_trim_empty(void)
{
    char s[] = "";
    TEST_ASSERT_EQUAL_STRING("", trim(s));
}

void test_trim_already_clean(void)
{
    char s[] = "hello";
    TEST_ASSERT_EQUAL_STRING("hello", trim(s));
}

void test_trim_leading_trailing_and_interior(void)
{
    char s[] = "  hello world  ";
    TEST_ASSERT_EQUAL_STRING("hello world", trim(s));
}

void test_trim_all_spaces(void)
{
    char s[] = " \t\n ";
    TEST_ASSERT_EQUAL_STRING("", trim(s));
}

void test_trim_char_leading_trailing(void)
{
    char s[] = "xxabcx";
    TEST_ASSERT_EQUAL_STRING("abc", trim_char(s, 'x'));
}

void test_trim_char_empty(void)
{
    char s[] = "";
    TEST_ASSERT_EQUAL_STRING("", trim_char(s, 'x'));
}

void test_remove_duplicated_char_empty(void)
{
    char s[] = "";
    remove_duplicated_char(s, 'a');
    TEST_ASSERT_EQUAL_STRING("", s);
}

void test_remove_duplicated_char_none(void)
{
    char s[] = "abc";
    remove_duplicated_char(s, 'x');
    TEST_ASSERT_EQUAL_STRING("abc", s);
}

void test_remove_duplicated_char_collapses_runs(void)
{
    char s[] = "aaabbb";
    remove_duplicated_char(s, 'a');
    TEST_ASSERT_EQUAL_STRING("abbb", s);
}

void test_remove_duplicated_char_mixed(void)
{
    char s[] = "a,,b,,,c";
    remove_duplicated_char(s, ',');
    TEST_ASSERT_EQUAL_STRING("a,b,c", s);
}

void test_remove_all_char(void)
{
    char empty[] = "";
    remove_all_char(empty, 'a');
    TEST_ASSERT_EQUAL_STRING("", empty);

    char none[] = "abc";
    remove_all_char(none, 'x');
    TEST_ASSERT_EQUAL_STRING("abc", none);

    char cr[] = "a\r\nb\rc";
    remove_all_char(cr, '\r');
    TEST_ASSERT_EQUAL_STRING("a\nbc", cr);

    char all[] = "aaa";
    remove_all_char(all, 'a');
    TEST_ASSERT_EQUAL_STRING("", all);
}

void test_terminate_at_first_eol(void)
{
    char none[] = "hello";
    terminate_at_first_eol(none);
    TEST_ASSERT_EQUAL_STRING("hello", none);

    char nl[] = "hello\nworld";
    terminate_at_first_eol(nl);
    TEST_ASSERT_EQUAL_STRING("hello", nl);

    char cr[] = "hello\rworld";
    terminate_at_first_eol(cr);
    TEST_ASSERT_EQUAL_STRING("hello", cr);

    char crlf[] = "hello\r\nworld";
    terminate_at_first_eol(crlf);
    TEST_ASSERT_EQUAL_STRING("hello", crlf);
}

void test_split_single_token(void)
{
    char buf[] = "only";
    size_t len = 0;
    char **v = split(buf, ',', &len, 0, 0);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL(1, len);
    TEST_ASSERT_EQUAL_STRING("only", v[0]);
    free(v);
}

void test_split_several_and_consecutive_delimiters(void)
{
    char buf[] = "a,,b,c,";
    size_t len = 0;
    char **v = split(buf, ',', &len, 0, 0);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL(3, len);
    TEST_ASSERT_EQUAL_STRING("a", v[0]);
    TEST_ASSERT_EQUAL_STRING("b", v[1]);
    TEST_ASSERT_EQUAL_STRING("c", v[2]);
    free(v);
}

void test_split_plus_and_ofs(void)
{
    char buf[] = "a,b";
    size_t len = 0;
    char **v = split(buf, ',', &len, 1, 1);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL(3, len);
    TEST_ASSERT_EQUAL_STRING("a", v[1]);
    TEST_ASSERT_EQUAL_STRING("b", v[2]);
    free(v);
}

void test_string_to_bool_true_values(void)
{
    const char *vals[] = { "1", "true", "TRUE", "on", "yes", "Y", "enable", "Enabled" };
    for (size_t i = 0; i < DIM(vals); i++) {
        bool result = false;
        TEST_ASSERT_NO_THROW(string_to_bool(vals[i], &result));
        TEST_ASSERT_TRUE_MESSAGE(result, vals[i]);
    }
}

void test_string_to_bool_false_values(void)
{
    const char *vals[] = { "0", "false", "OFF", "no", "N", "disable", "Disabled" };
    for (size_t i = 0; i < DIM(vals); i++) {
        bool result = true;
        TEST_ASSERT_NO_THROW(string_to_bool(vals[i], &result));
        TEST_ASSERT_FALSE_MESSAGE(result, vals[i]);
    }
}

void test_string_to_bool_rejects_garbage(void)
{
    bool result = false;
    TEST_ASSERT_THROWS("not a boolean value", string_to_bool("", &result));
    TEST_ASSERT_THROWS("not a boolean value", string_to_bool("maybe", &result));
}

void test_string_to_int_valid_and_invalid(void)
{
    int v = 0;
    TEST_ASSERT_NO_THROW(string_to_int("0", &v));
    TEST_ASSERT_EQUAL(0, v);
    TEST_ASSERT_NO_THROW(string_to_int("-42", &v));
    TEST_ASSERT_EQUAL(-42, v);
    TEST_ASSERT_NO_THROW(string_to_int("2147483647", &v));
    TEST_ASSERT_EQUAL(INT_MAX, v);

    TEST_ASSERT_THROWS("not a number", string_to_int("", &v));
    TEST_ASSERT_THROWS("not a number", string_to_int("12x", &v));
    TEST_ASSERT_THROWS("not a number", string_to_int("x12", &v));
    TEST_ASSERT_THROWS("out of range", string_to_int("2147483648", &v));
    TEST_ASSERT_THROWS("out of range", string_to_int("9999999999999999999999", &v));
}

void test_string_to_uint_valid_and_invalid(void)
{
    unsigned int v = 0;
    TEST_ASSERT_NO_THROW(string_to_uint("0", &v));
    TEST_ASSERT_EQUAL(0, v);
    TEST_ASSERT_NO_THROW(string_to_uint("42", &v));
    TEST_ASSERT_EQUAL(42, v);

    TEST_ASSERT_THROWS("not a number", string_to_uint("", &v));
    TEST_ASSERT_THROWS("not a number", string_to_uint("12x", &v));
    TEST_ASSERT_THROWS("out of range", string_to_uint("9999999999999999999999", &v));
    /* strtoul("-1") wraps to ULONG_MAX, which is out of range for uint on 64-bit. */
    TEST_ASSERT_THROWS("out of range", string_to_uint("-1", &v));
}

void test_string_to_interval(void)
{
    unsigned int v = 0;
    TEST_ASSERT_NO_THROW(string_to_interval("hourly", &v));
    TEST_ASSERT_EQUAL(60 * 60, v);
    TEST_ASSERT_NO_THROW(string_to_interval("DAILY", &v));
    TEST_ASSERT_EQUAL(60 * 60 * 24, v);
    TEST_ASSERT_NO_THROW(string_to_interval("weekly", &v));
    TEST_ASSERT_EQUAL(60 * 60 * 24 * 7, v);
    TEST_ASSERT_NO_THROW(string_to_interval("monthly", &v));
    TEST_ASSERT_EQUAL(60 * 60 * 24 * 30, v);
    TEST_ASSERT_NO_THROW(string_to_interval("yearly", &v));
    TEST_ASSERT_EQUAL(60 * 60 * 24 * 365, v);
    TEST_ASSERT_NO_THROW(string_to_interval("15", &v));
    TEST_ASSERT_EQUAL(15, v);
    TEST_ASSERT_THROWS("not a number", string_to_interval("tomorrow", &v));
}

void test_string_to_mode(void)
{
    mode_t v = 0;
    TEST_ASSERT_NO_THROW(string_to_mode("022", &v));
    TEST_ASSERT_EQUAL(022, v);
    TEST_ASSERT_NO_THROW(string_to_mode("0022", &v));
    TEST_ASSERT_EQUAL(022, v);
    TEST_ASSERT_NO_THROW(string_to_mode("777", &v));
    TEST_ASSERT_EQUAL(0777, v);
    TEST_ASSERT_THROWS("not a number", string_to_mode("", &v));
    TEST_ASSERT_THROWS("not a number", string_to_mode("8", &v));
    TEST_ASSERT_THROWS("out of range", string_to_mode("1000", &v));
}

void test_string_to_uid_gid(void)
{
    uid_t uid = 0;
    gid_t gid = 0;
    TEST_ASSERT_NO_THROW(string_to_uid("0", &uid));
    TEST_ASSERT_EQUAL(0, uid);
    TEST_ASSERT_NO_THROW(string_to_gid("0", &gid));
    TEST_ASSERT_EQUAL(0, gid);
    TEST_ASSERT_NO_THROW(string_to_uid("root", &uid));
    TEST_ASSERT_EQUAL(0, uid);
    TEST_ASSERT_NO_THROW(string_to_gid("root", &gid));
    TEST_ASSERT_EQUAL(0, gid);
    TEST_ASSERT_THROWS("unknown user", string_to_uid("this_user_does_not_exist_xyz", &uid));
    TEST_ASSERT_THROWS("unknown group", string_to_gid("this_group_does_not_exist_xyz", &gid));
}

typedef struct {
    char lines[16][256];
    int fds[16];
    int count;
} lines_acc_t;

static void line_acc_cb(int fd, const char *line, void *data)
{
    lines_acc_t *acc = data;
    TEST_ASSERT_TRUE(acc->count < 16);
    acc->fds[acc->count] = fd;
    snprintf(acc->lines[acc->count], sizeof(acc->lines[0]), "%s", line);
    acc->count++;
}

static bool always_exit(void *data)
{
    (void)data;
    return true;
}

void test_read_file_missing_and_buffers(void)
{
    char *buf = NULL;
    TEST_ASSERT_THROWS("could not open file", read_file("/no/such/cinit-file", &buf, 0));

    char *dir = test_mkdtemp();
    char *path = test_join(dir, "f");
    test_write_file(path, "hello");

    char storage[16];
    char *p = storage;
    TEST_ASSERT_NO_THROW(read_file(path, &p, sizeof(storage)));
    TEST_ASSERT_EQUAL_STRING("hello", storage);

    char *alloc = NULL;
    TEST_ASSERT_NO_THROW(read_file(path, &alloc, 0));
    TEST_ASSERT_EQUAL_STRING("hello", alloc);
    free(alloc);

    char tiny[3];
    p = tiny;
    TEST_ASSERT_THROWS("buffer too small", read_file(path, &p, sizeof(tiny)));

    /* Exact fit: file length + NUL. */
    char exact[6];
    p = exact;
    TEST_ASSERT_NO_THROW(read_file(path, &p, sizeof(exact)));
    TEST_ASSERT_EQUAL_STRING("hello", exact);

    test_write_file(path, "");
    alloc = NULL;
    TEST_ASSERT_NO_THROW(read_file(path, &alloc, 0));
    TEST_ASSERT_EQUAL_STRING("", alloc);
    free(alloc);
    free(path);
}

void test_read_lines_newlines_and_remainder(void)
{
    int p[2];
    TEST_ASSERT_EQUAL(0, pipe(p));
    const char *data = "a\n\nb\rpartial";
    TEST_ASSERT_EQUAL((ssize_t)strlen(data), write(p[1], data, strlen(data)));
    close(p[1]);

    lines_acc_t acc = { 0 };
    TEST_ASSERT_EQUAL(0, read_lines(&p[0], 1, line_acc_cb, NULL, &acc));
    close(p[0]);

    /* Empty lines are skipped (j != 0). Remainder without EOL is flushed on EOF. */
    TEST_ASSERT_EQUAL(3, acc.count);
    TEST_ASSERT_EQUAL_STRING("a", acc.lines[0]);
    TEST_ASSERT_EQUAL_STRING("b", acc.lines[1]);
    TEST_ASSERT_EQUAL_STRING("partial", acc.lines[2]);
}

void test_read_lines_two_fds_and_exit_callback(void)
{
    int a[2], b[2];
    TEST_ASSERT_EQUAL(0, pipe(a));
    TEST_ASSERT_EQUAL(0, pipe(b));
    TEST_ASSERT_EQUAL(4, write(a[1], "out\n", 4));
    TEST_ASSERT_EQUAL(4, write(b[1], "err\n", 4));
    close(a[1]);
    close(b[1]);

    int fds[2] = { a[0], b[0] };
    lines_acc_t acc = { 0 };
    TEST_ASSERT_EQUAL(0, read_lines(fds, 2, line_acc_cb, NULL, &acc));
    close(a[0]);
    close(b[0]);
    TEST_ASSERT_EQUAL(2, acc.count);

    int dummy[2];
    TEST_ASSERT_EQUAL(0, pipe(dummy));
    acc.count = 0;
    TEST_ASSERT_EQUAL(0, read_lines(&dummy[0], 1, line_acc_cb, always_exit, &acc));
    TEST_ASSERT_EQUAL(0, acc.count);
    close(dummy[0]);
    close(dummy[1]);
}

void test_load_value_as_string_missing_file_and_regular_file(void)
{
    char *buf = NULL;
    TEST_ASSERT_FALSE(load_value_as_string("/no/such/cinit-cfg", &buf, 0));

    char *dir = test_mkdtemp();
    char *path = test_join(dir, "val");
    test_write_file(path, "hello\n");

    char storage[32];
    char *p = storage;
    TEST_ASSERT_TRUE(load_value_as_string(path, &p, sizeof(storage)));
    TEST_ASSERT_EQUAL_STRING("hello\n", storage);
    free(path);
}

void test_load_value_as_string_executable(void)
{
    char *dir = test_mkdtemp();
    char *path = test_join(dir, "script");
    test_write_exec(path, "#!/bin/sh\necho from-script\n");

    char *buf = NULL;
    TEST_ASSERT_TRUE(load_value_as_string(path, &buf, 0));
    TEST_ASSERT_EQUAL_STRING("from-script", buf);
    free(buf);

    test_write_exec(path, "#!/bin/sh\nexit 3\n");
    buf = NULL;
    TEST_ASSERT_THROWS("command failed", load_value_as_string(path, &buf, 0));
    free(path);
}

void test_load_value_as_bool(void)
{
    bool v = false;
    TEST_ASSERT_FALSE(load_value_as_bool("/no/such/cinit-cfg", &v));

    char *dir = test_mkdtemp();
    char *path = test_join(dir, "flag");

    test_write_file(path, "");
    TEST_ASSERT_TRUE(load_value_as_bool(path, &v));
    TEST_ASSERT_TRUE(v);

    test_write_file(path, "  FALSE  \nignored\n");
    TEST_ASSERT_TRUE(load_value_as_bool(path, &v));
    TEST_ASSERT_FALSE(v);

    test_write_file(path, "0");
    TEST_ASSERT_TRUE(load_value_as_bool(path, &v));
    TEST_ASSERT_FALSE(v);

    test_write_file(path, "maybe");
    TEST_ASSERT_THROWS("could not load", load_value_as_bool(path, &v));
    free(path);
}

void test_load_value_typed_converters(void)
{
    char *dir = test_mkdtemp();
    char *path = test_join(dir, "item");

    int i = 0;
    test_write_file(path, "  -7 \n");
    TEST_ASSERT_TRUE(load_value_as_int(path, &i));
    TEST_ASSERT_EQUAL(-7, i);
    TEST_ASSERT_FALSE(load_value_as_int("/no/such/cinit-cfg", &i));
    test_write_file(path, "nope");
    TEST_ASSERT_THROWS("could not load", load_value_as_int(path, &i));

    unsigned int u = 0;
    test_write_file(path, "9");
    TEST_ASSERT_TRUE(load_value_as_uint(path, &u));
    TEST_ASSERT_EQUAL(9, u);

    test_write_file(path, "hourly");
    TEST_ASSERT_TRUE(load_value_as_interval(path, &u));
    TEST_ASSERT_EQUAL(3600, u);

    uid_t uid = 99;
    test_write_file(path, "0");
    TEST_ASSERT_TRUE(load_value_as_uid(path, &uid));
    TEST_ASSERT_EQUAL(0, uid);

    gid_t gid = 99;
    test_write_file(path, "root");
    TEST_ASSERT_TRUE(load_value_as_gid(path, &gid));
    TEST_ASSERT_EQUAL(0, gid);

    mode_t mode = 0;
    test_write_file(path, "077");
    TEST_ASSERT_TRUE(load_value_as_mode(path, &mode));
    TEST_ASSERT_EQUAL(077, mode);
    free(path);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_trim_empty);
    RUN_TEST(test_trim_already_clean);
    RUN_TEST(test_trim_leading_trailing_and_interior);
    RUN_TEST(test_trim_all_spaces);
    RUN_TEST(test_trim_char_leading_trailing);
    RUN_TEST(test_trim_char_empty);
    RUN_TEST(test_remove_duplicated_char_empty);
    RUN_TEST(test_remove_duplicated_char_none);
    RUN_TEST(test_remove_duplicated_char_collapses_runs);
    RUN_TEST(test_remove_duplicated_char_mixed);
    RUN_TEST(test_remove_all_char);
    RUN_TEST(test_terminate_at_first_eol);
    RUN_TEST(test_split_single_token);
    RUN_TEST(test_split_several_and_consecutive_delimiters);
    RUN_TEST(test_split_plus_and_ofs);
    RUN_TEST(test_string_to_bool_true_values);
    RUN_TEST(test_string_to_bool_false_values);
    RUN_TEST(test_string_to_bool_rejects_garbage);
    RUN_TEST(test_string_to_int_valid_and_invalid);
    RUN_TEST(test_string_to_uint_valid_and_invalid);
    RUN_TEST(test_string_to_interval);
    RUN_TEST(test_string_to_mode);
    RUN_TEST(test_string_to_uid_gid);
    RUN_TEST(test_read_file_missing_and_buffers);
    RUN_TEST(test_read_lines_newlines_and_remainder);
    RUN_TEST(test_read_lines_two_fds_and_exit_callback);
    RUN_TEST(test_load_value_as_string_missing_file_and_regular_file);
    RUN_TEST(test_load_value_as_string_executable);
    RUN_TEST(test_load_value_as_bool);
    RUN_TEST(test_load_value_typed_converters);
    return UNITY_END();
}
