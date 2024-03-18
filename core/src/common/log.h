#pragma once

typedef void (* log_sink_t)(char c);

void log_init(log_sink_t sink);

void log(const char *tag, const char *fmt, ...);
void log_warning(const char *tag, const char *fmt, ...);
[[noreturn]] void log_panic(const char *tag, const char *fmt, ...);