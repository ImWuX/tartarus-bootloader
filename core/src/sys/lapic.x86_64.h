#pragma once
#include <stdint.h>

bool lapic_is_supported();
uint32_t lapic_id();
void lapic_ipi_init(uint8_t lapic_id);
void lapic_ipi_startup(uint8_t lapic_id, void *startup_page);