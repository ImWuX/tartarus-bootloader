#include "acpi.h"
#include <stdbool.h>

#define RSDP_REGION_ONE_BASE 0x80000
#define RSDP_REGION_ONE_END 0x9FFFF
#define RSDP_REGION_TWO_BASE 0xE0000
#define RSDP_REGION_TWO_END 0xFFFFF
#define RSDP_IDENTIFIER 0x2052545020445352 // "RSD PTR "
#define RSDP_SIZE 20

static bool checksum(uint8_t *src, uint32_t size) {
    uint32_t checksum = 0;
    for(uint8_t i = 0; i < size; i++) checksum += src[i];
    return (checksum & 1) == 0;
}

static uintptr_t scan_region(uintptr_t start, uintptr_t end) {
    uintptr_t addr = start;
    while(addr < end) {
        if(*(uint64_t *) addr == RSDP_IDENTIFIER && checksum((uint8_t *) addr, RSDP_SIZE)) return addr;
        addr += 16;
    }
    return 0;
}

uintptr_t acpi_find_rsdp() {
    uintptr_t address = scan_region(RSDP_REGION_ONE_BASE, RSDP_REGION_ONE_END);
    if(!address) address = scan_region(RSDP_REGION_TWO_BASE, RSDP_REGION_TWO_END);
    return address;
}