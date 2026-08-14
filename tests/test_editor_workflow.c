#include "editor/editor_actions.h"
#include "editor/editor_preferences.h"
#include "editor/editor_workflow.h"

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

typedef struct AfActionFixture {
    AfEditorAction storage[8];
    AfEditorActionRegistry registry;
    uint32_t execution_count;
    AfEditorActionId last_action;
} AfActionFixture;

static AfResult af_test_execute_action(void *user, AfEditorActionId action_id) {
    AfActionFixture *fixture = user;
    ++fixture->execution_count;
    fixture->last_action = action_id;
    return AF_RESULT_SUCCESS;
}

static bool af_test_action_fixture(AfActionFixture *fixture) {
    const AfEditorActionDefinition definitions[] = {
        {.id = 1U,
         .label = "Save Project",
         .tooltip = "Save the current project to disk.",
         .keywords = "write disk file",
         .default_shortcut = {'S', AF_EDITOR_SHORTCUT_MODIFIER_CTRL},
         .contexts = AF_EDITOR_ACTION_CONTEXT_GLOBAL,
         .execute = af_test_execute_action,
         .user = fixture},
        {.id = 2U,
         .label = "Open Project",
         .tooltip = "Open an Arcflow project.",
         .keywords = "load file",
         .default_shortcut = {'O', AF_EDITOR_SHORTCUT_MODIFIER_CTRL},
         .contexts = AF_EDITOR_ACTION_CONTEXT_GLOBAL,
         .execute = af_test_execute_action,
         .user = fixture},
        {.id = 3U,
         .label = "Delete Selection",
         .tooltip = "Delete selected hierarchy objects.",
         .keywords = "remove objects",
         .default_shortcut = {AF_EDITOR_KEY_DELETE, 0U},
         .contexts = AF_EDITOR_ACTION_CONTEXT_HIERARCHY,
         .execute = af_test_execute_action,
         .user = fixture},
        {.id = 4U,
         .label = "Frame Selection",
         .tooltip = "Fit selected objects in the canvas.",
         .keywords = "canvas zoom fit",
         .default_shortcut = {'F', 0U},
         .contexts = AF_EDITOR_ACTION_CONTEXT_CANVAS,
         .execute = af_test_execute_action,
         .user = fixture},
        {.id = 5U,
         .label = "Find Object",
         .tooltip = "Search the scene hierarchy.",
         .keywords = "filter search hierarchy",
         .default_shortcut = {'F', AF_EDITOR_SHORTCUT_MODIFIER_CTRL},
         .contexts = AF_EDITOR_ACTION_CONTEXT_HIERARCHY,
         .execute = af_test_execute_action,
         .user = fixture}};
    *fixture = (AfActionFixture){0};
    CHECK(af_editor_action_registry_init(&fixture->registry, fixture->storage, 8U) ==
          AF_RESULT_SUCCESS);
    for (uint32_t index = 0U; index < sizeof(definitions) / sizeof(definitions[0]); ++index) {
        CHECK(af_editor_action_register(&fixture->registry, &definitions[index]) ==
              AF_RESULT_SUCCESS);
    }
    return true;
}

static bool af_test_action_registry(void) {
    AfActionFixture fixture;
    AfEditorActionId action_id = 0U;
    AfEditorActionId conflict = 0U;
    AfEditorActionSearchResult results[5];
    uint32_t count = 0U;
    char shortcut[AF_EDITOR_SHORTCUT_TEXT_CAPACITY];
    CHECK(af_test_action_fixture(&fixture));
    CHECK(fixture.registry.count == 5U);
    CHECK(strcmp(af_editor_action_find(&fixture.registry, 1U)->tooltip,
                 "Save the current project to disk.") == 0);
    CHECK(af_editor_shortcut_format((AfEditorShortcut){'s', AF_EDITOR_SHORTCUT_MODIFIER_CTRL |
                                                                AF_EDITOR_SHORTCUT_MODIFIER_SHIFT},
                                    shortcut, sizeof(shortcut)) == AF_RESULT_SUCCESS);
    CHECK(strcmp(shortcut, "CTRL+SHIFT+S") == 0);

    CHECK(af_editor_action_set_shortcut(&fixture.registry, 2U,
                                        (AfEditorShortcut){'S', AF_EDITOR_SHORTCUT_MODIFIER_CTRL},
                                        &conflict) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(conflict == 1U && af_editor_action_find(&fixture.registry, 2U)->shortcut.key == 'O');
    CHECK(af_editor_action_set_shortcut(&fixture.registry, 2U,
                                        (AfEditorShortcut){'p', AF_EDITOR_SHORTCUT_MODIFIER_CTRL},
                                        &conflict) == AF_RESULT_SUCCESS);
    CHECK(af_editor_action_find(&fixture.registry, 2U)->shortcut.key == 'P');
    CHECK(af_editor_action_resolve_shortcut(
              &fixture.registry, (AfEditorShortcut){'s', AF_EDITOR_SHORTCUT_MODIFIER_CTRL},
              AF_EDITOR_ACTION_CONTEXT_CANVAS, &action_id) == AF_RESULT_SUCCESS);
    CHECK(action_id == 1U);
    CHECK(af_editor_action_resolve_shortcut(
              &fixture.registry, (AfEditorShortcut){AF_EDITOR_KEY_DELETE, 0U},
              AF_EDITOR_ACTION_CONTEXT_CANVAS, &action_id) == AF_RESULT_NOT_FOUND);
    CHECK(af_editor_action_set_enabled(&fixture.registry, 1U, false) == AF_RESULT_SUCCESS);
    CHECK(af_editor_action_resolve_shortcut(
              &fixture.registry, (AfEditorShortcut){'S', AF_EDITOR_SHORTCUT_MODIFIER_CTRL},
              AF_EDITOR_ACTION_CONTEXT_CANVAS, &action_id) == AF_RESULT_NOT_FOUND);
    CHECK(af_editor_action_execute(&fixture.registry, 1U) == AF_RESULT_UNSUPPORTED_FEATURE);
    CHECK(af_editor_action_set_enabled(&fixture.registry, 1U, true) == AF_RESULT_SUCCESS);
    CHECK(af_editor_action_execute(&fixture.registry, 1U) == AF_RESULT_SUCCESS);
    CHECK(fixture.execution_count == 1U && fixture.last_action == 1U);

    CHECK(af_editor_action_search(&fixture.registry, "save disk", AF_EDITOR_ACTION_CONTEXT_CANVAS,
                                  results, 5U, &count) == AF_RESULT_SUCCESS);
    CHECK(count == 1U && results[0].action_id == 1U);
    CHECK(af_editor_action_search(&fixture.registry, "fit", AF_EDITOR_ACTION_CONTEXT_CANVAS,
                                  results, 5U, &count) == AF_RESULT_SUCCESS);
    CHECK(count == 1U && results[0].action_id == 4U);
    CHECK(af_editor_action_set_visible(&fixture.registry, 4U, false) == AF_RESULT_SUCCESS);
    CHECK(af_editor_action_search(&fixture.registry, "fit", AF_EDITOR_ACTION_CONTEXT_CANVAS,
                                  results, 5U, &count) == AF_RESULT_SUCCESS);
    CHECK(count == 0U);
    return true;
}

static bool af_test_command_palette(void) {
    AfActionFixture fixture;
    AfEditorCommandPalette palette;
    AfEditorActionId action_id = 0U;
    CHECK(af_test_action_fixture(&fixture));
    CHECK(af_editor_command_palette_init(&palette, &fixture.registry, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_palette_open(&palette, AF_EDITOR_ACTION_CONTEXT_CANVAS) ==
          AF_RESULT_SUCCESS);
    CHECK(palette.open && palette.result_count == 3U);
    CHECK(af_editor_command_palette_handle_key(&palette, AF_EDITOR_COMMAND_PALETTE_END, NULL) ==
          AF_RESULT_SUCCESS);
    CHECK(palette.selected_index == 2U);
    CHECK(af_editor_command_palette_handle_key(&palette, AF_EDITOR_COMMAND_PALETTE_PAGE_UP, NULL) ==
          AF_RESULT_SUCCESS);
    CHECK(palette.selected_index == 0U);
    CHECK(af_editor_command_palette_handle_key(&palette, AF_EDITOR_COMMAND_PALETTE_UP, NULL) ==
          AF_RESULT_SUCCESS);
    CHECK(palette.selected_index == 2U);
    CHECK(af_editor_command_palette_set_query(&palette, "frame canvas") == AF_RESULT_SUCCESS);
    CHECK(palette.result_count == 1U);
    CHECK(af_editor_command_palette_selected(&palette)->id == 4U);
    CHECK(af_editor_command_palette_handle_key(&palette, AF_EDITOR_COMMAND_PALETTE_ACTIVATE,
                                               &action_id) == AF_RESULT_SUCCESS);
    CHECK(action_id == 4U && !palette.open);

    CHECK(af_editor_command_palette_open(&palette, AF_EDITOR_ACTION_CONTEXT_HIERARCHY) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_command_palette_set_query(&palette, "no matching command") ==
          AF_RESULT_SUCCESS);
    CHECK(palette.result_count == 0U);
    CHECK(af_editor_command_palette_handle_key(&palette, AF_EDITOR_COMMAND_PALETTE_ACTIVATE,
                                               &action_id) == AF_RESULT_NOT_FOUND);
    CHECK(af_editor_command_palette_handle_key(&palette, AF_EDITOR_COMMAND_PALETTE_CANCEL, NULL) ==
          AF_RESULT_SUCCESS);
    CHECK(!palette.open);
    return true;
}

static bool af_test_preferences_round_trip(void) {
    static char serialized[AF_EDITOR_PREFERENCES_MAX_FILE_SIZE + 16U];
    static char second[AF_EDITOR_PREFERENCES_MAX_FILE_SIZE + 16U];
    AfEditorPreferences preferences;
    AfEditorPreferences parsed;
    AfEditorDockPreference profiler = {
        .extent_pixels = 480U, .order = 1U, .placement = AF_EDITOR_DOCK_RIGHT, .visible = true};
    size_t size = 0U;
    size_t second_size = 0U;
    af_editor_preferences_default(&preferences);
    (void)snprintf(preferences.workspace.name, sizeof(preferences.workspace.name), "%s",
                   "Animation & VFX");
    preferences.theme = AF_EDITOR_THEME_LIGHT;
    preferences.autosave_interval_seconds = 45U;
    preferences.accessibility.text_scale = 1.5F;
    preferences.accessibility.reduced_motion = AF_REDUCED_MOTION_SIMPLIFY;
    preferences.accessibility.high_contrast = true;
    preferences.accessibility.color_blind_preview = AF_COLOR_BLIND_PREVIEW_TRITANOPIA;
    preferences.show_extended_tooltips = false;
    preferences.onboarding_complete = true;
    CHECK(af_editor_preferences_set_dock(&preferences, AF_EDITOR_DOCK_PROFILER, &profiler) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_preferences_set_shortcut(
              &preferences, 1U,
              (AfEditorShortcut){'S', AF_EDITOR_SHORTCUT_MODIFIER_CTRL |
                                          AF_EDITOR_SHORTCUT_MODIFIER_SHIFT}) == AF_RESULT_SUCCESS);
    CHECK(af_editor_preferences_set_shortcut(&preferences, 2U, (AfEditorShortcut){0U, 0U}) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_preferences_add_recent_project(
              &preferences, "/tmp/Project One/scene%main.afproj") == AF_RESULT_SUCCESS);
    CHECK(af_editor_preferences_add_recent_project(&preferences, "C:\\Arcflow\\สอง.afproj") ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_preferences_add_recent_project(
              &preferences, "/tmp/Project One/scene%main.afproj") == AF_RESULT_SUCCESS);
    CHECK(preferences.recent_project_count == 2U);
    CHECK(strcmp(preferences.recent_projects[0].path, "/tmp/Project One/scene%main.afproj") == 0);
    CHECK(af_editor_preferences_validate(&preferences) == AF_RESULT_SUCCESS);
    CHECK(af_editor_preferences_serialize(&preferences, serialized, sizeof(serialized), &size) ==
          AF_RESULT_SUCCESS);
    CHECK(size != 0U && strstr(serialized, "Project%20One") != NULL &&
          strstr(serialized, "scene%25main") != NULL);
    CHECK(af_editor_preferences_parse(serialized, size, &parsed) == AF_RESULT_SUCCESS);
    CHECK(parsed.onboarding_complete);
    CHECK(strcmp(parsed.workspace.name, "Animation & VFX") == 0);
    CHECK(parsed.theme == AF_EDITOR_THEME_LIGHT && parsed.autosave_interval_seconds == 45U);
    CHECK(parsed.accessibility.text_scale == 1.5F && parsed.accessibility.high_contrast);
    CHECK(parsed.workspace.docks[AF_EDITOR_DOCK_PROFILER].visible &&
          parsed.workspace.docks[AF_EDITOR_DOCK_PROFILER].extent_pixels == 480U);
    CHECK(parsed.shortcut_override_count == 2U && parsed.recent_project_count == 2U);
    CHECK(af_editor_preferences_find_shortcut(&parsed, 1U)->shortcut.modifiers ==
          (AF_EDITOR_SHORTCUT_MODIFIER_CTRL | AF_EDITOR_SHORTCUT_MODIFIER_SHIFT));
    CHECK(strcmp(parsed.recent_projects[1].path, "C:\\Arcflow\\สอง.afproj") == 0);
    CHECK(af_editor_preferences_serialize(&parsed, second, sizeof(second), &second_size) ==
          AF_RESULT_SUCCESS);
    CHECK(size == second_size && memcmp(serialized, second, size) == 0);
    CHECK(af_editor_preferences_remove_recent_project(&parsed, "C:\\Arcflow\\สอง.afproj") ==
          AF_RESULT_SUCCESS);
    CHECK(parsed.recent_project_count == 1U);
    CHECK(af_editor_preferences_remove_shortcut(&parsed, 2U) == AF_RESULT_SUCCESS);
    CHECK(parsed.shortcut_override_count == 1U);
    return true;
}

static bool af_test_preferences_apply_shortcuts(void) {
    AfActionFixture fixture;
    AfEditorPreferences preferences;
    AfEditorActionId first_conflict = 0U;
    AfEditorActionId second_conflict = 0U;
    CHECK(af_test_action_fixture(&fixture));
    af_editor_preferences_default(&preferences);
    CHECK(af_editor_preferences_set_shortcut(
              &preferences, 1U, (AfEditorShortcut){'O', AF_EDITOR_SHORTCUT_MODIFIER_CTRL}) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_preferences_set_shortcut(
              &preferences, 2U, (AfEditorShortcut){'S', AF_EDITOR_SHORTCUT_MODIFIER_CTRL}) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_preferences_apply_shortcuts(&preferences, &fixture.registry, &first_conflict,
                                                &second_conflict) == AF_RESULT_SUCCESS);
    CHECK(af_editor_action_find(&fixture.registry, 1U)->shortcut.key == 'O');
    CHECK(af_editor_action_find(&fixture.registry, 2U)->shortcut.key == 'S');

    CHECK(af_editor_preferences_set_shortcut(
              &preferences, 2U, (AfEditorShortcut){'O', AF_EDITOR_SHORTCUT_MODIFIER_CTRL}) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_preferences_apply_shortcuts(&preferences, &fixture.registry, &first_conflict,
                                                &second_conflict) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(first_conflict == 1U && second_conflict == 2U);
    CHECK(af_editor_action_find(&fixture.registry, 1U)->shortcut.key == 'O');
    CHECK(af_editor_action_find(&fixture.registry, 2U)->shortcut.key == 'S');
    return true;
}

static bool af_test_preferences_reject_malformed(void) {
    static const char unsupported[] = "ARCFLOW_EDITOR_PREFERENCES 99\nend\n";
    static const char bad_dock[] = "ARCFLOW_EDITOR_PREFERENCES 1\ndock 0 1 9 0 260\nend\n";
    static const char bad_escape[] = "ARCFLOW_EDITOR_PREFERENCES 1\nrecent bad%0Gpath\nend\n";
    static const char duplicate_shortcut[] =
        "ARCFLOW_EDITOR_PREFERENCES 1\nshortcut 7 65 1\nshortcut 7 66 1\nend\n";
    static const char embedded_zero[] = "ARCFLOW_EDITOR_PREFERENCES 1\0end\n";
    AfEditorPreferences output;
    af_editor_preferences_default(&output);
    output.autosave_interval_seconds = 1234U;
    CHECK(af_editor_preferences_parse(unsupported, sizeof(unsupported) - 1U, &output) ==
          AF_RESULT_UNSUPPORTED_VERSION);
    CHECK(output.autosave_interval_seconds == 1234U);
    CHECK(af_editor_preferences_parse(bad_dock, sizeof(bad_dock) - 1U, &output) ==
          AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_editor_preferences_parse(bad_escape, sizeof(bad_escape) - 1U, &output) ==
          AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_editor_preferences_parse(duplicate_shortcut, sizeof(duplicate_shortcut) - 1U,
                                      &output) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_editor_preferences_parse(embedded_zero, sizeof(embedded_zero) - 1U, &output) ==
          AF_RESULT_INVALID_ARGUMENT);
    CHECK(output.autosave_interval_seconds == 1234U);
    return true;
}

static bool af_test_workspace_round_trip(void) {
    char text[2048];
    AfEditorWorkspace workspace;
    AfEditorWorkspace parsed;
    size_t size = 0U;
    af_editor_workspace_default(&workspace);
    workspace.left_width = 312.5F;
    workspace.right_width = 288.0F;
    workspace.timeline_height = 240.0F;
    workspace.profiling_visible = true;
    workspace.validation_visible = true;
    workspace.guides[0] = (AfEditorGuide){.position = 144.5F, .vertical = true};
    workspace.guides[1] = (AfEditorGuide){.position = 72.0F, .vertical = false};
    workspace.guide_count = 2U;
    CHECK(af_editor_workspace_serialize(&workspace, text, sizeof(text), &size) ==
          AF_RESULT_SUCCESS);
    CHECK(size > 0U);
    CHECK(af_editor_workspace_parse(text, size, &parsed) == AF_RESULT_SUCCESS);
    CHECK(parsed.left_width == 312.5F && parsed.right_width == 288.0F);
    CHECK(parsed.timeline_height == 240.0F && parsed.profiling_visible &&
          parsed.validation_visible);
    CHECK(parsed.guide_count == 2U && parsed.guides[0].vertical &&
          parsed.guides[0].position == 144.5F && !parsed.guides[1].vertical);
    parsed.left_width = 1.0F;
    parsed.right_width = 9999.0F;
    parsed.timeline_height = 1.0F;
    CHECK(af_editor_workspace_validate(&parsed, 1000.0F, 700.0F) == AF_RESULT_SUCCESS);
    CHECK(parsed.left_width == 150.0F && parsed.right_width == 400.0F &&
          parsed.timeline_height == 120.0F);
    text[size] = 'x';
    CHECK(af_editor_workspace_parse(text, size + 1U, &parsed) == AF_RESULT_INVALID_PACKAGE);
    CHECK(af_editor_workspace_parse("arcflow_workspace 2\n", 20U, &parsed) ==
          AF_RESULT_INVALID_PACKAGE);
    return true;
}

static bool af_test_scene_workflow_tools(void) {
    AfSceneNode storage[6] = {
        {.id = 1U,
         .parent_id = AF_INVALID_NODE_ID,
         .type = AF_NODE_RECTANGLE,
         .local_transform = {.position = {10.0F, 20.0F}, .scale = {1.0F, 1.0F}},
         .bounds = {20.0F, 10.0F},
         .opacity = 1.0F,
         .visible = true},
        {.id = 2U,
         .parent_id = AF_INVALID_NODE_ID,
         .type = AF_NODE_ELLIPSE,
         .local_transform = {.position = {70.0F, 40.0F}, .scale = {1.0F, 1.0F}},
         .bounds = {10.0F, 10.0F},
         .opacity = 1.0F,
         .visible = true},
        {.id = 3U,
         .parent_id = 1U,
         .type = AF_NODE_ROUNDED_RECTANGLE,
         .local_transform = {.position = {35.0F, 60.0F}, .scale = {1.0F, 1.0F}},
         .bounds = {30.0F, 10.0F},
         .opacity = 1.0F,
         .visible = true}};
    char names[6][64] = {"Header", "body Circle", "Header Child"};
    const uint64_t all_ids[3] = {1U, 2U, 3U};
    const uint64_t copy_ids[2] = {1U, 3U};
    uint32_t matches[3] = {0U};
    uint32_t match_count = 0U;
    uint64_t pasted_ids[2] = {0U};
    uint32_t pasted_count = 0U;
    uint64_t next_id = 10U;
    AfEditorClipboard clipboard;
    AfScene scene;
    af_scene_init(&scene, storage, 6U);
    scene.count = 3U;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK(af_editor_filter_nodes(&scene, (const char (*)[64])names, "HEADER", matches, 3U,
                                 &match_count) == AF_RESULT_SUCCESS);
    CHECK(match_count == 2U && matches[0] == 0U && matches[1] == 2U);
    CHECK(af_editor_align_nodes(&scene, all_ids, 3U, AF_EDITOR_ALIGN_LEFT) == AF_RESULT_SUCCESS);
    CHECK(storage[0].local_transform.position.x == 10.0F &&
          storage[1].local_transform.position.x == 10.0F &&
          storage[2].local_transform.position.x == 10.0F);
    storage[0].local_transform.position.x = 0.0F;
    storage[1].local_transform.position.x = 90.0F;
    storage[2].local_transform.position.x = 30.0F;
    CHECK(af_editor_distribute_nodes(&scene, all_ids, 3U, AF_EDITOR_DISTRIBUTE_HORIZONTAL) ==
          AF_RESULT_SUCCESS);
    CHECK(storage[2].local_transform.position.x == 40.0F);
    CHECK(af_editor_clipboard_copy(&scene, (const char (*)[64])names, copy_ids, 2U, &clipboard) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_clipboard_paste(&clipboard, &scene, names, &next_id, (AfVec2){5.0F, 7.0F},
                                    pasted_ids, 2U, &pasted_count) == AF_RESULT_SUCCESS);
    CHECK(pasted_count == 2U && pasted_ids[0] == 10U && pasted_ids[1] == 11U && next_id == 12U);
    CHECK(storage[4].parent_id == pasted_ids[0]);
    CHECK(strcmp(names[3], "Header") == 0 && strcmp(names[4], "Header Child") == 0);
    next_id = AF_INVALID_NODE_ID;
    CHECK(af_editor_clipboard_paste(&clipboard, &scene, names, &next_id, (AfVec2){0.0F, 0.0F},
                                    pasted_ids, 2U, &pasted_count) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(scene.count == 5U && next_id == AF_INVALID_NODE_ID);
    return true;
}

int main(void) {
    static const struct {
        const char *name;
        bool (*function)(void);
    } tests[] = {{"action registry", af_test_action_registry},
                 {"command palette", af_test_command_palette},
                 {"preferences round trip", af_test_preferences_round_trip},
                 {"preferences shortcut application", af_test_preferences_apply_shortcuts},
                 {"preferences malformed input", af_test_preferences_reject_malformed},
                 {"workspace round trip", af_test_workspace_round_trip},
                 {"scene workflow tools", af_test_scene_workflow_tools}};
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
