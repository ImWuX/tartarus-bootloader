#include <log.h>
#include <vmm.h>
#include <pmm.h>

void load() {
    log_clear();
    log("Tartarus | Protected Mode\n");

    pmm_initialize();
    vmm_initialize(g_memap, g_memap_length);
}