#ifndef AF_EDITOR_ACTIONS_H
#define AF_EDITOR_ACTIONS_H

#include "arcflow/arcflow_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_EDITOR_MAX_ACTIONS 256U
#define AF_EDITOR_ACTION_LABEL_CAPACITY 64U
#define AF_EDITOR_ACTION_TOOLTIP_CAPACITY 192U
#define AF_EDITOR_ACTION_KEYWORDS_CAPACITY 128U
#define AF_EDITOR_ACTION_SEARCH_CAPACITY 128U
#define AF_EDITOR_COMMAND_QUERY_CAPACITY 96U
#define AF_EDITOR_SHORTCUT_TEXT_CAPACITY 64U

typedef uint32_t AfEditorActionId;

typedef enum AfEditorActionContext {
    AF_EDITOR_ACTION_CONTEXT_GLOBAL = 1U << 0U,
    AF_EDITOR_ACTION_CONTEXT_CANVAS = 1U << 1U,
    AF_EDITOR_ACTION_CONTEXT_HIERARCHY = 1U << 2U,
    AF_EDITOR_ACTION_CONTEXT_INSPECTOR = 1U << 3U,
    AF_EDITOR_ACTION_CONTEXT_TIMELINE = 1U << 4U,
    AF_EDITOR_ACTION_CONTEXT_CURVE_EDITOR = 1U << 5U,
    AF_EDITOR_ACTION_CONTEXT_ASSET_BROWSER = 1U << 6U,
    AF_EDITOR_ACTION_CONTEXT_TEXT_INPUT = 1U << 7U
} AfEditorActionContext;

#define AF_EDITOR_ACTION_CONTEXT_ALL UINT32_C(0x000000ff)

typedef enum AfEditorShortcutModifier {
    AF_EDITOR_SHORTCUT_MODIFIER_CTRL = 1U << 0U,
    AF_EDITOR_SHORTCUT_MODIFIER_SHIFT = 1U << 1U,
    AF_EDITOR_SHORTCUT_MODIFIER_ALT = 1U << 2U,
    AF_EDITOR_SHORTCUT_MODIFIER_SUPER = 1U << 3U
} AfEditorShortcutModifier;

#define AF_EDITOR_SHORTCUT_MODIFIER_MASK UINT32_C(0x0000000f)

typedef enum AfEditorSpecialKey {
    AF_EDITOR_KEY_ESCAPE = 0x100U,
    AF_EDITOR_KEY_ENTER,
    AF_EDITOR_KEY_TAB,
    AF_EDITOR_KEY_SPACE,
    AF_EDITOR_KEY_BACKSPACE,
    AF_EDITOR_KEY_DELETE,
    AF_EDITOR_KEY_HOME,
    AF_EDITOR_KEY_END,
    AF_EDITOR_KEY_PAGE_UP,
    AF_EDITOR_KEY_PAGE_DOWN,
    AF_EDITOR_KEY_LEFT,
    AF_EDITOR_KEY_RIGHT,
    AF_EDITOR_KEY_UP,
    AF_EDITOR_KEY_DOWN,
    AF_EDITOR_KEY_F1 = 0x120U,
    AF_EDITOR_KEY_F2,
    AF_EDITOR_KEY_F3,
    AF_EDITOR_KEY_F4,
    AF_EDITOR_KEY_F5,
    AF_EDITOR_KEY_F6,
    AF_EDITOR_KEY_F7,
    AF_EDITOR_KEY_F8,
    AF_EDITOR_KEY_F9,
    AF_EDITOR_KEY_F10,
    AF_EDITOR_KEY_F11,
    AF_EDITOR_KEY_F12
} AfEditorSpecialKey;

typedef struct AfEditorShortcut {
    uint32_t key;
    uint32_t modifiers;
} AfEditorShortcut;

typedef AfResult (*AfEditorActionExecuteFn)(void *user, AfEditorActionId action_id);

typedef struct AfEditorActionDefinition {
    AfEditorActionId id;
    const char *label;
    const char *tooltip;
    const char *keywords;
    AfEditorShortcut default_shortcut;
    uint32_t contexts;
    AfEditorActionExecuteFn execute;
    void *user;
} AfEditorActionDefinition;

typedef struct AfEditorAction {
    AfEditorActionId id;
    char label[AF_EDITOR_ACTION_LABEL_CAPACITY];
    char tooltip[AF_EDITOR_ACTION_TOOLTIP_CAPACITY];
    char keywords[AF_EDITOR_ACTION_KEYWORDS_CAPACITY];
    AfEditorShortcut default_shortcut;
    AfEditorShortcut shortcut;
    uint32_t contexts;
    AfEditorActionExecuteFn execute;
    void *user;
    bool enabled;
    bool visible;
} AfEditorAction;

typedef struct AfEditorActionRegistry {
    AfEditorAction *actions;
    uint32_t count;
    uint32_t capacity;
} AfEditorActionRegistry;

typedef struct AfEditorActionSearchResult {
    AfEditorActionId action_id;
    uint32_t score;
} AfEditorActionSearchResult;

typedef enum AfEditorCommandPaletteKey {
    AF_EDITOR_COMMAND_PALETTE_UP = 0,
    AF_EDITOR_COMMAND_PALETTE_DOWN,
    AF_EDITOR_COMMAND_PALETTE_HOME,
    AF_EDITOR_COMMAND_PALETTE_END,
    AF_EDITOR_COMMAND_PALETTE_PAGE_UP,
    AF_EDITOR_COMMAND_PALETTE_PAGE_DOWN,
    AF_EDITOR_COMMAND_PALETTE_ACTIVATE,
    AF_EDITOR_COMMAND_PALETTE_CANCEL
} AfEditorCommandPaletteKey;

typedef struct AfEditorCommandPalette {
    const AfEditorActionRegistry *registry;
    AfEditorActionSearchResult results[AF_EDITOR_ACTION_SEARCH_CAPACITY];
    char query[AF_EDITOR_COMMAND_QUERY_CAPACITY];
    uint32_t result_count;
    uint32_t selected_index;
    uint32_t page_size;
    uint32_t contexts;
    bool open;
} AfEditorCommandPalette;

AfResult af_editor_action_registry_init(AfEditorActionRegistry *registry, AfEditorAction *actions,
                                        uint32_t capacity);
AfResult af_editor_action_register(AfEditorActionRegistry *registry,
                                   const AfEditorActionDefinition *definition);
const AfEditorAction *af_editor_action_find(const AfEditorActionRegistry *registry,
                                            AfEditorActionId action_id);
AfResult af_editor_action_set_enabled(AfEditorActionRegistry *registry, AfEditorActionId action_id,
                                      bool enabled);
AfResult af_editor_action_set_visible(AfEditorActionRegistry *registry, AfEditorActionId action_id,
                                      bool visible);
AfResult af_editor_action_set_shortcut(AfEditorActionRegistry *registry, AfEditorActionId action_id,
                                       AfEditorShortcut shortcut,
                                       AfEditorActionId *out_conflicting_action_id);
AfResult af_editor_action_reset_shortcut(AfEditorActionRegistry *registry,
                                         AfEditorActionId action_id,
                                         AfEditorActionId *out_conflicting_action_id);
AfResult af_editor_action_shortcut_conflict(const AfEditorActionRegistry *registry,
                                            AfEditorActionId action_id, AfEditorShortcut shortcut,
                                            AfEditorActionId *out_conflicting_action_id);
AfResult af_editor_action_resolve_shortcut(const AfEditorActionRegistry *registry,
                                           AfEditorShortcut shortcut, uint32_t contexts,
                                           AfEditorActionId *out_action_id);
AfResult af_editor_action_execute(const AfEditorActionRegistry *registry,
                                  AfEditorActionId action_id);
AfResult af_editor_action_search(const AfEditorActionRegistry *registry, const char *query,
                                 uint32_t contexts, AfEditorActionSearchResult *results,
                                 uint32_t result_capacity, uint32_t *out_result_count);
AfEditorShortcut af_editor_shortcut_normalize(AfEditorShortcut shortcut);
bool af_editor_shortcut_equal(AfEditorShortcut left, AfEditorShortcut right);
bool af_editor_shortcut_is_bound(AfEditorShortcut shortcut);
AfResult af_editor_shortcut_format(AfEditorShortcut shortcut, char *output, size_t output_capacity);

AfResult af_editor_command_palette_init(AfEditorCommandPalette *palette,
                                        const AfEditorActionRegistry *registry, uint32_t page_size);
AfResult af_editor_command_palette_open(AfEditorCommandPalette *palette, uint32_t contexts);
void af_editor_command_palette_close(AfEditorCommandPalette *palette);
AfResult af_editor_command_palette_set_query(AfEditorCommandPalette *palette, const char *query);
AfResult af_editor_command_palette_handle_key(AfEditorCommandPalette *palette,
                                              AfEditorCommandPaletteKey key,
                                              AfEditorActionId *out_action_id);
const AfEditorAction *af_editor_command_palette_selected(const AfEditorCommandPalette *palette);

#endif
