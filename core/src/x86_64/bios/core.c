#include <stdint.h>
#include <cpuid.h>
#include <common/pmm.h>
#include <common/log.h>
#include <x86_64/bios/e820.h>
#include <x86_64/common/msr.h>

#define CPUID_NX (1 << 20)
#define MSR_EFER 0xC0000080
#define MSR_EFER_NX (1 << 11)
#define E820_MAX 512

typedef int SYMBOL[];

extern SYMBOL __tartarus_start;
extern SYMBOL __tartarus_end;

static bool g_nx_support = false;

static void log_sink(char c) {
    asm volatile("outb %0, %1" : : "a" (c), "Nd" (0x3F8));
}

[[noreturn]] void core() {
    log_sink('\n');
    log_init(log_sink);
    log("CORE", "Tartarus Bootloader (BIOS)");

    // Enable NX
    unsigned int edx = 0, unused;
    if(__get_cpuid(0x80000001, &unused, &unused, &unused, &edx) != 0) g_nx_support = (edx & CPUID_NX) != 0;
    if(g_nx_support) {
        msr_write(MSR_EFER, msr_read(MSR_EFER) | MSR_EFER_NX);
    } else {
        log_warning("CORE", "CPU does not support EFER.NXE");
    }

    // Setup physical memory
    e820_entry_t e820[E820_MAX];
    int e820_size = e820_load((void *) &e820, E820_MAX);
    for(int i = 0; i < e820_size; i++) {
        tartarus_memory_map_type_t type;
        switch(e820[i].type) {
            case E820_TYPE_USABLE: type = TARTARUS_MEMORY_MAP_TYPE_USABLE; break;
            case E820_TYPE_ACPI_RECLAIMABLE: type = TARTARUS_MEMORY_MAP_TYPE_ACPI_RECLAIMABLE; break;
            case E820_TYPE_ACPI_NVS: type = TARTARUS_MEMORY_MAP_TYPE_ACPI_NVS; break;
            case E820_TYPE_BAD: type = TARTARUS_MEMORY_MAP_TYPE_BAD; break;
            case E820_TYPE_RESERVED:
            default: type = TARTARUS_MEMORY_MAP_TYPE_RESERVED; break;
        }
        pmm_init_add((tartarus_memory_map_entry_t) { .base = e820[i].address, .length = e820[i].length, .type = type });
    }
    pmm_init_sanitize();
    log("CORE", "Initialized physical memory");

    // Protect initial stack
    pmm_convert(TARTARUS_MEMORY_MAP_TYPE_USABLE, TARTARUS_MEMORY_MAP_TYPE_BOOT_RECLAIMABLE, 0x6000, 0x1000);
    pmm_convert(TARTARUS_MEMORY_MAP_TYPE_USABLE, TARTARUS_MEMORY_MAP_TYPE_BOOT_RECLAIMABLE, (uintptr_t) __tartarus_start, (uintptr_t) __tartarus_end - (uintptr_t) __tartarus_start);

    for(;;);
}