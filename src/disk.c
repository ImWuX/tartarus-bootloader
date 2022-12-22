#include "disk.h"
#include <pmm.h>
#include <int.h>
#include <log.h>

#define BOOT_DRIVE_ADDRESS 0x7C40

static bool g_int13extensions;

void disk_initialize() {
    int_regs_t regs;
    pmm_set(0, &regs, sizeof(int_regs_t));
    regs.eax = 0x41 << 8;
    regs.ebx = 0x55AA;
    regs.edx = disk_drive();
    int_exec(0x13, &regs);
    g_int13extensions = !(regs.eflags & INT_REGS_EFLAGS_CF);
}

bool disk_read(uint64_t first_sector, uint16_t sector_count, void *dest) {
    if(g_int13extensions) {
        disk_address_packet_t packet;
        pmm_set(0, &packet, sizeof(disk_address_packet_t));
        packet.size = sizeof(disk_address_packet_t);
        packet.sector_count = sector_count;
        packet.src_lba = first_sector;
        packet.dest_offset = int_16bit_offset(dest);
        packet.dest_segment = int_16bit_segment(dest);

        int_regs_t regs;
        pmm_set(0, &regs, sizeof(int_regs_t));
        regs.eax = 0x42 << 8;
        regs.edx = disk_drive();
        regs.esi = (uint32_t) int_16bit_offset(&packet);
        regs.ds = int_16bit_segment(&packet);
        int_exec(0x13, &regs);
        return regs.eflags & INT_REGS_EFLAGS_CF;
    } else {
        log_panic("CHS disk reading is not currently supported\n");
        __builtin_unreachable();
    }
}

uint8_t disk_drive() {
    return *((uint8_t *) BOOT_DRIVE_ADDRESS);
}