#ifndef AF_STABILITY_DIAGNOSTICS_H
#define AF_STABILITY_DIAGNOSTICS_H

#include "arcflow/arcflow_result.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_STABILITY_COMPONENT_CAPACITY 48U
#define AF_STABILITY_MESSAGE_CAPACITY 192U
#define AF_STABILITY_MAX_DIAGNOSTICS 1024U

typedef enum AfStabilitySeverity {
    AF_STABILITY_CRITICAL = 0,
    AF_STABILITY_HIGH,
    AF_STABILITY_MEDIUM,
    AF_STABILITY_LOW,
    AF_STABILITY_SEVERITY_COUNT
} AfStabilitySeverity;

typedef enum AfStabilityCategory {
    AF_STABILITY_DATA_LOSS = 0,
    AF_STABILITY_REMOTE_CODE_EXECUTION,
    AF_STABILITY_PACKAGE_PARSER_MEMORY_CORRUPTION,
    AF_STABILITY_ORDINARY_WORKFLOW_CRASH,
    AF_STABILITY_VULKAN_VALIDATION,
    AF_STABILITY_PACKAGE_COMPATIBILITY,
    AF_STABILITY_UNBOUNDED_MEMORY_GROWTH,
    AF_STABILITY_SAVE_LOAD,
    AF_STABILITY_SECURITY,
    AF_STABILITY_RENDERING,
    AF_STABILITY_ACCESSIBILITY,
    AF_STABILITY_WORKFLOW,
    AF_STABILITY_MIGRATION,
    AF_STABILITY_PERFORMANCE,
    AF_STABILITY_HARDWARE,
    AF_STABILITY_CATEGORY_COUNT
} AfStabilityCategory;

typedef struct AfStabilityDiagnostic {
    uint64_t id;
    uint64_t first_sequence;
    uint64_t last_sequence;
    uint64_t occurrence_count;
    AfResult result;
    AfStabilitySeverity severity;
    AfStabilityCategory category;
    char component[AF_STABILITY_COMPONENT_CAPACITY];
    char message[AF_STABILITY_MESSAGE_CAPACITY];
    bool resolved;
} AfStabilityDiagnostic;

typedef struct AfStabilityDiagnostics {
    AfStabilityDiagnostic *items;
    uint32_t count;
    uint32_t capacity;
    uint64_t sequence;
    uint64_t dropped_count;
} AfStabilityDiagnostics;

typedef struct AfStabilitySummary {
    uint32_t unresolved_by_severity[AF_STABILITY_SEVERITY_COUNT];
    uint32_t release_blocker_count;
    uint32_t resolved_count;
    uint64_t total_occurrence_count;
    uint64_t dropped_count;
    bool release_ready;
} AfStabilitySummary;

AfResult af_stability_diagnostics_init(AfStabilityDiagnostics *diagnostics,
                                       AfStabilityDiagnostic *storage, uint32_t capacity);
void af_stability_diagnostics_reset(AfStabilityDiagnostics *diagnostics);
AfResult af_stability_diagnostics_report(AfStabilityDiagnostics *diagnostics, uint64_t id,
                                         AfStabilitySeverity severity, AfStabilityCategory category,
                                         AfResult result, const char *component,
                                         const char *message);
AfResult af_stability_diagnostics_resolve(AfStabilityDiagnostics *diagnostics, uint64_t id);
uint32_t af_stability_diagnostics_clear_resolved(AfStabilityDiagnostics *diagnostics);
const AfStabilityDiagnostic *
af_stability_diagnostics_find(const AfStabilityDiagnostics *diagnostics, uint64_t id);
bool af_stability_diagnostic_is_release_blocker(const AfStabilityDiagnostic *diagnostic);
AfStabilitySummary af_stability_diagnostics_summarize(const AfStabilityDiagnostics *diagnostics);
const char *af_stability_severity_name(AfStabilitySeverity severity);
const char *af_stability_category_name(AfStabilityCategory category);

#endif
