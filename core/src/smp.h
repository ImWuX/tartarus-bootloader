#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#include <drivers/acpi.h>

void smp_initialize_aps(acpi_sdt_header_t *sdt);

#endif