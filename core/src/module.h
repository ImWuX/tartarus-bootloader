#ifndef MODULE_H
#define MODULE_H

#include <stdint.h>

typedef struct module {
    char *name;
    uint64_t base;
    uint64_t size;
    struct module *next;
} module_t;

#endif