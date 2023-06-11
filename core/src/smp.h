#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#ifdef __AMD64
#include <drivers/acpi.h>
#endif

#ifdef __AMD64
void *smp_initialize_aps(acpi_sdt_header_t *sdt, uintptr_t reserved_page, void *pml4);
#endif

#endif