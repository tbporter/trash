#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>

#include "esh-debug.h"

int errprintf(const char* format, ...) {
    va_list list;
    va_start(list, format);
    int length = vfprintf(stderr, format, list);
    va_end(list);
    return length;
}
