#ifndef DRIVERS_LAPIC_H
#define DRIVERS_LAPIC_H

#include <stdint.h>
#include <stdbool.h>

#define LAPIC_REG_ID 0x20
#define LAPIC_REG_ICR1 0x300
#define LAPIC_REG_ICR2 0x310

bool lapic_supported();
uint8_t lapic_bsp();
void lapic_write(uint32_t reg, uint32_t value);
uint32_t lapic_read(uint32_t reg);

#endif