#include "config.h"
#include <log.h>
#include <memory/heap.h>

#define CONFIG1_FILE "TARTARUSCFG"
#define CONFIG2_DIR "TARTARUS   "
#define CONFIG2_FILE "CONFIG  CFG"

static bool find_value(char *config_data, uint32_t config_data_size, const char *key, uint32_t *offset) {
    bool match = true;
    uint32_t local_offset = 0;
    for(uint32_t i = 0; i < config_data_size; i++) {
        switch(config_data[i]) {
            case ' ':
            case '\t':
                break;
            case '=':
                if(!match) break;
                *offset = i + 1;
                return false;
            case '\n':
                match = true;
                local_offset = 0;
                break;
            default:
                match = key[local_offset++] == config_data[i];
                break;
        }
    }
    return true;
}

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

int config_get_int(fat_file_t *cfg, const char *key, int fallback) {
    char *config_data = heap_alloc(cfg->size);
    if(fat_read(cfg, 0, cfg->size, config_data) != cfg->size) {
        log_warning("CONFIG", "Failed to read config. Using fallback value for %s.\n", key);
        return fallback;
    }
    uint32_t value_offset;
    if(find_value(config_data, cfg->size, key, &value_offset)) goto ret_fallback;
    int value = 0;
    bool strip = true;
    bool negative = false;
    for(uint32_t i = value_offset; i < cfg->size; i++) {
        if(config_data[i] == '\n') break;
        if(strip && (config_data[i] == ' ' || config_data[i] == '\t')) continue;
        if(strip && !negative && config_data[i] == '-') {
            strip = false;
            negative = true;
            continue;
        }
        if(config_data[i] < '0' || config_data[i] > '9') {
            log_warning("CONIFG", "Invalid value for %s. Using fallback value.\n", key);
            goto ret_fallback;
        }
        strip = false;
        value *= 10;
        value += config_data[i] - '0';
    }
    if(negative) value *= -1;
    heap_free(config_data);
    return value;
    ret_fallback:
    heap_free(config_data);
    return fallback;
}