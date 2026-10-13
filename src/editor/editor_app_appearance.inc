/* Presets share geometry between rendering and pointer handling. */
static const struct {
    AfEditorTheme theme;
    const char *name;
} af_editor_appearance_presets[] = {
    {AF_EDITOR_THEME_DARK, "Studio"},       {AF_EDITOR_THEME_MIDNIGHT, "Midnight"},
    {AF_EDITOR_THEME_GRAPHITE, "Graphite"}, {AF_EDITOR_THEME_LIGHT, "Daylight"},
    {AF_EDITOR_THEME_FOREST, "Forest"},     {AF_EDITOR_THEME_EMBER, "Ember"},
    {AF_EDITOR_THEME_ROSE, "Rose"},         {AF_EDITOR_THEME_PAPER, "Paper"}};
#define AF_EDITOR_APPEARANCE_WIDTH 680.0F
#define AF_EDITOR_APPEARANCE_HEIGHT 616.0F
#define AF_EDITOR_APPEARANCE_PRESETS                                                               \
    (sizeof(af_editor_appearance_presets) / sizeof(af_editor_appearance_presets[0]))

static uint32_t *af_editor_appearance_rgb(AfEditorState *editor, uint32_t index) {
    return index == 0   ? &editor->preferences.theme_accent_rgb
           : index == 1 ? &editor->preferences.theme_panel_rgb
                        : &editor->preferences.theme_canvas_rgb;
}
static void af_editor_appearance_save(AfEditorState *editor) {
    editor->palette = af_editor_theme_palette(&editor->preferences);
#ifdef __EMSCRIPTEN__
    af_web_set_ui_colors(af_editor_theme_rgb(editor->palette.panel),
                         af_editor_theme_rgb(editor->palette.accent),
                         af_editor_theme_rgb(editor->palette.text));
#endif
    af_editor_save_preferences(editor);
#ifdef __EMSCRIPTEN__
    af_web_sync_files();
#endif
}
static void af_editor_appearance_custom(AfEditorState *editor) {
    if (editor->preferences.theme == AF_EDITOR_THEME_CUSTOM)
        return;
    const AfEditorPalette palette = af_editor_theme_palette(&editor->preferences);
    editor->preferences.theme_accent_rgb = af_editor_theme_rgb(palette.accent);
    editor->preferences.theme_panel_rgb = af_editor_theme_rgb(palette.panel);
    editor->preferences.theme_canvas_rgb = af_editor_theme_rgb(palette.canvas);
    editor->preferences.theme = AF_EDITOR_THEME_CUSTOM;
}
static bool af_editor_appearance_finish_hex(AfEditorState *editor, bool apply) {
    if (!editor->appearance_hex_active)
        return true;
    if (apply) {
        AfColor color;
        if (strlen(editor->appearance_hex) != 6 ||
            !af_editor_inspector_parse_hex_color(editor->appearance_hex, &color)) {
            af_editor_status(editor, "Enter six hex digits, for example A38DFF");
            return false;
        }
        af_editor_appearance_custom(editor);
        *af_editor_appearance_rgb(editor, editor->appearance_color) = af_editor_theme_rgb(color);
        af_editor_appearance_save(editor);
    }
    editor->appearance_hex_active = false;
    (void)af_platform_text_input_set(&editor->window, false);
    return true;
}
static void af_editor_appearance_begin_hex(AfEditorState *editor) {
    const AfEditorPalette palette = af_editor_theme_palette(&editor->preferences);
    const AfColor color = editor->appearance_color == 0   ? palette.accent
                          : editor->appearance_color == 1 ? palette.panel
                                                          : palette.canvas;
    (void)snprintf(editor->appearance_hex, sizeof(editor->appearance_hex), "%06X",
                   af_editor_theme_rgb(color));
    editor->appearance_hex_active = true;
    editor->appearance_hex_replace = true;
    (void)af_platform_text_input_set(&editor->window, true);
}
static void af_editor_appearance_append(AfEditorState *editor, const char *text) {
    if (!editor->appearance_hex_active)
        return;
    if (editor->appearance_hex_replace) {
        editor->appearance_hex[0] = '\0';
        editor->appearance_hex_replace = false;
    }
    size_t length = strlen(editor->appearance_hex);
    while (*text && length < 6) {
        const char c = *text++;
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            editor->appearance_hex[length++] = c;
    }
    editor->appearance_hex[length] = '\0';
}
static void af_editor_appearance_step(AfEditorState *editor, int amount) {
    af_editor_appearance_custom(editor);
    uint32_t *rgb = af_editor_appearance_rgb(editor, editor->appearance_color);
    *rgb = (uint32_t)fmin(fmax((double)*rgb + amount, 0), 16777215);
    af_editor_appearance_save(editor);
}
static void af_editor_appearance_motion(AfEditorState *editor, float x) {
    if (!editor->appearance_drag ||
        (!editor->appearance_drag_changed && fabsf(x - editor->appearance_drag_x) < 3))
        return;
    af_editor_appearance_custom(editor);
    *af_editor_appearance_rgb(editor, editor->appearance_color) = (uint32_t)fmin(
        fmax((double)editor->appearance_drag_rgb + roundf(x - editor->appearance_drag_x), 0),
        16777215);
    editor->appearance_drag_changed = true;
    editor->palette = af_editor_theme_palette(&editor->preferences);
}
static void af_editor_appearance_up(AfEditorState *editor) {
    if (!editor->appearance_drag)
        return;
    editor->appearance_drag = false;
    if (editor->appearance_drag_changed)
        af_editor_appearance_save(editor);
    else
        af_editor_appearance_begin_hex(editor);
}
static bool af_editor_appearance_key(AfEditorState *editor, const SDL_KeyboardEvent *event) {
    if (!editor->appearance_visible)
        return false;
    if (event->key == SDLK_ESCAPE) {
        if (editor->appearance_drag) {
            editor->preferences.theme = editor->appearance_drag_theme;
            for (uint32_t i = 0; i < 3; ++i)
                *af_editor_appearance_rgb(editor, i) = editor->appearance_drag_colors[i];
            editor->appearance_drag = false;
            af_editor_appearance_save(editor);
        } else if (editor->appearance_hex_active)
            (void)af_editor_appearance_finish_hex(editor, false);
        else {
            af_editor_appearance_up(editor);
            editor->appearance_visible = false;
        }
    } else if (editor->appearance_hex_active) {
        const bool control = (event->mod & SDL_KMOD_CTRL) != 0;
        if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER)
            (void)af_editor_appearance_finish_hex(editor, true);
        else if (control && event->key == SDLK_A)
            editor->appearance_hex_replace = true;
        else if (control && event->key == SDLK_V) {
            char *text = SDL_GetClipboardText();
            if (text) {
                af_editor_appearance_append(editor, text);
                SDL_free(text);
            }
        } else if (control && event->key == SDLK_C)
            (void)SDL_SetClipboardText(editor->appearance_hex);
        else if (event->key == SDLK_BACKSPACE || event->key == SDLK_DELETE) {
            const size_t length = strlen(editor->appearance_hex);
            if (editor->appearance_hex_replace)
                editor->appearance_hex[0] = '\0';
            else if (length)
                editor->appearance_hex[length - 1] = '\0';
            editor->appearance_hex_replace = false;
        }
    }
    return true;
}
static bool af_editor_appearance_mouse(AfEditorState *editor, int width, int height, float mx,
                                       float my) {
    if (!editor->appearance_visible)
        return false;
    const float x = ((float)width - AF_EDITOR_APPEARANCE_WIDTH) * .5F,
                y = ((float)height - AF_EDITOR_APPEARANCE_HEIGHT) * .5F;
    if (!af_editor_appearance_finish_hex(editor, true))
        return true;
    if (af_editor_hovered_rect(editor, x + 622, y + 18, 36, 30)) {
        editor->appearance_visible = false;
        return true;
    }
    for (uint32_t i = 0; i < AF_EDITOR_APPEARANCE_PRESETS; ++i) {
        const float left = x + 24 + (float)(i % 4) * 160;
        const float top = y + 98 + (float)(i / 4) * 84;
        if (mx >= left && mx < left + 152 && my >= top && my < top + 76) {
            editor->preferences.theme = af_editor_appearance_presets[i].theme;
            af_editor_appearance_save(editor);
            return true;
        }
    }
    if (mx >= x + 544 && mx < x + 654 && my >= y + 274 && my < y + 298) {
        editor->preferences.theme = AF_EDITOR_THEME_CUSTOM;
        af_editor_appearance_save(editor);
        return true;
    }
    for (uint32_t i = 0; i < 3; ++i) {
        const float top = y + 302 + (float)i * 48;
        if (my >= top && my < top + 36 && mx >= x + 216 && mx < x + 654) {
            editor->appearance_color = i;
            if (mx >= x + 608)
                af_editor_appearance_step(editor, 1);
            else if (mx >= x + 566)
                af_editor_appearance_step(editor, -1);
            else {
                const AfEditorPalette palette = af_editor_theme_palette(&editor->preferences);
                editor->appearance_drag_rgb = af_editor_theme_rgb(i == 0   ? palette.accent
                                                                  : i == 1 ? palette.panel
                                                                           : palette.canvas);
                editor->appearance_drag_theme = editor->preferences.theme;
                for (uint32_t color = 0; color < 3; ++color)
                    editor->appearance_drag_colors[color] =
                        *af_editor_appearance_rgb(editor, color);
                editor->appearance_drag_x = mx;
                editor->appearance_drag = true;
                editor->appearance_drag_changed = false;
            }
            return true;
        }
    }
    if (mx >= x + 216 && mx < x + 652 && my >= y + 470 && my < y + 504) {
        const uint32_t col = (uint32_t)((mx - x - 216) / 109);
        const uint32_t levels[] = {0, 40, 80, 100};
        editor->preferences.theme_depth_percent = levels[col];
        af_editor_appearance_save(editor);
    } else if (mx >= x + 24 && mx < x + 180 && my >= y + 554 && my < y + 588) {
        AfEditorPreferences defaults;
        af_editor_preferences_default(&defaults);
        editor->preferences.theme = defaults.theme;
        editor->preferences.theme_depth_percent = defaults.theme_depth_percent;
        editor->preferences.theme_accent_rgb = defaults.theme_accent_rgb;
        editor->preferences.theme_panel_rgb = defaults.theme_panel_rgb;
        editor->preferences.theme_canvas_rgb = defaults.theme_canvas_rgb;
        af_editor_appearance_save(editor);
    }
    return true;
}
static void af_editor_render_appearance(AfEditorState *editor, int width, int height) {
    if (!editor->appearance_visible)
        return;
    const float x = ((float)width - AF_EDITOR_APPEARANCE_WIDTH) * .5F,
                y = ((float)height - AF_EDITOR_APPEARANCE_HEIGHT) * .5F;
    const int32_t layer = 270010;
    af_editor_add_rect(editor, 0, 0, (float)width, (float)height, (AfColor){0, 0, 0, .65F},
                       layer - 10);
    af_editor_add_button_rounded(editor, x, y, AF_EDITOR_APPEARANCE_WIDTH,
                                 AF_EDITOR_APPEARANCE_HEIGHT, AF_UI_PANEL, 16, layer);
    af_editor_add_text(editor, x + 24, y + 24, "Appearance", 2, 550, AF_UI_TEXT, layer + 10);
    af_editor_add_text(editor, x + 24, y + 59, "Make your studio feel like yours.", 1, 570,
                       AF_UI_MUTED, layer + 10);
    af_editor_add_button_rect(editor, x + 622, y + 18, 36, 30, AF_UI_SURFACE, layer + 12);
    af_editor_add_text(editor, x + 635, y + 27, "X", .8F, 18, AF_UI_TEXT, layer + 16);
    for (uint32_t i = 0; i < AF_EDITOR_APPEARANCE_PRESETS; ++i) {
        AfEditorPreferences preference = editor->preferences;
        preference.theme = af_editor_appearance_presets[i].theme;
        const AfEditorPalette palette = af_editor_theme_palette(&preference);
        const float left = x + 24 + (float)(i % 4) * 160;
        const float top = y + 98 + (float)(i / 4) * 84;
        af_editor_add_button_rounded(editor, left, top, 152, 76,
                                     editor->preferences.theme == preference.theme ? AF_UI_ACCENT
                                                                                   : AF_UI_BORDER,
                                     8, layer + 12);
        af_editor_add_surface(editor, left + 3, top + 3, 146, 70, palette.panel, 6, layer + 15);
        af_editor_add_surface(editor, left + 12, top + 14, 128, 32, palette.canvas, 4, layer + 16);
        af_editor_add_surface(editor, left + 18, top + 20, 22, 20, palette.accent, 3, layer + 17);
        af_editor_add_rect(editor, left + 47, top + 22, 82, 2, palette.muted, layer + 17);
        af_editor_add_rect(editor, left + 47, top + 31, 58, 2, palette.border, layer + 17);
        af_editor_add_text(editor, left + 12, top + 56, af_editor_appearance_presets[i].name, 1,
                           128, palette.text, layer + 20);
    }
    af_editor_add_text(editor, x + 24, y + 282, "CUSTOM COLORS", .75F, 300, AF_UI_MUTED,
                       layer + 20);
    af_editor_add_button_rect(editor, x + 544, y + 274, 110, 24,
                              editor->preferences.theme == AF_EDITOR_THEME_CUSTOM ? AF_UI_PRIMARY
                                                                                  : AF_UI_SURFACE,
                              layer + 12);
    af_editor_add_text(editor, x + 556, y + 282, "Use custom", .8F, 91, AF_UI_TEXT, layer + 20);
    const char *labels[] = {"Accent", "Panels", "Workspace"};
    const AfColor colors[] = {AF_UI_ACCENT, AF_UI_PANEL, AF_UI_CANVAS};
    for (uint32_t i = 0; i < 3; ++i) {
        const float top = y + 302 + (float)i * 48;
        af_editor_add_text(editor, x + 24, top + 11, labels[i], 1, 175, AF_UI_TEXT, layer + 20);
        af_editor_add_inset_field(editor, x + 216, top, 334, 36, AF_UI_FIELD, layer + 12);
        if (editor->appearance_hex_active && editor->appearance_color == i)
            af_editor_add_rect(editor, x + 268, top + 27, 69, 1, AF_UI_ACCENT, layer + 18);
        af_editor_add_surface(editor, x + 226, top + 7, 26, 22, colors[i], 4, layer + 16);
        char hex[16];
        (void)snprintf(hex, sizeof(hex), "#%06X", af_editor_theme_rgb(colors[i]));
        af_editor_add_text(editor, x + 268, top + 12,
                           editor->appearance_hex_active && editor->appearance_color == i
                               ? editor->appearance_hex
                               : hex,
                           1, 265, AF_UI_TEXT, layer + 20);
        af_editor_add_button_rect(editor, x + 566, top, 36, 36, AF_UI_SURFACE, layer + 12);
        af_editor_add_button_rect(editor, x + 608, top, 36, 36, AF_UI_SURFACE, layer + 12);
        af_editor_add_text(editor, x + 579, top + 12, "-", 1, 16, AF_UI_TEXT, layer + 20);
        af_editor_add_text(editor, x + 620, top + 12, "+", 1, 16, AF_UI_TEXT, layer + 20);
    }
    af_editor_add_text(editor, x + 24, y + 479, "Surface depth", 1, 185, AF_UI_TEXT, layer + 20);
    const char *depths[] = {"Flat", "Subtle", "Studio", "Deep"};
    const uint32_t levels[] = {0, 40, 80, 100};
    for (uint32_t i = 0; i < 4; ++i) {
        const float left = x + 216 + (float)i * 109;
        af_editor_add_button_rect(
            editor, left, y + 470, 101, 34,
            editor->preferences.theme_depth_percent == levels[i] ? AF_UI_PRIMARY : AF_UI_SURFACE,
            layer + 12);
        af_editor_add_text(editor, left + 12, y + 481, depths[i], .9F, 80, AF_UI_TEXT, layer + 20);
    }
    af_editor_add_text(editor, x + 216, y + 521,
                       "Click a hex value to type; drag or use - / + to step.", .75F, 435,
                       AF_UI_MUTED, layer + 20);
    af_editor_add_button_rect(editor, x + 24, y + 554, 156, 34, AF_UI_SURFACE, layer + 12);
    af_editor_add_text(editor, x + 37, y + 565, "Reset appearance", .85F, 136, AF_UI_TEXT,
                       layer + 20);
    af_editor_add_text(editor, x + 216, y + 565, "Saved automatically / Esc closes", .8F, 435,
                       AF_UI_MUTED, layer + 20);
}
