#ifndef LOG_H
#define LOG_H

void log(char *str, ...);
void log_putchar(char c);
[[noreturn]] void log_panic(char *str);

#endif