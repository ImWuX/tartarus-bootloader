#ifndef INT_H
#define INT_H

#include <stdint.h>

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