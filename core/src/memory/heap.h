#pragma once
#include <stdint.h>
#include <stddef.h>

void *heap_alloc(size_t size);
void *heap_realloc(void *ptr, size_t new_size);
void heap_free(void *address);