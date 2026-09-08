#include "test_helpers.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#define TEST_MAX_TEMPDIRS 16

static char g_saved_cwd[4096];
static char *g_tempdirs[TEST_MAX_TEMPDIRS];
static int g_ntempdirs;

void test_helpers_set_up(void)
{
    if (getcwd(g_saved_cwd, sizeof(g_saved_cwd)) == NULL) {
        g_saved_cwd[0] = '\0';
        TEST_FAIL_MESSAGE("getcwd failed");
    }
}

void test_helpers_tear_down(void)
{
    if (g_saved_cwd[0] != '\0') {
        if (chdir(g_saved_cwd) != 0) {
            /* Best-effort restore; cleanup still proceeds. */
        }
    }
    for (int i = 0; i < g_ntempdirs; i++) {
        if (g_tempdirs[i]) {
            test_rm_rf(g_tempdirs[i]);
            free(g_tempdirs[i]);
            g_tempdirs[i] = NULL;
        }
    }
    g_ntempdirs = 0;
}

char *test_join(const char *dir, const char *name)
{
    size_t n = strlen(dir) + 1 + strlen(name) + 1;
    char *p = malloc(n);
    TEST_ASSERT_NOT_NULL(p);
    snprintf(p, n, "%s/%s", dir, name);
    return p;
}

void test_rm_rf(const char *path)
{
    DIR *d = opendir(path);
    if (d == NULL) {
        unlink(path);
        return;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        char *child = test_join(path, de->d_name);
        test_rm_rf(child);
        free(child);
    }
    closedir(d);
    rmdir(path);
}

char *test_mkdtemp(void)
{
    char tmpl[] = "/tmp/cinit-test-XXXXXX";
    char *dir = mkdtemp(tmpl);
    TEST_ASSERT_NOT_NULL_MESSAGE(dir, "mkdtemp failed");
    TEST_ASSERT_TRUE_MESSAGE(g_ntempdirs < TEST_MAX_TEMPDIRS, "too many temp dirs");
    char *copy = strdup(dir);
    TEST_ASSERT_NOT_NULL(copy);
    g_tempdirs[g_ntempdirs++] = copy;
    return copy;
}

void test_mkdir(const char *path)
{
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        TEST_FAIL_MESSAGE("mkdir failed");
    }
}

void test_write_file(const char *path, const char *contents)
{
    FILE *f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
    if (contents != NULL && contents[0] != '\0') {
        TEST_ASSERT_EQUAL((int)strlen(contents), (int)fwrite(contents, 1, strlen(contents), f));
    }
    TEST_ASSERT_EQUAL(0, fclose(f));
}

void test_write_exec(const char *path, const char *script)
{
    test_write_file(path, script);
    TEST_ASSERT_EQUAL_MESSAGE(0, chmod(path, 0755), path);
}

void test_fd_capture_start(test_fd_capture_t *c, int fd)
{
    int p[2];
    TEST_ASSERT_EQUAL(0, pipe(p));
    c->target_fd = fd;
    c->saved_fd = dup(fd);
    TEST_ASSERT_TRUE(c->saved_fd >= 0);
    TEST_ASSERT_TRUE(dup2(p[1], fd) >= 0);
    close(p[1]);
    c->read_fd = p[0];
}

char *test_fd_capture_stop(test_fd_capture_t *c)
{
    TEST_ASSERT_TRUE(dup2(c->saved_fd, c->target_fd) >= 0);
    close(c->saved_fd);

    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    TEST_ASSERT_NOT_NULL(buf);
    buf[0] = '\0';

    while (1) {
        if (len + 512 >= cap) {
            cap *= 2;
            char *nbuf = realloc(buf, cap);
            TEST_ASSERT_NOT_NULL(nbuf);
            buf = nbuf;
        }
        ssize_t n = read(c->read_fd, buf + len, cap - len - 1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {
            break;
        }
        len += (size_t)n;
        buf[len] = '\0';
    }
    close(c->read_fd);
    return buf;
}
