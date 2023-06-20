#include "smp.h"
#include <stddef.h>
#include <drivers/lapic.h>
#include <drivers/tsc.h>
#include <libc.h>
#include <log.h>
#include <memory/pmm.h>
#include <gdt.h>

#define CYCLES_10MIL 10000000
#define PAGE_SIZE 0x1000

#define ICR_ASSERT 0x4000
#define ICR_INIT 0x500
#define ICR_STARTUP 0x600

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

typedef struct {
    uint8_t init;
    uint8_t apic_id;
    uint32_t pml4;
    uint64_t wait_on_address;
    uint32_t heap;
    uint16_t gdtr_limit;
    uint32_t gdtr_base;
} __attribute__((packed)) ap_info_t;

typedef int SYMBOL[];

extern SYMBOL g_initap_start;
extern SYMBOL g_initap_end;

void *smp_initialize_aps(acpi_sdt_header_t *sdt, uintptr_t reserved_page, void *pml4) {
    madt_t *madt = (madt_t *) sdt;
    uint8_t bsp_id = lapic_bsp();

    size_t apinit_size = (uintptr_t) g_initap_end - (uintptr_t) g_initap_start;
    if(apinit_size + sizeof(ap_info_t) > PAGE_SIZE) log_panic("SMP", "Unable to fit initap into a page");
    memcpy((void *) reserved_page, (void *) g_initap_start, apinit_size);

    void *woa_page = pmm_alloc_page(PMM_AREA_MAX);
    memset(woa_page, 0, PAGE_SIZE);

    ap_info_t *ap_info = (ap_info_t *) (reserved_page + apinit_size);
    ap_info->pml4 = (uint32_t) (uintptr_t) pml4;
    ap_info->gdtr_limit = g_gdtr.limit;
    ap_info->gdtr_base = (uint32_t) g_gdtr.base;

    uint32_t count = 0;
    while(count < madt->sdt_header.length) {
        madt_record_t *record = (madt_record_t *) ((uintptr_t) madt + sizeof(madt_t) + count);
        switch(record->type) {
            case MADT_LAPIC:
                madt_record_lapic_t *lapic_record = (madt_record_lapic_t *) record;
                if(lapic_record->lapic_id == bsp_id) break;
                ap_info->init = 0;
                ap_info->apic_id = lapic_record->lapic_id;
                ap_info->wait_on_address = (uint64_t) ((uintptr_t) woa_page + ap_info->apic_id * 16);
                ap_info->heap = (uint32_t) (uintptr_t) pmm_alloc(PMM_AREA_EXTENDED, 4) + PAGE_SIZE * 4;

                lapic_write(LAPIC_REG_ICR2, lapic_record->lapic_id << 24);
                lapic_write(LAPIC_REG_ICR1, ICR_ASSERT | ICR_INIT);

                tsc_block(CYCLES_10MIL);

                lapic_write(LAPIC_REG_ICR2, lapic_record->lapic_id << 24);
                lapic_write(LAPIC_REG_ICR1, ICR_ASSERT | ICR_STARTUP | (reserved_page / PAGE_SIZE));

                for(int i = 0; i < 1000; i++) {
                    tsc_block(CYCLES_10MIL);
                    uint8_t value = 0;
                    asm volatile("lock xadd %0, %1" : "+r" (value) : "m" (ap_info->init) : "memory");
                    if(value > 0) {
                        log(">> LAPIC(%i) Initialized\n", (uint64_t) lapic_record->lapic_id);
                        goto success;
                    }
                }

                log(">> LAPIC(%i) Failed to initialize\n", (uint64_t) lapic_record->lapic_id);
                success:
                break;
            case MADT_LX2APIC:
                log_panic("SMP", "x2APIC is not currently supported");
                break;
        }
        count += record->length;
    }

    return woa_page;
}