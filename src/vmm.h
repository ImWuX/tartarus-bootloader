#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <boot/memap.h>

#define HHDM_OFFSET 0xFFFF800000000000

typedef struct {
    uint64_t entries[512];
} __attribute__((packed)) page_table_t;

typedef enum {
    PT_FLAG_PRESENT = 0,
    PT_FLAG_READ_WRITE,
    PT_FLAG_USER_SUPERVISOR,
    PT_FLAG_WRITE_THROUGH,
    PT_FLAG_CACHE_DISABLED,
    PT_FLAG_ACCESSED
} pt_entry_flags_t;

void vmm_initialize(boot_memap_entry_t *memory_map, uint16_t memory_map_length);
void vmm_map_memory(uint64_t physical_address, uint64_t virtual_address);
void vmm_map_memory_2mb(uint64_t physical_address, uint64_t virtual_address);

#endif