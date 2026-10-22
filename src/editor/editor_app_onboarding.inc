#define AF_EDITOR_WELCOME_WIDTH 760.0F
#define AF_EDITOR_WELCOME_HEIGHT 500.0F
#define AF_EDITOR_EXAMPLE_WIDTH 820.0F
#define AF_EDITOR_EXAMPLE_HEIGHT 520.0F
#define AF_EDITOR_GUIDE_WIDTH 920.0F
#define AF_EDITOR_GUIDE_HEIGHT 590.0F

/* Shape paragraphs with the same font metrics as the UI, then draw each line.
 * Copy byte boundaries before drawing: add_text reuses the editor's shaping buffers. */
static void af_editor_onboarding_paragraph(AfEditorState *editor, float x, float y,
                                           const char *text, float scale, float width,
                                           AfColor color, int32_t layer) {
    const size_t length = text == NULL ? 0U : strlen(text);
    if (length == 0U || length >= 1024U) {
        return;
    }
    AfTextLayout layout = {.glyphs = editor->text_glyphs,
                           .glyph_capacity = AF_EDITOR_TEXT_GLYPH_CAPACITY,
                           .lines = editor->text_lines,
                           .line_capacity = AF_EDITOR_TEXT_LINE_CAPACITY};
    const AfTextRequest request = {.utf8 = text,
                                   .byte_count = (uint32_t)length,
                                   .font_ids = {AF_EDITOR_TEXT_FONT_ID},
                                   .font_count = 1U,
                                   .font_size = ceilf(9.0F + scale * 4.0F),
                                   .maximum_width = width,
                                   .maximum_lines = 16U,
                                   .line_spacing = 1.0F,
                                   .direction = AF_TEXT_DIRECTION_LTR,
                                   .wrap = AF_TEXT_WRAP_WORD,
                                   .kerning = true,
                                   .ligatures = true};
    if (editor->text_engine == NULL ||
        af_text_shape(editor->text_engine, &request, &layout) != AF_RESULT_SUCCESS) {
        af_editor_add_text(editor, x, y, text, scale, width, color, layer);
        return;
    }
    const uint32_t count = layout.line_count < 16U ? layout.line_count : 16U;
    uint32_t boundaries[17] = {0};
    for (uint32_t line = 0U; line < count; ++line) {
        boundaries[line] = layout.glyphs[layout.lines[line].glyph_start].cluster;
    }
    boundaries[count] = (uint32_t)length;
    for (uint32_t line = 0U; line < count; ++line) {
        char buffer[1024];
        const uint32_t size = boundaries[line + 1U] - boundaries[line];
        memcpy(buffer, text + boundaries[line], size);
        buffer[size] = '\0';
        af_editor_add_text(editor, x, y + (float)line * 18.0F, buffer, scale, width, color, layer);
    }
}

static void af_editor_onboarding_finish_welcome(AfEditorState *editor) {
    editor->welcome_visible = false;
    editor->preferences.onboarding_complete = true;
}

static void af_editor_onboarding_show_welcome(AfEditorState *editor) {
    editor->welcome_visible = true;
    editor->example_browser_visible = false;
    editor->guide_browser_visible = false;
    editor->help_overlay = false;
}

static void af_editor_onboarding_show_examples(AfEditorState *editor) {
    editor->welcome_visible = false;
    editor->preferences.onboarding_complete = true;
    editor->example_browser_visible = true;
    editor->guide_browser_visible = false;
    editor->example_selected_index = AF_PROJECT_TEMPLATE_GUIDED_BASICS;
}

static void af_editor_onboarding_show_guides(AfEditorState *editor, AfOnboardingTopic topic) {
    editor->welcome_visible = false;
    editor->preferences.onboarding_complete = true;
    editor->example_browser_visible = false;
    editor->guide_browser_visible = true;
    editor->guide_topic =
        (uint32_t)topic < af_onboarding_guide_count() ? topic : AF_ONBOARDING_QUICK_START;
    editor->guide_step = 0U;
}

static void af_editor_onboarding_start_guided(AfEditorState *editor) {
    if (af_editor_create_template(editor, AF_PROJECT_TEMPLATE_GUIDED_BASICS)) {
        af_editor_onboarding_finish_welcome(editor);
        editor->example_browser_visible = false;
        editor->guided_topic = AF_ONBOARDING_QUICK_START;
        editor->guided_tour_visible = true;
        editor->guided_step = 0U;
        af_editor_status(editor, "Guided project ready; follow the checklist");
    }
}

static void af_editor_render_welcome(AfEditorState *editor, int width, int height) {
    static const char *const labels[] = {"Create your first animation", "Start with a template",
                                         "Explore example projects", "Find a little guidance"};
    static const char *const details[] = {
        "A ready-to-animate scene. Six simple steps. Your first moment of motion.",
        "From a blank canvas to buttons, vectors and particles.",
        "Open a working scene and discover how it all comes together.",
        "Practical guides for creating, animating and sharing your work."};
    char version[160];
    const float x = ((float)width - AF_EDITOR_WELCOME_WIDTH) * 0.5F;
    const float y = ((float)height - AF_EDITOR_WELCOME_HEIGHT) * 0.5F;
    if (!editor->welcome_visible) {
        return;
    }
    (void)snprintf(version, sizeof(version), "ARCFLOW   /   %s", AF_VERSION_STRING);
    af_editor_add_rect(editor, 0, 0, (float)width, (float)height,
                       (AfColor){.025F, .028F, .04F, .92F}, 249000);
    af_editor_add_surface(editor, x - 1, y - 1, AF_EDITOR_WELCOME_WIDTH + 2,
                          AF_EDITOR_WELCOME_HEIGHT + 2, AF_UI_BORDER, 17, 249010);
    af_editor_add_surface(editor, x, y, AF_EDITOR_WELCOME_WIDTH, AF_EDITOR_WELCOME_HEIGHT,
                          AF_UI_PANEL, 16, 249011);
    af_editor_add_text(editor, x + 30, y + 24, version, .7F, AF_EDITOR_WELCOME_WIDTH - 60,
                       AF_UI_ACCENT, 249020);
    af_editor_add_text(editor, x + 30, y + 52, "Make something move.", 5.5F,
                       AF_EDITOR_WELCOME_WIDTH - 60, AF_UI_TEXT, 249020);
    af_editor_add_text(editor, x + 30, y + 97,
                       "A space for your ideas. Tools to bring them to life.", 1.1F,
                       AF_EDITOR_WELCOME_WIDTH - 60, AF_UI_MUTED, 249020);
    for (uint32_t index = 0U; index < 4U; ++index) {
        const float row_y = y + 128.0F + (float)index * 74.0F;
        af_editor_add_button_rounded(editor, x + 28, row_y, AF_EDITOR_WELCOME_WIDTH - 56, 60,
                                     index == 0 ? AF_UI_PRIMARY : AF_UI_SURFACE, 8, 249020);
        af_editor_add_text(editor, x + AF_EDITOR_WELCOME_WIDTH - 62, row_y + 20, "→", 1.2F, 20,
                           index == 0 ? AF_UI_TEXT : AF_UI_ACCENT, 249030);
        af_editor_add_text(editor, x + 48.0F, row_y + 11.0F, labels[index], 1.2F, 520.0F,
                           (AfColor){0.9F, 0.94F, 1.0F, 1.0F}, 249030);
        af_editor_add_text(editor, x + 48.0F, row_y + 36.0F, details[index], 0.63F, 620.0F,
                           index == 0 ? AF_UI_TEXT : AF_UI_MUTED, 249030);
    }
    af_editor_add_text(
        editor, x + 30.0F, y + 452.0F,
        "Esc  Continue to your workspace          You can return here from Help > Welcome.", 0.64F,
        AF_EDITOR_WELCOME_WIDTH - 60.0F, (AfColor){0.45F, 0.52F, 0.64F, 1.0F}, 249030);
}

static void af_editor_render_guided_tour(AfEditorState *editor, int width, int height) {
    const AfOnboardingGuide *guide = af_onboarding_guide(editor->guided_topic);
    const float box_width = 430.0F;
    const float box_height = 300.0F;
    const float x = (float)width - editor->right_width - box_width - 14.0F;
    const float y = editor->toolbar_height + 14.0F;
    char progress[128];
    (void)height;
    if (!editor->guided_tour_visible || guide == NULL || editor->guided_step >= guide->step_count) {
        return;
    }
    (void)snprintf(progress, sizeof(progress), "%s  %u / %u", guide->title,
                   editor->guided_step + 1U, guide->step_count);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){box_width, box_height}, (AfColor){0.075F, 0.09F, 0.125F, 0.98F}, 8.0F, 247000);
    af_editor_add_text(editor, x + 18.0F, y + 16.0F, progress, 0.9F, box_width - 36.0F,
                       (AfColor){0.62F, 0.84F, 1.0F, 1.0F}, 247010);
    af_editor_onboarding_paragraph(editor, x + 18.0F, y + 53.0F, guide->steps[editor->guided_step],
                                   0.86F, box_width - 36.0F, (AfColor){0.86F, 0.89F, 0.95F, 1.0F},
                                   247010);
    af_editor_onboarding_paragraph(editor, x + 18.0F, y + 140.0F,
                                   guide->details[editor->guided_step], 0.8F, box_width - 36.0F,
                                   (AfColor){0.5F, 0.58F, 0.7F, 1.0F}, 247010);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 18.0F, .ty = y + 244.0F},
                        (AfVec2){112.0F, 36.0F}, (AfColor){0.15F, 0.2F, 0.3F, 1.0F}, 4.0F, 247010);
    af_editor_add_text(editor, x + 43.0F, y + 257.0F, "BACK", 0.7F, 80.0F,
                       (AfColor){0.8F, 0.84F, 0.92F, 1.0F}, 247020);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 144.0F, .ty = y + 244.0F},
                        (AfVec2){160.0F, 36.0F}, (AfColor){0.18F, 0.42F, 0.68F, 1.0F}, 4.0F,
                        247010);
    af_editor_add_text(editor, x + 180.0F, y + 257.0F,
                       editor->guided_step + 1U == guide->step_count ? "FINISH" : "NEXT STEP", 0.7F,
                       120.0F, (AfColor){0.92F, 0.96F, 1.0F, 1.0F}, 247020);
    af_editor_add_text(editor, x + 328.0F, y + 257.0F, "CLOSE", 0.66F, 78.0F,
                       (AfColor){0.58F, 0.65F, 0.76F, 1.0F}, 247020);
}

static void af_editor_render_example_browser(AfEditorState *editor, int width, int height) {
    const float x = ((float)width - AF_EDITOR_EXAMPLE_WIDTH) * 0.5F;
    const float y = ((float)height - AF_EDITOR_EXAMPLE_HEIGHT) * 0.5F;
    if (!editor->example_browser_visible) {
        return;
    }
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.62F}, 249000);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                        (AfVec2){AF_EDITOR_EXAMPLE_WIDTH, AF_EDITOR_EXAMPLE_HEIGHT},
                        (AfColor){0.075F, 0.085F, 0.115F, 1.0F}, 9.0F, 249010);
    af_editor_add_text(editor, x + 24.0F, y + 18.0F, "EXAMPLE PROJECT BROWSER", 1.3F, 520.0F,
                       (AfColor){0.62F, 0.84F, 1.0F, 1.0F}, 249020);
    af_editor_add_text(editor, x + 24.0F, y + 48.0F,
                       "Every example is deterministic, compilable, and available from the CLI.",
                       0.68F, 750.0F, (AfColor){0.5F, 0.59F, 0.72F, 1.0F}, 249020);
    for (uint32_t index = 0U; index < af_project_template_count(); ++index) {
        const AfProjectTemplateInfo *info = af_project_template_info((AfProjectTemplateId)index);
        const float card_x = x + 22.0F + (float)(index % 3U) * 260.0F;
        const float card_y = y + 82.0F + (float)(index / 3U) * 160.0F;
        const bool selected = index == editor->example_selected_index;
        af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = card_x, .ty = card_y},
                            (AfVec2){246.0F, 145.0F},
                            selected ? (AfColor){0.18F, 0.36F, 0.6F, 1.0F}
                                     : (AfColor){0.115F, 0.135F, 0.18F, 1.0F},
                            6.0F, 249020);
        af_editor_add_text(editor, card_x + 15.0F, card_y + 16.0F, info->name, 0.87F, 216.0F,
                           (AfColor){0.9F, 0.93F, 0.98F, 1.0F}, 249030);
        af_editor_add_text(editor, card_x + 15.0F, card_y + 47.0F, info->description, 0.61F, 216.0F,
                           (AfColor){0.6F, 0.68F, 0.79F, 1.0F}, 249030);
        af_editor_add_text(editor, card_x + 15.0F, card_y + 112.0F, info->feature_label, 0.62F,
                           216.0F, (AfColor){0.5F, 0.82F, 0.7F, 1.0F}, 249030);
    }
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 22.0F, .ty = y + 426.0F},
                        (AfVec2){776.0F, 48.0F}, (AfColor){0.18F, 0.42F, 0.68F, 1.0F}, 5.0F,
                        249020);
    af_editor_add_text(editor, x + 270.0F, y + 443.0F, "CREATE SELECTED EXAMPLE", 0.82F, 300.0F,
                       (AfColor){0.94F, 0.97F, 1.0F, 1.0F}, 249030);
    af_editor_add_text(editor, x + 24.0F, y + 492.0F,
                       "ARROWS SELECT  -  ENTER CREATES  -  ESC CLOSES", 0.64F, 760.0F,
                       (AfColor){0.46F, 0.53F, 0.65F, 1.0F}, 249030);
}

static void af_editor_render_guide_browser(AfEditorState *editor, int width, int height) {
    const AfOnboardingGuide *guide = af_onboarding_guide(editor->guide_topic);
    const uint32_t topic_count = af_onboarding_guide_count();
    const uint32_t visible_topic_count = topic_count < 7U ? topic_count : 7U;
    uint32_t first_topic =
        (uint32_t)editor->guide_topic > 3U ? (uint32_t)editor->guide_topic - 3U : 0U;
    const float x = ((float)width - AF_EDITOR_GUIDE_WIDTH) * 0.5F;
    const float y = ((float)height - AF_EDITOR_GUIDE_HEIGHT) * 0.5F;
    char version[128];
    char step_label[48];
    if (!editor->guide_browser_visible || guide == NULL) {
        return;
    }
    if (first_topic + visible_topic_count > topic_count) {
        first_topic = topic_count - visible_topic_count;
    }
    if (editor->guide_step >= guide->step_count) {
        editor->guide_step = 0U;
    }
    (void)snprintf(version, sizeof(version), "ARCFLOW %s  %s  -  LIVE REFERENCE", AF_VERSION_STRING,
                   AF_RELEASE_CHANNEL);
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.64F}, 249000);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                        (AfVec2){AF_EDITOR_GUIDE_WIDTH, AF_EDITOR_GUIDE_HEIGHT},
                        (AfColor){0.075F, 0.085F, 0.115F, 1.0F}, 9.0F, 249010);
    af_editor_add_text(editor, x + 22.0F, y + 18.0F, version, 0.82F, AF_EDITOR_GUIDE_WIDTH - 44.0F,
                       (AfColor){0.62F, 0.84F, 1.0F, 1.0F}, 249020);
    af_editor_add_rect(editor, x + 254.0F, y + 54.0F, 1.0F, 500.0F,
                       (AfColor){0.24F, 0.28F, 0.36F, 1.0F}, 249020);
    af_editor_add_text(editor, x + 22.0F, y + 63.0F, "TOPICS", 0.7F, 212.0F,
                       (AfColor){0.46F, 0.56F, 0.7F, 1.0F}, 249030);
    for (uint32_t visible = 0U; visible < visible_topic_count; ++visible) {
        const uint32_t index = first_topic + visible;
        const AfOnboardingGuide *topic = af_onboarding_guide((AfOnboardingTopic)index);
        const float row_y = y + 90.0F + (float)visible * 48.0F;
        if (index == (uint32_t)editor->guide_topic) {
            af_editor_add_rect(editor, x + 12.0F, row_y, 232.0F, 40.0F,
                               (AfColor){0.18F, 0.36F, 0.6F, 1.0F}, 249020);
        }
        af_editor_add_text(editor, x + 22.0F, row_y + 13.0F, topic->title, 0.68F, 212.0F,
                           (AfColor){0.78F, 0.83F, 0.91F, 1.0F}, 249030);
    }
    af_editor_add_text(editor, x + 22.0F, y + 444.0F, "UP / DOWN BROWSES ALL TOPICS", 0.58F, 212.0F,
                       (AfColor){0.46F, 0.53F, 0.65F, 1.0F}, 249030);
    af_editor_add_text(editor, x + 282.0F, y + 64.0F, guide->title, 1.2F, 600.0F,
                       (AfColor){0.9F, 0.94F, 1.0F, 1.0F}, 249030);
    af_editor_onboarding_paragraph(editor, x + 282.0F, y + 101.0F, guide->summary, 0.78F, 600.0F,
                       (AfColor){0.58F, 0.67F, 0.79F, 1.0F}, 249030);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 282.0F, .ty = y + 152.0F},
                        (AfVec2){604.0F, 260.0F}, (AfColor){0.105F, 0.13F, 0.18F, 1.0F}, 7.0F,
                        249025);
    (void)snprintf(step_label, sizeof(step_label), "STEP %u OF %u", editor->guide_step + 1U,
                   guide->step_count);
    af_editor_add_text(editor, x + 308.0F, y + 180.0F, step_label, 0.82F, 540.0F,
                       (AfColor){0.48F, 0.8F, 1.0F, 1.0F}, 249040);
    af_editor_onboarding_paragraph(editor, x + 308.0F, y + 216.0F, guide->steps[editor->guide_step],
                                   1.0F, 550.0F, (AfColor){0.86F, 0.9F, 0.96F, 1.0F}, 249040);
    af_editor_onboarding_paragraph(editor, x + 308.0F, y + 300.0F,
                                   guide->details[editor->guide_step], 0.72F, 550.0F, AF_UI_MUTED,
                                   249040);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 282.0F, .ty = y + 434.0F},
                        (AfVec2){292.0F, 48.0F}, (AfColor){0.15F, 0.22F, 0.34F, 1.0F}, 5.0F,
                        249030);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 594.0F, .ty = y + 434.0F},
                        (AfVec2){292.0F, 48.0F}, (AfColor){0.18F, 0.42F, 0.68F, 1.0F}, 5.0F,
                        249030);
    af_editor_add_text(editor, x + 365.0F, y + 451.0F, "PREVIOUS STEP", 0.72F, 180.0F,
                       (AfColor){0.82F, 0.87F, 0.95F, 1.0F}, 249040);
    af_editor_add_text(editor, x + 686.0F, y + 451.0F, "NEXT STEP", 0.72F, 160.0F,
                       (AfColor){0.94F, 0.97F, 1.0F, 1.0F}, 249040);
    af_editor_add_button_rounded(editor, x + 282.0F, y + 490.0F, 292.0F, 26.0F, AF_UI_SURFACE, 5,
                                 249030);
    af_editor_add_text(editor, x + 318.0F, y + 497.0F, "PRACTICE IN EDITOR", 0.68F, 250.0F,
                       AF_UI_TEXT, 249040);
    af_editor_add_button_rounded(editor, x + 594.0F, y + 490.0F, 292.0F, 26.0F, AF_UI_SURFACE, 5,
                                 249030);
    af_editor_add_text(editor, x + 630.0F, y + 497.0F, "WEB LESSONS / PDF", 0.68F, 250.0F,
                       AF_UI_TEXT, 249040);
    af_editor_add_text(editor, x + 282.0F, y + 538.0F,
                       "UP / DOWN TOPIC  -  LEFT / RIGHT STEP  -  ESC CLOSES", 0.62F, 600.0F,
                       (AfColor){0.46F, 0.53F, 0.65F, 1.0F}, 249040);
}

static void af_editor_render_onboarding(AfEditorState *editor, int width, int height) {
    af_editor_render_guided_tour(editor, width, height);
    af_editor_render_welcome(editor, width, height);
    af_editor_render_example_browser(editor, width, height);
    af_editor_render_guide_browser(editor, width, height);
}

static bool af_editor_onboarding_mouse_down(AfEditorState *editor, int width, int height, float x,
                                            float y) {
    if (editor->welcome_visible) {
        const float box_x = ((float)width - AF_EDITOR_WELCOME_WIDTH) * 0.5F;
        const float box_y = ((float)height - AF_EDITOR_WELCOME_HEIGHT) * 0.5F;
        if (x >= box_x + 28.0F && x < box_x + AF_EDITOR_WELCOME_WIDTH - 28.0F &&
            y >= box_y + 128.0F && y < box_y + 424.0F) {
            const uint32_t row = (uint32_t)((y - box_y - 128.0F) / 74.0F);
            if (row == 0U) {
                af_editor_onboarding_start_guided(editor);
            } else if (row == 1U) {
                af_editor_onboarding_finish_welcome(editor);
                editor->template_panel_visible = true;
                editor->template_selected_index = AF_PROJECT_TEMPLATE_GUIDED_BASICS;
            } else if (row == 2U) {
                af_editor_onboarding_show_examples(editor);
            } else if (row == 3U) {
                af_editor_onboarding_show_guides(editor, AF_ONBOARDING_QUICK_START);
            }
        }
        return true;
    }
    if (editor->example_browser_visible) {
        const float box_x = ((float)width - AF_EDITOR_EXAMPLE_WIDTH) * 0.5F;
        const float box_y = ((float)height - AF_EDITOR_EXAMPLE_HEIGHT) * 0.5F;
        if (x >= box_x + 22.0F && x < box_x + 802.0F && y >= box_y + 82.0F && y < box_y + 387.0F) {
            const uint32_t column = (uint32_t)((x - box_x - 22.0F) / 260.0F);
            const uint32_t row = (uint32_t)((y - box_y - 82.0F) / 160.0F);
            const uint32_t index = row * 3U + column;
            if (index < af_project_template_count()) {
                editor->example_selected_index = index;
            }
        } else if (x >= box_x + 22.0F && x < box_x + 798.0F && y >= box_y + 426.0F &&
                   y < box_y + 474.0F) {
            const uint32_t selected = editor->example_selected_index;
            editor->example_browser_visible = false;
            (void)af_editor_create_template(editor, selected);
        }
        return true;
    }
    if (editor->guide_browser_visible) {
        const AfOnboardingGuide *guide = af_onboarding_guide(editor->guide_topic);
        const uint32_t topic_count = af_onboarding_guide_count();
        const uint32_t visible_topic_count = topic_count < 7U ? topic_count : 7U;
        uint32_t first_topic =
            (uint32_t)editor->guide_topic > 3U ? (uint32_t)editor->guide_topic - 3U : 0U;
        const float box_x = ((float)width - AF_EDITOR_GUIDE_WIDTH) * 0.5F;
        const float box_y = ((float)height - AF_EDITOR_GUIDE_HEIGHT) * 0.5F;
        if (first_topic + visible_topic_count > topic_count) {
            first_topic = topic_count - visible_topic_count;
        }
        if (x >= box_x + 12.0F && x < box_x + 244.0F && y >= box_y + 58.0F && y >= box_y + 90.0F &&
            y < box_y + 90.0F + (float)visible_topic_count * 48.0F) {
            const uint32_t row = (uint32_t)((y - box_y - 90.0F) / 48.0F);
            if (first_topic + row < topic_count) {
                editor->guide_topic = (AfOnboardingTopic)(first_topic + row);
                editor->guide_step = 0U;
            }
        } else if (y >= box_y + 490.0F && y < box_y + 516.0F && guide != NULL) {
            if (x >= box_x + 282.0F && x < box_x + 574.0F) {
                editor->guided_topic = editor->guide_topic;
                editor->guided_step = editor->guide_step;
                editor->guide_browser_visible = false;
                editor->guided_tour_visible = true;
            } else if (x >= box_x + 594.0F && x < box_x + 886.0F) {
                char url[256];
                (void)snprintf(url, sizeof(url), "https://stmsi.github.io/arcflow/learn/#%s",
                               guide->slug);
                if (!SDL_OpenURL(url)) {
                    af_editor_status(editor, "Could not open tutorials in your browser");
                }
            }
        } else if (y >= box_y + 434.0F && y < box_y + 482.0F && guide != NULL) {
            if (x >= box_x + 282.0F && x < box_x + 574.0F && editor->guide_step > 0U) {
                --editor->guide_step;
            } else if (x >= box_x + 594.0F && x < box_x + 886.0F &&
                       editor->guide_step + 1U < guide->step_count) {
                ++editor->guide_step;
            }
        }
        return true;
    }
    if (editor->guided_tour_visible) {
        const AfOnboardingGuide *guide = af_onboarding_guide(editor->guided_topic);
        const float box_x = (float)width - editor->right_width - 430.0F - 14.0F;
        const float box_y = editor->toolbar_height + 14.0F;
        if (x < box_x || x >= box_x + 430.0F || y < box_y || y >= box_y + 300.0F) {
            return false;
        }
        if (y >= box_y + 244.0F && y < box_y + 280.0F) {
            if (x >= box_x + 18.0F && x < box_x + 130.0F && editor->guided_step > 0U) {
                --editor->guided_step;
            } else if (x >= box_x + 144.0F && x < box_x + 304.0F && guide != NULL) {
                if (editor->guided_step + 1U < guide->step_count) {
                    ++editor->guided_step;
                } else {
                    editor->guided_tour_visible = false;
                    af_editor_status(editor, "Tutorial complete");
                }
            } else if (x >= box_x + 304.0F) {
                editor->guided_tour_visible = false;
            }
        }
        return true;
    }
    return false;
}

static bool af_editor_onboarding_key(AfEditorState *editor, const SDL_KeyboardEvent *event) {
    if (!event->down) {
        return false;
    }
    if (editor->welcome_visible) {
        if (event->key == SDLK_ESCAPE) {
            af_editor_onboarding_finish_welcome(editor);
        } else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            af_editor_onboarding_start_guided(editor);
        }
        return true;
    }
    if (editor->example_browser_visible) {
        const uint32_t count = af_project_template_count();
        if (event->key == SDLK_ESCAPE) {
            editor->example_browser_visible = false;
        } else if (event->key == SDLK_LEFT && editor->example_selected_index > 0U) {
            --editor->example_selected_index;
        } else if (event->key == SDLK_RIGHT && editor->example_selected_index + 1U < count) {
            ++editor->example_selected_index;
        } else if (event->key == SDLK_UP && editor->example_selected_index >= 3U) {
            editor->example_selected_index -= 3U;
        } else if (event->key == SDLK_DOWN && editor->example_selected_index + 3U < count) {
            editor->example_selected_index += 3U;
        } else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            const uint32_t selected = editor->example_selected_index;
            editor->example_browser_visible = false;
            (void)af_editor_create_template(editor, selected);
        }
        return true;
    }
    if (editor->guide_browser_visible) {
        if (event->key == SDLK_ESCAPE) {
            editor->guide_browser_visible = false;
        } else if (event->key == SDLK_UP && (uint32_t)editor->guide_topic > 0U) {
            editor->guide_topic = (AfOnboardingTopic)((uint32_t)editor->guide_topic - 1U);
            editor->guide_step = 0U;
        } else if (event->key == SDLK_DOWN &&
                   (uint32_t)editor->guide_topic + 1U < af_onboarding_guide_count()) {
            editor->guide_topic = (AfOnboardingTopic)((uint32_t)editor->guide_topic + 1U);
            editor->guide_step = 0U;
        } else if (event->key == SDLK_LEFT && editor->guide_step > 0U) {
            --editor->guide_step;
        } else if (event->key == SDLK_RIGHT) {
            const AfOnboardingGuide *guide = af_onboarding_guide(editor->guide_topic);
            if (guide != NULL && editor->guide_step + 1U < guide->step_count) {
                ++editor->guide_step;
            }
        }
        return true;
    }
    if (editor->guided_tour_visible && event->key == SDLK_ESCAPE) {
        editor->guided_tour_visible = false;
        return true;
    }
    return false;
}
