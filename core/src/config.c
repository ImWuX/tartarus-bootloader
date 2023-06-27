#include "config.h"
#include <log.h>
#include <libc.h>
#include <memory/heap.h>

#define CONFIG1_FILE "TARTARUSCFG"
#define CONFIG2_DIR "TARTARUS   "
#define CONFIG2_FILE "CONFIG  CFG"

static bool find_value(char *config_data, uint32_t config_data_size, const char *key, uint32_t *offset) {
    bool match = true;
    bool key_end = false;
    uint32_t local_offset = 0;
    for(uint32_t i = 0; i < config_data_size; i++) {
        switch(config_data[i]) {
            case ' ':
            case '\t':
                if(!key_end) match = false;
                break;
            case '=':
                if(!match || !key_end) break;
                *offset = i + 1;
                return false;
            case '\n':
                key_end = false;
                match = true;
                local_offset = 0;
                break;
            default:
                if(key_end) break;
                match = key[local_offset++] == config_data[i];
                key_end = key[local_offset] == 0;
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

bool config_get_int_ext(fat_file_t *cfg, const char *key, int *out) {
    char *config_data = heap_alloc(cfg->size);
    if(fat_read(cfg, 0, cfg->size, config_data) != cfg->size) log_panic("CONFIG", "Failed to read config\n");
    uint32_t value_offset;
    if(find_value(config_data, cfg->size, key, &value_offset)) goto ret_fail;
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
            log_warning("CONIFG", "Invalid value for %s\n", key);
            goto ret_fail;
        }
        strip = false;
        value *= 10;
        value += config_data[i] - '0';
    }
    if(negative) value *= -1;
    heap_free(config_data);
    *out = value;
    return false;
    ret_fail:
    heap_free(config_data);
    return true;

}

int config_get_int(fat_file_t *cfg, const char *key, int fallback) {
    int value;
    if(config_get_int_ext(cfg, key, &value)) return fallback;
    return value;
}

bool config_get_string_ext(fat_file_t *cfg, const char *key, char **out) {
    char *config_data = heap_alloc(cfg->size);
    if(fat_read(cfg, 0, cfg->size, config_data) != cfg->size) log_panic("CONFIG", "Failed to read config\n");
    uint32_t value_offset;
    if(find_value(config_data, cfg->size, key, &value_offset)) goto ret_fail;
    int str_length = 0;
    bool strip = true;
    for(uint32_t i = value_offset; i < cfg->size; i++) {
        if(strip && (config_data[i] == ' ' || config_data[i] == '\t')) {
            value_offset++;
            continue;
        }
        if(config_data[i] == '\n') break;
        strip = false;
        str_length++;
    }
    char *str = heap_alloc(str_length + 1);
    memcpy(str, config_data + value_offset, str_length);
    str[str_length] = 0;
    *out = str;
    heap_free(config_data);
    return false;
    ret_fail:
    heap_free(config_data);
    return true;

}

char *config_get_string(fat_file_t *cfg, const char *key, char *fallback) {
    char *value;
    if(config_get_string_ext(cfg, key, &value)) return fallback;
    return value;
}

bool config_get_bool_ext(fat_file_t *cfg, const char *key, bool *out) {
    char *str;
    if(config_get_string_ext(cfg, key, &str)) return true;
    *out = strcmp(str, "true") == 0;
    heap_free(str);
    return false;
}

bool config_get_bool(fat_file_t *cfg, const char *key, bool fallback) {
    bool value;
    if(config_get_bool_ext(cfg, key, &value)) return fallback;
    return value;
}