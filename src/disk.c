#include "disk.h"
#include <pmm.h>
#include <int.h>
#include <log.h>

void disk_initialize() {
    int_regs_t regs;
    pmm_set(0, &regs, sizeof(int_regs_t));
    regs.eax = 0x41 << 8;
    regs.ebx = 0x55AA;
    regs.edx = 0x80;
    int_exec(0x13, &regs);

    if(regs.eflags & INT_REGS_EFLAGS_CF) {
        log("Unsupported\n");
    } else {
        log("Supported\n");
    }
}

// bool disk_read(uint64_t first_sector, uint32_t sector_count, void *dest) {
    
// }