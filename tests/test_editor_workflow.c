#include "editor/editor_actions.h"
#include "editor/editor_export.h"
#include "editor/editor_preferences.h"
#include "editor/editor_snapping.h"
#include "editor/editor_workflow.h"
#include "project/interchange.h"
#include <math.h>

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

static bool af_test_object_snapping(void) {
    const AfEditorSnapTarget target = {.bounds = {{100, 200}, {180, 260}}, .id = 42};
    for (uint32_t axis = 0; axis < 2; ++axis)
        for (uint32_t feature = 0; feature < 3; ++feature) {
            AfVec2 point = {500, 500};
            const float value = axis ? 200 + 30 * (float)feature : 100 + 40 * (float)feature;
            if (axis)
                point.y = value + 3;
            else
                point.x = value + 3;
            AfEditorSnapResult result = af_editor_snap_bounds((AfEditorSnapBounds){point, point},
                                                              &target, 1, 1, (AfVec2){0});
            CHECK(result.axes[axis].active && result.axes[axis].feature == feature &&
                  result.axes[axis].id == 42);
            CHECK(fabsf((axis ? result.correction.y : result.correction.x) + 3) < .001F);
        }
    /* Tolerance is measured in screen pixels, independent of zoom. */
    for (uint32_t i = 0; i < 3; ++i) {
        const float zooms[] = {.1F, 1, 8};
        const float zoom = zooms[i];
        AfVec2 point = {100 - 7 / zoom, 900};
        AfEditorSnapResult r = af_editor_snap_bounds((AfEditorSnapBounds){point, point}, &target, 1,
                                                     zoom, (AfVec2){0});
        CHECK(r.axes[0].active);
        point.x = 100 - 9 / zoom;
        r = af_editor_snap_bounds((AfEditorSnapBounds){point, point}, &target, 1, zoom,
                                  (AfVec2){0});
        CHECK(!r.axes[0].active && !r.axes[1].active);
    }
    /* No objects means no snapping, even near grid coordinates. */
    AfEditorSnapResult r =
        af_editor_snap_bounds((AfEditorSnapBounds){{19, 39}, {39, 59}}, NULL, 0, 1, (AfVec2){0});
    CHECK(r.correction.x == 0 && r.correction.y == 0);
    r = af_editor_snap_bounds((AfEditorSnapBounds){{97, 197}, {97, 197}}, &target, 1, 1,
                              (AfVec2){1, 0});
    CHECK(r.correction.x == 3 && r.correction.y == 0 && !r.axes[1].active);
    r = af_editor_snap_bounds((AfEditorSnapBounds){{97, 197}, {97, 197}}, &target, 1, 1,
                              (AfVec2){1, 1});
    CHECK(fabsf(r.correction.x - 3) < .001F && fabsf(r.correction.y - 3) < .001F);
    const AfEditorSnapTarget nearest[] = {target, {.bounds = {{104, 300}, {184, 360}}, .id = 43}};
    r = af_editor_snap_bounds((AfEditorSnapBounds){{103, 800}, {103, 800}}, nearest, 2, 1,
                              (AfVec2){0});
    CHECK(r.axes[0].id == 43 && r.correction.x == 1);
    return true;
}
static bool af_test_adaptive_grid(void) {
    float previous = INFINITY;
    for (float zoom = .1F; zoom < 8.1F; zoom *= 1.1F) {
        const float step = af_editor_grid_interval(20, zoom);
        CHECK(step * zoom >= 9.999F && step * zoom < 50.001F && step <= previous);
        previous = step;
    }
    CHECK(af_editor_grid_interval(20, .1F) > af_editor_grid_interval(20, 1));
    CHECK(af_editor_grid_interval(20, 8) < af_editor_grid_interval(20, 1));
    CHECK(af_editor_grid_interval(20, 0) == 0 && af_editor_grid_interval(NAN, 1) == 0);
    return true;
}

static bool af_test_export_scope(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfProject source = {0}, selected = {0}, full = {0};
    AfPackageBlob before = {0}, after = {0}, package = {0};
    CHECK(af_project_create(&source, &allocator, 16) == AF_RESULT_SUCCESS);
    source.canvas_size = (AfVec2){800, 600};
    AfSceneNode parent = {
        .id = 1,
        .parent_id = AF_INVALID_NODE_ID,
        .type = AF_NODE_RECTANGLE,
        .local_transform = {.position = {100, 80}, .scale = {2, 1}, .rotation = .3F},
        .bounds = {200, 100},
        .visible = true,
        .opacity = .8F,
        .fill = {.2F, .3F, .4F, 1}};
    AfSceneNode child = {.id = 2,
                         .parent_id = 1,
                         .type = AF_NODE_ELLIPSE,
                         .local_transform = {.position = {12, 34}, .scale = {1, 1}},
                         .bounds = {40, 30},
                         .visible = true,
                         .opacity = 1,
                         .fill = {.9F, .1F, .2F, 1}};
    AfSceneNode other = child;
    other.id = 3;
    other.parent_id = AF_INVALID_NODE_ID;
    other.type = AF_NODE_PATH;
    CHECK(af_project_add_node(&source, &parent, "Parent artwork") == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&source, &child, "Selected child") == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&source, &other, "Unrelated path") == AF_RESULT_SUCCESS);
    AfPathCommand commands[] = {{.type = AF_PATH_MOVE, .point = {0, 0}},
                                {.type = AF_PATH_LINE, .point = {40, 30}}};
    AfPath path = {.id = 50, .node_id = 3, .command_count = 2, .geometry_generation = 1};
    CHECK(af_project_add_path(&source, &path, commands) == AF_RESULT_SUCCESS);
    AfProjectClip clip = {.id = 100, .name = "Animation", .duration = 2, .loop_end = 2};
    CHECK(af_project_add_clip(&source, &clip) == AF_RESULT_SUCCESS);
    for (uint32_t i = 0; i < 3; ++i) {
        AfProjectTrack track = {
            .id = 101 + i, .clip_id = 100, .node_id = 1 + i, .property = AF_PROPERTY_POSITION};
        AfProjectKeyframe key = {.id = 110 + i,
                                 .track_id = track.id,
                                 .keyframe = {.time = 0,
                                              .value = {100 + (float)i, 80 + (float)i},
                                              .interpolation = AF_INTERPOLATION_LINEAR}};
        CHECK(af_project_add_track(&source, &track) == AF_RESULT_SUCCESS);
        CHECK(af_project_add_keyframe(&source, &key) == AF_RESULT_SUCCESS);
    }
    /* Inheritance may refer to an object that is outside the selected subtree. */
    AfStyle *style = af_project_find_node_style(&source, 2);
    CHECK(style);
    style->parent_style_id = af_style_find_node_const(&source.style_data, 3)->id;
    style->inherit_flags = AF_STYLE_INHERIT_FILL;
    CHECK(af_scene_evaluate_world(&source.scene) == AF_RESULT_SUCCESS);
    const AfAffine2 world = af_scene_find_const(&source.scene, 2)->world_transform;
    CHECK(af_project_serialize(&source, &allocator, &before) == AF_RESULT_SUCCESS);
    const uint64_t id = 2;
    AfEditorExportOptions options = {
        .size = {320, 240}, .origin = {20, 30}, .selection_only = true};
    CHECK(af_editor_export_project(&source, &options, &id, 1, &allocator, &selected) ==
          AF_RESULT_SUCCESS);
    CHECK(selected.scene.count == 2 && selected.path_data.path_count == 0 &&
          selected.track_count == 2 && selected.keyframe_count == 2);
    CHECK(af_scene_find_const(&selected.scene, 1)->type == AF_NODE_GROUP);
    CHECK(!af_scene_find_const(&selected.scene, 3));
    CHECK(selected.canvas_size.x == 320 && selected.canvas_size.y == 240);
    const AfSceneNode *kept = af_scene_find_const(&selected.scene, 2);
    CHECK(fabsf(kept->world_transform.tx - world.tx + 20) < .001F &&
          fabsf(kept->world_transform.ty - world.ty + 30) < .001F);
    CHECK(selected.keyframes[0].keyframe.value[0] == 80 &&
          selected.keyframes[0].keyframe.value[1] == 50);
    CHECK(af_project_validate(&selected) == AF_RESULT_SUCCESS);
    CHECK(af_project_compile(&selected, &allocator, &package) == AF_RESULT_SUCCESS);
    af_package_blob_destroy(&allocator, &package);
    char svg[8192];
    size_t size = 0;
    AfInterchangeDiagnostics diagnostics = {0};
    CHECK(af_interchange_export_static_svg(&selected.scene, selected.canvas_size, svg, sizeof(svg),
                                           &size, &diagnostics) == AF_RESULT_SUCCESS);
    CHECK(strstr(svg, "width=\"320\" height=\"240\"") && strstr(svg, "<ellipse") &&
          !strstr(svg, "<rect"));
    CHECK(af_interchange_export_lottie_json(&selected, 100, NULL, svg, sizeof(svg), &size,
                                            &diagnostics) == AF_RESULT_SUCCESS);
    CHECK(strstr(svg, "\"w\":320") && strstr(svg, "\"h\":240") && !strstr(svg, "Unrelated path"));
    af_project_destroy(&selected);
    /* A selected parent includes descendants and retains its own artwork. */
    const uint64_t root = 1;
    CHECK(af_editor_export_project(&source, &options, &root, 1, &allocator, &selected) ==
          AF_RESULT_SUCCESS);
    CHECK(selected.scene.count == 2 && selected.scene.nodes[0].type == AF_NODE_RECTANGLE);
    af_project_destroy(&selected);
    options.selection_only = false;
    CHECK(af_editor_export_project(&source, &options, NULL, 0, &allocator, &full) ==
          AF_RESULT_SUCCESS);
    CHECK(full.scene.count == 3 && full.path_data.path_count == 1 && full.track_count == 3);
    af_project_destroy(&full);
    options.selection_only = true;
    CHECK(af_editor_export_project(&source, &options, NULL, 0, &allocator, &selected) ==
          AF_RESULT_INVALID_ARGUMENT);
    const uint64_t missing = 999;
    CHECK(af_editor_export_project(&source, &options, &missing, 1, &allocator, &selected) ==
          AF_RESULT_NOT_FOUND);
    options.size.x = NAN;
    CHECK(af_editor_export_project(&source, &options, &id, 1, &allocator, &selected) ==
          AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_project_serialize(&source, &allocator, &after) == AF_RESULT_SUCCESS);
    CHECK(before.size == after.size && !memcmp(before.data, after.data, before.size));
    af_package_blob_destroy(&allocator, &before);
    af_package_blob_destroy(&allocator, &after);
    af_project_destroy(&source);
    return true;
}

static bool af_test_export_flow_and_canvas_scale(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfProject source = {0}, output = {0};
    CHECK(af_project_create(&source, &allocator, 8) == AF_RESULT_SUCCESS);
    source.layout_data.reference_size = (AfVec2){400, 300};
    source.layout_data.target_size = (AfVec2){800, 600};
    source.layout_data.scale_mode = AF_CANVAS_SCALE_FIT;
    AfSceneNode parent = {.id = 1,
                          .parent_id = AF_INVALID_NODE_ID,
                          .type = AF_NODE_GROUP,
                          .bounds = {300, 100},
                          .local_transform = {.scale = {1, 1}},
                          .visible = true,
                          .opacity = 1};
    CHECK(af_project_add_node(&source, &parent, "Flow parent") == AF_RESULT_SUCCESS);
    AfLayoutNode layout = af_layout_node_default(1);
    layout.direction = AF_LAYOUT_DIRECTION_HORIZONTAL;
    layout.spacing = 10;
    CHECK(af_project_set_layout(&source, &layout) == AF_RESULT_SUCCESS);
    AfSceneNode sibling = {.id = 2,
                           .parent_id = 1,
                           .type = AF_NODE_RECTANGLE,
                           .bounds = {50, 40},
                           .local_transform = {.scale = {1, 1}},
                           .visible = true,
                           .opacity = 1,
                           .fill = {1, 0, 0, 1}};
    CHECK(af_project_add_node(&source, &sibling, "Space reserving sibling") == AF_RESULT_SUCCESS);
    sibling.id = 3;
    CHECK(af_project_add_node(&source, &sibling, "Selected artwork") == AF_RESULT_SUCCESS);
    CHECK(af_scene_evaluate_world(&source.scene) == AF_RESULT_SUCCESS);
    const AfAffine2 before = af_scene_find_const(&source.scene, 3)->world_transform;
    const uint64_t id = 3;
    const AfEditorExportOptions options = {
        .size = {150, 120}, .origin = {50, 30}, .selection_only = true};
    CHECK(af_editor_export_project(&source, &options, &id, 1, &allocator, &output) ==
          AF_RESULT_SUCCESS);
    CHECK(af_scene_find_const(&output.scene, 2)->type == AF_NODE_GROUP);
    const AfAffine2 after = af_scene_find_const(&output.scene, 3)->world_transform;
    CHECK(fabsf(after.tx - before.tx + 50) < .001F && fabsf(after.ty - before.ty + 30) < .001F);
    char svg[4096];
    size_t size = 0;
    CHECK(af_interchange_export_static_svg(&output.scene, output.canvas_size, svg, sizeof(svg),
                                           &size, NULL) == AF_RESULT_SUCCESS);
    const char *rect = strstr(svg, "<rect");
    CHECK(rect && !strstr(rect + 1, "<rect"));
    af_project_destroy(&output);
    af_project_destroy(&source);
    return true;
}

static bool af_test_export_image_pixels(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfProject source = {0}, output = {0};
    CHECK(af_project_create(&source, &allocator, 8) == AF_RESULT_SUCCESS);
    const uint8_t pixels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    AfImageAsset asset = {.id = 1,
                          .width = 2,
                          .height = 2,
                          .source_format = AF_IMAGE_SOURCE_PNG,
                          .source_path = "assets/example.png",
                          .name = "Example"};
    uint64_t asset_id = 0;
    bool duplicate = false;
    CHECK(af_image_add(&source.image_data, &asset, pixels, &asset_id, &duplicate) ==
          AF_RESULT_SUCCESS);
    source.next_asset_id = asset_id + 1;
    AfSceneNode node = {.id = 1,
                        .parent_id = AF_INVALID_NODE_ID,
                        .type = AF_NODE_IMAGE,
                        .local_transform = {.scale = {1, 1}},
                        .bounds = {40, 40},
                        .visible = true,
                        .opacity = 1,
                        .fill = {1, 1, 1, 1}};
    CHECK(af_project_add_node(&source, &node, "Image") == AF_RESULT_SUCCESS);
    AfImageBinding binding = {.node_id = 1, .asset_id = asset_id};
    CHECK(af_image_set_binding(&source.image_data, &binding) == AF_RESULT_SUCCESS);
    AfEditorExportOptions options = {.size = {100, 80}, .selection_only = true};
    const uint64_t id = 1;
    CHECK(af_editor_export_project(&source, &options, &id, 1, &allocator, &output) ==
          AF_RESULT_SUCCESS);
    const AfImageAsset *copy = af_image_find_const(&output.image_data, asset_id);
    CHECK(copy && !copy->missing && copy->pixels != source.image_data.assets[0].pixels &&
          !memcmp(copy->pixels, pixels, sizeof(pixels)));
    AfPackageBlob package = {0};
    CHECK(af_project_compile(&output, &allocator, &package) == AF_RESULT_SUCCESS);
    af_package_blob_destroy(&allocator, &package);
    af_project_destroy(&output);
    af_project_destroy(&source);
    return true;
}

static bool af_test_curve_world_bounds(void) {
    AfPathCommand commands[] = {
        {.type = AF_PATH_MOVE, .point = {0, 0}},
        {.type = AF_PATH_CUBIC, .point = {100, 0}, .control1 = {0, -100}, .control2 = {100, -100}}};
    AfPath path = {.id = 5, .node_id = 1, .command_count = 2, .geometry_generation = 1};
    AfPathData paths = {.paths = &path,
                        .path_count = 1,
                        .path_capacity = 1,
                        .commands = commands,
                        .command_count = 2,
                        .command_capacity = 2};
    AfSceneNode node = {.id = 1,
                        .parent_id = AF_INVALID_NODE_ID,
                        .type = AF_NODE_PATH,
                        .bounds = {100, 20},
                        .resolved_bounds = {100, 20},
                        .world_visible = true,
                        .world_opacity = 1,
                        .world_transform = {.m00 = 1, .m11 = 1, .tx = 10, .ty = 20}};
    AfScene scene = {.nodes = &node, .count = 1, .capacity = 1, .paths = &paths};
    AfEditorSnapBounds bounds;
    CHECK(af_editor_scene_node_bounds(&scene, &node, false, &bounds));
    CHECK(fabsf(bounds.minimum.x - 10) < .001F && fabsf(bounds.maximum.x - 110) < .001F);
    CHECK(fabsf(bounds.minimum.y + 55) < .001F && fabsf(bounds.maximum.y - 20) < .001F);
    node.world_transform = (AfAffine2){.m01 = -1, .m10 = 1, .tx = 10, .ty = 20};
    CHECK(af_editor_scene_node_bounds(&scene, &node, false, &bounds));
    CHECK(fabsf(bounds.maximum.x - 85) < .001F && fabsf(bounds.maximum.y - 120) < .001F);
    node.world_visible = false;
    CHECK(!af_editor_scene_node_bounds(&scene, &node, false, &bounds));
    return true;
}

int main(void) {
    static const struct {
        const char *name;
        bool (*function)(void);
    } tests[] = {
        {"curve extrema and transformed bounds", af_test_curve_world_bounds},
        {"export scope, hierarchy, animation and isolation", af_test_export_scope},
        {"export preserves flow placement and scaled canvas", af_test_export_flow_and_canvas_scale},
        {"export preserves decoded images", af_test_export_image_pixels},
        {"object alignment snapping", af_test_object_snapping},
        {"adaptive visual grid", af_test_adaptive_grid},
        {"action registry", af_test_action_registry},
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
