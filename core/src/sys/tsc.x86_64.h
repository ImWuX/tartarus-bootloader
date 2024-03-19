#pragma once
#include <stdint.h>

uint64_t tsc_read();
void tsc_block(uint64_t cycles);