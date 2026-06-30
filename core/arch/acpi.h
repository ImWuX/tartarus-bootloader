#pragma once

#include "dev/acpi.h"

void *arch_acpi_find_rsdp();
void arch_acpi_map_tables(acpi_rsdp_t *rsdp);
