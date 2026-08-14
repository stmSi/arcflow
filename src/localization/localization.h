#ifndef AF_LOCALIZATION_H
#define AF_LOCALIZATION_H

#include "arcflow/arcflow_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_LOCALIZATION_MAX_LOCALES 32U
#define AF_LOCALIZATION_MAX_TRANSLATIONS 4096U
#define AF_LOCALIZATION_MAX_MISSING_DIAGNOSTICS 256U
#define AF_LOCALIZATION_MAX_FONT_FALLBACKS 8U
#define AF_LOCALIZATION_LOCALE_TAG_CAPACITY 32U
#define AF_LOCALIZATION_SEPARATOR_CAPACITY 8U
#define AF_LOCALIZATION_MERIDIEM_CAPACITY 16U
#define AF_LOCALIZATION_TEXT_CAPACITY 256U
#define AF_LOCALIZATION_FORMAT_CAPACITY 1024U
#define AF_LOCALIZATION_MAX_FRACTION_DIGITS 9U
#define AF_LOCALIZATION_MAX_EXPANSION_PERCENT 200U

#define AF_ACCESSIBILITY_MAX_NODES 4096U
#define AF_ACCESSIBILITY_LABEL_CAPACITY 128U
#define AF_ACCESSIBILITY_DESCRIPTION_CAPACITY 256U
#define AF_ACCESSIBILITY_MIN_TEXT_SCALE 0.5F
#define AF_ACCESSIBILITY_MAX_TEXT_SCALE 3.0F
#define AF_ACCESSIBILITY_MAX_TEXT_SIZE 4096.0F

typedef enum AfLocaleDateOrder {
    AF_LOCALE_DATE_YEAR_MONTH_DAY = 0,
    AF_LOCALE_DATE_DAY_MONTH_YEAR,
    AF_LOCALE_DATE_MONTH_DAY_YEAR,
    AF_LOCALE_DATE_ORDER_COUNT
} AfLocaleDateOrder;

typedef struct AfLocaleDefinition {
    uint64_t id;
    uint64_t fallback_locale_id;
    char tag[AF_LOCALIZATION_LOCALE_TAG_CAPACITY];
    char decimal_separator[AF_LOCALIZATION_SEPARATOR_CAPACITY];
    char grouping_separator[AF_LOCALIZATION_SEPARATOR_CAPACITY];
    char date_separator[AF_LOCALIZATION_SEPARATOR_CAPACITY];
    char time_separator[AF_LOCALIZATION_SEPARATOR_CAPACITY];
    char ante_meridiem[AF_LOCALIZATION_MERIDIEM_CAPACITY];
    char post_meridiem[AF_LOCALIZATION_MERIDIEM_CAPACITY];
    uint64_t font_fallback_ids[AF_LOCALIZATION_MAX_FONT_FALLBACKS];
    uint32_t font_fallback_count;
    AfLocaleDateOrder date_order;
    bool use_24_hour_time;
    bool right_to_left;
    bool mirror_layout;
} AfLocaleDefinition;

typedef struct AfTranslation {
    uint64_t locale_id;
    uint64_t key_id;
    char text[AF_LOCALIZATION_TEXT_CAPACITY];
} AfTranslation;

typedef struct AfMissingTranslationDiagnostic {
    uint64_t locale_id;
    uint64_t key_id;
    uint64_t occurrence_count;
} AfMissingTranslationDiagnostic;

typedef struct AfLocalization {
    AfLocaleDefinition *locales;
    uint32_t locale_count;
    uint32_t locale_capacity;
    AfTranslation *translations;
    uint32_t translation_count;
    uint32_t translation_capacity;
    AfMissingTranslationDiagnostic *missing_diagnostics;
    uint32_t missing_diagnostic_count;
    uint32_t missing_diagnostic_capacity;
    uint64_t active_locale_id;
    uint64_t default_locale_id;
    uint64_t dropped_missing_diagnostic_count;
    uint32_t expansion_percent;
} AfLocalization;

typedef struct AfLocalDate {
    uint32_t year;
    uint32_t month;
    uint32_t day;
} AfLocalDate;

typedef struct AfLocalTime {
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
} AfLocalTime;

typedef struct AfLocalizationFormatArguments {
    double number;
    AfLocalDate date;
    AfLocalTime time;
    uint32_t fractional_digits;
    bool use_grouping;
    bool has_number;
    bool has_date;
    bool has_time;
} AfLocalizationFormatArguments;

AfResult af_localization_init(AfLocalization *localization, AfLocaleDefinition *locales,
                              uint32_t locale_capacity, AfTranslation *translations,
                              uint32_t translation_capacity,
                              AfMissingTranslationDiagnostic *missing_diagnostics,
                              uint32_t missing_diagnostic_capacity);
AfResult af_localization_add_locale(AfLocalization *localization, const AfLocaleDefinition *locale);
AfResult af_localization_set_locale(AfLocalization *localization, uint64_t locale_id);
const AfLocaleDefinition *af_localization_active_locale(const AfLocalization *localization);
AfResult af_localization_set_translation(AfLocalization *localization, uint64_t locale_id,
                                         uint64_t key_id, const char *text);
AfResult af_localization_get_text(AfLocalization *localization, uint64_t key_id,
                                  const char **out_text, bool *out_used_fallback);
AfResult af_localization_set_expansion(AfLocalization *localization, uint32_t percent);
AfResult af_localization_format_number(const AfLocalization *localization, double value,
                                       uint32_t fractional_digits, bool use_grouping, char *output,
                                       size_t output_capacity, size_t *out_length);
AfResult af_localization_format_date(const AfLocalization *localization, AfLocalDate date,
                                     char *output, size_t output_capacity, size_t *out_length);
AfResult af_localization_format_time(const AfLocalization *localization, AfLocalTime time,
                                     char *output, size_t output_capacity, size_t *out_length);
AfResult af_localization_format_message(AfLocalization *localization, uint64_t key_id,
                                        const AfLocalizationFormatArguments *arguments,
                                        char *output, size_t output_capacity, size_t *out_length);
AfResult af_localization_expand_text(const char *text, uint32_t percent, char *output,
                                     size_t output_capacity, size_t *out_length);
bool af_localization_is_right_to_left(const AfLocalization *localization);
bool af_localization_mirrors_layout(const AfLocalization *localization);
AfResult af_localization_font_fallbacks(const AfLocalization *localization,
                                        const uint64_t **out_font_ids, uint32_t *out_count);
const AfMissingTranslationDiagnostic *
af_localization_missing_diagnostics(const AfLocalization *localization, uint32_t *out_count,
                                    uint64_t *out_dropped_diagnostic_count);
void af_localization_clear_missing_diagnostics(AfLocalization *localization);

typedef enum AfAccessibilityRole {
    AF_ACCESSIBILITY_ROLE_NONE = 0,
    AF_ACCESSIBILITY_ROLE_BUTTON,
    AF_ACCESSIBILITY_ROLE_CHECKBOX,
    AF_ACCESSIBILITY_ROLE_SLIDER,
    AF_ACCESSIBILITY_ROLE_TEXT,
    AF_ACCESSIBILITY_ROLE_IMAGE,
    AF_ACCESSIBILITY_ROLE_LINK,
    AF_ACCESSIBILITY_ROLE_LIST,
    AF_ACCESSIBILITY_ROLE_LIST_ITEM,
    AF_ACCESSIBILITY_ROLE_TEXT_FIELD,
    AF_ACCESSIBILITY_ROLE_MENU_ITEM,
    AF_ACCESSIBILITY_ROLE_PROGRESS,
    AF_ACCESSIBILITY_ROLE_COUNT
} AfAccessibilityRole;

typedef enum AfAccessibilityState {
    AF_ACCESSIBILITY_STATE_DISABLED = 1U << 0U,
    AF_ACCESSIBILITY_STATE_SELECTED = 1U << 1U,
    AF_ACCESSIBILITY_STATE_CHECKED = 1U << 2U,
    AF_ACCESSIBILITY_STATE_EXPANDED = 1U << 3U,
    AF_ACCESSIBILITY_STATE_BUSY = 1U << 4U,
    AF_ACCESSIBILITY_STATE_HIDDEN = 1U << 5U,
    AF_ACCESSIBILITY_STATE_READ_ONLY = 1U << 6U,
    AF_ACCESSIBILITY_STATE_REQUIRED = 1U << 7U
} AfAccessibilityState;

typedef struct AfAccessibilityNode {
    uint64_t node_id;
    char label[AF_ACCESSIBILITY_LABEL_CAPACITY];
    char description[AF_ACCESSIBILITY_DESCRIPTION_CAPACITY];
    uint32_t states;
    uint32_t focus_order;
    AfAccessibilityRole role;
    bool focusable;
} AfAccessibilityNode;

typedef struct AfAccessibilityTree {
    AfAccessibilityNode *nodes;
    uint32_t count;
    uint32_t capacity;
} AfAccessibilityTree;

typedef enum AfReducedMotionMode {
    AF_REDUCED_MOTION_OFF = 0,
    AF_REDUCED_MOTION_SIMPLIFY,
    AF_REDUCED_MOTION_DISABLE,
    AF_REDUCED_MOTION_MODE_COUNT
} AfReducedMotionMode;

typedef enum AfColorBlindPreview {
    AF_COLOR_BLIND_PREVIEW_NONE = 0,
    AF_COLOR_BLIND_PREVIEW_PROTANOPIA,
    AF_COLOR_BLIND_PREVIEW_DEUTERANOPIA,
    AF_COLOR_BLIND_PREVIEW_TRITANOPIA,
    AF_COLOR_BLIND_PREVIEW_ACHROMATOPSIA,
    AF_COLOR_BLIND_PREVIEW_COUNT
} AfColorBlindPreview;

typedef struct AfAccessibilitySettings {
    float text_scale;
    AfReducedMotionMode reduced_motion;
    AfColorBlindPreview color_blind_preview;
    bool high_contrast;
} AfAccessibilitySettings;

AfResult af_accessibility_tree_init(AfAccessibilityTree *tree, AfAccessibilityNode *nodes,
                                    uint32_t capacity);
AfResult af_accessibility_set_node(AfAccessibilityTree *tree, const AfAccessibilityNode *node);
AfResult af_accessibility_remove_node(AfAccessibilityTree *tree, uint64_t node_id);
const AfAccessibilityNode *af_accessibility_find_node(const AfAccessibilityTree *tree,
                                                      uint64_t node_id);
const AfAccessibilityNode *af_accessibility_nodes(const AfAccessibilityTree *tree,
                                                  uint32_t *out_count);
AfResult af_accessibility_focus_next(const AfAccessibilityTree *tree, uint64_t current_node_id,
                                     bool wrap, uint64_t *out_node_id);
AfResult af_accessibility_focus_previous(const AfAccessibilityTree *tree, uint64_t current_node_id,
                                         bool wrap, uint64_t *out_node_id);
void af_accessibility_settings_default(AfAccessibilitySettings *settings);
AfResult af_accessibility_settings_validate(const AfAccessibilitySettings *settings);
AfResult af_accessibility_scaled_text_size(const AfAccessibilitySettings *settings, float base_size,
                                           float *out_size);
AfResult af_accessibility_adjust_motion_duration(const AfAccessibilitySettings *settings,
                                                 float duration, float *out_duration);

#endif
