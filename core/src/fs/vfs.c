#include "vfs.h"
#include <lib/mem.h>
#include <memory/heap.h>

vfs_node_t *vfs_lookup(vfs_t *vfs, char *path) {
    int comp_start = 0, comp_end = 0;

    vfs_node_t *current_node = vfs->root;
    do {
        switch(path[comp_end]) {
            case 0:
            case '/':
                if(comp_start == comp_end) {
                    comp_start++;
                    break;
                }
                int comp_length = comp_end - comp_start;
                char *component = heap_alloc(comp_length + 1);
                memcpy(component, path + comp_start, comp_length);
                component[comp_length] = 0;
                comp_start = comp_end + 1;

                current_node = current_node->ops->lookup(current_node, component);
                heap_free(component);

                if(current_node == NULL) return NULL;
                break;
        }
    } while(path[comp_end++]);
    return current_node;
}