#include "plugin/plugin_host.h"

#include <stdalign.h>
#include <stdio.h>
#include <string.h>

typedef struct AfPluginSlot AfPluginSlot;

typedef struct AfPluginCallbackFrame {
    AfEditorPluginRegistrationToken token;
    AfEditorPluginCapabilities capability;
} AfPluginCallbackFrame;

struct AfPluginSlot {
    struct AfPluginHost *host;
    AfEditorPluginDescriptor descriptor;
    AfEditorPluginHostApi host_api;
    void *plugin_user;
    AfEditorPluginRegistrationToken token;
    uint32_t generation;
    uint32_t callback_depth;
    uint32_t failure_count;
    bool occupied;
    bool initialized;
    bool pending_unload;
    bool unloading;
    bool crashed;
    char name[AF_EDITOR_PLUGIN_NAME_CAPACITY];
    char vendor[AF_EDITOR_PLUGIN_NAME_CAPACITY];
};

struct AfPluginHost {
    AfAllocator *allocator;
    size_t maximum_transfer_size;
    AfPluginDiagnosticFn diagnostic;
    void *diagnostic_user;
    AfPluginCompilerValidateFn validate_compiler_output;
    void *compiler_validation_user;
    AfPluginSlot slots[AF_PLUGIN_HOST_MAX_PLUGINS];
    AfPluginDiagnostic diagnostics[AF_PLUGIN_HOST_MAX_DIAGNOSTICS];
    uint32_t diagnostic_start;
    uint32_t diagnostic_count;
    uint64_t diagnostic_sequence;
    AfPluginCallbackFrame callback_stack[AF_PLUGIN_HOST_MAX_CALLBACK_DEPTH];
    uint32_t callback_depth;
    uint32_t registration_count;
};

static bool af_plugin_string_valid(const char *text, size_t capacity, bool allow_empty) {
    if (text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < capacity; ++index) {
        if (text[index] == '\0') {
            return allow_empty || index != 0U;
        }
    }
    return false;
}

static void af_plugin_copy_string(char *output, size_t capacity, const char *text) {
    size_t index = 0U;
    if (output == NULL || capacity == 0U) {
        return;
    }
    if (text != NULL) {
        while (index + 1U < capacity && text[index] != '\0') {
            output[index] = text[index];
            ++index;
        }
    }
    output[index] = '\0';
}

static void af_plugin_diagnostic(AfPluginHost *host, const AfPluginSlot *slot, AfResult result,
                                 AfEditorPluginDiagnosticLevel level, AfPluginDiagnosticStage stage,
                                 AfEditorPluginCapabilities capability, const char *name,
                                 const char *message) {
    AfPluginDiagnostic *diagnostic = NULL;
    uint32_t index = 0U;
    if (host == NULL) {
        return;
    }
    if (host->diagnostic_count == AF_PLUGIN_HOST_MAX_DIAGNOSTICS) {
        host->diagnostic_start = (host->diagnostic_start + 1U) % AF_PLUGIN_HOST_MAX_DIAGNOSTICS;
        --host->diagnostic_count;
    }
    index = (host->diagnostic_start + host->diagnostic_count) % AF_PLUGIN_HOST_MAX_DIAGNOSTICS;
    diagnostic = &host->diagnostics[index];
    *diagnostic = (AfPluginDiagnostic){.result = result,
                                       .level = level,
                                       .stage = stage,
                                       .registration_token = slot == NULL ? 0U : slot->token,
                                       .capability = capability,
                                       .sequence = ++host->diagnostic_sequence};
    af_plugin_copy_string(diagnostic->plugin_name, sizeof(diagnostic->plugin_name),
                          slot == NULL ? name : slot->name);
    af_plugin_copy_string(diagnostic->message, sizeof(diagnostic->message), message);
    ++host->diagnostic_count;
    if (host->diagnostic != NULL) {
        host->diagnostic(host->diagnostic_user, diagnostic);
    }
}

static AfEditorPluginRegistrationToken af_plugin_make_token(uint32_t index, uint32_t generation) {
    return ((uint64_t)generation << 32U) | ((uint64_t)index + 1U);
}

static AfResult af_plugin_slot(AfPluginHost *host, AfEditorPluginRegistrationToken token,
                               AfPluginSlot **out_slot) {
    const uint32_t encoded_index = (uint32_t)token;
    const uint32_t generation = (uint32_t)(token >> 32U);
    uint32_t index = 0U;
    if (host == NULL || out_slot == NULL || encoded_index == 0U || generation == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    index = encoded_index - 1U;
    if (index >= AF_PLUGIN_HOST_MAX_PLUGINS || !host->slots[index].occupied ||
        host->slots[index].generation != generation || host->slots[index].token != token) {
        return AF_RESULT_STALE_HANDLE;
    }
    *out_slot = &host->slots[index];
    return AF_RESULT_SUCCESS;
}

static AfResult af_plugin_callback_begin(AfPluginHost *host, AfPluginSlot *slot,
                                         AfEditorPluginCapabilities capability) {
    if (host == NULL || slot == NULL || !slot->occupied || slot->unloading || slot->crashed) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (host->callback_depth >= AF_PLUGIN_HOST_MAX_CALLBACK_DEPTH) {
        af_plugin_diagnostic(host, slot, AF_RESULT_LIMIT_EXCEEDED,
                             AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR, AF_PLUGIN_STAGE_CALLBACK,
                             capability, NULL, "Plugin callback nesting limit exceeded");
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    host->callback_stack[host->callback_depth++] =
        (AfPluginCallbackFrame){.token = slot->token, .capability = capability};
    ++slot->callback_depth;
    return AF_RESULT_SUCCESS;
}

static void af_plugin_finish_unload(AfPluginHost *host, AfPluginSlot *slot);

static void af_plugin_callback_end(AfPluginHost *host, AfPluginSlot *slot) {
    if (host == NULL || slot == NULL || host->callback_depth == 0U || slot->callback_depth == 0U) {
        return;
    }
    --host->callback_depth;
    --slot->callback_depth;
    if (slot->pending_unload && slot->callback_depth == 0U && !slot->unloading) {
        af_plugin_finish_unload(host, slot);
    }
}

static AfResult af_plugin_report_from_api(void *context, AfEditorPluginDiagnosticLevel level,
                                          const char *message) {
    AfPluginSlot *slot = context;
    if (slot == NULL || slot->host == NULL || !slot->occupied ||
        level > AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR ||
        !af_plugin_string_valid(message, AF_PLUGIN_HOST_DIAGNOSTIC_CAPACITY, true)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    af_plugin_diagnostic(
        slot->host, slot,
        level == AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR ? AF_RESULT_INVALID_ARGUMENT : AF_RESULT_SUCCESS,
        level, AF_PLUGIN_STAGE_CALLBACK,
        slot->callback_depth == 0U
            ? 0U
            : slot->host->callback_stack[slot->host->callback_depth - 1U].capability,
        NULL, message);
    return AF_RESULT_SUCCESS;
}

static AfResult af_plugin_request_unload_from_api(void *context) {
    AfPluginSlot *slot = context;
    if (slot == NULL || slot->host == NULL || !slot->occupied) {
        return AF_RESULT_STALE_HANDLE;
    }
    if (slot->unloading) {
        return AF_RESULT_SUCCESS;
    }
    return af_plugin_host_unregister(slot->host, slot->token);
}

static void af_plugin_clear_slot(AfPluginHost *host, AfPluginSlot *slot) {
    const uint32_t generation = slot->generation;
    *slot = (AfPluginSlot){.host = host, .generation = generation};
    if (host->registration_count != 0U) {
        --host->registration_count;
    }
}

static void af_plugin_finish_unload(AfPluginHost *host, AfPluginSlot *slot) {
    AfResult callback_result = AF_RESULT_SUCCESS;
    if (host == NULL || slot == NULL || !slot->occupied || slot->callback_depth != 0U ||
        slot->unloading) {
        return;
    }
    slot->pending_unload = false;
    if (slot->initialized && slot->descriptor.unload != NULL) {
        callback_result = af_plugin_callback_begin(host, slot, 0U);
        if (callback_result == AF_RESULT_SUCCESS) {
            slot->unloading = true;
            slot->descriptor.unload(slot->plugin_user);
            af_plugin_callback_end(host, slot);
        } else {
            af_plugin_diagnostic(host, slot, AF_RESULT_LIMIT_EXCEEDED,
                                 AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR, AF_PLUGIN_STAGE_UNLOAD, 0U,
                                 NULL, "Plugin unload callback could not run safely");
        }
    }
    slot->unloading = true;
    if (slot->occupied) {
        af_plugin_clear_slot(host, slot);
    }
}

static AfResult af_plugin_descriptor_validate(const AfEditorPluginDescriptor *descriptor) {
    if (descriptor == NULL || descriptor->struct_size < sizeof(*descriptor) ||
        descriptor->api_version != AF_EDITOR_PLUGIN_API_VERSION || descriptor->plugin_id == 0U ||
        descriptor->plugin_version == 0U || descriptor->reserved != 0U ||
        descriptor->capabilities == 0U ||
        (descriptor->capabilities & ~AF_EDITOR_PLUGIN_CAP_ALL) != 0U ||
        !af_plugin_string_valid(descriptor->name, AF_EDITOR_PLUGIN_NAME_CAPACITY, false) ||
        !af_plugin_string_valid(descriptor->vendor, AF_EDITOR_PLUGIN_NAME_CAPACITY, true) ||
        descriptor->invoke == NULL) {
        return descriptor != NULL && descriptor->api_version != AF_EDITOR_PLUGIN_API_VERSION
                   ? AF_RESULT_UNSUPPORTED_VERSION
                   : AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_plugin_host_create(const AfPluginHostCreateInfo *create_info, AfPluginHost **out_host) {
    AfPluginHost *host = NULL;
    if (create_info == NULL || out_host == NULL ||
        create_info->struct_size < sizeof(*create_info) ||
        create_info->api_version != AF_PLUGIN_HOST_API_VERSION || create_info->allocator == NULL) {
        return create_info != NULL && create_info->api_version != AF_PLUGIN_HOST_API_VERSION
                   ? AF_RESULT_UNSUPPORTED_VERSION
                   : AF_RESULT_INVALID_ARGUMENT;
    }
    *out_host = NULL;
    host = af_allocate(create_info->allocator, sizeof(*host), alignof(AfPluginHost));
    if (host == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(host, 0, sizeof(*host));
    host->allocator = create_info->allocator;
    host->maximum_transfer_size = create_info->maximum_transfer_size == 0U
                                      ? AF_PLUGIN_HOST_DEFAULT_MAX_TRANSFER_SIZE
                                      : create_info->maximum_transfer_size;
    host->diagnostic = create_info->diagnostic;
    host->diagnostic_user = create_info->diagnostic_user;
    host->validate_compiler_output = create_info->validate_compiler_output;
    host->compiler_validation_user = create_info->compiler_validation_user;
    for (uint32_t index = 0U; index < AF_PLUGIN_HOST_MAX_PLUGINS; ++index) {
        host->slots[index].host = host;
    }
    *out_host = host;
    return AF_RESULT_SUCCESS;
}

void af_plugin_host_destroy(AfPluginHost *host) {
    AfAllocator *allocator = NULL;
    if (host == NULL) {
        return;
    }
    if (host->callback_depth != 0U) {
        af_plugin_diagnostic(host, NULL, AF_RESULT_INVALID_ARGUMENT,
                             AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR, AF_PLUGIN_STAGE_UNLOAD, 0U,
                             "Arcflow Plugin Host", "Host destruction rejected during callback");
        return;
    }
    for (uint32_t index = 0U; index < AF_PLUGIN_HOST_MAX_PLUGINS; ++index) {
        if (host->slots[index].occupied) {
            af_plugin_finish_unload(host, &host->slots[index]);
        }
    }
    allocator = host->allocator;
    af_deallocate(allocator, host);
}

AfResult af_plugin_host_register(AfPluginHost *host, const AfPluginRegistrationInfo *info,
                                 AfEditorPluginRegistrationToken *out_token) {
    AfPluginSlot *slot = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (host == NULL || info == NULL || out_token == NULL || info->struct_size < sizeof(*info) ||
        info->api_version != AF_PLUGIN_HOST_API_VERSION) {
        return info != NULL && info->api_version != AF_PLUGIN_HOST_API_VERSION
                   ? AF_RESULT_UNSUPPORTED_VERSION
                   : AF_RESULT_INVALID_ARGUMENT;
    }
    *out_token = 0U;
    result = af_plugin_descriptor_validate(info->descriptor);
    if (result != AF_RESULT_SUCCESS) {
        af_plugin_diagnostic(
            host, NULL, result, AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR, AF_PLUGIN_STAGE_REGISTRATION, 0U,
            info->descriptor == NULL ? "Unknown plugin" : info->descriptor->name,
            result == AF_RESULT_UNSUPPORTED_VERSION ? "Plugin API version is incompatible"
                                                    : "Plugin descriptor is invalid");
        return result;
    }
    if (!info->trusted) {
        af_plugin_diagnostic(host, NULL, AF_RESULT_UNSUPPORTED_FEATURE,
                             AF_EDITOR_PLUGIN_DIAGNOSTIC_WARNING, AF_PLUGIN_STAGE_REGISTRATION,
                             info->descriptor->capabilities, info->descriptor->name,
                             "Plugin registration requires explicit trusted opt-in");
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    if ((info->descriptor->capabilities & AF_EDITOR_PLUGIN_CAP_COMPILER_TRANSFORM) != 0U &&
        host->validate_compiler_output == NULL) {
        af_plugin_diagnostic(host, NULL, AF_RESULT_UNSUPPORTED_FEATURE,
                             AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR, AF_PLUGIN_STAGE_REGISTRATION,
                             AF_EDITOR_PLUGIN_CAP_COMPILER_TRANSFORM, info->descriptor->name,
                             "Compiler transform requires the host package-validation gate");
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    if (af_plugin_host_find(host, info->descriptor->plugin_id,
                            &(AfEditorPluginRegistrationToken){0}) == AF_RESULT_SUCCESS) {
        af_plugin_diagnostic(host, NULL, AF_RESULT_INVALID_ARGUMENT,
                             AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR, AF_PLUGIN_STAGE_REGISTRATION,
                             info->descriptor->capabilities, info->descriptor->name,
                             "A plugin with this stable identifier is already registered");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < AF_PLUGIN_HOST_MAX_PLUGINS; ++index) {
        if (!host->slots[index].occupied) {
            slot = &host->slots[index];
            ++slot->generation;
            if (slot->generation == 0U) {
                ++slot->generation;
            }
            slot->occupied = true;
            slot->initialized = true;
            slot->descriptor = *info->descriptor;
            slot->plugin_user = info->plugin_user;
            slot->token = af_plugin_make_token(index, slot->generation);
            af_plugin_copy_string(slot->name, sizeof(slot->name), info->descriptor->name);
            af_plugin_copy_string(slot->vendor, sizeof(slot->vendor), info->descriptor->vendor);
            slot->descriptor.name = slot->name;
            slot->descriptor.vendor = slot->vendor;
            slot->host_api =
                (AfEditorPluginHostApi){.struct_size = sizeof(AfEditorPluginHostApi),
                                        .api_version = AF_EDITOR_PLUGIN_API_VERSION,
                                        .registration_token = slot->token,
                                        .host_context = slot,
                                        .report_diagnostic = af_plugin_report_from_api,
                                        .request_unload = af_plugin_request_unload_from_api};
            ++host->registration_count;
            break;
        }
    }
    if (slot == NULL) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (slot->descriptor.load != NULL) {
        result = af_plugin_callback_begin(host, slot, 0U);
        if (result == AF_RESULT_SUCCESS) {
            result = slot->descriptor.load(slot->plugin_user, &slot->host_api);
            af_plugin_callback_end(host, slot);
        }
        if (result != AF_RESULT_SUCCESS) {
            if (slot->occupied) {
                ++slot->failure_count;
                af_plugin_diagnostic(host, slot, result, AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR,
                                     AF_PLUGIN_STAGE_LOAD, 0U, NULL, "Plugin load callback failed");
                af_plugin_finish_unload(host, slot);
            }
            return result;
        }
        if (!slot->occupied) {
            return AF_RESULT_STALE_HANDLE;
        }
    }
    *out_token = slot->token;
    af_plugin_diagnostic(host, slot, AF_RESULT_SUCCESS, AF_EDITOR_PLUGIN_DIAGNOSTIC_INFO,
                         AF_PLUGIN_STAGE_REGISTRATION, slot->descriptor.capabilities, NULL,
                         "Trusted plugin registered");
    return AF_RESULT_SUCCESS;
}

AfResult af_plugin_host_unregister(AfPluginHost *host, AfEditorPluginRegistrationToken token) {
    AfPluginSlot *slot = NULL;
    const AfResult result = af_plugin_slot(host, token, &slot);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    if (slot->unloading) {
        return AF_RESULT_SUCCESS;
    }
    slot->pending_unload = true;
    if (slot->callback_depth == 0U) {
        af_plugin_finish_unload(host, slot);
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_plugin_host_invoke(AfPluginHost *host, AfEditorPluginRegistrationToken token,
                               const AfPluginInvokeInfo *info) {
    AfPluginSlot *slot = NULL;
    AfEditorPluginRequest request = {0};
    AfEditorPluginResponse response = {.struct_size = sizeof(AfEditorPluginResponse),
                                       .api_version = AF_EDITOR_PLUGIN_API_VERSION};
    AfResult result = AF_RESULT_SUCCESS;
    if (host == NULL || info == NULL || info->struct_size < sizeof(*info) ||
        info->api_version != AF_PLUGIN_HOST_API_VERSION || info->capability == 0U ||
        (info->capability & (info->capability - 1U)) != 0U ||
        (info->capability & ~AF_EDITOR_PLUGIN_CAP_ALL) != 0U ||
        (info->input_size != 0U && info->input_data == NULL) ||
        info->input_size > host->maximum_transfer_size) {
        return info != NULL && info->api_version != AF_PLUGIN_HOST_API_VERSION
                   ? AF_RESULT_UNSUPPORTED_VERSION
                   : AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_plugin_slot(host, token, &slot);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    if ((slot->descriptor.capabilities & info->capability) == 0U) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    request = (AfEditorPluginRequest){.struct_size = sizeof(AfEditorPluginRequest),
                                      .api_version = AF_EDITOR_PLUGIN_API_VERSION,
                                      .capability = info->capability,
                                      .request_id = info->request_id,
                                      .input_data = info->input_data,
                                      .input_size = info->input_size};
    result = af_plugin_callback_begin(host, slot, info->capability);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    result = slot->descriptor.invoke(slot->plugin_user, &request, &response);
    if (result == AF_RESULT_SUCCESS &&
        (response.struct_size < sizeof(response) ||
         response.api_version != AF_EDITOR_PLUGIN_API_VERSION ||
         response.output_size > host->maximum_transfer_size ||
         (response.output_size != 0U && response.output_data == NULL))) {
        result = response.api_version != AF_EDITOR_PLUGIN_API_VERSION
                     ? AF_RESULT_UNSUPPORTED_VERSION
                     : AF_RESULT_INVALID_ARGUMENT;
    }
    if (result == AF_RESULT_SUCCESS &&
        info->capability == AF_EDITOR_PLUGIN_CAP_COMPILER_TRANSFORM) {
        if (response.output_size == 0U || host->validate_compiler_output == NULL) {
            result = AF_RESULT_INVALID_PACKAGE;
        } else {
            result = host->validate_compiler_output(host->compiler_validation_user,
                                                    response.output_data, response.output_size);
        }
        if (result != AF_RESULT_SUCCESS) {
            af_plugin_diagnostic(host, slot, result, AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR,
                                 AF_PLUGIN_STAGE_COMPILER_VALIDATION, info->capability, NULL,
                                 "Compiler transform output failed mandatory package validation");
        }
    }
    if (result == AF_RESULT_SUCCESS && response.output_size != 0U && info->consume_output != NULL) {
        result =
            info->consume_output(info->output_user, response.output_data, response.output_size);
    }
    if (result != AF_RESULT_SUCCESS) {
        const char *message = "Plugin extension callback failed";
        ++slot->failure_count;
        if (response.diagnostic != NULL &&
            af_plugin_string_valid(response.diagnostic, AF_PLUGIN_HOST_DIAGNOSTIC_CAPACITY, true)) {
            message = response.diagnostic;
        }
        af_plugin_diagnostic(host, slot, result, AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR,
                             AF_PLUGIN_STAGE_CALLBACK, info->capability, NULL, message);
    }
    af_plugin_callback_end(host, slot);
    return result;
}

AfResult af_plugin_host_find(const AfPluginHost *host, uint64_t plugin_id,
                             AfEditorPluginRegistrationToken *out_token) {
    if (host == NULL || plugin_id == 0U || out_token == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_token = 0U;
    for (uint32_t index = 0U; index < AF_PLUGIN_HOST_MAX_PLUGINS; ++index) {
        if (host->slots[index].occupied && host->slots[index].descriptor.plugin_id == plugin_id) {
            *out_token = host->slots[index].token;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

uint32_t af_plugin_host_registration_count(const AfPluginHost *host) {
    return host == NULL ? 0U : host->registration_count;
}

uint32_t af_plugin_host_diagnostic_count(const AfPluginHost *host) {
    return host == NULL ? 0U : host->diagnostic_count;
}

AfResult af_plugin_host_diagnostic(const AfPluginHost *host, uint32_t newest_offset,
                                   AfPluginDiagnostic *out_diagnostic) {
    uint32_t index = 0U;
    if (host == NULL || out_diagnostic == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (newest_offset >= host->diagnostic_count) {
        return AF_RESULT_NOT_FOUND;
    }
    index = (host->diagnostic_start + host->diagnostic_count - newest_offset - 1U) %
            AF_PLUGIN_HOST_MAX_DIAGNOSTICS;
    *out_diagnostic = host->diagnostics[index];
    return AF_RESULT_SUCCESS;
}

AfResult af_plugin_host_active_callback(const AfPluginHost *host,
                                        AfEditorPluginRegistrationToken *out_token,
                                        AfEditorPluginCapabilities *out_capability) {
    if (host == NULL || out_token == NULL || out_capability == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_token = 0U;
    *out_capability = 0U;
    if (host->callback_depth == 0U) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_token = host->callback_stack[host->callback_depth - 1U].token;
    *out_capability = host->callback_stack[host->callback_depth - 1U].capability;
    return AF_RESULT_SUCCESS;
}

AfResult af_plugin_host_report_crash(AfPluginHost *host, AfEditorPluginRegistrationToken token,
                                     const char *message) {
    AfPluginSlot *slot = NULL;
    AfEditorPluginCapabilities capability = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (host == NULL ||
        !af_plugin_string_valid(message, AF_PLUGIN_HOST_DIAGNOSTIC_CAPACITY, true)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (token == 0U) {
        if (host->callback_depth == 0U) {
            return AF_RESULT_NOT_FOUND;
        }
        token = host->callback_stack[host->callback_depth - 1U].token;
    }
    result = af_plugin_slot(host, token, &slot);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    slot->crashed = true;
    slot->pending_unload = true;
    ++slot->failure_count;
    if (host->callback_depth != 0U &&
        host->callback_stack[host->callback_depth - 1U].token == token) {
        capability = host->callback_stack[host->callback_depth - 1U].capability;
    }
    af_plugin_diagnostic(host, slot, AF_RESULT_INVALID_ARGUMENT, AF_EDITOR_PLUGIN_DIAGNOSTIC_ERROR,
                         AF_PLUGIN_STAGE_CRASH, capability, NULL,
                         message[0] == '\0' ? "Plugin crashed during host callback" : message);
    if (slot->callback_depth == 0U) {
        af_plugin_finish_unload(host, slot);
    }
    return AF_RESULT_SUCCESS;
}
