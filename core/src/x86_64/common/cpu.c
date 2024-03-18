#include <cpu.h>

void cpu_halt() {
    asm volatile("hlt");
}