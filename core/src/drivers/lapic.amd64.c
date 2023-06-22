#include "lapic.h"
#include <cpuid.h>
#include <drivers/msr.h>
#include <log.h>

#define CPUID_APIC (1 << 9)

#define MSR_LAPIC_BASE 0x1B
#define MSR_LAPIC_BASE_MASK 0xFFFFF000

bool lapic_supported() {
    unsigned int edx = 0, unused = 0;
    if(__get_cpuid(1, &unused, &unused, &unused, &edx) == 0) return false;
    return edx & CPUID_APIC;
}

uint8_t lapic_bsp() {
    unsigned int ebx = 0, unused = 0;
    if(__get_cpuid(1, &unused, &ebx, &unused, &unused) == 0) log_panic("LAPIC", "CPUID failure when trying to retrieve BSP");
    return ebx >> 24;
}

void lapic_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *) (uintptr_t) ((msr_read(MSR_LAPIC_BASE) & MSR_LAPIC_BASE_MASK) + reg) = value;
}

uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t *) (uintptr_t) ((msr_read(MSR_LAPIC_BASE) & MSR_LAPIC_BASE_MASK) + reg);
}