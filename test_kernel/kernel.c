#include <stdint.h>
#include <stdbool.h>
#include <stdnoreturn.h>

noreturn void kmain() {
    uint8_t *buf = (uint8_t *) 0xB8000;

    for(int i = 0; i < 5; i++) {
        buf[i * 2] = 'Z';
    }

    while(true) asm("hlt");
}