#ifndef AF_LOG_H
#define AF_LOG_H

#include <stdarg.h>

typedef enum AfLogLevel { AF_LOG_DEBUG = 0, AF_LOG_INFO, AF_LOG_WARNING, AF_LOG_ERROR } AfLogLevel;

typedef void (*AfLogCallback)(AfLogLevel level, const char *message, void *user);

void af_log_set_callback(AfLogCallback callback, void *user);
void af_log(AfLogLevel level, const char *format, ...);
void af_log_v(AfLogLevel level, const char *format, va_list arguments);

#endif
