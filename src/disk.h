#ifndef DISK_H
#define DISK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t size;
    uint8_t rsv0;
    uint16_t sector_count;
    uint16_t dest_offset;
    uint16_t dest_segment;
    uint64_t src_lba;
} __attribute__((packed)) disk_address_packet_t;

void disk_initialize();
bool disk_read(uint64_t first_sector, uint16_t sector_count, void *dest);
uint8_t disk_drive();

#endif