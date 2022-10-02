TARGET = cinit

RM = rm -f

CPPFLAGS = -MMD
CFLAGS = -Wall -Werror -std=gnu11 -Os -fomit-frame-pointer -DCEXCEPTION_USE_CONFIG_FILE
LDFLAGS = -static -Wl,--strip-all
LDLIBS = -lpthread

# Logging to stdout or stderr doesn't make dany differnce once in the
# container's log.
CFLAGS += -DSINGLE_CHILD_STDOUT_STDERR_STREAM

SOURCES = cinit.c utils.c exec.c log.c CException.c
OBJECTS = $(patsubst %.c, %.o, $(SOURCES))
DEPENDS = $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

clean:
	-$(RM) $(OBJECTS)
	-$(RM) $(TARGET)
	-$(RM) $(DEPENDS)

-include $(DEPENDS)
