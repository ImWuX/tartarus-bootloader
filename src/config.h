#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

bool config_read_bool(char *key, bool fallback);
int config_read_int(char *key, int fallback);
char *config_read_string(char *key, char *fallback);
void config_initialize();

#endif