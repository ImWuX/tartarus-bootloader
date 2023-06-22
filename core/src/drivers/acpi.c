#include "acpi.h"
#include <core.h>
#include <log.h>

static bool checksum(uint8_t *src, uint32_t size) {
    uint32_t checksum = 0;
    for(uint8_t i = 0; i < size; i++) checksum += src[i];
    return (checksum & 1) == 0;
}

acpi_sdt_header_t *acpi_find_table(acpi_rsdp_t *rsdp, const char *signature) {
    int entry_count;
    bool extended = false;
    uintptr_t buffer;
    if(rsdp->revision > 0) {
        acpi_rsdp_ext_t *rsdp_ext = (acpi_rsdp_ext_t *) rsdp;
        if(!checksum(((uint8_t *) rsdp_ext) + sizeof(acpi_rsdp_t), sizeof(acpi_rsdp_ext_t) - sizeof(acpi_rsdp_t))) log_panic("ACPI", "Checksum for Extended RSDP failed");
        acpi_sdt_header_t *xsdt = (acpi_sdt_header_t *) (uintptr_t) rsdp_ext->xsdt_address;
        entry_count = (xsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint64_t);
        extended = true;
        buffer = (uintptr_t) xsdt + sizeof(acpi_sdt_header_t);
    } else {
        acpi_sdt_header_t *rsdt = (acpi_sdt_header_t *) (uintptr_t) rsdp->rsdt_address;
        entry_count = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
        buffer = (uintptr_t) rsdt + sizeof(acpi_sdt_header_t);
    }
    for(int i = 0; i < entry_count; i++) {
        acpi_sdt_header_t *sdt = (acpi_sdt_header_t *) (uintptr_t) (extended ? *(uint64_t *) buffer : *(uint32_t *) buffer);
        buffer += extended ? sizeof(uint64_t) : sizeof(uint32_t);
        bool match = true;
        for(int j = 0; j < 4; j++) {
            if(signature[j] != sdt->signature[j]) match = false;
        }
        if(match) return sdt;
    }
    return 0;
}

#if defined __BIOS && defined __AMD64
#define RSDP_FIXED_REGION_BASE 0xE0000
#define RSDP_FIXED_REGION_END 0xFFFFF
#define RSDP_IDENTIFIER 0x2052545020445352 // "RSD PTR "
#define RSDP_SIZE 20

#define BIOS_AREA 0x400
#define BIOS_AREA_EBDA 0xE

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
#elif defined __UEFI
static bool compare_guid(EFI_GUID a, EFI_GUID b) {
    bool data4_match = true;
    for(int i = 0; i < 8; i++) if(a.Data4[i] != b.Data4[i]) data4_match = false;
    return a.Data1 == b.Data1 && a.Data2 == b.Data2 && a.Data3 == b.Data3 && data4_match;
}

static uintptr_t find_table(EFI_GUID guid) {
    EFI_CONFIGURATION_TABLE *table = g_st->ConfigurationTable;
    for(UINTN i = 0; i < g_st->NumberOfTableEntries; i++) {
        if(compare_guid(table->VendorGuid, guid)) return (uintptr_t) table->VendorTable;
        table++;
    }
    return 0;
}

acpi_rsdp_t *acpi_find_rsdp() {
    EFI_GUID v1 = ACPI_TABLE_GUID;
    EFI_GUID v2 = ACPI_20_TABLE_GUID;
    uintptr_t address = find_table(v2);
    if(!address) address = find_table(v1);
    return (acpi_rsdp_t *) address;
}
#else
#error Invalid target or missing implementation
#endif