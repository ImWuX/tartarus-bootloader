#ifndef CONFIG_H
#define CONFIG_H

#include <fs/fat.h>

fat_file_t *config_find(fat_info_t *fs);
bool config_get_int_ext(fat_file_t *cfg, const char *key, int *out);
int config_get_int(fat_file_t *cfg, const char *key, int fallback);
bool config_get_string_exto(fat_file_t *cfg, const char *key, char **out, int occurance);
bool config_get_string_ext(fat_file_t *cfg, const char *key, char **out);
char *config_get_string(fat_file_t *cfg, const char *key, char *fallback);
bool config_get_bool_ext(fat_file_t *cfg, const char *key, bool *out);
bool config_get_bool(fat_file_t *cfg, const char *key, bool fallback);

#endif