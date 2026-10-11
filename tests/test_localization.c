#include "af/localization/localization.h"

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

typedef struct AfLocalizationFixture {
    AfLocaleDefinition locales[3];
    AfTranslation translations[8];
    AfMissingTranslationDiagnostic diagnostics[2];
    AfLocalization localization;
} AfLocalizationFixture;

static bool af_test_localization_fixture(AfLocalizationFixture *fixture) {
    const AfLocaleDefinition english = {.id = 1U,
                                        .tag = "en-US",
                                        .decimal_separator = ".",
                                        .grouping_separator = ",",
                                        .date_separator = "/",
                                        .time_separator = ":",
                                        .ante_meridiem = "AM",
                                        .post_meridiem = "PM",
                                        .font_fallback_ids = {101U, 102U},
                                        .font_fallback_count = 2U,
                                        .date_order = AF_LOCALE_DATE_MONTH_DAY_YEAR};
    const AfLocaleDefinition french = {.id = 2U,
                                       .fallback_locale_id = 1U,
                                       .tag = "fr-FR",
                                       .decimal_separator = ",",
                                       .grouping_separator = " ",
                                       .date_separator = "/",
                                       .time_separator = ":",
                                       .ante_meridiem = "AM",
                                       .post_meridiem = "PM",
                                       .font_fallback_ids = {201U},
                                       .font_fallback_count = 1U,
                                       .date_order = AF_LOCALE_DATE_DAY_MONTH_YEAR,
                                       .use_24_hour_time = true};
    const AfLocaleDefinition arabic = {.id = 3U,
                                       .fallback_locale_id = 1U,
                                       .tag = "ar",
                                       .decimal_separator = ".",
                                       .grouping_separator = ",",
                                       .date_separator = "-",
                                       .time_separator = ":",
                                       .ante_meridiem = "AM",
                                       .post_meridiem = "PM",
                                       .font_fallback_ids = {301U, 302U},
                                       .font_fallback_count = 2U,
                                       .date_order = AF_LOCALE_DATE_YEAR_MONTH_DAY,
                                       .use_24_hour_time = true,
                                       .right_to_left = true,
                                       .mirror_layout = true};
    *fixture = (AfLocalizationFixture){0};
    CHECK(af_localization_init(&fixture->localization, fixture->locales, 3U, fixture->translations,
                               8U, fixture->diagnostics, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_localization_add_locale(&fixture->localization, &english) == AF_RESULT_SUCCESS);
    CHECK(af_localization_add_locale(&fixture->localization, &french) == AF_RESULT_SUCCESS);
    CHECK(af_localization_add_locale(&fixture->localization, &arabic) == AF_RESULT_SUCCESS);
    CHECK(af_localization_set_translation(&fixture->localization, 1U, 10U, "Open") ==
          AF_RESULT_SUCCESS);
    CHECK(af_localization_set_translation(&fixture->localization, 2U, 10U, "Ouvrir") ==
          AF_RESULT_SUCCESS);
    CHECK(af_localization_set_translation(&fixture->localization, 1U, 20U,
                                          "Value {number}, date {date}, time {time}, literal {{") ==
          AF_RESULT_SUCCESS);
    return true;
}

static bool af_test_locale_switch_and_diagnostics(void) {
    AfLocalizationFixture fixture;
    const char *text = NULL;
    const uint64_t *font_ids = NULL;
    const AfMissingTranslationDiagnostic *diagnostics = NULL;
    uint32_t count = 0U;
    uint64_t dropped = 0U;
    bool fallback = false;
    CHECK(af_test_localization_fixture(&fixture));
    CHECK(af_localization_set_locale(&fixture.localization, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_localization_get_text(&fixture.localization, 10U, &text, &fallback) ==
          AF_RESULT_SUCCESS);
    CHECK(strcmp(text, "Ouvrir") == 0 && !fallback);
    CHECK(af_localization_get_text(&fixture.localization, 20U, &text, &fallback) ==
          AF_RESULT_SUCCESS);
    CHECK(fallback && strstr(text, "{number}") != NULL);
    CHECK(af_localization_font_fallbacks(&fixture.localization, &font_ids, &count) ==
          AF_RESULT_SUCCESS);
    CHECK(count == 1U && font_ids[0] == 201U);

    CHECK(af_localization_get_text(&fixture.localization, 999U, &text, NULL) ==
          AF_RESULT_NOT_FOUND);
    CHECK(af_localization_get_text(&fixture.localization, 999U, &text, NULL) ==
          AF_RESULT_NOT_FOUND);
    CHECK(af_localization_get_text(&fixture.localization, 1000U, &text, NULL) ==
          AF_RESULT_NOT_FOUND);
    CHECK(af_localization_get_text(&fixture.localization, 1001U, &text, NULL) ==
          AF_RESULT_NOT_FOUND);
    diagnostics = af_localization_missing_diagnostics(&fixture.localization, &count, &dropped);
    CHECK(diagnostics != NULL && count == 2U && dropped == 1U);
    CHECK(diagnostics[0].locale_id == 2U && diagnostics[0].key_id == 999U &&
          diagnostics[0].occurrence_count == 2U);
    af_localization_clear_missing_diagnostics(&fixture.localization);
    (void)af_localization_missing_diagnostics(&fixture.localization, &count, &dropped);
    CHECK(count == 0U && dropped == 0U);

    CHECK(af_localization_set_locale(&fixture.localization, 3U) == AF_RESULT_SUCCESS);
    CHECK(af_localization_is_right_to_left(&fixture.localization));
    CHECK(af_localization_mirrors_layout(&fixture.localization));
    CHECK(af_localization_set_locale(&fixture.localization, 77U) == AF_RESULT_NOT_FOUND);
    CHECK(af_localization_active_locale(&fixture.localization)->id == 3U);
    return true;
}

static bool af_test_locale_formatting(void) {
    AfLocalizationFixture fixture;
    AfLocalizationFormatArguments arguments = {.number = -1234567.5,
                                               .date = {2024U, 2U, 29U},
                                               .time = {13U, 5U, 9U},
                                               .fractional_digits = 2U,
                                               .use_grouping = true,
                                               .has_number = true,
                                               .has_date = true,
                                               .has_time = true};
    char output[256];
    char short_output[8];
    size_t length = 0U;
    CHECK(af_test_localization_fixture(&fixture));
    CHECK(af_localization_format_number(&fixture.localization, arguments.number, 2U, true, output,
                                        sizeof(output), &length) == AF_RESULT_SUCCESS);
    CHECK(strcmp(output, "-1,234,567.50") == 0 && length == strlen(output));
    CHECK(af_localization_format_date(&fixture.localization, arguments.date, output, sizeof(output),
                                      NULL) == AF_RESULT_SUCCESS);
    CHECK(strcmp(output, "02/29/2024") == 0);
    CHECK(af_localization_format_time(&fixture.localization, arguments.time, output, sizeof(output),
                                      NULL) == AF_RESULT_SUCCESS);
    CHECK(strcmp(output, "01:05:09 PM") == 0);
    CHECK(af_localization_format_date(&fixture.localization, (AfLocalDate){2023U, 2U, 29U}, output,
                                      sizeof(output), NULL) == AF_RESULT_INVALID_ARGUMENT);

    CHECK(af_localization_set_locale(&fixture.localization, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_localization_format_number(&fixture.localization, arguments.number, 2U, true, output,
                                        sizeof(output), NULL) == AF_RESULT_SUCCESS);
    CHECK(strcmp(output, "-1 234 567,50") == 0);
    CHECK(af_localization_format_date(&fixture.localization, arguments.date, output, sizeof(output),
                                      NULL) == AF_RESULT_SUCCESS);
    CHECK(strcmp(output, "29/02/2024") == 0);
    CHECK(af_localization_format_time(&fixture.localization, arguments.time, output, sizeof(output),
                                      NULL) == AF_RESULT_SUCCESS);
    CHECK(strcmp(output, "13:05:09") == 0);
    CHECK(af_localization_format_message(&fixture.localization, 20U, &arguments, output,
                                         sizeof(output), &length) == AF_RESULT_SUCCESS);
    CHECK(strcmp(output, "Value -1 234 567,50, date 29/02/2024, time 13:05:09, literal {") == 0);
    CHECK(length == strlen(output));
    CHECK(af_localization_format_message(&fixture.localization, 20U, &arguments, short_output,
                                         sizeof(short_output), NULL) == AF_RESULT_LIMIT_EXCEEDED);

    CHECK(af_localization_set_expansion(&fixture.localization, 150U) == AF_RESULT_SUCCESS);
    CHECK(af_localization_format_message(&fixture.localization, 10U, NULL, output, sizeof(output),
                                         &length) == AF_RESULT_SUCCESS);
    CHECK(strcmp(output, "Ouvrir~~~") == 0 && length == 9U);
    CHECK(af_localization_expand_text("abc", 200U, output, sizeof(output), &length) ==
          AF_RESULT_SUCCESS);
    CHECK(strcmp(output, "abc~~~") == 0 && length == 6U);
    return true;
}

static bool af_test_accessibility(void) {
    AfAccessibilityNode storage[5];
    AfAccessibilityTree tree;
    AfAccessibilitySettings settings;
    uint64_t focused = 0U;
    uint32_t count = 0U;
    float value = 0.0F;
    const AfAccessibilityNode button = {.node_id = 30U,
                                        .label = "Confirm",
                                        .description = "Accept changes",
                                        .focus_order = 2U,
                                        .role = AF_ACCESSIBILITY_ROLE_BUTTON,
                                        .focusable = true};
    const AfAccessibilityNode first = {.node_id = 20U,
                                       .label = "Name",
                                       .focus_order = 1U,
                                       .role = AF_ACCESSIBILITY_ROLE_TEXT_FIELD,
                                       .focusable = true};
    const AfAccessibilityNode tie = {.node_id = 10U,
                                     .label = "Back",
                                     .focus_order = 1U,
                                     .role = AF_ACCESSIBILITY_ROLE_BUTTON,
                                     .focusable = true};
    const AfAccessibilityNode disabled = {.node_id = 40U,
                                          .label = "Unavailable",
                                          .states = AF_ACCESSIBILITY_STATE_DISABLED,
                                          .focus_order = 3U,
                                          .role = AF_ACCESSIBILITY_ROLE_BUTTON,
                                          .focusable = true};
    CHECK(af_accessibility_tree_init(&tree, storage, 5U) == AF_RESULT_SUCCESS);
    CHECK(af_accessibility_set_node(&tree, &button) == AF_RESULT_SUCCESS);
    CHECK(af_accessibility_set_node(&tree, &first) == AF_RESULT_SUCCESS);
    CHECK(af_accessibility_set_node(&tree, &tie) == AF_RESULT_SUCCESS);
    CHECK(af_accessibility_set_node(&tree, &disabled) == AF_RESULT_SUCCESS);
    CHECK(af_accessibility_nodes(&tree, &count) == storage && count == 4U);
    CHECK(strcmp(af_accessibility_find_node(&tree, 30U)->description, "Accept changes") == 0);
    CHECK(af_accessibility_focus_next(&tree, 0U, false, &focused) == AF_RESULT_SUCCESS &&
          focused == 10U);
    CHECK(af_accessibility_focus_next(&tree, 10U, false, &focused) == AF_RESULT_SUCCESS &&
          focused == 20U);
    CHECK(af_accessibility_focus_next(&tree, 20U, false, &focused) == AF_RESULT_SUCCESS &&
          focused == 30U);
    CHECK(af_accessibility_focus_next(&tree, 30U, false, &focused) == AF_RESULT_NOT_FOUND);
    CHECK(af_accessibility_focus_next(&tree, 30U, true, &focused) == AF_RESULT_SUCCESS &&
          focused == 10U);
    CHECK(af_accessibility_focus_previous(&tree, 10U, true, &focused) == AF_RESULT_SUCCESS &&
          focused == 30U);
    CHECK(af_accessibility_remove_node(&tree, 20U) == AF_RESULT_SUCCESS);
    CHECK(af_accessibility_find_node(&tree, 20U) == NULL && tree.count == 3U);

    af_accessibility_settings_default(&settings);
    CHECK(af_accessibility_settings_validate(&settings) == AF_RESULT_SUCCESS);
    settings.text_scale = 1.5F;
    settings.reduced_motion = AF_REDUCED_MOTION_SIMPLIFY;
    settings.high_contrast = true;
    settings.color_blind_preview = AF_COLOR_BLIND_PREVIEW_DEUTERANOPIA;
    CHECK(af_accessibility_scaled_text_size(&settings, 20.0F, &value) == AF_RESULT_SUCCESS &&
          value == 30.0F);
    CHECK(af_accessibility_adjust_motion_duration(&settings, 1.0F, &value) == AF_RESULT_SUCCESS &&
          value == 0.1F);
    settings.reduced_motion = AF_REDUCED_MOTION_DISABLE;
    CHECK(af_accessibility_adjust_motion_duration(&settings, 1.0F, &value) == AF_RESULT_SUCCESS &&
          value == 0.0F);
    settings.text_scale = 10.0F;
    CHECK(af_accessibility_settings_validate(&settings) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

int main(void) {
    static const struct {
        const char *name;
        bool (*function)(void);
    } tests[] = {{"locale switching and diagnostics", af_test_locale_switch_and_diagnostics},
                 {"locale formatting", af_test_locale_formatting},
                 {"accessibility metadata", af_test_accessibility}};
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
