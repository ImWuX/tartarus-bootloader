#ifndef LOG_H
#define LOG_H

#include <stdint.h>

void log(char *str, ...);
void log_putchar(char c);
void log_panic(char *str);
void log_clear();

#endif