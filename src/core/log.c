#include "core/log.h"

#include <stdio.h>

static AfLogCallback af_log_callback;
static void *af_log_user;

void af_log_set_callback(AfLogCallback callback, void *user) {
    af_log_callback = callback;
    af_log_user = user;
}

void af_log_v(AfLogLevel level, const char *format, va_list arguments) {
    static const char *const labels[] = {"debug", "info", "warning", "error"};
    char message[512];
    if (format == NULL) {
        return;
    }
    (void)vsnprintf(message, sizeof(message), format, arguments);
    if (af_log_callback != NULL) {
        af_log_callback(level, message, af_log_user);
    } else {
        (void)fprintf(level >= AF_LOG_WARNING ? stderr : stdout, "Arcflow [%s]: %s\n",
                      labels[(unsigned)level < 4U ? (unsigned)level : 3U], message);
    }
}

void af_log(AfLogLevel level, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    af_log_v(level, format, arguments);
    va_end(arguments);
}
