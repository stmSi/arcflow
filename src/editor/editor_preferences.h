#ifndef AF_EDITOR_PREFERENCES_H
#define AF_EDITOR_PREFERENCES_H

#include "arcflow/arcflow_result.h"
#include "editor/editor_actions.h"
#include "localization/localization.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_EDITOR_PREFERENCES_VERSION 1U
#define AF_EDITOR_PREFERENCES_MAX_FILE_SIZE (64U * 1024U)
#define AF_EDITOR_PREFERENCES_MAX_LINE_SIZE 2048U
#define AF_EDITOR_WORKSPACE_NAME_CAPACITY 48U
#define AF_EDITOR_RECENT_PROJECT_CAPACITY 16U
#define AF_EDITOR_RECENT_PATH_CAPACITY 512U
#define AF_EDITOR_SHORTCUT_OVERRIDE_CAPACITY AF_EDITOR_MAX_ACTIONS
#define AF_EDITOR_AUTOSAVE_MAX_SECONDS 86400U
#define AF_EDITOR_DOCK_MIN_EXTENT 120U
#define AF_EDITOR_DOCK_MAX_EXTENT 4096U

typedef enum AfEditorTheme {
    AF_EDITOR_THEME_SYSTEM = 0,
    AF_EDITOR_THEME_DARK,
    AF_EDITOR_THEME_LIGHT,
    AF_EDITOR_THEME_COUNT
} AfEditorTheme;

typedef enum AfEditorDockId {
    AF_EDITOR_DOCK_HIERARCHY = 0,
    AF_EDITOR_DOCK_INSPECTOR,
    AF_EDITOR_DOCK_TIMELINE,
    AF_EDITOR_DOCK_CURVE_EDITOR,
    AF_EDITOR_DOCK_ASSET_BROWSER,
    AF_EDITOR_DOCK_PROFILER,
    AF_EDITOR_DOCK_VALIDATION,
    AF_EDITOR_DOCK_CONSOLE,
    AF_EDITOR_DOCK_COUNT
} AfEditorDockId;

typedef enum AfEditorDockPlacement {
    AF_EDITOR_DOCK_LEFT = 0,
    AF_EDITOR_DOCK_RIGHT,
    AF_EDITOR_DOCK_BOTTOM,
    AF_EDITOR_DOCK_FLOATING,
    AF_EDITOR_DOCK_PLACEMENT_COUNT
} AfEditorDockPlacement;

typedef struct AfEditorDockPreference {
    uint32_t extent_pixels;
    uint32_t order;
    AfEditorDockPlacement placement;
    bool visible;
} AfEditorDockPreference;

typedef struct AfEditorWorkspacePreferences {
    char name[AF_EDITOR_WORKSPACE_NAME_CAPACITY];
    AfEditorDockPreference docks[AF_EDITOR_DOCK_COUNT];
} AfEditorWorkspacePreferences;

typedef struct AfEditorShortcutOverride {
    AfEditorActionId action_id;
    AfEditorShortcut shortcut;
} AfEditorShortcutOverride;

typedef struct AfEditorRecentProject {
    char path[AF_EDITOR_RECENT_PATH_CAPACITY];
} AfEditorRecentProject;

typedef struct AfEditorPreferences {
    AfEditorWorkspacePreferences workspace;
    AfAccessibilitySettings accessibility;
    AfEditorShortcutOverride shortcut_overrides[AF_EDITOR_SHORTCUT_OVERRIDE_CAPACITY];
    AfEditorRecentProject recent_projects[AF_EDITOR_RECENT_PROJECT_CAPACITY];
    uint32_t shortcut_override_count;
    uint32_t recent_project_count;
    uint32_t autosave_interval_seconds;
    AfEditorTheme theme;
    bool show_tooltips;
    bool show_extended_tooltips;
    bool reopen_last_project;
} AfEditorPreferences;

void af_editor_preferences_default(AfEditorPreferences *preferences);
AfResult af_editor_preferences_validate(const AfEditorPreferences *preferences);
AfResult af_editor_preferences_set_dock(AfEditorPreferences *preferences, AfEditorDockId dock,
                                        const AfEditorDockPreference *state);
AfResult af_editor_preferences_set_shortcut(AfEditorPreferences *preferences,
                                            AfEditorActionId action_id, AfEditorShortcut shortcut);
AfResult af_editor_preferences_remove_shortcut(AfEditorPreferences *preferences,
                                               AfEditorActionId action_id);
const AfEditorShortcutOverride *
af_editor_preferences_find_shortcut(const AfEditorPreferences *preferences,
                                    AfEditorActionId action_id);
AfResult af_editor_preferences_apply_shortcuts(const AfEditorPreferences *preferences,
                                               AfEditorActionRegistry *registry,
                                               AfEditorActionId *out_first_conflicting_action_id,
                                               AfEditorActionId *out_second_conflicting_action_id);
AfResult af_editor_preferences_add_recent_project(AfEditorPreferences *preferences,
                                                  const char *path);
AfResult af_editor_preferences_remove_recent_project(AfEditorPreferences *preferences,
                                                     const char *path);
void af_editor_preferences_clear_recent_projects(AfEditorPreferences *preferences);
AfResult af_editor_preferences_serialize(const AfEditorPreferences *preferences, char *output,
                                         size_t output_capacity, size_t *out_size);
AfResult af_editor_preferences_parse(const char *data, size_t size,
                                     AfEditorPreferences *out_preferences);

#endif
