#include "e820.h"
#include <stdbool.h>
#include <int.h>
#include <log.h>

#define MAGIC_NUMBER 0x534D4150

void e820_load() {
    uint16_t count = 0;

    int_regs_t regs;
    regs.es = 0;
    regs.edi = E820_ADDRESS + 2;
    regs.ebx = 0;
    while(true) {
        *((uint32_t *) regs.edi + 20) = 1;
        regs.eax = 0xE820;
        regs.ecx = 24;
        regs.edx = MAGIC_NUMBER;
        int_exec(0x15, &regs);

        if(regs.eax == MAGIC_NUMBER && !(regs.eflags & INT_REGS_EFLAGS_CF) && regs.ebx != 0) {
            regs.edi += 24;
            count++;
        } else {
            *((uint16_t *) E820_ADDRESS) = count;
            break;
        }
    }
}