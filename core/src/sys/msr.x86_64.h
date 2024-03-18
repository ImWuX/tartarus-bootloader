#pragma once
#include <stdint.h>

static inline uint64_t msr_read(uint64_t msr) {
    uint32_t low;
    uint32_t high;
    asm volatile("rdmsr" : "=a" (low), "=d" (high) : "c" ((uint32_t) msr));
    return low + ((uint64_t) high << 32);
}

static inline void msr_write(uint64_t msr, uint64_t value) {
    asm volatile("wrmsr" : : "a" ((uint32_t) value), "d" ((uint32_t) (value >> 32)), "c" ((uint32_t) msr));
}