#include "heap.h"
#include <memory/pmm.h>
#include <log.h>

#define MINIMUM_ENTRY_SIZE 8

typedef struct heap_entry {
    bool free;
    size_t size;
    struct heap_entry *next;
    struct heap_entry *last;
} heap_entry_t;

static heap_entry_t *g_heap;

static heap_entry_t *split(heap_entry_t *entry, size_t truncate_to) {
    size_t left = entry->size - truncate_to;
    entry->size = truncate_to;
    heap_entry_t *new_entry = (heap_entry_t *) ((uintptr_t) entry + sizeof(heap_entry_t) + entry->size);
    new_entry->size = left - sizeof(heap_entry_t);
    new_entry->free = true;
    new_entry->next = entry->next;
    new_entry->last = entry;
    if(entry->next) entry->next->last = new_entry;
    entry->next = new_entry;
    return new_entry;
}

static void delete(heap_entry_t *entry) {
    if(entry->last) entry->last->next = entry->next;
    if(entry->next) entry->next->last = entry->last;
}

static heap_entry_t *alloc(int pages) {
    heap_entry_t *new_entry = pmm_alloc(PMM_AREA_MAX, pages);
    new_entry->free = true;
    new_entry->size = PAGE_SIZE - sizeof(heap_entry_t);
    new_entry->next = g_heap;
    g_heap = new_entry;
    return new_entry;
}

void *heap_alloc(size_t size) {
    for(heap_entry_t *current_entry = g_heap; current_entry; current_entry = current_entry->next) {
        if(!current_entry->free || current_entry->size < size) continue;
        if(current_entry->size - size >= sizeof(heap_entry_t) + MINIMUM_ENTRY_SIZE) split(current_entry, size);
        current_entry->free = false;
        return (void *) ((uintptr_t) current_entry + sizeof(heap_entry_t));
    }
    alloc((size + sizeof(heap_entry_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    return heap_alloc(size);
}

void heap_free(void *address) {
    heap_entry_t *entry = (heap_entry_t *) ((uintptr_t) address - sizeof(heap_entry_t));
    entry->free = true;

    for(heap_entry_t *current_entry = g_heap; current_entry; current_entry = current_entry->next) {
        if(current_entry == entry || !current_entry->free) continue;
        if((uintptr_t) current_entry + current_entry->size + sizeof(heap_entry_t) == (uintptr_t) entry) {
            current_entry->size += entry->size + sizeof(heap_entry_t);
            delete(entry);
            entry = current_entry;
        }
        if((uintptr_t) entry + entry->size + sizeof(heap_entry_t) == (uintptr_t) current_entry) {
            entry->size += current_entry->size + sizeof(heap_entry_t);
            delete(current_entry);
        }
    }
}