#ifndef LOG_H
#define LOG_H

void log(char *str, ...);
void log_putchar(char c);
void log_warning(char *src, char *str, ...);
[[noreturn]] void log_panic(char *src, char *str, ...);

#endif