#include "stability/diagnostics.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expression);   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static bool af_test_stability_lifecycle(void) {
    AfStabilityDiagnostic storage[4];
    AfStabilityDiagnostics diagnostics;
    AfStabilitySummary summary;
    const AfStabilityDiagnostic *diagnostic = NULL;
    CHECK(af_stability_diagnostics_init(&diagnostics, storage, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_stability_diagnostics_report(&diagnostics, 100U, AF_STABILITY_MEDIUM,
                                          AF_STABILITY_RENDERING, AF_RESULT_VULKAN_ERROR,
                                          "renderer",
                                          "Animated pose did not refresh") == AF_RESULT_SUCCESS);
    CHECK(af_stability_diagnostics_report(
              &diagnostics, 100U, AF_STABILITY_HIGH, AF_STABILITY_RENDERING, AF_RESULT_VULKAN_ERROR,
              "renderer", "Animated pose remained stale") == AF_RESULT_SUCCESS);
    diagnostic = af_stability_diagnostics_find(&diagnostics, 100U);
    CHECK(diagnostic != NULL && diagnostic->severity == AF_STABILITY_HIGH &&
          diagnostic->occurrence_count == 2U && !diagnostic->resolved);
    summary = af_stability_diagnostics_summarize(&diagnostics);
    CHECK(summary.unresolved_by_severity[AF_STABILITY_HIGH] == 1U);
    CHECK(summary.release_ready && summary.total_occurrence_count == 2U);
    CHECK(af_stability_diagnostics_resolve(&diagnostics, 100U) == AF_RESULT_SUCCESS);
    summary = af_stability_diagnostics_summarize(&diagnostics);
    CHECK(summary.resolved_count == 1U && summary.release_ready);
    CHECK(af_stability_diagnostics_clear_resolved(&diagnostics) == 1U && diagnostics.count == 0U);
    return true;
}

static bool af_test_release_blockers(void) {
    AfStabilityDiagnostic storage[4];
    AfStabilityDiagnostics diagnostics;
    AfStabilitySummary summary;
    CHECK(af_stability_diagnostics_init(&diagnostics, storage, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_stability_diagnostics_report(&diagnostics, 1U, AF_STABILITY_HIGH,
                                          AF_STABILITY_DATA_LOSS, AF_RESULT_IO_ERROR, "project",
                                          "Atomic save recovery failed") == AF_RESULT_SUCCESS);
    CHECK(af_stability_diagnostics_report(
              &diagnostics, 2U, AF_STABILITY_HIGH, AF_STABILITY_SECURITY, AF_RESULT_INVALID_PACKAGE,
              "package", "Untrusted offset bypassed validation") == AF_RESULT_SUCCESS);
    CHECK(af_stability_diagnostics_report(&diagnostics, 3U, AF_STABILITY_LOW, AF_STABILITY_WORKFLOW,
                                          AF_RESULT_NOT_FOUND, "editor",
                                          "Shortcut label is unclear") == AF_RESULT_SUCCESS);
    summary = af_stability_diagnostics_summarize(&diagnostics);
    CHECK(summary.release_blocker_count == 2U && !summary.release_ready);
    CHECK(strcmp(af_stability_severity_name(AF_STABILITY_CRITICAL), "Critical") == 0);
    CHECK(strcmp(af_stability_category_name(AF_STABILITY_MIGRATION), "Migration") == 0);
    CHECK(af_stability_diagnostics_resolve(&diagnostics, 1U) == AF_RESULT_SUCCESS);
    CHECK(af_stability_diagnostics_resolve(&diagnostics, 2U) == AF_RESULT_SUCCESS);
    summary = af_stability_diagnostics_summarize(&diagnostics);
    CHECK(summary.release_blocker_count == 0U && summary.release_ready);
    return true;
}

static bool af_test_bounded_diagnostics(void) {
    AfStabilityDiagnostic storage[1];
    AfStabilityDiagnostics diagnostics;
    AfStabilitySummary summary;
    CHECK(af_stability_diagnostics_init(&diagnostics, storage, 1U) == AF_RESULT_SUCCESS);
    CHECK(af_stability_diagnostics_report(&diagnostics, 1U, AF_STABILITY_LOW, AF_STABILITY_HARDWARE,
                                          AF_RESULT_UNSUPPORTED_FEATURE, "gpu",
                                          "Device profile is not in the beta matrix") ==
          AF_RESULT_SUCCESS);
    CHECK(af_stability_diagnostics_report(&diagnostics, 2U, AF_STABILITY_LOW, AF_STABILITY_HARDWARE,
                                          AF_RESULT_UNSUPPORTED_FEATURE, "gpu",
                                          "Driver profile is not in the beta matrix") ==
          AF_RESULT_LIMIT_EXCEEDED);
    summary = af_stability_diagnostics_summarize(&diagnostics);
    CHECK(summary.dropped_count == 1U && !summary.release_ready);
    CHECK(af_stability_diagnostics_report(&diagnostics, 1U, AF_STABILITY_LOW,
                                          AF_STABILITY_RENDERING, AF_RESULT_VULKAN_ERROR, "gpu",
                                          "Identifier collision") == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_stability_diagnostics_resolve(&diagnostics, 99U) == AF_RESULT_NOT_FOUND);
    af_stability_diagnostics_reset(&diagnostics);
    CHECK(diagnostics.count == 0U && diagnostics.dropped_count == 0U);
    return true;
}

int main(void) {
    static const struct {
        const char *name;
        bool (*function)(void);
    } tests[] = {{"diagnostic lifecycle", af_test_stability_lifecycle},
                 {"release blockers", af_test_release_blockers},
                 {"bounded diagnostics", af_test_bounded_diagnostics}};
    size_t passed = 0U;
    for (size_t index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (tests[index].function()) {
            ++passed;
            (void)printf("[PASS] %s\n", tests[index].name);
        } else {
            (void)printf("[FAIL] %s\n", tests[index].name);
        }
    }
    (void)printf("%zu/%zu tests passed\n", passed, sizeof(tests) / sizeof(tests[0]));
    return passed == sizeof(tests) / sizeof(tests[0]) ? 0 : 1;
}
