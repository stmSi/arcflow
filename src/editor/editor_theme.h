#ifndef AF_EDITOR_THEME_H
#define AF_EDITOR_THEME_H

#include "af/core/math.h"
#include "editor/editor_preferences.h"
#include <math.h>

typedef struct AfEditorPalette {
    AfColor canvas, panel, surface, field, border, text, muted;
    AfColor accent, primary, selection, success, warning;
    bool light;
} AfEditorPalette;

static inline AfColor af_editor_theme_color(uint32_t rgb) {
    return (AfColor){(float)((rgb >> 16) & 255U) / 255, (float)((rgb >> 8) & 255U) / 255,
                     (float)(rgb & 255U) / 255, 1};
}
static inline uint32_t af_editor_theme_rgb(AfColor color) {
    return ((uint32_t)roundf(color.r * 255) << 16) | ((uint32_t)roundf(color.g * 255) << 8) |
           (uint32_t)roundf(color.b * 255);
}
static inline AfColor af_editor_theme_mix(AfColor a, AfColor b, float t) {
    return (AfColor){a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                     a.a + (b.a - a.a) * t};
}
static inline float af_editor_theme_brightness(AfColor color) {
    return color.r * .2126F + color.g * .7152F + color.b * .0722F;
}
static inline AfEditorPalette af_editor_theme_palette(const AfEditorPreferences *preferences) {
    uint32_t accent = 0xA38DFFU, panel = 0x1D2430U, canvas = 0x111720U;
    if (preferences->theme == AF_EDITOR_THEME_MIDNIGHT) {
        accent = 0x5ADBC6U;
        panel = 0x14232CU;
        canvas = 0x0C171EU;
    } else if (preferences->theme == AF_EDITOR_THEME_GRAPHITE) {
        accent = 0xEAB578U;
        panel = 0x272625U;
        canvas = 0x191918U;
    } else if (preferences->theme == AF_EDITOR_THEME_LIGHT) {
        accent = 0x6252C8U;
        panel = 0xE3E6EDU;
        canvas = 0xC7CCD6U;
    } else if (preferences->theme == AF_EDITOR_THEME_FOREST) {
        accent = 0x9FD58CU;
        panel = 0x1C2B25U;
        canvas = 0x101C17U;
    } else if (preferences->theme == AF_EDITOR_THEME_EMBER) {
        accent = 0xFFAA70U;
        panel = 0x30231FU;
        canvas = 0x201612U;
    } else if (preferences->theme == AF_EDITOR_THEME_ROSE) {
        accent = 0xF4A6CDU;
        panel = 0x2E2233U;
        canvas = 0x1C1522U;
    } else if (preferences->theme == AF_EDITOR_THEME_PAPER) {
        accent = 0x98663CU;
        panel = 0xEEE7DBU;
        canvas = 0xD6CBB8U;
    } else if (preferences->theme == AF_EDITOR_THEME_CUSTOM) {
        accent = preferences->theme_accent_rgb;
        panel = preferences->theme_panel_rgb;
        canvas = preferences->theme_canvas_rgb;
    }
    AfEditorPalette palette = {.canvas = af_editor_theme_color(canvas),
                               .panel = af_editor_theme_color(panel),
                               .accent = af_editor_theme_color(accent)};
    palette.light = af_editor_theme_brightness(palette.panel) > .55F;
    const AfColor white = {1, 1, 1, 1}, black = {.025F, .032F, .048F, 1};
    palette.surface = af_editor_theme_mix(palette.panel, palette.light ? white : palette.accent,
                                          palette.light ? .48F : .055F);
    palette.field = af_editor_theme_mix(palette.panel, palette.light ? white : black,
                                        palette.light ? .74F : .30F);
    palette.border = af_editor_theme_mix(palette.panel, palette.light ? black : white,
                                         palette.light ? .18F : .15F);
    palette.text = palette.light ? black : (AfColor){.93F, .945F, .97F, 1};
    palette.muted = af_editor_theme_mix(palette.panel, palette.text, palette.light ? .64F : .63F);
    palette.primary =
        af_editor_theme_mix(palette.panel, palette.accent, palette.light ? .22F : .43F);
    palette.selection = af_editor_theme_mix(palette.panel, palette.accent, .18F);
    palette.success = af_editor_theme_color(0x72DDB7U);
    palette.warning = af_editor_theme_color(0xF2C07CU);
    return palette;
}

/* Studio chrome uses live preferences; artwork and property swatches keep their colors. */
#define AF_UI_CANVAS (editor->palette.canvas)
#define AF_UI_PANEL (editor->palette.panel)
#define AF_UI_SURFACE (editor->palette.surface)
#define AF_UI_FIELD (editor->palette.field)
#define AF_UI_BORDER (editor->palette.border)
#define AF_UI_TEXT (editor->palette.text)
#define AF_UI_MUTED (editor->palette.muted)
#define AF_UI_ACCENT (editor->palette.accent)
#define AF_UI_PRIMARY (editor->palette.primary)
#define AF_UI_SELECTION (editor->palette.selection)
#define AF_UI_SUCCESS (editor->palette.success)
#define AF_UI_WARNING (editor->palette.warning)

#endif
