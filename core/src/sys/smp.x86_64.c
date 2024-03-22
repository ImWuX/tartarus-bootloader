#include "smp.x86_64.h"
#include <cpuid.h>
#include <log.h>
#include <lib/mem.h>
#include <memory/pmm.h>
#include <memory/heap.h>
#include <sys/lapic.x86_64.h>
#include <sys/gdt.x86_64.h>
#include <sys/cpu.x86_64.h>
#include <sys/tsc.x86_64.h>

#define CYCLES_10MIL 10000000

typedef struct {
    acpi_sdt_header_t sdt_header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed)) madt_t;

typedef enum {
    MADT_LAPIC = 0,
    MADT_IOAPIC,
    MADT_SOURCE_OVERRIDE,
    MADT_NMI_SOURCE,
    MADT_NMI,
    MADT_LAPIC_ADDRESS,
    MADT_LX2APIC = 9
} madt_record_types_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_record_t;

typedef struct {
    madt_record_t base;
    uint8_t acpi_processor_id;
    uint8_t lapic_id;
    uint32_t flags;
} __attribute__((packed)) madt_record_lapic_t;

// ap_info_t has to match boot_info in apinit.x86_64.asm
typedef struct {
    uint8_t init;
    uint8_t lapic_id;
    uint32_t pml4;
    uint64_t wait_on_address;
    uint32_t stack;
    uint16_t gdtr_limit;
    uint32_t gdtr_base;
    uint8_t set_nx;
} __attribute__((packed)) ap_info_t;

typedef int SYMBOL[];

extern SYMBOL g_apinit_start;
extern SYMBOL g_apinit_end;

static uint8_t bsp_lapic_id() {
    unsigned int ebx = 0, unused = 0;
    if(__get_cpuid(1, &unused, &ebx, &unused, &unused) == 0) log_panic("SMP", "CPUID failed when trying to retrieve BSP lapic id");
    return ebx >> 24;
}

smp_cpu_t *smp_initialize_aps(acpi_sdt_header_t *madt_header, void *reserved_init_page, void *address_space) {
    madt_t *madt = (madt_t *) madt_header;
    uint8_t bsp_id = bsp_lapic_id();
    if(bsp_id != lapic_id()) log_warning("SMP", "Current LAPIC id does not match BSP id. This behavior is unexpected and undefined");

    size_t apinit_size = (uintptr_t) g_apinit_end - (uintptr_t) g_apinit_start;
    if(apinit_size + sizeof(ap_info_t) > PMM_PAGE_SIZE) log_panic("SMP", "Unable to fit AP init code into a page");
    memcpy(reserved_init_page, (void *) g_apinit_start, apinit_size);

    ap_info_t *ap_info = (ap_info_t *) (reserved_init_page + apinit_size);
    ap_info->pml4 = (uintptr_t) address_space;
    ap_info->gdtr_limit = g_gdtr.limit;
    ap_info->gdtr_base = g_gdtr.base;
    ap_info->set_nx = g_cpu_nx_support;

    size_t wow_count = 0;
    void *wow_page = pmm_alloc_page(PMM_AREA_MAX);

    size_t cpu_count = 0;
    size_t cpu_limit = 0;
    smp_cpu_t *cpus = NULL;

    for(size_t count = sizeof(madt_t); count < madt->sdt_header.length; count += ((madt_record_t *) ((uintptr_t) madt + count))->length) {
        madt_record_t *record = (madt_record_t *) ((uintptr_t) madt + count);
        switch(record->type) {
            case MADT_LAPIC:
                madt_record_lapic_t *lapic_record = (madt_record_lapic_t *) record;
                if(cpu_count >= cpu_limit) {
                    cpu_limit += 4;
                    cpus = heap_realloc(cpus, cpu_limit * sizeof(smp_cpu_t));
                }
                cpus[cpu_count].acpi_id = lapic_record->acpi_processor_id;
                cpus[cpu_count].lapic_id = lapic_record->lapic_id;
                cpus[cpu_count].wake_on_write = NULL;
                cpus[cpu_count].is_bsp = false;
                if(lapic_record->lapic_id == bsp_id) {
                    cpus[cpu_count].is_bsp = true;
                    goto success;
                }
                cpus[cpu_count].wake_on_write = heap_alloc(sizeof(uint64_t));
                *cpus[cpu_count].wake_on_write = 0;

                ap_info->init = 0;
                ap_info->lapic_id = lapic_record->lapic_id;
                ap_info->wait_on_address = (uintptr_t) cpus[cpu_count].wake_on_write;
                ap_info->stack = (uintptr_t) pmm_alloc(PMM_AREA_EXTENDED, 4) + PMM_PAGE_SIZE * 4;

                lapic_ipi_init(lapic_record->lapic_id);
                tsc_block(CYCLES_10MIL);
                lapic_ipi_startup(lapic_record->lapic_id, reserved_init_page);

                for(int i = 0; i < 1000; i++) {
                    tsc_block(CYCLES_10MIL);
                    uint8_t value = 0;
                    asm volatile("lock xadd %0, %1" : "+r" (value) : "m" (ap_info->init) : "memory");
                    if(value > 0) goto success;
                }
                log_warning("SMP", "lapic[%u] failed to initialize", lapic_record->lapic_id);
                break;
                success:
                log("SMP", "lapic[%u] initialized", lapic_record->lapic_id);
                cpu_count++;
                break;
            case MADT_LX2APIC:
                log_warning("SMP", "x2APIC is not currently supported, ignoring MADT entry");
                break;
        }
    }
    return cpus;
}