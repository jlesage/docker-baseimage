TARGET = logmonitor

CC = $(TOOLCHAIN_PREFIX)gcc
STRIP = $(TOOLCHAIN_PREFIX)strip
RM = rm -f

CPPFLAGS = -MMD
CFLAGS = -Wall -Werror -std=gnu11
LDFLAGS = -static
LDLIBS =

SOURCES = logmonitor.c
OBJECTS = $(patsubst %.c, %.o, $(SOURCES))
DEPENDS = $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
	$(STRIP) $@

clean:
	-$(RM) $(OBJECTS)
	-$(RM) $(TARGET)
	-$(RM) $(DEPENDS)

-include $(DEPENDS)
