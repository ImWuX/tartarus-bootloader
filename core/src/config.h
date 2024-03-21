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
char *config_read_value(config_t *config, const char *key, char *default_value);