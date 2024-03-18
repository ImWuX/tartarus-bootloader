#include "e820.h"
#include <x86_64/bios/int.h>

#define MAGIC_NUMBER ((uint16_t) 0x534D4150)

int e820_load(void *dest, int max) {
    int count = 0;

    int_regs_t regs = { .edi = (uint32_t) dest };
    while(count < max) {
        *((uint32_t *) (uintptr_t) (regs.edi + 20)) = 1;
        regs.eax = 0xE820;
        regs.ecx = 24;
        regs.edx = MAGIC_NUMBER;
        int_exec(0x15, &regs);

        count++;
        if(regs.eax != MAGIC_NUMBER || (regs.eflags & INT_REGS_EFLAGS_CF) || regs.ebx == 0) break;
        regs.edi += 24;
    }
    return count;
}