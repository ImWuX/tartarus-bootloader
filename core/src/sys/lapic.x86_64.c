#include "lapic.x86_64.h"
#include <cpuid.h>
#include <log.h>
#include <memory/pmm.h>
#include <sys/msr.x86_64.h>

#define CPUID_APIC (1 << 9)

#define MSR_LAPIC_BASE 0x1B
#define MSR_LAPIC_BASE_MASK 0xFFFFF000

#define REG_ID 0x20
#define REG_ICR1 0x300
#define REG_ICR2 0x310

#define ICR_ASSERT 0x4000
#define ICR_INIT 0x500
#define ICR_STARTUP 0x600

static inline void lapic_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *) (uintptr_t) ((msr_read(MSR_LAPIC_BASE) & MSR_LAPIC_BASE_MASK) + reg) = value;
}

static inline uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t *) (uintptr_t) ((msr_read(MSR_LAPIC_BASE) & MSR_LAPIC_BASE_MASK) + reg);
}

bool lapic_is_supported() {
    unsigned int edx = 0, unused = 0;
    if(__get_cpuid(1, &unused, &unused, &unused, &edx) == 0) return false;
    return edx & CPUID_APIC;
}

uint32_t lapic_id() {
    return (uint8_t) (lapic_read(REG_ID) >> 24);
}

void lapic_ipi_init(uint8_t lapic_id) {
    lapic_write(REG_ICR2, (uint32_t) lapic_id << 24);
    lapic_write(REG_ICR1, ICR_ASSERT | ICR_INIT);
}

void lapic_ipi_startup(uint8_t lapic_id, void *startup_page) {
    lapic_write(REG_ICR2, (uint32_t) lapic_id << 24);
    lapic_write(REG_ICR1, ICR_ASSERT | ICR_STARTUP | ((uintptr_t) startup_page / PMM_PAGE_SIZE));
}