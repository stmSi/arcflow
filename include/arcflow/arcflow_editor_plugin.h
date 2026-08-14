#ifndef ARCFLOW_EDITOR_PLUGIN_H
#define ARCFLOW_EDITOR_PLUGIN_H

#include "arcflow/arcflow_result.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AF_EDITOR_PLUGIN_API_VERSION 1U
#define AF_EDITOR_PLUGIN_NAME_CAPACITY 64U

typedef uint64_t AfEditorPluginRegistrationToken;
typedef uint64_t AfEditorPluginCapabilities;

#define AF_EDITOR_PLUGIN_CAP_IMPORTER (UINT64_C(1) << 0U)
#define AF_EDITOR_PLUGIN_CAP_EXPORTER (UINT64_C(1) << 1U)
#define AF_EDITOR_PLUGIN_CAP_CUSTOM_PANEL (UINT64_C(1) << 2U)
#define AF_EDITOR_PLUGIN_CAP_ASSET_PROCESSOR (UINT64_C(1) << 3U)
#define AF_EDITOR_PLUGIN_CAP_VALIDATION_PASS (UINT64_C(1) << 4U)
#define AF_EDITOR_PLUGIN_CAP_COMPILER_TRANSFORM (UINT64_C(1) << 5U)
#define AF_EDITOR_PLUGIN_CAP_PREVIEW_TOOL (UINT64_C(1) << 6U)
#define AF_EDITOR_PLUGIN_CAP_ALL                                                                   \
    (AF_EDITOR_PLUGIN_CAP_IMPORTER | AF_EDITOR_PLUGIN_CAP_EXPORTER |                               \
     AF_EDITOR_PLUGIN_CAP_CUSTOM_PANEL | AF_EDITOR_PLUGIN_CAP_ASSET_PROCESSOR |                    \
     AF_EDITOR_PLUGIN_CAP_VALIDATION_PASS | AF_EDITOR_PLUGIN_CAP_COMPILER_TRANSFORM |              \
     AF_EDITOR_PLUGIN_CAP_PREVIEW_TOOL)

typedef enum AfEditorPluginDiagnosticLevel {
    AF_EDITOR_PLUGIN_DIAGNOSTIC_INFO = 0,
    AF_EDITOR_PLUGIN_DIAGNOSTIC_WARNING,
    AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR
} AfEditorPluginDiagnosticLevel;

typedef struct AfEditorPluginHostApi AfEditorPluginHostApi;
typedef struct AfEditorPluginRequest AfEditorPluginRequest;
typedef struct AfEditorPluginResponse AfEditorPluginResponse;

typedef AfResult (*AfEditorPluginLoadFn)(void *plugin_user, const AfEditorPluginHostApi *host_api);
typedef void (*AfEditorPluginUnloadFn)(void *plugin_user);
typedef AfResult (*AfEditorPluginInvokeFn)(void *plugin_user, const AfEditorPluginRequest *request,
                                           AfEditorPluginResponse *response);

struct AfEditorPluginHostApi {
    uint32_t struct_size;
    uint32_t api_version;
    AfEditorPluginRegistrationToken registration_token;
    void *host_context;
    AfResult (*report_diagnostic)(void *host_context, AfEditorPluginDiagnosticLevel level,
                                  const char *message);
    AfResult (*request_unload)(void *host_context);
};

struct AfEditorPluginRequest {
    uint32_t struct_size;
    uint32_t api_version;
    AfEditorPluginCapabilities capability;
    uint64_t request_id;
    const void *input_data;
    size_t input_size;
};

struct AfEditorPluginResponse {
    uint32_t struct_size;
    uint32_t api_version;
    const void *output_data;
    size_t output_size;
    const char *diagnostic;
};

typedef struct AfEditorPluginDescriptor {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t plugin_id;
    uint32_t plugin_version;
    uint32_t reserved;
    AfEditorPluginCapabilities capabilities;
    const char *name;
    const char *vendor;
    AfEditorPluginLoadFn load;
    AfEditorPluginUnloadFn unload;
    AfEditorPluginInvokeFn invoke;
} AfEditorPluginDescriptor;

#ifdef __cplusplus
}
#endif

#endif
