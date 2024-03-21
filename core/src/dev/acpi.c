#include "acpi.h"
#include <log.h>

acpi_sdt_header_t *acpi_find_table(acpi_rsdp_t *rsdp, const char *signature) {
    int entry_count;
    bool extended = false;
    uintptr_t buffer;
    if(rsdp->revision > 0) {
        acpi_rsdp_ext_t *rsdp_ext = (acpi_rsdp_ext_t *) rsdp;
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