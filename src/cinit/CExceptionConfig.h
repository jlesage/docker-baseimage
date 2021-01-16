#ifndef _CEXCEPTION_CONFIG_H
#define _CEXCEPTION_CONFIG_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char mMessage[256];
} exception_t;

#define CEXCEPTION_T exception_t

#define CEXCEPTION_NONE (exception_t){ "" }

#define CEXCEPTION_IS_NONE(e) (e.mMessage[0] == '\0')

#define CEXCEPTION_NO_CATCH_HANDLER(e) { \
    printf("Unhandled exception: %s\n", e.mMessage); \
    assert(!"Unhandled exception"); \
}

#define ThrowMessage(...) do { \
    CEXCEPTION_T new_e; \
    int s = snprintf(new_e.mMessage, sizeof(new_e.mMessage), __VA_ARGS__); \
    assert(s < sizeof(new_e.mMessage)); \
    Throw (new_e); \
} while (0)

#define ThrowMessageWithErrno(...) do { \
    CEXCEPTION_T new_e; \
    int s = snprintf(new_e.mMessage, sizeof(new_e.mMessage), __VA_ARGS__); \
    assert(s < sizeof(new_e.mMessage)); \
    size_t used = strlen(new_e.mMessage); \
    size_t remaining_size = sizeof(new_e.mMessage) - used; \
    s = snprintf(new_e.mMessage + used, remaining_size, "%s", strerror(errno)); \
    assert(s < remaining_size); \
    Throw (new_e); \
} while (0)

#endif // _CEXCEPTION_CONFIG_H
