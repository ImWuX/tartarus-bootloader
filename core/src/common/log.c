#include "log.h"
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <lib/format.h>
#include <sys/cpu.h>

static log_sink_t g_sink = NULL;

static void internal_fmt_list(const char *fmt, va_list list) {
    if(g_sink != NULL) format(g_sink, fmt, list);
}

static void internal_fmt(const char *fmt, ...) {
    va_list list;
    va_start(list, str);
    internal_fmt_list(fmt, list);
    va_end(list);
}

static void common_log(const char *level, const char *tag, const char *fmt, va_list list) {
    internal_fmt("[TARTARUS/%s::%s] ", tag, level);
    internal_fmt_list(fmt, list);
    internal_fmt("\n");
}

void log_init(log_sink_t sink) {
    g_sink = sink;
}

void log(const char *tag, const char *fmt, ...) {
    va_list list;
    va_start(list, str);
    common_log("LOG", tag, fmt, list);
    va_end(list);
}

void log_warning(const char *tag, const char *fmt, ...) {
    va_list list;
    va_start(list, str);
    common_log("WARN", tag, fmt, list);
    va_end(list);
}

[[noreturn]] void log_panic(const char *tag, const char *fmt, ...) {
    va_list list;
    va_start(list, str);
    common_log("PANIC", tag, fmt, list);
    va_end(list);
    for(;;) cpu_halt();
    __builtin_unreachable();
}