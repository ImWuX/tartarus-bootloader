#include "libc.h"

int memcmp(const void* lhs, const void* rhs, size_t count) {
    for(; count--; lhs++, rhs++) {
        unsigned char r = *(unsigned char *) rhs;
        unsigned char l = *(unsigned char *) lhs;
        if(r != l) return l - r;
    }
    return 0;
}