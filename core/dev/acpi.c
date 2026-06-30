#include "acpi.h"

#include "arch/ptm.h"
#include "common/log.h"
#include "lib/math.h"
#include "lib/mem.h"
#include "memory/pmm.h"

#include <stddef.h>

acpi_sdt_header_t *acpi_find_table(acpi_rsdp_t *rsdp, const char *signature) {
    if(rsdp == NULL) {
        log(LOG_LEVEL_ERROR, "acpi: RSDP is NULL");
        return NULL;
    }
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
    return NULL;
}


bool check_padding(uint64_t base, uint64_t length) {
    uint64_t top = base + length;

    for(size_t i = 0; i < g_pmm_map_size; i++) {
        pmm_map_entry_t *entry = &g_pmm_map[i];
        if(entry->base >= top || entry->base + entry->length <= base) { continue; }
        if(entry->type != PMM_MAP_TYPE_FREE && entry->type != PMM_MAP_TYPE_RESERVED) { return false; }
    }

    return true;
}

void map_table(uintptr_t addr, size_t length) {
    uint64_t aligned_base = MATH_FLOOR(addr, PTM_PAGE_GRANULARITY);
    uint64_t aligned_top = MATH_CEIL(addr + length, PTM_PAGE_GRANULARITY);

    if(!check_padding(aligned_base, addr - aligned_base)) { aligned_base = addr; }
    if(!check_padding(addr + length, aligned_top - (addr + length))) { aligned_base = addr; }

    pmm_map_type_t map_type = -1;
    for(size_t i = 0; i < g_pmm_map_size; i++) {
        pmm_map_entry_t *entry = &g_pmm_map[i];
        if(addr >= entry->base && addr < entry->base + entry->length) {
            map_type = entry->type;
            break;
        }
    }

    if(map_type != PMM_MAP_TYPE_ACPI_RECLAIMABLE && map_type == PMM_MAP_TYPE_RESERVED) { pmm_map_set(aligned_base, aligned_top - aligned_base, PMM_MAP_TYPE_ACPI_TABLES, true); }
}

void arch_acpi_map_tables(acpi_rsdp_t *rsdp) {
    if(rsdp == NULL) {
        log(LOG_LEVEL_ERROR, "acpi: RSDP is NULL");
        return;
    }

    int entry_count;
    bool extended = false;
    uintptr_t buffer;

    size_t rsdp_length;
    if(rsdp->revision < 2) {
        rsdp_length = sizeof(acpi_rsdp_t);
    } else {
        acpi_rsdp_ext_t *rsdp_ext = (acpi_rsdp_ext_t *) rsdp;
        rsdp_length = rsdp_ext->length;
    }

    map_table((uintptr_t) rsdp, rsdp_length);

    if(rsdp->revision > 0) {
        acpi_rsdp_ext_t *rsdp_ext = (acpi_rsdp_ext_t *) rsdp;
        acpi_sdt_header_t *xsdt = (acpi_sdt_header_t *) (uintptr_t) rsdp_ext->xsdt_address;
        entry_count = (xsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint64_t);
        extended = true;
        buffer = (uintptr_t) xsdt + sizeof(acpi_sdt_header_t);

        map_table((uintptr_t) xsdt, xsdt->length);
    } else {
        acpi_sdt_header_t *rsdt = (acpi_sdt_header_t *) (uintptr_t) rsdp->rsdt_address;
        entry_count = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
        buffer = (uintptr_t) rsdt + sizeof(acpi_sdt_header_t);

        map_table((uintptr_t) rsdt, rsdt->length);
    }

    for(int i = 0; i < entry_count; i++) {
        acpi_sdt_header_t *sdt = (acpi_sdt_header_t *) (uintptr_t) (extended ? *(uint64_t *) buffer : *(uint32_t *) buffer);
        buffer += extended ? sizeof(uint64_t) : sizeof(uint32_t);
        log(LOG_LEVEL_INFO, "acpi: mapping table %c%c%c%c at %#lx", sdt->signature[0], sdt->signature[1], sdt->signature[2], sdt->signature[3], (uintptr_t) sdt);
        map_table((uintptr_t) sdt, sdt->length);
    }

    acpi_sdt_header_t *fadt = acpi_find_table(rsdp, "FACP");
    if(fadt == NULL) {
        log(LOG_LEVEL_WARN, "acpi: FADT table not found");
        return;
    }

    void *fadt_base = (void *) fadt;

    if(fadt->length >= 132 + 8) {
        uint64_t x_facs;
        memcpy(&x_facs, fadt_base + 132, sizeof(uint64_t));
        acpi_sdt_header_t *hdr = (acpi_sdt_header_t *) x_facs;
        if(x_facs != 0) { map_table((uintptr_t) hdr, hdr->length); }
    }
    if(fadt->length >= 140 + 8) {
        uint64_t x_dsdt;
        memcpy(&x_dsdt, fadt_base + 140, sizeof(uint64_t));
        acpi_sdt_header_t *hdr = (acpi_sdt_header_t *) x_dsdt;
        if(x_dsdt != 0) { map_table((uintptr_t) hdr, hdr->length); }
    }
    if(fadt->length >= 36 + 4) {
        uint32_t facs;
        memcpy(&facs, fadt_base + 36, sizeof(uint32_t));
        acpi_sdt_header_t *hdr = (acpi_sdt_header_t *) (uintptr_t) facs;
        if(facs != 0) { map_table((uintptr_t) hdr, hdr->length); }
    }
    if(fadt->length >= 40 + 4) {
        uint32_t dsdt;
        memcpy(&dsdt, fadt_base + 40, sizeof(uint32_t));
        acpi_sdt_header_t *hdr = (acpi_sdt_header_t *) (uintptr_t) dsdt;
        if(dsdt != 0) { map_table((uintptr_t) hdr, hdr->length); }
    }
}
