#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#ifdef __AMD64
#include <drivers/acpi.h>
#endif

typedef struct smp_cpu {
    uint8_t acpi_id;
#ifdef __AMD64
    uint8_t lapic_id;
#endif
    bool is_bsp;
    uint64_t *wake_on_write;
    struct smp_cpu *next;
} smp_cpu_t;

#ifdef __AMD64
smp_cpu_t *smp_initialize_aps(acpi_sdt_header_t *sdt, uintptr_t reserved_page, void *pml4);
#endif

#endif