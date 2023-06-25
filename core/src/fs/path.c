#include "path.h"
#include <memory/heap.h>
#include <libc.h>

void *path_resolve(const char *path, void *fs, void *(* root_lookup)(void *fs, const char *name), void *(* dir_lookup)(void *dir, const char *name)) {
    int file_start = 0;
    void *curfile = 0;
    for(int i = 0; i < MAX_PATH; i++) {
        if(path[i] && path[i] != '/') continue;
        int filename_length = i - file_start;
        if(filename_length <= 0) continue;
        char *filename = heap_alloc(filename_length);
        memcpy(filename, path + file_start, filename_length);
        if(curfile) curfile = dir_lookup(curfile, filename);
        else curfile = root_lookup(fs, filename);
        heap_free(filename);
        file_start = i + 1;
        if(!path[i]) break;
    }
    return curfile;
}