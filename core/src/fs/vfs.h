#pragma once
#include <stddef.h>

typedef struct {
    struct vfs_node *root;
    void *data;
} vfs_t;

typedef struct vfs_node {
    vfs_t *vfs;
    struct vfs_node_ops *ops;
    void *data;
} vfs_node_t;

typedef struct {
    size_t size;
} vfs_attr_t;

typedef struct vfs_node_ops {
    vfs_node_t *(* lookup)(vfs_node_t *node, char *name);
    size_t (* read)(vfs_node_t *node, void *dest, size_t offset, size_t count);
    void (* attr)(vfs_node_t *node, vfs_attr_t *attr);
} vfs_node_ops_t;

vfs_node_t *vfs_lookup(vfs_t *vfs, char *path);