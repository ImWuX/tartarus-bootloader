#ifndef __TARTARUS_BOOTLOADER_HEADER
#define __TARTARUS_BOOTLOADER_HEADER

#include <stdint.h>

#define __TARTARUS_PACKED __attribute__((packed))

#ifndef __TARTARUS_ARCH_OVERRIDE
#if defined __x86_64__ || defined __i386__ || defined _M_X64 || defined _M_IX86
#ifndef __AMD64
#define __AMD64
#endif
#endif
#endif

#ifdef __TARTARUS_NO_PTR
#define __TARTARUS_PTR(TYPE) uint64_t
#else
#define __TARTARUS_PTR(TYPE) TYPE
#endif

typedef struct {
    __TARTARUS_PTR(void *) address;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} __TARTARUS_PACKED tartarus_fb_t;

typedef enum {
    TARTARUS_MEMAP_TYPE_USABLE = 0,
    TARTARUS_MEMAP_TYPE_BOOT_RECLAIMABLE,
    TARTARUS_MEMAP_TYPE_ACPI_RECLAIMABLE,
    TARTARUS_MEMAP_TYPE_ACPI_NVS,
    TARTARUS_MEMAP_TYPE_RESERVED,
    TARTARUS_MEMAP_TYPE_BAD
} tartarus_mmap_type_t;

typedef struct {
    uint64_t base;
    uint64_t length;
    tartarus_mmap_type_t type;
} __TARTARUS_PACKED tartarus_mmap_entry_t;

#ifdef __AMD64
typedef struct {
uint8_t apic_id;
__TARTARUS_PTR(uint64_t *) wake_on_write;
} __TARTARUS_PACKED tartarus_cpu_t;
#else
#error Invalid arch or missing implementation
#endif

typedef struct {
    uint64_t kernel_vaddr;
    uint64_t kernel_paddr;
    uint64_t kernel_size;
    __TARTARUS_PTR(void *) acpi_rsdp;
    tartarus_fb_t framebuffer;
    __TARTARUS_PTR(tartarus_mmap_entry_t *) memory_map;
    uint16_t memory_map_size;
    uint64_t hhdm_base;
    uint64_t hhdm_size;
    uint8_t bsp_index;
    uint8_t cpu_count;
    __TARTARUS_PTR(tartarus_cpu_t *) cpus;
} __TARTARUS_PACKED tartarus_boot_info_t;

#endif