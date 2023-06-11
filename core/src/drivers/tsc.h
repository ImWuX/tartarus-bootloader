#ifndef DRIVERS_TSC_H
#define DRIVERS_TSC_H

#include <stdint.h>

uint64_t tsc_read();
void tsc_block(uint64_t cycles);

#endif