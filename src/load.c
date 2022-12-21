#include <log.h>
#include <vmm.h>
#include <pmm.h>
#include <int.h>

void load() {
    log_clear();
    log("Tartarus | Protected Mode\n");

    pmm_initialize();
    log("Tartarus | Physical Memory Initialized\n");

    vmm_initialize(g_memap, g_memap_length);
    log("Tartarus | Virtual Memory initialized\n");
}