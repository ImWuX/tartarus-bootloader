#ifndef LIBC_H
#define LIBC_H

#include <stddef.h>

void *memset(void *dest, int ch, size_t count);
void *memcpy(void *dest, const void *src, size_t count);

#endif