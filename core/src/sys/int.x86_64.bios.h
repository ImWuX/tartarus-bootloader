#pragma once
#include <stdint.h>

#define INT_16BIT_SEGMENT(value) ((uint16_t)(((uint32_t)(value) & 0xffff0) >> 4))
#define INT_16BIT_OFFSET(value) ((uint16_t)(((uint32_t)(value) & 0x0000f) >> 0))
#define INT_16BIT_DESEGMENT(segment, offset) (((uint32_t)(segment) << 4) + (uint32_t)(offset))

#define INT_REGS_EFLAGS_CF (1 << 0)
#define INT_REGS_EFLAGS_ZF (1 << 6)

typedef struct {
    uint16_t gs, fs, es, ds;
    uint32_t eflags, ebp, edi, esi, edx, ecx, ebx, eax;
} __attribute__((packed)) int_regs_t;

void int_exec(uint8_t int_no, int_regs_t *regs);