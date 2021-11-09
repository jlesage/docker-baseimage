TARGET = cinit

RM = rm -f

CPPFLAGS = -MMD
CFLAGS = -Wall -Werror -std=gnu11 -Os -fomit-frame-pointer -DCEXCEPTION_USE_CONFIG_FILE
LDFLAGS = -static -Wl,--strip-all
LDLIBS = -lpthread

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
