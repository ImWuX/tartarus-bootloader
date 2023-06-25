#ifndef FS_PATH_H
#define FS_PATH_H

#define MAX_PATH 256

void *path_resolve(const char *path, void *fs, void *(* root_lookup)(void *fs, const char *name), void *(* dir_lookup)(void *dir, const char *name));

#endif