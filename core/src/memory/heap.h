#ifndef MEMORY_HEAP_H
#define MEMORY_HEAP_H

#include <stdint.h>
#include <stddef.h>

void *heap_alloc(size_t size);
void heap_free(void *address);

#endif