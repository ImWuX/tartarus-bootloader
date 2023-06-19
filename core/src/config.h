#ifndef CONFIG_H
#define CONFIG_H

#include <fs/fat.h>

fat_file_t *config_find(fat_info_t *fs);
int config_get_int(fat_file_t *cfg, const char *key, int fallback);
char *config_get_string(fat_file_t *cfg, const char *key, char * fallback);

#endif