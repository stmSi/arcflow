#include "core/error.h"

#include <stdio.h>

const char *af_result_string(AfResult result) {
    static const char *const names[] = {
        "success",        "invalid argument", "out of memory",       "I/O error",
        "limit exceeded", "invalid package",  "unsupported version", "unsupported feature",
        "stale handle",   "not found",        "Vulkan error"};
    const unsigned value = (unsigned)result;
    return value < (sizeof(names) / sizeof(names[0])) ? names[value] : "unknown error";
}

void af_error_clear(AfError *error) {
    if (error != NULL) {
        *error = (AfError){0};
    }
}

void af_error_set(AfError *error, AfResult code, AfSubsystem subsystem, int32_t native_code,
                  const char *file, uint32_t line, const char *message) {
    if (error == NULL) {
        return;
    }
    *error = (AfError){.code = code,
                       .subsystem = subsystem,
                       .native_code = native_code,
                       .source_file = file,
                       .source_line = line};
    (void)snprintf(error->message, sizeof(error->message), "%s", message != NULL ? message : "");
}
