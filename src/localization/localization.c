#include "localization/localization.h"

#include "core/safe_math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define AF_ACCESSIBILITY_STATE_MASK ((1U << 8U) - 1U)
#define AF_LOCALIZATION_MAX_FALLBACK_DEPTH AF_LOCALIZATION_MAX_LOCALES

typedef struct AfLocalizationWriter {
    char *data;
    size_t capacity;
    size_t length;
} AfLocalizationWriter;

static bool af_string_length_bounded(const char *text, size_t capacity, size_t *out_length) {
    const char *end = NULL;
    if (text == NULL) {
        return false;
    }
    end = memchr(text, '\0', capacity);
    if (end == NULL) {
        return false;
    }
    if (out_length != NULL) {
        *out_length = (size_t)(end - text);
    }
    return true;
}

static bool af_string_bounded(const char *text, size_t capacity) {
    return af_string_length_bounded(text, capacity, NULL);
}

static AfResult af_copy_bounded(char *destination, size_t capacity, const char *source) {
    size_t length = 0U;
    if (destination == NULL || capacity == 0U || source == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!af_string_length_bounded(source, capacity, &length)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(destination, source, length + 1U);
    return AF_RESULT_SUCCESS;
}

static bool af_locale_definition_valid(const AfLocaleDefinition *locale) {
    if (locale == NULL || locale->id == 0U || locale->fallback_locale_id == locale->id ||
        locale->font_fallback_count > AF_LOCALIZATION_MAX_FONT_FALLBACKS ||
        locale->date_order >= AF_LOCALE_DATE_ORDER_COUNT ||
        !af_string_bounded(locale->tag, sizeof(locale->tag)) || locale->tag[0] == '\0' ||
        !af_string_bounded(locale->decimal_separator, sizeof(locale->decimal_separator)) ||
        locale->decimal_separator[0] == '\0' ||
        !af_string_bounded(locale->grouping_separator, sizeof(locale->grouping_separator)) ||
        !af_string_bounded(locale->date_separator, sizeof(locale->date_separator)) ||
        locale->date_separator[0] == '\0' ||
        !af_string_bounded(locale->time_separator, sizeof(locale->time_separator)) ||
        locale->time_separator[0] == '\0' ||
        !af_string_bounded(locale->ante_meridiem, sizeof(locale->ante_meridiem)) ||
        !af_string_bounded(locale->post_meridiem, sizeof(locale->post_meridiem))) {
        return false;
    }
    if (!locale->use_24_hour_time &&
        (locale->ante_meridiem[0] == '\0' || locale->post_meridiem[0] == '\0')) {
        return false;
    }
    for (uint32_t index = 0U; index < locale->font_fallback_count; ++index) {
        if (locale->font_fallback_ids[index] == 0U) {
            return false;
        }
        for (uint32_t other = index + 1U; other < locale->font_fallback_count; ++other) {
            if (locale->font_fallback_ids[index] == locale->font_fallback_ids[other]) {
                return false;
            }
        }
    }
    return true;
}

static AfLocaleDefinition *af_localization_find_locale(AfLocalization *localization,
                                                       uint64_t locale_id) {
    if (localization == NULL || localization->locales == NULL || locale_id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < localization->locale_count; ++index) {
        if (localization->locales[index].id == locale_id) {
            return &localization->locales[index];
        }
    }
    return NULL;
}

static const AfLocaleDefinition *
af_localization_find_locale_const(const AfLocalization *localization, uint64_t locale_id) {
    return af_localization_find_locale((AfLocalization *)localization, locale_id);
}

static const AfTranslation *af_localization_find_translation(const AfLocalization *localization,
                                                             uint64_t locale_id, uint64_t key_id) {
    if (localization == NULL || localization->translations == NULL || locale_id == 0U ||
        key_id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < localization->translation_count; ++index) {
        const AfTranslation *translation = &localization->translations[index];
        if (translation->locale_id == locale_id && translation->key_id == key_id) {
            return translation;
        }
    }
    return NULL;
}

static AfResult af_localization_validate_fallbacks(const AfLocalization *localization) {
    for (uint32_t index = 0U; index < localization->locale_count; ++index) {
        uint64_t visited[AF_LOCALIZATION_MAX_LOCALES];
        uint32_t visited_count = 0U;
        uint64_t locale_id = localization->locales[index].id;
        while (locale_id != 0U) {
            const AfLocaleDefinition *locale = NULL;
            if (visited_count >= AF_LOCALIZATION_MAX_FALLBACK_DEPTH) {
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            for (uint32_t visited_index = 0U; visited_index < visited_count; ++visited_index) {
                if (visited[visited_index] == locale_id) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
            }
            visited[visited_count++] = locale_id;
            locale = af_localization_find_locale_const(localization, locale_id);
            if (locale == NULL) {
                return AF_RESULT_NOT_FOUND;
            }
            locale_id = locale->fallback_locale_id;
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_localization_record_missing(AfLocalization *localization, uint64_t key_id) {
    uint64_t count = 0U;
    if (localization == NULL || key_id == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < localization->missing_diagnostic_count; ++index) {
        AfMissingTranslationDiagnostic *diagnostic = &localization->missing_diagnostics[index];
        if (diagnostic->locale_id == localization->active_locale_id &&
            diagnostic->key_id == key_id) {
            if (af_u64_add_overflow(diagnostic->occurrence_count, 1U, &count)) {
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            diagnostic->occurrence_count = count;
            return AF_RESULT_SUCCESS;
        }
    }
    if (localization->missing_diagnostic_count < localization->missing_diagnostic_capacity) {
        localization->missing_diagnostics[localization->missing_diagnostic_count++] =
            (AfMissingTranslationDiagnostic){.locale_id = localization->active_locale_id,
                                             .key_id = key_id,
                                             .occurrence_count = 1U};
        return AF_RESULT_SUCCESS;
    }
    if (af_u64_add_overflow(localization->dropped_missing_diagnostic_count, 1U, &count)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    localization->dropped_missing_diagnostic_count = count;
    return AF_RESULT_SUCCESS;
}

AfResult af_localization_init(AfLocalization *localization, AfLocaleDefinition *locales,
                              uint32_t locale_capacity, AfTranslation *translations,
                              uint32_t translation_capacity,
                              AfMissingTranslationDiagnostic *missing_diagnostics,
                              uint32_t missing_diagnostic_capacity) {
    if (localization == NULL || locale_capacity == 0U ||
        locale_capacity > AF_LOCALIZATION_MAX_LOCALES || locales == NULL ||
        translation_capacity > AF_LOCALIZATION_MAX_TRANSLATIONS ||
        (translation_capacity != 0U && translations == NULL) ||
        missing_diagnostic_capacity > AF_LOCALIZATION_MAX_MISSING_DIAGNOSTICS ||
        (missing_diagnostic_capacity != 0U && missing_diagnostics == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *localization = (AfLocalization){.locales = locales,
                                     .locale_capacity = locale_capacity,
                                     .translations = translations,
                                     .translation_capacity = translation_capacity,
                                     .missing_diagnostics = missing_diagnostics,
                                     .missing_diagnostic_capacity = missing_diagnostic_capacity,
                                     .expansion_percent = 100U};
    return AF_RESULT_SUCCESS;
}

AfResult af_localization_add_locale(AfLocalization *localization,
                                    const AfLocaleDefinition *locale) {
    AfResult result = AF_RESULT_SUCCESS;
    if (localization == NULL || !af_locale_definition_valid(locale) ||
        af_localization_find_locale(localization, locale->id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (localization->locale_count >= localization->locale_capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (uint32_t index = 0U; index < localization->locale_count; ++index) {
        if (strcmp(localization->locales[index].tag, locale->tag) == 0) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    localization->locales[localization->locale_count++] = *locale;
    result = af_localization_validate_fallbacks(localization);
    if (result != AF_RESULT_SUCCESS) {
        --localization->locale_count;
        return result;
    }
    if (localization->default_locale_id == 0U) {
        localization->default_locale_id = locale->id;
        localization->active_locale_id = locale->id;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_localization_set_locale(AfLocalization *localization, uint64_t locale_id) {
    if (localization == NULL || locale_id == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (af_localization_find_locale(localization, locale_id) == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    localization->active_locale_id = locale_id;
    return AF_RESULT_SUCCESS;
}

const AfLocaleDefinition *af_localization_active_locale(const AfLocalization *localization) {
    return localization != NULL
               ? af_localization_find_locale_const(localization, localization->active_locale_id)
               : NULL;
}

AfResult af_localization_set_translation(AfLocalization *localization, uint64_t locale_id,
                                         uint64_t key_id, const char *text) {
    if (localization == NULL || key_id == 0U || text == NULL ||
        af_localization_find_locale(localization, locale_id) == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!af_string_bounded(text, AF_LOCALIZATION_TEXT_CAPACITY)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (uint32_t index = 0U; index < localization->translation_count; ++index) {
        AfTranslation *translation = &localization->translations[index];
        if (translation->locale_id == locale_id && translation->key_id == key_id) {
            return af_copy_bounded(translation->text, sizeof(translation->text), text);
        }
    }
    if (localization->translation_count >= localization->translation_capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    localization->translations[localization->translation_count] =
        (AfTranslation){.locale_id = locale_id, .key_id = key_id};
    if (af_copy_bounded(localization->translations[localization->translation_count].text,
                        AF_LOCALIZATION_TEXT_CAPACITY, text) != AF_RESULT_SUCCESS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    ++localization->translation_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_localization_get_text(AfLocalization *localization, uint64_t key_id,
                                  const char **out_text, bool *out_used_fallback) {
    uint64_t locale_id = 0U;
    uint32_t depth = 0U;
    if (localization == NULL || key_id == 0U || out_text == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_text = NULL;
    if (out_used_fallback != NULL) {
        *out_used_fallback = false;
    }
    locale_id = localization->active_locale_id;
    while (locale_id != 0U && depth++ < AF_LOCALIZATION_MAX_FALLBACK_DEPTH) {
        const AfTranslation *translation =
            af_localization_find_translation(localization, locale_id, key_id);
        const AfLocaleDefinition *locale =
            af_localization_find_locale_const(localization, locale_id);
        if (translation != NULL) {
            *out_text = translation->text;
            if (out_used_fallback != NULL) {
                *out_used_fallback = locale_id != localization->active_locale_id;
            }
            return AF_RESULT_SUCCESS;
        }
        if (locale == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        locale_id = locale->fallback_locale_id;
    }
    (void)af_localization_record_missing(localization, key_id);
    return AF_RESULT_NOT_FOUND;
}

AfResult af_localization_set_expansion(AfLocalization *localization, uint32_t percent) {
    if (localization == NULL || percent < 100U || percent > AF_LOCALIZATION_MAX_EXPANSION_PERCENT) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    localization->expansion_percent = percent;
    return AF_RESULT_SUCCESS;
}

static AfResult af_writer_append(AfLocalizationWriter *writer, const char *text, size_t length) {
    size_t end = 0U;
    if (writer == NULL || text == NULL || writer->capacity == 0U ||
        af_size_add_overflow(writer->length, length, &end) || end >= writer->capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(writer->data + writer->length, text, length);
    writer->length = end;
    writer->data[writer->length] = '\0';
    return AF_RESULT_SUCCESS;
}

static AfResult af_writer_append_string(AfLocalizationWriter *writer, const char *text) {
    return text != NULL ? af_writer_append(writer, text, strlen(text)) : AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_writer_append_char(AfLocalizationWriter *writer, char value) {
    return af_writer_append(writer, &value, 1U);
}

AfResult af_localization_format_number(const AfLocalization *localization, double value,
                                       uint32_t fractional_digits, bool use_grouping, char *output,
                                       size_t output_capacity, size_t *out_length) {
    const AfLocaleDefinition *locale = af_localization_active_locale(localization);
    char ascii[128];
    char localized[AF_LOCALIZATION_FORMAT_CAPACITY];
    AfLocalizationWriter writer = {.data = localized, .capacity = sizeof(localized)};
    const char *decimal = NULL;
    size_t integer_start = 0U;
    size_t integer_length = 0U;
    int written = 0;
    if (locale == NULL || output == NULL || output_capacity == 0U ||
        fractional_digits > AF_LOCALIZATION_MAX_FRACTION_DIGITS || !isfinite(value)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    output[0] = '\0';
    written = snprintf(ascii, sizeof(ascii), "%.*f", (int)fractional_digits, value);
    if (written < 0 || (size_t)written >= sizeof(ascii)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    decimal = strchr(ascii, '.');
    integer_start = ascii[0] == '-' ? 1U : 0U;
    integer_length =
        decimal != NULL ? (size_t)(decimal - ascii) - integer_start : strlen(ascii) - integer_start;
    localized[0] = '\0';
    if (integer_start != 0U && af_writer_append_char(&writer, '-') != AF_RESULT_SUCCESS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (size_t index = 0U; index < integer_length; ++index) {
        const size_t remaining = integer_length - index;
        if (use_grouping && index != 0U && remaining % 3U == 0U &&
            af_writer_append_string(&writer, locale->grouping_separator) != AF_RESULT_SUCCESS) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        if (af_writer_append_char(&writer, ascii[integer_start + index]) != AF_RESULT_SUCCESS) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    }
    if (decimal != NULL &&
        (af_writer_append_string(&writer, locale->decimal_separator) != AF_RESULT_SUCCESS ||
         af_writer_append_string(&writer, decimal + 1U) != AF_RESULT_SUCCESS)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (writer.length >= output_capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(output, localized, writer.length + 1U);
    if (out_length != NULL) {
        *out_length = writer.length;
    }
    return AF_RESULT_SUCCESS;
}

static bool af_local_date_valid(AfLocalDate date) {
    static const uint32_t days[12] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    uint32_t maximum = 0U;
    bool leap = false;
    if (date.year == 0U || date.year > 9999U || date.month == 0U || date.month > 12U ||
        date.day == 0U) {
        return false;
    }
    maximum = days[date.month - 1U];
    leap = date.year % 4U == 0U && (date.year % 100U != 0U || date.year % 400U == 0U);
    if (date.month == 2U && leap) {
        maximum = 29U;
    }
    return date.day <= maximum;
}

AfResult af_localization_format_date(const AfLocalization *localization, AfLocalDate date,
                                     char *output, size_t output_capacity, size_t *out_length) {
    const AfLocaleDefinition *locale = af_localization_active_locale(localization);
    uint32_t first = 0U;
    uint32_t second = 0U;
    uint32_t third = 0U;
    char text[64];
    int written = 0;
    if (locale == NULL || !af_local_date_valid(date) || output == NULL || output_capacity == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (locale->date_order == AF_LOCALE_DATE_YEAR_MONTH_DAY) {
        first = date.year;
        second = date.month;
        third = date.day;
        written = snprintf(text, sizeof(text), "%04u%s%02u%s%02u", first, locale->date_separator,
                           second, locale->date_separator, third);
    } else {
        first = locale->date_order == AF_LOCALE_DATE_DAY_MONTH_YEAR ? date.day : date.month;
        second = locale->date_order == AF_LOCALE_DATE_DAY_MONTH_YEAR ? date.month : date.day;
        third = date.year;
        written = snprintf(text, sizeof(text), "%02u%s%02u%s%04u", first, locale->date_separator,
                           second, locale->date_separator, third);
    }
    if (written < 0 || (size_t)written >= sizeof(text) || (size_t)written >= output_capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(output, text, (size_t)written + 1U);
    if (out_length != NULL) {
        *out_length = (size_t)written;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_localization_format_time(const AfLocalization *localization, AfLocalTime time,
                                     char *output, size_t output_capacity, size_t *out_length) {
    const AfLocaleDefinition *locale = af_localization_active_locale(localization);
    char text[64];
    uint32_t display_hour = time.hour;
    const char *meridiem = "";
    int written = 0;
    if (locale == NULL || time.hour > 23U || time.minute > 59U || time.second > 59U ||
        output == NULL || output_capacity == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!locale->use_24_hour_time) {
        meridiem = time.hour < 12U ? locale->ante_meridiem : locale->post_meridiem;
        display_hour = time.hour % 12U;
        if (display_hour == 0U) {
            display_hour = 12U;
        }
    }
    written = locale->use_24_hour_time ? snprintf(text, sizeof(text), "%02u%s%02u%s%02u",
                                                  display_hour, locale->time_separator, time.minute,
                                                  locale->time_separator, time.second)
                                       : snprintf(text, sizeof(text), "%02u%s%02u%s%02u %s",
                                                  display_hour, locale->time_separator, time.minute,
                                                  locale->time_separator, time.second, meridiem);
    if (written < 0 || (size_t)written >= sizeof(text) || (size_t)written >= output_capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(output, text, (size_t)written + 1U);
    if (out_length != NULL) {
        *out_length = (size_t)written;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_localization_expand_text(const char *text, uint32_t percent, char *output,
                                     size_t output_capacity, size_t *out_length) {
    AfLocalizationWriter writer = {.data = output, .capacity = output_capacity};
    size_t text_length = 0U;
    size_t scaled_length = 0U;
    size_t rounded_length = 0U;
    size_t target_length = 0U;
    if (text == NULL || output == NULL || output_capacity == 0U || percent < 100U ||
        percent > AF_LOCALIZATION_MAX_EXPANSION_PERCENT) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    output[0] = '\0';
    if (!af_string_length_bounded(text, AF_LOCALIZATION_FORMAT_CAPACITY, &text_length)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (af_size_mul_overflow(text_length, percent, &scaled_length) ||
        af_size_add_overflow(scaled_length, 99U, &rounded_length)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    target_length = rounded_length / 100U;
    if (target_length >= output_capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (af_writer_append(&writer, text, text_length) != AF_RESULT_SUCCESS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    while (writer.length < target_length) {
        if (af_writer_append_char(&writer, '~') != AF_RESULT_SUCCESS) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    }
    if (out_length != NULL) {
        *out_length = writer.length;
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_localization_format_placeholder(const AfLocalization *localization,
                                                   const char *placeholder,
                                                   size_t placeholder_length,
                                                   const AfLocalizationFormatArguments *arguments,
                                                   char output[], size_t output_capacity,
                                                   size_t *out_length) {
    if (placeholder_length == 6U && memcmp(placeholder, "number", 6U) == 0) {
        return arguments != NULL && arguments->has_number
                   ? af_localization_format_number(
                         localization, arguments->number, arguments->fractional_digits,
                         arguments->use_grouping, output, output_capacity, out_length)
                   : AF_RESULT_NOT_FOUND;
    }
    if (placeholder_length == 4U && memcmp(placeholder, "date", 4U) == 0) {
        return arguments != NULL && arguments->has_date
                   ? af_localization_format_date(localization, arguments->date, output,
                                                 output_capacity, out_length)
                   : AF_RESULT_NOT_FOUND;
    }
    if (placeholder_length == 4U && memcmp(placeholder, "time", 4U) == 0) {
        return arguments != NULL && arguments->has_time
                   ? af_localization_format_time(localization, arguments->time, output,
                                                 output_capacity, out_length)
                   : AF_RESULT_NOT_FOUND;
    }
    return AF_RESULT_NOT_FOUND;
}

AfResult af_localization_format_message(AfLocalization *localization, uint64_t key_id,
                                        const AfLocalizationFormatArguments *arguments,
                                        char *output, size_t output_capacity, size_t *out_length) {
    const char *text = NULL;
    char formatted[AF_LOCALIZATION_FORMAT_CAPACITY];
    char expanded[AF_LOCALIZATION_FORMAT_CAPACITY];
    AfLocalizationWriter writer = {.data = formatted, .capacity = sizeof(formatted)};
    AfResult result = AF_RESULT_SUCCESS;
    if (localization == NULL || output == NULL || output_capacity == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    output[0] = '\0';
    result = af_localization_get_text(localization, key_id, &text, NULL);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    formatted[0] = '\0';
    for (size_t index = 0U; text[index] != '\0';) {
        if (text[index] != '{') {
            result = af_writer_append_char(&writer, text[index++]);
        } else if (text[index + 1U] == '{') {
            result = af_writer_append_char(&writer, '{');
            index += 2U;
        } else {
            const char *close = strchr(text + index + 1U, '}');
            const char *placeholder = text + index + 1U;
            char replacement[128];
            size_t replacement_length = 0U;
            if (close == NULL) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            result = af_localization_format_placeholder(
                localization, placeholder, (size_t)(close - placeholder), arguments, replacement,
                sizeof(replacement), &replacement_length);
            if (result == AF_RESULT_SUCCESS) {
                result = af_writer_append(&writer, replacement, replacement_length);
            }
            index = (size_t)(close - text) + 1U;
        }
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    text = formatted;
    if (localization->expansion_percent != 100U) {
        result = af_localization_expand_text(formatted, localization->expansion_percent, expanded,
                                             sizeof(expanded), NULL);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        text = expanded;
    }
    if (strlen(text) >= output_capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(output, text, strlen(text) + 1U);
    if (out_length != NULL) {
        *out_length = strlen(text);
    }
    return AF_RESULT_SUCCESS;
}

bool af_localization_is_right_to_left(const AfLocalization *localization) {
    const AfLocaleDefinition *locale = af_localization_active_locale(localization);
    return locale != NULL && locale->right_to_left;
}

bool af_localization_mirrors_layout(const AfLocalization *localization) {
    const AfLocaleDefinition *locale = af_localization_active_locale(localization);
    return locale != NULL && locale->mirror_layout;
}

AfResult af_localization_font_fallbacks(const AfLocalization *localization,
                                        const uint64_t **out_font_ids, uint32_t *out_count) {
    const AfLocaleDefinition *locale = af_localization_active_locale(localization);
    if (locale == NULL || out_font_ids == NULL || out_count == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_font_ids = locale->font_fallback_ids;
    *out_count = locale->font_fallback_count;
    return AF_RESULT_SUCCESS;
}

const AfMissingTranslationDiagnostic *
af_localization_missing_diagnostics(const AfLocalization *localization, uint32_t *out_count,
                                    uint64_t *out_dropped_diagnostic_count) {
    if (out_count != NULL) {
        *out_count = localization != NULL ? localization->missing_diagnostic_count : 0U;
    }
    if (out_dropped_diagnostic_count != NULL) {
        *out_dropped_diagnostic_count =
            localization != NULL ? localization->dropped_missing_diagnostic_count : 0U;
    }
    return localization != NULL ? localization->missing_diagnostics : NULL;
}

void af_localization_clear_missing_diagnostics(AfLocalization *localization) {
    if (localization != NULL) {
        localization->missing_diagnostic_count = 0U;
        localization->dropped_missing_diagnostic_count = 0U;
    }
}

static bool af_accessibility_node_valid(const AfAccessibilityNode *node) {
    return node != NULL && node->node_id != 0U && node->role < AF_ACCESSIBILITY_ROLE_COUNT &&
           (node->states & ~AF_ACCESSIBILITY_STATE_MASK) == 0U &&
           af_string_bounded(node->label, sizeof(node->label)) &&
           af_string_bounded(node->description, sizeof(node->description));
}

AfResult af_accessibility_tree_init(AfAccessibilityTree *tree, AfAccessibilityNode *nodes,
                                    uint32_t capacity) {
    if (tree == NULL || capacity > AF_ACCESSIBILITY_MAX_NODES ||
        (capacity != 0U && nodes == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *tree = (AfAccessibilityTree){.nodes = nodes, .capacity = capacity};
    return AF_RESULT_SUCCESS;
}

AfResult af_accessibility_set_node(AfAccessibilityTree *tree, const AfAccessibilityNode *node) {
    if (tree == NULL || !af_accessibility_node_valid(node)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < tree->count; ++index) {
        if (tree->nodes[index].node_id == node->node_id) {
            tree->nodes[index] = *node;
            return AF_RESULT_SUCCESS;
        }
    }
    if (tree->count >= tree->capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    tree->nodes[tree->count++] = *node;
    return AF_RESULT_SUCCESS;
}

AfResult af_accessibility_remove_node(AfAccessibilityTree *tree, uint64_t node_id) {
    if (tree == NULL || node_id == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < tree->count; ++index) {
        if (tree->nodes[index].node_id == node_id) {
            if (index + 1U < tree->count) {
                memmove(&tree->nodes[index], &tree->nodes[index + 1U],
                        (size_t)(tree->count - index - 1U) * sizeof(tree->nodes[0]));
            }
            --tree->count;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

const AfAccessibilityNode *af_accessibility_find_node(const AfAccessibilityTree *tree,
                                                      uint64_t node_id) {
    if (tree == NULL || tree->nodes == NULL || node_id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < tree->count; ++index) {
        if (tree->nodes[index].node_id == node_id) {
            return &tree->nodes[index];
        }
    }
    return NULL;
}

const AfAccessibilityNode *af_accessibility_nodes(const AfAccessibilityTree *tree,
                                                  uint32_t *out_count) {
    if (out_count != NULL) {
        *out_count = tree != NULL ? tree->count : 0U;
    }
    return tree != NULL ? tree->nodes : NULL;
}

static bool af_accessibility_focusable(const AfAccessibilityNode *node) {
    return node->focusable &&
           (node->states & (AF_ACCESSIBILITY_STATE_DISABLED | AF_ACCESSIBILITY_STATE_HIDDEN)) == 0U;
}

static int af_accessibility_focus_compare(const AfAccessibilityNode *left,
                                          const AfAccessibilityNode *right) {
    if (left->focus_order != right->focus_order) {
        return left->focus_order < right->focus_order ? -1 : 1;
    }
    if (left->node_id != right->node_id) {
        return left->node_id < right->node_id ? -1 : 1;
    }
    return 0;
}

static AfResult af_accessibility_focus_query(const AfAccessibilityTree *tree,
                                             uint64_t current_node_id, bool forward, bool wrap,
                                             uint64_t *out_node_id) {
    const AfAccessibilityNode *current = af_accessibility_find_node(tree, current_node_id);
    const AfAccessibilityNode *candidate = NULL;
    const AfAccessibilityNode *boundary = NULL;
    if (tree == NULL || out_node_id == NULL || (current_node_id != 0U && current == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_node_id = 0U;
    for (uint32_t index = 0U; index < tree->count; ++index) {
        const AfAccessibilityNode *node = &tree->nodes[index];
        int comparison = 0;
        if (!af_accessibility_focusable(node)) {
            continue;
        }
        if (boundary == NULL || (forward ? af_accessibility_focus_compare(node, boundary) < 0
                                         : af_accessibility_focus_compare(node, boundary) > 0)) {
            boundary = node;
        }
        if (current == NULL) {
            continue;
        }
        comparison = af_accessibility_focus_compare(node, current);
        if ((forward && comparison > 0) || (!forward && comparison < 0)) {
            if (candidate == NULL ||
                (forward ? af_accessibility_focus_compare(node, candidate) < 0
                         : af_accessibility_focus_compare(node, candidate) > 0)) {
                candidate = node;
            }
        }
    }
    if (current == NULL) {
        candidate = boundary;
    } else if (candidate == NULL && wrap) {
        candidate = boundary;
    }
    if (candidate == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_node_id = candidate->node_id;
    return AF_RESULT_SUCCESS;
}

AfResult af_accessibility_focus_next(const AfAccessibilityTree *tree, uint64_t current_node_id,
                                     bool wrap, uint64_t *out_node_id) {
    return af_accessibility_focus_query(tree, current_node_id, true, wrap, out_node_id);
}

AfResult af_accessibility_focus_previous(const AfAccessibilityTree *tree, uint64_t current_node_id,
                                         bool wrap, uint64_t *out_node_id) {
    return af_accessibility_focus_query(tree, current_node_id, false, wrap, out_node_id);
}

void af_accessibility_settings_default(AfAccessibilitySettings *settings) {
    if (settings != NULL) {
        *settings = (AfAccessibilitySettings){.text_scale = 1.0F};
    }
}

AfResult af_accessibility_settings_validate(const AfAccessibilitySettings *settings) {
    if (settings == NULL || !isfinite(settings->text_scale) ||
        settings->text_scale < AF_ACCESSIBILITY_MIN_TEXT_SCALE ||
        settings->text_scale > AF_ACCESSIBILITY_MAX_TEXT_SCALE ||
        settings->reduced_motion >= AF_REDUCED_MOTION_MODE_COUNT ||
        settings->color_blind_preview >= AF_COLOR_BLIND_PREVIEW_COUNT) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_accessibility_scaled_text_size(const AfAccessibilitySettings *settings, float base_size,
                                           float *out_size) {
    float size = 0.0F;
    if (af_accessibility_settings_validate(settings) != AF_RESULT_SUCCESS || out_size == NULL ||
        !isfinite(base_size) || base_size <= 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    size = base_size * settings->text_scale;
    if (!isfinite(size) || size > AF_ACCESSIBILITY_MAX_TEXT_SIZE) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    *out_size = size;
    return AF_RESULT_SUCCESS;
}

AfResult af_accessibility_adjust_motion_duration(const AfAccessibilitySettings *settings,
                                                 float duration, float *out_duration) {
    if (af_accessibility_settings_validate(settings) != AF_RESULT_SUCCESS || out_duration == NULL ||
        !isfinite(duration) || duration < 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (settings->reduced_motion == AF_REDUCED_MOTION_DISABLE) {
        *out_duration = 0.0F;
    } else if (settings->reduced_motion == AF_REDUCED_MOTION_SIMPLIFY) {
        *out_duration = duration > 0.1F ? 0.1F : duration;
    } else {
        *out_duration = duration;
    }
    return AF_RESULT_SUCCESS;
}
