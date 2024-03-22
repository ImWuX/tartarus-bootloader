#pragma once
#include <stdint.h>
#include <dev/acpi.h>

typedef struct {
    uint8_t acpi_id;
    uint8_t lapic_id;
    bool is_bsp;
    uint64_t *wake_on_write;
} smp_cpu_t;

smp_cpu_t *smp_initialize_aps(acpi_sdt_header_t *madt_header, void *reserved_init_page, void *address_space);