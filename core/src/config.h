#ifndef CONFIG_H
#define CONFIG_H

#include <fs/fat.h>

fat_file_t *config_find(fat_info_t *fs);

#endif