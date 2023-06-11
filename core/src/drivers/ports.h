#ifndef DRIVERS_PORTS_H
#define DRIVERS_PORTS_H

#include <stdint.h>

void ports_outb(uint16_t port, uint8_t value);
uint8_t ports_inb(uint16_t port);

#endif