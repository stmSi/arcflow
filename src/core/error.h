#ifndef AF_ERROR_H
#define AF_ERROR_H

#include "arcflow/arcflow_result.h"

#include <stdint.h>

typedef enum AfSubsystem {
    AF_SUBSYSTEM_CORE = 0,
    AF_SUBSYSTEM_FILE,
    AF_SUBSYSTEM_SCENE,
    AF_SUBSYSTEM_ANIMATION,
    AF_SUBSYSTEM_PACKAGE,
    AF_SUBSYSTEM_RUNTIME,
    AF_SUBSYSTEM_VULKAN
} AfSubsystem;

typedef struct AfError {
    AfResult code;
    AfSubsystem subsystem;
    int32_t native_code;
    const char *source_file;
    uint32_t source_line;
    char message[192];
} AfError;

void af_error_clear(AfError *error);
void af_error_set(AfError *error, AfResult code, AfSubsystem subsystem, int32_t native_code,
                  const char *file, uint32_t line, const char *message);

#define AF_ERROR_SET(error, code, subsystem, native_code, message)                                 \
    af_error_set((error), (code), (subsystem), (native_code), __FILE__, (uint32_t)__LINE__,        \
                 (message))

#endif
