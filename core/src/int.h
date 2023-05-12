#ifndef INT_H
#define INT_H

#include <stdint.h>

#define int_16bit_segment(value) ((uint16_t)(((uint32_t)(value) & 0xffff0) >> 4))
#define int_16bit_offset(value) ((uint16_t)(((uint32_t)(value) & 0x0000f) >> 0))
#define int_16bit_desegment(segment, offset) (((uint32_t)(segment) << 4) + (uint32_t)(offset))

#define INT_REGS_EFLAGS_CF (1 << 0)
#define INT_REGS_EFLAGS_ZF (1 << 6)

typedef struct {
    uint16_t gs;
    uint16_t fs;
    uint16_t es;
    uint16_t ds;
    uint32_t eflags;
    uint32_t ebp;
    uint32_t edi;
    uint32_t esi;
    uint32_t edx;
    uint32_t ecx;
    uint32_t ebx;
    uint32_t eax;
} __attribute__((packed)) int_regs_t;

void int_exec(uint8_t int_no, int_regs_t *regs);

#endif