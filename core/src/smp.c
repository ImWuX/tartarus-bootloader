#include "smp.h"
#include <drivers/lapic.h>
#include <log.h>

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

void smp_initialize_aps(acpi_sdt_header_t *sdt) {
    madt_t *madt = (madt_t *) sdt;
    uint8_t bsp_id = lapic_bsp();

    uint32_t count = 0;
    while(count < madt->sdt_header.length) {
        madt_record_t *record = (madt_record_t *) ((uintptr_t) madt + sizeof(madt_t) + count);
        switch(record->type) {
            case MADT_LAPIC:
                madt_record_lapic_t *lapic_record = (madt_record_lapic_t *) record;
                if(lapic_record->lapic_id == bsp_id) break;
                log(">> LAPIC(%i) %x\n", (uint64_t) lapic_record->lapic_id, (uint64_t) lapic_record->flags);
                break;
            case MADT_LX2APIC:
                log_panic("SMP", "x2APIC is not currently supported");
                break;
        }
        count += record->length;
    }
}