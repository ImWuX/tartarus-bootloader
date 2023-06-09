#ifndef DRIVERS_MSR_H
#define DRIVERS_MSR_H

#include <stdint.h>

uint64_t msr_read(uint64_t msr);
void msr_write(uint64_t msr, uint64_t value);

#endif