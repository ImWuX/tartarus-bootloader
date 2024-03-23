#pragma once
#include <fs/vfs.h>

typedef struct {
    char *key, *value;
} config_entry_t;

typedef struct {
    unsigned int entry_count;
    config_entry_t entries[];
} config_t;

config_t *config_parse(vfs_node_t *config_node);
void config_free(config_t *config);

char *config_read_string(config_t *config, const char *key);
bool config_read_bool(config_t *config, const char *key, bool default_value);
int config_read_int(config_t *config, const char *key, int default_value);