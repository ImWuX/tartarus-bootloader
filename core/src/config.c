#include "config.h"
#include <memory/heap.h>

#define CONFIG1_FILE "TARTARUSCFG"
#define CONFIG2_DIR "TARTARUS   "
#define CONFIG2_FILE "CONFIG  CFG"

fat_file_t *config_find(fat_info_t *fs) {
    fat_file_t *cfg = fat_root_lookup(fs, CONFIG1_FILE);
    if(cfg) {
        if(!cfg->is_dir) return cfg;
        heap_free(cfg);
    }

    fat_file_t *dir = fat_root_lookup(fs, CONFIG2_DIR);
    if(dir) {
        if(dir->is_dir) {
            cfg = fat_dir_lookup(dir, CONFIG2_FILE);
            heap_free(dir);
            if(cfg) {
                if(!cfg->is_dir) return cfg;
                heap_free(cfg);
            }
        }
        heap_free(dir);
    }

    return 0;
}
