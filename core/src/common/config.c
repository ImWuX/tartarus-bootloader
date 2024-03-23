#include "config.h"
#include <stddef.h>
#include <common/log.h>
#include <lib/mem.h>
#include <lib/str.h>
#include <memory/heap.h>

#define IS_WHITESPACE(CHAR) ((CHAR) == ' ' || (CHAR) == '\t')

typedef struct {
    size_t start, length;
    size_t next;
} token_t;

static token_t tokenize(char *data, size_t data_size, size_t start, char terminator) {
    size_t length = 0;
    while(start + length < data_size && data[start + length] != terminator) {
        if(IS_WHITESPACE(data[start + length]) && length == 0) {
            start++;
            continue;
        }
        length++;
    }
    size_t next = start + length + 1;
    while(length > 0 && IS_WHITESPACE(data[start + length - 1])) length--;
    return (token_t) { .start = start, .length = length, .next = next };
}

static config_entry_t *find_entry(config_t *config, const char *key) {
    for(unsigned int i = 0; i < config->entry_count; i++) {
        if(strcmp(config->entries[i].key, key) != 0) continue;
        return &config->entries[i];
    }
    return NULL;
}

config_t *config_parse(vfs_node_t *config_node) {
    vfs_attr_t attr;
    config_node->ops->attr(config_node, &attr);

    char *config_data = heap_alloc(attr.size);
    size_t read_count = config_node->ops->read(config_node, config_data, 0, attr.size);
    if(read_count != attr.size) log_panic("CONFIG", "Failed to read config");

    unsigned int entry_limit = 1;
    unsigned int entry_count = 0;
    config_entry_t *entries = heap_alloc(sizeof(config_entry_t) * entry_limit);
    for(size_t i = 0; i < attr.size;) {
        token_t key = tokenize(config_data, attr.size, i, '=');
        token_t value = tokenize(config_data, attr.size, key.next + 1, '\n');
        i = value.next;
        if(key.length == 0 || value.length == 0) continue;

        if(entry_count >= entry_limit) {
            entry_limit += 5;
            entries = heap_realloc(entries, sizeof(config_entry_t) * entry_limit);
        }

        char *key_str = heap_alloc(key.length + 1);
        memcpy(key_str, &config_data[key.start], key.length);
        key_str[key.length] = 0;
        char *value_str = heap_alloc(value.length + 1);
        memcpy(value_str, &config_data[value.start], value.length);
        value_str[value.length] = 0;

        entries[entry_count++] = (config_entry_t) { .key = key_str, .value = value_str };
    }

    config_t *config = heap_alloc(sizeof(config_t) + sizeof(config_entry_t) * entry_count);
    config->entry_count = entry_count;
    memcpy(&config->entries, entries, sizeof(config_entry_t) * entry_count);
    heap_free(entries);
    return config;
}

void config_free(config_t *config) {
    for(unsigned int i = 0; i < config->entry_count; i++) {
        heap_free(config->entries[i].key);
        heap_free(config->entries[i].value);
    }
    heap_free(config);
}

char *config_read_string(config_t *config, const char *key) {
    config_entry_t *entry = find_entry(config, key);
    if(entry == NULL) return NULL;
    return entry->value;
}

bool config_read_bool(config_t *config, const char *key, bool default_value) {
    config_entry_t *entry = find_entry(config, key);
    if(entry == NULL) return default_value;
    return strcmp(entry->value, "true") == 0 || strcmp(entry->value, "TRUE") == 0;
}