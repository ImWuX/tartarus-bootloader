#include "config.h"
#include <log.h>
#include <fat32.h>
#include <pmm.h>
#include <vmm.h>

#define CONFIG_FILE "TRTRS   CFG"
#define TRUESTR "true"
#define FALSESTR "false"

static char *g_config;
static int g_config_size;

static int findkey(char *key) {
    bool match = true;
    int key_index = 0;
    for(int i = 0; i < g_config_size; i++) {
        if(g_config[i] == '\n') {
            match = true;
            key_index = 0;
            continue;
        }
        if(match && g_config[i] == '=') {
            if(!key[key_index])
                return i + 1;
            else
                match = false;
        }
        if(match && (!key[key_index] || key[key_index++] != g_config[i])) match = false;
    }
    return -1;
}

bool config_read_bool(char *key, bool fallback) {
    int index = findkey(key);
    if(index < 0) return fallback;
    bool true_match = true;
    bool false_match = true;
    int i = 0;
    while(index < g_config_size) {
        if(g_config[index] == '\n') break;
        if(g_config[index] != TRUESTR[i]) true_match = false;
        if(g_config[index] != FALSESTR[i]) false_match = false;
        index++;
        i++;
    }
    if(!true_match && !false_match) log_panic("Config file has invalid values");
    return true_match;
}

int config_read_int(char *key, int fallback) {
    int index = findkey(key);
    if(index < 0) return fallback;
    int value = 0;
    while(index < g_config_size) {
        if(g_config[index] == '\n') break;
        if(g_config[index] < '0' || g_config[index] > '9') log_panic("Config file has invalid values");
        value *= 10;
        value += g_config[index] - '0';
        index++;
    }
    return value;
}

void config_initialize() {
    fat_file_info file_info;
    if(fat32_root_find((uint8_t *) CONFIG_FILE, &file_info)) log_panic("Could not find config file");
    if(file_info.size > 0x1000) log_panic("Config file exceeded maximum size limit");

    void *address = pmm_request_page(1);
    vmm_map_memory((uint64_t) (uintptr_t) address, (uint64_t) (uintptr_t) address);
    fat32_read(file_info.cluster_number, 0, file_info.size, address);

    g_config = (char *) address;
    g_config_size = file_info.size;
}
