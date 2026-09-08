TARGET = cinit

RM = rm -f

CPPFLAGS = -MMD
CFLAGS = -Wall -Werror -std=gnu11 -Os -fomit-frame-pointer -DCEXCEPTION_USE_CONFIG_FILE
LDFLAGS = -fuse-ld=lld -static -Wl,--strip-all
LDLIBS = -lpthread

# Logging to stdout or stderr doesn't make dany differnce once in the
# container's log.
CFLAGS += -DSINGLE_CHILD_STDOUT_STDERR_STREAM

SOURCES = cinit.c utils.c exec.c log.c CException.c
OBJECTS = $(patsubst %.c, %.o, $(SOURCES))
DEPENDS = $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

# --- unit tests (host/native; not used for the static image binary) ---
TEST_CFLAGS = -Wall -Werror -std=gnu11 -g -O0 \
	-DCEXCEPTION_USE_CONFIG_FILE -DSINGLE_CHILD_STDOUT_STDERR_STREAM
TEST_CPPFLAGS = -I. -Itests -Itests/unity
TEST_LDLIBS = -lpthread

UNITY_SRC = tests/unity/unity.c
HELPERS_SRC = tests/test_helpers.c
COMMON_SRC = utils.c exec.c log.c CException.c

TEST_BINS = tests/test_utils tests/test_log tests/test_exec tests/test_cinit

tests/test_utils: tests/test_utils.c $(HELPERS_SRC) $(COMMON_SRC) $(UNITY_SRC)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ $(TEST_LDLIBS) -o $@

tests/test_log: tests/test_log.c $(HELPERS_SRC) $(COMMON_SRC) $(UNITY_SRC)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ $(TEST_LDLIBS) -o $@

tests/test_exec: tests/test_exec.c $(HELPERS_SRC) $(COMMON_SRC) $(UNITY_SRC)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ $(TEST_LDLIBS) -o $@

# cinit.c is #include'd by test_cinit.c; list it as a dependency so edits
# rebuild the tests, but do not compile it as a second translation unit.
TEST_CINIT_SRCS = tests/test_cinit.c tests/libc_wraps.c $(HELPERS_SRC) $(COMMON_SRC) $(UNITY_SRC)
TEST_WRAP = -Wl,--wrap=forkpty -Wl,--wrap=waitpid -Wl,--wrap=kill \
	-Wl,--wrap=execve -Wl,--wrap=_exit -Wl,--wrap=pthread_create \
	-Wl,--wrap=pthread_join -Wl,--wrap=nanosleep -Wl,--wrap=clock_gettime \
	-Wl,--wrap=setuid -Wl,--wrap=setgid -Wl,--wrap=setgroups \
	-Wl,--wrap=setpriority -Wl,--wrap=setpgid -Wl,--wrap=umask \
	-Wl,--wrap=chdir -Wl,--wrap=access
# forkpty is in libutil on glibc and in libc on musl. Probe with the same $(CC)
# used to link so `make test CC=clang` works on both.
tests/test_cinit: $(TEST_CINIT_SRCS) cinit.c tests/libc_wraps.h
	@probe=$$(mktemp); \
	libutil=""; \
	if printf 'int main(void){return 0;}\n' | $(CC) -x c - -lutil -o "$$probe" >/dev/null 2>&1; then \
		libutil="-lutil"; \
	fi; \
	rm -f "$$probe"; \
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) -Wno-unused-function -DCINIT_TESTING $(TEST_CINIT_SRCS) $(TEST_WRAP) $(TEST_LDLIBS) $$libutil -o $@

.PHONY: test
test: $(TEST_BINS)
	./tests/test_utils
	./tests/test_log
	./tests/test_exec
	./tests/test_cinit

clean:
	-$(RM) $(OBJECTS)
	-$(RM) $(TARGET)
	-$(RM) $(DEPENDS)
	-$(RM) $(TEST_BINS)

-include $(DEPENDS)
