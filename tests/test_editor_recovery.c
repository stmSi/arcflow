#include "af/core/allocator.h"
#include "af/core/file_io.h"
#include "editor/editor_recovery.h"

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

static void af_test_recovery_cleanup(const AfEditorRecoveryState *state) {
    char path[AF_EDITOR_RECOVERY_PATH_CAPACITY + 16U];
    (void)remove(state->paths.project);
    (void)remove(state->paths.autosave);
    (void)remove(state->paths.recovery);
    for (uint32_t index = 1U; index <= AF_EDITOR_RECOVERY_AUTOSAVE_BACKUPS; ++index) {
        (void)snprintf(path, sizeof(path), "%s.bak%u", state->paths.autosave, index);
        (void)remove(path);
    }
    for (uint32_t index = 1U; index <= AF_EDITOR_RECOVERY_PROJECT_BACKUPS; ++index) {
        (void)snprintf(path, sizeof(path), "%s.bak%u", state->paths.project, index);
        (void)remove(path);
    }
}

static bool af_test_recovery_round_trip(void) {
    static const uint8_t original[] = "original project";
    static const uint8_t recovered[] = "recovered project";
    AfEditorRecoveryState state;
    AfEditorRecoveryCandidate candidate = {.struct_size = sizeof(candidate),
                                           .api_version = AF_EDITOR_RECOVERY_API_VERSION};
    AfEditorRecoverySettings settings = {.struct_size = sizeof(settings),
                                         .api_version = AF_EDITOR_RECOVERY_API_VERSION};
    AfAllocator allocator;
    AfFileData restored = {0};
    bool written = false;
    af_allocator_init_default(&allocator);
    CHECK(af_editor_recovery_query_settings(&settings) == AF_RESULT_SUCCESS);
    CHECK(settings.autosave_interval_ms == AF_EDITOR_RECOVERY_AUTOSAVE_INTERVAL_MS);
    CHECK(af_editor_recovery_init(&state, "arcflow_test_recovery.afproj", 100U) ==
          AF_RESULT_SUCCESS);
    af_test_recovery_cleanup(&state);
    CHECK(af_file_write_atomic(state.paths.project, original, sizeof(original), 0U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_recovery_note_change(&state, 200U) == AF_RESULT_SUCCESS);
    CHECK(!af_editor_recovery_autosave_due(&state, 201U));
    CHECK(af_editor_recovery_next_due_ms(&state) == 100U + AF_EDITOR_RECOVERY_AUTOSAVE_INTERVAL_MS);
    CHECK(af_editor_recovery_autosave_due(&state, 100U + AF_EDITOR_RECOVERY_AUTOSAVE_INTERVAL_MS));
    CHECK(af_editor_recovery_write_autosave(&state, 10U, recovered, sizeof(recovered), 30100U,
                                            &allocator, &written) == AF_RESULT_SUCCESS);
    CHECK(written && !af_editor_recovery_autosave_due(&state, 60000U));
    CHECK(af_editor_recovery_find_candidate(&state, 10U, &allocator, &candidate) ==
          AF_RESULT_SUCCESS);
    CHECK(candidate.status == AF_EDITOR_RECOVERY_STATUS_AVAILABLE &&
          candidate.source == AF_EDITOR_RECOVERY_SOURCE_AUTOSAVE &&
          candidate.payload_size == sizeof(recovered));
    CHECK(af_editor_recovery_restore(&state, 10U, &candidate, &allocator) == AF_RESULT_SUCCESS);
    CHECK(!state.dirty);
    CHECK(af_file_read_limited(state.paths.project, 1024U, &allocator, &restored) ==
          AF_RESULT_SUCCESS);
    CHECK(restored.size == sizeof(recovered) &&
          memcmp(restored.data, recovered, sizeof(recovered)) == 0);
    af_file_data_destroy(&allocator, &restored);
    af_test_recovery_cleanup(&state);
    return true;
}

static bool af_test_recovery_rejects_corruption(void) {
    static const uint8_t corrupt[] = "not a recovery envelope";
    AfEditorRecoveryState state;
    AfEditorRecoveryCandidate candidate = {.struct_size = sizeof(candidate),
                                           .api_version = AF_EDITOR_RECOVERY_API_VERSION};
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    CHECK(af_editor_recovery_init(&state, "arcflow_test_corrupt_recovery.afproj", 0U) ==
          AF_RESULT_SUCCESS);
    af_test_recovery_cleanup(&state);
    CHECK(af_file_write_atomic(state.paths.autosave, corrupt, sizeof(corrupt), 0U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_recovery_find_candidate(&state, 10U, &allocator, &candidate) ==
          AF_RESULT_SUCCESS);
    CHECK(candidate.status == AF_EDITOR_RECOVERY_STATUS_CORRUPT &&
          (candidate.issues & AF_EDITOR_RECOVERY_ISSUE_CORRUPT) != 0U);
    CHECK(af_editor_recovery_discard(&state) == AF_RESULT_SUCCESS);
    af_test_recovery_cleanup(&state);
    return true;
}

int main(void) {
    const bool round_trip = af_test_recovery_round_trip();
    const bool corruption = af_test_recovery_rejects_corruption();
    (void)printf("[%s] recovery round trip\n", round_trip ? "PASS" : "FAIL");
    (void)printf("[%s] corrupted recovery handling\n", corruption ? "PASS" : "FAIL");
    return round_trip && corruption ? 0 : 1;
}
