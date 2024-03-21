#include "acpi.h"

#define RSDP_FIXED_REGION_BASE 0xE0000
#define RSDP_FIXED_REGION_END 0xFFFFF
#define RSDP_IDENTIFIER 0x2052545020445352 // "RSD PTR "
#define RSDP_SIZE 20

#define BIOS_AREA 0x400
#define BIOS_AREA_EBDA 0xE

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

acpi_rsdp_t *acpi_find_rsdp() {
    uint16_t ebda_address = *(uint16_t *) ((uintptr_t) BIOS_AREA + BIOS_AREA_EBDA);
    uintptr_t address = 0;
    if(ebda_address) address = scan_region(ebda_address, ebda_address + 1024 - 1);
    if(!address) address = scan_region(RSDP_FIXED_REGION_BASE, RSDP_FIXED_REGION_END);
    return (acpi_rsdp_t *) address;
}