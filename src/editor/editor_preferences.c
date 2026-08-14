#include "editor/editor_preferences.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct AfEditorPreferenceWriter {
    char *data;
    size_t capacity;
    size_t length;
} AfEditorPreferenceWriter;

static bool af_editor_preference_string_length(const char *text, size_t capacity,
                                               size_t *out_length) {
    const char *end = text != NULL ? memchr(text, '\0', capacity) : NULL;
    if (end == NULL) {
        return false;
    }
    if (out_length != NULL) {
        *out_length = (size_t)(end - text);
    }
    return true;
}

static bool af_editor_dock_valid(const AfEditorDockPreference *dock) {
    return dock != NULL && dock->placement < AF_EDITOR_DOCK_PLACEMENT_COUNT &&
           dock->extent_pixels >= AF_EDITOR_DOCK_MIN_EXTENT &&
           dock->extent_pixels <= AF_EDITOR_DOCK_MAX_EXTENT && dock->order < AF_EDITOR_DOCK_COUNT;
}

static bool af_editor_preference_shortcut_valid(AfEditorShortcut shortcut) {
    char text[AF_EDITOR_SHORTCUT_TEXT_CAPACITY];
    return af_editor_shortcut_format(shortcut, text, sizeof(text)) == AF_RESULT_SUCCESS;
}

void af_editor_preferences_default(AfEditorPreferences *preferences) {
    if (preferences == NULL) {
        return;
    }
    *preferences = (AfEditorPreferences){.workspace = {.name = "Default"},
                                         .autosave_interval_seconds = 300U,
                                         .theme = AF_EDITOR_THEME_DARK,
                                         .show_tooltips = true,
                                         .show_extended_tooltips = true,
                                         .reopen_last_project = true};
    af_accessibility_settings_default(&preferences->accessibility);
    preferences->workspace.docks[AF_EDITOR_DOCK_HIERARCHY] = (AfEditorDockPreference){
        .extent_pixels = 260U, .placement = AF_EDITOR_DOCK_LEFT, .visible = true};
    preferences->workspace.docks[AF_EDITOR_DOCK_INSPECTOR] = (AfEditorDockPreference){
        .extent_pixels = 300U, .placement = AF_EDITOR_DOCK_RIGHT, .visible = true};
    preferences->workspace.docks[AF_EDITOR_DOCK_TIMELINE] = (AfEditorDockPreference){
        .extent_pixels = 280U, .placement = AF_EDITOR_DOCK_BOTTOM, .visible = true};
    preferences->workspace.docks[AF_EDITOR_DOCK_CURVE_EDITOR] = (AfEditorDockPreference){
        .extent_pixels = 280U, .order = 1U, .placement = AF_EDITOR_DOCK_BOTTOM};
    preferences->workspace.docks[AF_EDITOR_DOCK_ASSET_BROWSER] = (AfEditorDockPreference){
        .extent_pixels = 260U, .order = 1U, .placement = AF_EDITOR_DOCK_LEFT};
    preferences->workspace.docks[AF_EDITOR_DOCK_PROFILER] = (AfEditorDockPreference){
        .extent_pixels = 300U, .order = 1U, .placement = AF_EDITOR_DOCK_RIGHT};
    preferences->workspace.docks[AF_EDITOR_DOCK_VALIDATION] = (AfEditorDockPreference){
        .extent_pixels = 240U, .order = 2U, .placement = AF_EDITOR_DOCK_BOTTOM};
    preferences->workspace.docks[AF_EDITOR_DOCK_CONSOLE] = (AfEditorDockPreference){
        .extent_pixels = 240U, .order = 3U, .placement = AF_EDITOR_DOCK_BOTTOM};
}

AfResult af_editor_preferences_validate(const AfEditorPreferences *preferences) {
    if (preferences == NULL || preferences->theme >= AF_EDITOR_THEME_COUNT ||
        preferences->autosave_interval_seconds > AF_EDITOR_AUTOSAVE_MAX_SECONDS ||
        preferences->shortcut_override_count > AF_EDITOR_SHORTCUT_OVERRIDE_CAPACITY ||
        preferences->recent_project_count > AF_EDITOR_RECENT_PROJECT_CAPACITY ||
        !af_editor_preference_string_length(preferences->workspace.name,
                                            sizeof(preferences->workspace.name), NULL) ||
        preferences->workspace.name[0] == '\0' ||
        af_accessibility_settings_validate(&preferences->accessibility) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < AF_EDITOR_DOCK_COUNT; ++index) {
        if (!af_editor_dock_valid(&preferences->workspace.docks[index])) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < preferences->shortcut_override_count; ++index) {
        const AfEditorShortcutOverride *shortcut = &preferences->shortcut_overrides[index];
        if (shortcut->action_id == 0U || !af_editor_preference_shortcut_valid(shortcut->shortcut)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < preferences->shortcut_override_count; ++other) {
            if (shortcut->action_id == preferences->shortcut_overrides[other].action_id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (uint32_t index = 0U; index < preferences->recent_project_count; ++index) {
        const char *path = preferences->recent_projects[index].path;
        if (!af_editor_preference_string_length(path, AF_EDITOR_RECENT_PATH_CAPACITY, NULL) ||
            path[0] == '\0') {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < preferences->recent_project_count; ++other) {
            if (strcmp(path, preferences->recent_projects[other].path) == 0) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_preferences_set_dock(AfEditorPreferences *preferences, AfEditorDockId dock,
                                        const AfEditorDockPreference *state) {
    if (preferences == NULL || dock >= AF_EDITOR_DOCK_COUNT || !af_editor_dock_valid(state)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    preferences->workspace.docks[(uint32_t)dock] = *state;
    return AF_RESULT_SUCCESS;
}

const AfEditorShortcutOverride *
af_editor_preferences_find_shortcut(const AfEditorPreferences *preferences,
                                    AfEditorActionId action_id) {
    if (preferences == NULL || action_id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < preferences->shortcut_override_count; ++index) {
        if (preferences->shortcut_overrides[index].action_id == action_id) {
            return &preferences->shortcut_overrides[index];
        }
    }
    return NULL;
}

AfResult af_editor_preferences_set_shortcut(AfEditorPreferences *preferences,
                                            AfEditorActionId action_id, AfEditorShortcut shortcut) {
    AfEditorShortcutOverride *existing = NULL;
    if (preferences == NULL || action_id == 0U || !af_editor_preference_shortcut_valid(shortcut)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    shortcut = af_editor_shortcut_normalize(shortcut);
    existing =
        (AfEditorShortcutOverride *)af_editor_preferences_find_shortcut(preferences, action_id);
    if (existing != NULL) {
        existing->shortcut = shortcut;
        return AF_RESULT_SUCCESS;
    }
    if (preferences->shortcut_override_count >= AF_EDITOR_SHORTCUT_OVERRIDE_CAPACITY) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    preferences->shortcut_overrides[preferences->shortcut_override_count++] =
        (AfEditorShortcutOverride){.action_id = action_id, .shortcut = shortcut};
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_preferences_remove_shortcut(AfEditorPreferences *preferences,
                                               AfEditorActionId action_id) {
    if (preferences == NULL || action_id == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < preferences->shortcut_override_count; ++index) {
        if (preferences->shortcut_overrides[index].action_id == action_id) {
            if (index + 1U < preferences->shortcut_override_count) {
                memmove(&preferences->shortcut_overrides[index],
                        &preferences->shortcut_overrides[index + 1U],
                        (size_t)(preferences->shortcut_override_count - index - 1U) *
                            sizeof(preferences->shortcut_overrides[0]));
            }
            --preferences->shortcut_override_count;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

static bool af_editor_preference_contexts_overlap(uint32_t left, uint32_t right) {
    return (left & AF_EDITOR_ACTION_CONTEXT_GLOBAL) != 0U ||
           (right & AF_EDITOR_ACTION_CONTEXT_GLOBAL) != 0U || (left & right) != 0U;
}

AfResult af_editor_preferences_apply_shortcuts(const AfEditorPreferences *preferences,
                                               AfEditorActionRegistry *registry,
                                               AfEditorActionId *out_first_conflicting_action_id,
                                               AfEditorActionId *out_second_conflicting_action_id) {
    AfEditorShortcut effective[AF_EDITOR_MAX_ACTIONS];
    if (out_first_conflicting_action_id != NULL) {
        *out_first_conflicting_action_id = 0U;
    }
    if (out_second_conflicting_action_id != NULL) {
        *out_second_conflicting_action_id = 0U;
    }
    if (af_editor_preferences_validate(preferences) != AF_RESULT_SUCCESS || registry == NULL ||
        registry->count > AF_EDITOR_MAX_ACTIONS ||
        (registry->count != 0U && registry->actions == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < registry->count; ++index) {
        const AfEditorShortcutOverride *override =
            af_editor_preferences_find_shortcut(preferences, registry->actions[index].id);
        effective[index] =
            override != NULL ? override->shortcut : registry->actions[index].default_shortcut;
    }
    for (uint32_t index = 0U; index < preferences->shortcut_override_count; ++index) {
        if (af_editor_action_find(registry, preferences->shortcut_overrides[index].action_id) ==
            NULL) {
            return AF_RESULT_NOT_FOUND;
        }
    }
    for (uint32_t index = 0U; index < registry->count; ++index) {
        if (!af_editor_shortcut_is_bound(effective[index])) {
            continue;
        }
        for (uint32_t other = index + 1U; other < registry->count; ++other) {
            if (af_editor_preference_contexts_overlap(registry->actions[index].contexts,
                                                      registry->actions[other].contexts) &&
                af_editor_shortcut_equal(effective[index], effective[other])) {
                if (out_first_conflicting_action_id != NULL) {
                    *out_first_conflicting_action_id = registry->actions[index].id;
                }
                if (out_second_conflicting_action_id != NULL) {
                    *out_second_conflicting_action_id = registry->actions[other].id;
                }
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (uint32_t index = 0U; index < registry->count; ++index) {
        registry->actions[index].shortcut = af_editor_shortcut_normalize(effective[index]);
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_preferences_add_recent_project(AfEditorPreferences *preferences,
                                                  const char *path) {
    size_t length = 0U;
    uint32_t existing = UINT32_MAX;
    uint32_t move_count = 0U;
    if (preferences == NULL || path == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!af_editor_preference_string_length(path, AF_EDITOR_RECENT_PATH_CAPACITY, &length)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (length == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < preferences->recent_project_count; ++index) {
        if (strcmp(preferences->recent_projects[index].path, path) == 0) {
            existing = index;
            break;
        }
    }
    if (existing != UINT32_MAX) {
        move_count = existing;
    } else {
        if (preferences->recent_project_count < AF_EDITOR_RECENT_PROJECT_CAPACITY) {
            ++preferences->recent_project_count;
        }
        move_count = preferences->recent_project_count - 1U;
    }
    if (move_count != 0U) {
        memmove(&preferences->recent_projects[1], &preferences->recent_projects[0],
                (size_t)move_count * sizeof(preferences->recent_projects[0]));
    }
    memcpy(preferences->recent_projects[0].path, path, length + 1U);
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_preferences_remove_recent_project(AfEditorPreferences *preferences,
                                                     const char *path) {
    if (preferences == NULL || path == NULL || path[0] == '\0' ||
        !af_editor_preference_string_length(path, AF_EDITOR_RECENT_PATH_CAPACITY, NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < preferences->recent_project_count; ++index) {
        if (strcmp(preferences->recent_projects[index].path, path) == 0) {
            if (index + 1U < preferences->recent_project_count) {
                memmove(&preferences->recent_projects[index],
                        &preferences->recent_projects[index + 1U],
                        (size_t)(preferences->recent_project_count - index - 1U) *
                            sizeof(preferences->recent_projects[0]));
            }
            --preferences->recent_project_count;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

void af_editor_preferences_clear_recent_projects(AfEditorPreferences *preferences) {
    if (preferences != NULL) {
        preferences->recent_project_count = 0U;
    }
}

static AfResult af_editor_preference_write(AfEditorPreferenceWriter *writer, const char *format,
                                           ...) {
    va_list arguments;
    int written = 0;
    if (writer == NULL || format == NULL || writer->length >= writer->capacity) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    va_start(arguments, format);
    written = vsnprintf(writer->data + writer->length, writer->capacity - writer->length, format,
                        arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= writer->capacity - writer->length) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    writer->length += (size_t)written;
    return AF_RESULT_SUCCESS;
}

static bool af_editor_preference_unreserved(unsigned char value) {
    return (value >= (unsigned char)'A' && value <= (unsigned char)'Z') ||
           (value >= (unsigned char)'a' && value <= (unsigned char)'z') ||
           (value >= (unsigned char)'0' && value <= (unsigned char)'9') || value == '/' ||
           value == '.' || value == '_' || value == '-' || value == ':';
}

static AfResult af_editor_preference_encode(const char *text, char *output, size_t capacity) {
    static const char hexadecimal[] = "0123456789ABCDEF";
    size_t length = 0U;
    if (text == NULL || output == NULL || capacity == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        const unsigned char value = (unsigned char)text[index];
        const size_t required = af_editor_preference_unreserved(value) ? 1U : 3U;
        if (required >= capacity - length) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        if (required == 1U) {
            output[length++] = (char)value;
        } else {
            output[length++] = '%';
            output[length++] = hexadecimal[value >> 4U];
            output[length++] = hexadecimal[value & 0x0fU];
        }
    }
    output[length] = '\0';
    return AF_RESULT_SUCCESS;
}

static int af_editor_preference_hex(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

static AfResult af_editor_preference_decode(const char *text, char *output, size_t capacity) {
    size_t length = 0U;
    if (text == NULL || output == NULL || capacity == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        unsigned char value = (unsigned char)text[index];
        if (value == '%') {
            int high = 0;
            int low = 0;
            if (text[index + 1U] == '\0' || text[index + 2U] == '\0') {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            high = af_editor_preference_hex(text[index + 1U]);
            low = af_editor_preference_hex(text[index + 2U]);
            if (high < 0 || low < 0) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            value = (unsigned char)((unsigned int)high * 16U + (unsigned int)low);
            index += 2U;
            if (value == 0U) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        if (length + 1U >= capacity) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        output[length++] = (char)value;
    }
    output[length] = '\0';
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_preferences_serialize(const AfEditorPreferences *preferences, char *output,
                                         size_t output_capacity, size_t *out_size) {
    AfEditorPreferenceWriter writer = {.data = output,
                                       .capacity =
                                           output_capacity < AF_EDITOR_PREFERENCES_MAX_FILE_SIZE
                                               ? output_capacity
                                               : AF_EDITOR_PREFERENCES_MAX_FILE_SIZE};
    char encoded[AF_EDITOR_PREFERENCES_MAX_LINE_SIZE];
    uint32_t text_scale_milli = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (output == NULL || output_capacity == 0U ||
        af_editor_preferences_validate(preferences) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    output[0] = '\0';
    text_scale_milli = (uint32_t)(preferences->accessibility.text_scale * 1000.0F + 0.5F);
    result = af_editor_preference_write(&writer, "ARCFLOW_EDITOR_PREFERENCES %u\n",
                                        AF_EDITOR_PREFERENCES_VERSION);
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_preference_encode(preferences->workspace.name, encoded, sizeof(encoded));
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_preference_write(&writer, "workspace %s\n", encoded);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_preference_write(
            &writer,
            "theme %u\nautosave %u\ntext_scale_milli %u\nreduced_motion %u\nhigh_contrast "
            "%u\ncolor_blind %u\ntooltips %u\nextended_tooltips %u\nreopen_last %u\n",
            (uint32_t)preferences->theme, preferences->autosave_interval_seconds, text_scale_milli,
            (uint32_t)preferences->accessibility.reduced_motion,
            preferences->accessibility.high_contrast ? 1U : 0U,
            (uint32_t)preferences->accessibility.color_blind_preview,
            preferences->show_tooltips ? 1U : 0U, preferences->show_extended_tooltips ? 1U : 0U,
            preferences->reopen_last_project ? 1U : 0U);
    }
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < AF_EDITOR_DOCK_COUNT;
         ++index) {
        const AfEditorDockPreference *dock = &preferences->workspace.docks[index];
        result = af_editor_preference_write(&writer, "dock %u %u %u %u %u\n", index,
                                            dock->visible ? 1U : 0U, (uint32_t)dock->placement,
                                            dock->order, dock->extent_pixels);
    }
    for (uint32_t index = 0U;
         result == AF_RESULT_SUCCESS && index < preferences->shortcut_override_count; ++index) {
        const AfEditorShortcutOverride *shortcut = &preferences->shortcut_overrides[index];
        result = af_editor_preference_write(&writer, "shortcut %u %u %u\n", shortcut->action_id,
                                            shortcut->shortcut.key, shortcut->shortcut.modifiers);
    }
    for (uint32_t index = 0U;
         result == AF_RESULT_SUCCESS && index < preferences->recent_project_count; ++index) {
        result = af_editor_preference_encode(preferences->recent_projects[index].path, encoded,
                                             sizeof(encoded));
        if (result == AF_RESULT_SUCCESS) {
            result = af_editor_preference_write(&writer, "recent %s\n", encoded);
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_preference_write(&writer, "end\n");
    }
    if (result != AF_RESULT_SUCCESS) {
        output[0] = '\0';
        return result;
    }
    if (out_size != NULL) {
        *out_size = writer.length;
    }
    return AF_RESULT_SUCCESS;
}

static bool af_editor_preference_parse_u32(const char *text, uint32_t *out_value) {
    uint32_t value = 0U;
    if (text == NULL || text[0] == '\0' || out_value == NULL) {
        return false;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        const uint32_t digit = (uint32_t)(unsigned char)text[index] - (uint32_t)'0';
        if (text[index] < '0' || text[index] > '9' || value > (UINT32_MAX - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
    }
    *out_value = value;
    return true;
}

static uint32_t af_editor_preference_tokens(char *line, char *tokens[], uint32_t capacity) {
    uint32_t count = 0U;
    char *cursor = line;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        if (count >= capacity) {
            return capacity + 1U;
        }
        tokens[count++] = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
            ++cursor;
        }
        if (*cursor != '\0') {
            *cursor++ = '\0';
        }
    }
    return count;
}

static AfResult af_editor_preference_parse_scalar(AfEditorPreferences *preferences, char *tokens[],
                                                  uint32_t token_count) {
    uint32_t value = 0U;
    if (token_count != 2U || !af_editor_preference_parse_u32(tokens[1], &value)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (strcmp(tokens[0], "theme") == 0) {
        if (value >= AF_EDITOR_THEME_COUNT) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        preferences->theme = (AfEditorTheme)value;
    } else if (strcmp(tokens[0], "autosave") == 0) {
        if (value > AF_EDITOR_AUTOSAVE_MAX_SECONDS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        preferences->autosave_interval_seconds = value;
    } else if (strcmp(tokens[0], "text_scale_milli") == 0) {
        if (value < 500U || value > 3000U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        preferences->accessibility.text_scale = (float)value / 1000.0F;
    } else if (strcmp(tokens[0], "reduced_motion") == 0) {
        if (value >= AF_REDUCED_MOTION_MODE_COUNT) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        preferences->accessibility.reduced_motion = (AfReducedMotionMode)value;
    } else if (strcmp(tokens[0], "high_contrast") == 0) {
        if (value > 1U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        preferences->accessibility.high_contrast = value != 0U;
    } else if (strcmp(tokens[0], "color_blind") == 0) {
        if (value >= AF_COLOR_BLIND_PREVIEW_COUNT) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        preferences->accessibility.color_blind_preview = (AfColorBlindPreview)value;
    } else if (strcmp(tokens[0], "tooltips") == 0) {
        if (value > 1U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        preferences->show_tooltips = value != 0U;
    } else if (strcmp(tokens[0], "extended_tooltips") == 0) {
        if (value > 1U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        preferences->show_extended_tooltips = value != 0U;
    } else if (strcmp(tokens[0], "reopen_last") == 0) {
        if (value > 1U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        preferences->reopen_last_project = value != 0U;
    } else {
        return AF_RESULT_NOT_FOUND;
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_preference_parse_dock(AfEditorPreferences *preferences, char *tokens[],
                                                uint32_t token_count, bool seen[]) {
    uint32_t values[5];
    if (token_count != 6U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < 5U; ++index) {
        if (!af_editor_preference_parse_u32(tokens[index + 1U], &values[index])) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    if (values[0] >= AF_EDITOR_DOCK_COUNT || values[1] > 1U ||
        values[2] >= AF_EDITOR_DOCK_PLACEMENT_COUNT || values[3] >= AF_EDITOR_DOCK_COUNT ||
        values[4] < AF_EDITOR_DOCK_MIN_EXTENT || values[4] > AF_EDITOR_DOCK_MAX_EXTENT ||
        seen[values[0]]) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    seen[values[0]] = true;
    preferences->workspace.docks[values[0]] =
        (AfEditorDockPreference){.extent_pixels = values[4],
                                 .order = values[3],
                                 .placement = (AfEditorDockPlacement)values[2],
                                 .visible = values[1] != 0U};
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_preference_parse_shortcut(AfEditorPreferences *preferences,
                                                    char *tokens[], uint32_t token_count) {
    uint32_t action_id = 0U;
    uint32_t key = 0U;
    uint32_t modifiers = 0U;
    if (token_count != 4U || !af_editor_preference_parse_u32(tokens[1], &action_id) ||
        !af_editor_preference_parse_u32(tokens[2], &key) ||
        !af_editor_preference_parse_u32(tokens[3], &modifiers) ||
        af_editor_preferences_find_shortcut(preferences, action_id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return af_editor_preferences_set_shortcut(
        preferences, action_id, (AfEditorShortcut){.key = key, .modifiers = modifiers});
}

static AfResult af_editor_preference_append_recent(AfEditorPreferences *preferences,
                                                   const char *encoded) {
    AfEditorRecentProject recent = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (preferences->recent_project_count >= AF_EDITOR_RECENT_PROJECT_CAPACITY) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    result = af_editor_preference_decode(encoded, recent.path, sizeof(recent.path));
    if (result != AF_RESULT_SUCCESS || recent.path[0] == '\0') {
        return result != AF_RESULT_SUCCESS ? result : AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < preferences->recent_project_count; ++index) {
        if (strcmp(preferences->recent_projects[index].path, recent.path) == 0) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    preferences->recent_projects[preferences->recent_project_count++] = recent;
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_preferences_parse(const char *data, size_t size,
                                     AfEditorPreferences *out_preferences) {
    AfEditorPreferences preferences;
    bool seen_docks[AF_EDITOR_DOCK_COUNT] = {false};
    bool header_seen = false;
    bool end_seen = false;
    size_t offset = 0U;
    if (data == NULL || out_preferences == NULL || size == 0U ||
        size > AF_EDITOR_PREFERENCES_MAX_FILE_SIZE || memchr(data, '\0', size) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    af_editor_preferences_default(&preferences);
    preferences.shortcut_override_count = 0U;
    preferences.recent_project_count = 0U;
    while (offset < size) {
        char line[AF_EDITOR_PREFERENCES_MAX_LINE_SIZE];
        char *tokens[8];
        size_t line_length = 0U;
        uint32_t token_count = 0U;
        AfResult result = AF_RESULT_SUCCESS;
        while (offset + line_length < size && data[offset + line_length] != '\n') {
            ++line_length;
        }
        if (line_length >= sizeof(line)) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        memcpy(line, data + offset, line_length);
        if (line_length != 0U && line[line_length - 1U] == '\r') {
            --line_length;
        }
        line[line_length] = '\0';
        offset += line_length;
        if (offset < size && data[offset] == '\r') {
            ++offset;
        }
        if (offset < size && data[offset] == '\n') {
            ++offset;
        }
        token_count = af_editor_preference_tokens(line, tokens, 8U);
        if (token_count == 0U) {
            continue;
        }
        if (!header_seen) {
            uint32_t version = 0U;
            if (token_count != 2U || strcmp(tokens[0], "ARCFLOW_EDITOR_PREFERENCES") != 0 ||
                !af_editor_preference_parse_u32(tokens[1], &version)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            if (version != AF_EDITOR_PREFERENCES_VERSION) {
                return AF_RESULT_UNSUPPORTED_VERSION;
            }
            header_seen = true;
            continue;
        }
        if (end_seen) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (strcmp(tokens[0], "end") == 0) {
            if (token_count != 1U) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            end_seen = true;
            continue;
        }
        if (strcmp(tokens[0], "workspace") == 0) {
            if (token_count != 2U) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            result = af_editor_preference_decode(tokens[1], preferences.workspace.name,
                                                 sizeof(preferences.workspace.name));
        } else if (strcmp(tokens[0], "dock") == 0) {
            result = af_editor_preference_parse_dock(&preferences, tokens, token_count, seen_docks);
        } else if (strcmp(tokens[0], "shortcut") == 0) {
            result = af_editor_preference_parse_shortcut(&preferences, tokens, token_count);
        } else if (strcmp(tokens[0], "recent") == 0) {
            result = token_count == 2U ? af_editor_preference_append_recent(&preferences, tokens[1])
                                       : AF_RESULT_INVALID_ARGUMENT;
        } else {
            result = af_editor_preference_parse_scalar(&preferences, tokens, token_count);
            if (result == AF_RESULT_NOT_FOUND) {
                result = AF_RESULT_SUCCESS;
            }
        }
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    if (!header_seen || !end_seen ||
        af_editor_preferences_validate(&preferences) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_preferences = preferences;
    return AF_RESULT_SUCCESS;
}
