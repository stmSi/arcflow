#include "stability/diagnostics.h"

#include "core/safe_math.h"

#include <string.h>

static bool af_stability_severity_valid(AfStabilitySeverity severity) {
    return severity >= AF_STABILITY_CRITICAL && severity < AF_STABILITY_SEVERITY_COUNT;
}

static bool af_stability_category_valid(AfStabilityCategory category) {
    return category >= AF_STABILITY_DATA_LOSS && category < AF_STABILITY_CATEGORY_COUNT;
}

static bool af_stability_string_valid(const char *value, uint32_t capacity) {
    uint32_t index = 0U;
    if (value == NULL || value[0] == '\0') {
        return false;
    }
    while (index < capacity && value[index] != '\0') {
        const unsigned char byte = (unsigned char)value[index];
        if (byte < 32U || byte == 127U) {
            return false;
        }
        ++index;
    }
    return index < capacity;
}

static AfStabilityDiagnostic *af_stability_find_mutable(AfStabilityDiagnostics *diagnostics,
                                                        uint64_t id) {
    if (diagnostics == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < diagnostics->count; ++index) {
        if (diagnostics->items[index].id == id) {
            return &diagnostics->items[index];
        }
    }
    return NULL;
}

AfResult af_stability_diagnostics_init(AfStabilityDiagnostics *diagnostics,
                                       AfStabilityDiagnostic *storage, uint32_t capacity) {
    if (diagnostics == NULL || storage == NULL || capacity == 0U ||
        capacity > AF_STABILITY_MAX_DIAGNOSTICS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *diagnostics = (AfStabilityDiagnostics){.items = storage, .capacity = capacity};
    memset(storage, 0, (size_t)capacity * sizeof(*storage));
    return AF_RESULT_SUCCESS;
}

void af_stability_diagnostics_reset(AfStabilityDiagnostics *diagnostics) {
    if (diagnostics == NULL || diagnostics->items == NULL) {
        return;
    }
    memset(diagnostics->items, 0, (size_t)diagnostics->capacity * sizeof(*diagnostics->items));
    diagnostics->count = 0U;
    diagnostics->sequence = 0U;
    diagnostics->dropped_count = 0U;
}

AfResult af_stability_diagnostics_report(AfStabilityDiagnostics *diagnostics, uint64_t id,
                                         AfStabilitySeverity severity, AfStabilityCategory category,
                                         AfResult result, const char *component,
                                         const char *message) {
    AfStabilityDiagnostic *diagnostic = NULL;
    uint64_t next_sequence = 0U;
    uint64_t next_occurrence = 1U;
    if (diagnostics == NULL || diagnostics->items == NULL ||
        diagnostics->count > diagnostics->capacity || id == 0U ||
        !af_stability_severity_valid(severity) || !af_stability_category_valid(category) ||
        !af_stability_string_valid(component, AF_STABILITY_COMPONENT_CAPACITY) ||
        !af_stability_string_valid(message, AF_STABILITY_MESSAGE_CAPACITY)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (af_u64_add_overflow(diagnostics->sequence, 1U, &next_sequence)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    diagnostic = af_stability_find_mutable(diagnostics, id);
    if (diagnostic != NULL) {
        if (diagnostic->category != category || strcmp(diagnostic->component, component) != 0) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (af_u64_add_overflow(diagnostic->occurrence_count, 1U, &next_occurrence)) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    } else if (diagnostics->count >= diagnostics->capacity) {
        uint64_t dropped = 0U;
        if (af_u64_add_overflow(diagnostics->dropped_count, 1U, &dropped)) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        diagnostics->dropped_count = dropped;
        diagnostics->sequence = next_sequence;
        return AF_RESULT_LIMIT_EXCEEDED;
    } else {
        diagnostic = &diagnostics->items[diagnostics->count++];
        *diagnostic = (AfStabilityDiagnostic){.id = id,
                                              .first_sequence = next_sequence,
                                              .occurrence_count = 1U,
                                              .category = category};
    }
    diagnostic->last_sequence = next_sequence;
    diagnostic->occurrence_count = next_occurrence;
    diagnostic->result = result;
    diagnostic->severity =
        severity < diagnostic->severity || diagnostic->first_sequence == diagnostic->last_sequence
            ? severity
            : diagnostic->severity;
    diagnostic->resolved = false;
    (void)memcpy(diagnostic->component, component, strlen(component) + 1U);
    (void)memcpy(diagnostic->message, message, strlen(message) + 1U);
    diagnostics->sequence = next_sequence;
    return AF_RESULT_SUCCESS;
}

AfResult af_stability_diagnostics_resolve(AfStabilityDiagnostics *diagnostics, uint64_t id) {
    AfStabilityDiagnostic *diagnostic = af_stability_find_mutable(diagnostics, id);
    uint64_t next_sequence = 0U;
    if (diagnostic == NULL) {
        return diagnostics == NULL || diagnostics->items == NULL ? AF_RESULT_INVALID_ARGUMENT
                                                                 : AF_RESULT_NOT_FOUND;
    }
    if (diagnostic->resolved) {
        return AF_RESULT_SUCCESS;
    }
    if (af_u64_add_overflow(diagnostics->sequence, 1U, &next_sequence)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    diagnostic->resolved = true;
    diagnostic->last_sequence = next_sequence;
    diagnostics->sequence = next_sequence;
    return AF_RESULT_SUCCESS;
}

uint32_t af_stability_diagnostics_clear_resolved(AfStabilityDiagnostics *diagnostics) {
    uint32_t write = 0U;
    uint32_t removed = 0U;
    if (diagnostics == NULL || diagnostics->items == NULL) {
        return 0U;
    }
    for (uint32_t index = 0U; index < diagnostics->count; ++index) {
        if (diagnostics->items[index].resolved) {
            ++removed;
        } else {
            diagnostics->items[write++] = diagnostics->items[index];
        }
    }
    if (write < diagnostics->count) {
        memset(&diagnostics->items[write], 0,
               (size_t)(diagnostics->count - write) * sizeof(*diagnostics->items));
    }
    diagnostics->count = write;
    return removed;
}

const AfStabilityDiagnostic *
af_stability_diagnostics_find(const AfStabilityDiagnostics *diagnostics, uint64_t id) {
    if (diagnostics == NULL || diagnostics->items == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < diagnostics->count; ++index) {
        if (diagnostics->items[index].id == id) {
            return &diagnostics->items[index];
        }
    }
    return NULL;
}

bool af_stability_diagnostic_is_release_blocker(const AfStabilityDiagnostic *diagnostic) {
    if (diagnostic == NULL || diagnostic->resolved) {
        return false;
    }
    if (diagnostic->severity == AF_STABILITY_CRITICAL ||
        (diagnostic->category == AF_STABILITY_SECURITY &&
         diagnostic->severity <= AF_STABILITY_HIGH)) {
        return true;
    }
    return diagnostic->category >= AF_STABILITY_DATA_LOSS &&
           diagnostic->category <= AF_STABILITY_SAVE_LOAD;
}

AfStabilitySummary af_stability_diagnostics_summarize(const AfStabilityDiagnostics *diagnostics) {
    AfStabilitySummary summary = {0};
    if (diagnostics == NULL || diagnostics->items == NULL) {
        return summary;
    }
    summary.dropped_count = diagnostics->dropped_count;
    for (uint32_t index = 0U; index < diagnostics->count; ++index) {
        const AfStabilityDiagnostic *diagnostic = &diagnostics->items[index];
        if (UINT64_MAX - summary.total_occurrence_count < diagnostic->occurrence_count) {
            summary.total_occurrence_count = UINT64_MAX;
        } else {
            summary.total_occurrence_count += diagnostic->occurrence_count;
        }
        if (diagnostic->resolved) {
            ++summary.resolved_count;
            continue;
        }
        ++summary.unresolved_by_severity[(uint32_t)diagnostic->severity];
        if (af_stability_diagnostic_is_release_blocker(diagnostic)) {
            ++summary.release_blocker_count;
        }
    }
    summary.release_ready = summary.release_blocker_count == 0U && summary.dropped_count == 0U;
    return summary;
}

const char *af_stability_severity_name(AfStabilitySeverity severity) {
    static const char *const names[AF_STABILITY_SEVERITY_COUNT] = {"Critical", "High", "Medium",
                                                                   "Low"};
    return af_stability_severity_valid(severity) ? names[(uint32_t)severity] : "Unknown";
}

const char *af_stability_category_name(AfStabilityCategory category) {
    static const char *const names[AF_STABILITY_CATEGORY_COUNT] = {
        "Data loss",
        "Remote-code execution",
        "Package parser memory corruption",
        "Workflow crash",
        "Vulkan validation",
        "Package compatibility",
        "Memory growth",
        "Save/load",
        "Security",
        "Rendering",
        "Accessibility",
        "Workflow",
        "Migration",
        "Performance",
        "Hardware"};
    return af_stability_category_valid(category) ? names[(uint32_t)category] : "Unknown";
}
