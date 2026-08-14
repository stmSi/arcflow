#ifndef AF_PLUGIN_HOST_H
#define AF_PLUGIN_HOST_H

#include "arcflow/arcflow_editor_plugin.h"
#include "core/allocator.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_PLUGIN_HOST_API_VERSION 1U
#define AF_PLUGIN_HOST_MAX_PLUGINS 64U
#define AF_PLUGIN_HOST_MAX_DIAGNOSTICS 256U
#define AF_PLUGIN_HOST_MAX_CALLBACK_DEPTH 16U
#define AF_PLUGIN_HOST_DIAGNOSTIC_CAPACITY 192U
#define AF_PLUGIN_HOST_DEFAULT_MAX_TRANSFER_SIZE (64U * 1024U * 1024U)

typedef struct AfPluginHost AfPluginHost;

typedef enum AfPluginDiagnosticStage {
    AF_PLUGIN_STAGE_REGISTRATION = 0,
    AF_PLUGIN_STAGE_LOAD,
    AF_PLUGIN_STAGE_CALLBACK,
    AF_PLUGIN_STAGE_COMPILER_VALIDATION,
    AF_PLUGIN_STAGE_UNLOAD,
    AF_PLUGIN_STAGE_CRASH
} AfPluginDiagnosticStage;

typedef struct AfPluginDiagnostic {
    AfResult result;
    AfEditorPluginDiagnosticLevel level;
    AfPluginDiagnosticStage stage;
    AfEditorPluginRegistrationToken registration_token;
    AfEditorPluginCapabilities capability;
    uint64_t sequence;
    char plugin_name[AF_EDITOR_PLUGIN_NAME_CAPACITY];
    char message[AF_PLUGIN_HOST_DIAGNOSTIC_CAPACITY];
} AfPluginDiagnostic;

typedef void (*AfPluginDiagnosticFn)(void *user, const AfPluginDiagnostic *diagnostic);
typedef AfResult (*AfPluginCompilerValidateFn)(void *user, const void *data, size_t size);
typedef AfResult (*AfPluginOutputConsumerFn)(void *user, const void *data, size_t size);

typedef struct AfPluginHostCreateInfo {
    uint32_t struct_size;
    uint32_t api_version;
    AfAllocator *allocator;
    size_t maximum_transfer_size;
    AfPluginDiagnosticFn diagnostic;
    void *diagnostic_user;
    AfPluginCompilerValidateFn validate_compiler_output;
    void *compiler_validation_user;
} AfPluginHostCreateInfo;

typedef struct AfPluginRegistrationInfo {
    uint32_t struct_size;
    uint32_t api_version;
    const AfEditorPluginDescriptor *descriptor;
    void *plugin_user;
    bool trusted;
} AfPluginRegistrationInfo;

typedef struct AfPluginInvokeInfo {
    uint32_t struct_size;
    uint32_t api_version;
    AfEditorPluginCapabilities capability;
    uint64_t request_id;
    const void *input_data;
    size_t input_size;
    AfPluginOutputConsumerFn consume_output;
    void *output_user;
} AfPluginInvokeInfo;

AfResult af_plugin_host_create(const AfPluginHostCreateInfo *create_info, AfPluginHost **out_host);
void af_plugin_host_destroy(AfPluginHost *host);
AfResult af_plugin_host_register(AfPluginHost *host, const AfPluginRegistrationInfo *info,
                                 AfEditorPluginRegistrationToken *out_token);
AfResult af_plugin_host_unregister(AfPluginHost *host, AfEditorPluginRegistrationToken token);
AfResult af_plugin_host_invoke(AfPluginHost *host, AfEditorPluginRegistrationToken token,
                               const AfPluginInvokeInfo *info);
AfResult af_plugin_host_find(const AfPluginHost *host, uint64_t plugin_id,
                             AfEditorPluginRegistrationToken *out_token);
uint32_t af_plugin_host_registration_count(const AfPluginHost *host);
uint32_t af_plugin_host_diagnostic_count(const AfPluginHost *host);
AfResult af_plugin_host_diagnostic(const AfPluginHost *host, uint32_t newest_offset,
                                   AfPluginDiagnostic *out_diagnostic);
AfResult af_plugin_host_active_callback(const AfPluginHost *host,
                                        AfEditorPluginRegistrationToken *out_token,
                                        AfEditorPluginCapabilities *out_capability);
AfResult af_plugin_host_report_crash(AfPluginHost *host, AfEditorPluginRegistrationToken token,
                                     const char *message);

#endif
