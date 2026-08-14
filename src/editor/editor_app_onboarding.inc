#define AF_EDITOR_WELCOME_WIDTH 760.0F
#define AF_EDITOR_WELCOME_HEIGHT 500.0F
#define AF_EDITOR_EXAMPLE_WIDTH 820.0F
#define AF_EDITOR_EXAMPLE_HEIGHT 520.0F
#define AF_EDITOR_GUIDE_WIDTH 920.0F
#define AF_EDITOR_GUIDE_HEIGHT 590.0F

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
        editor->guided_tour_visible = true;
        editor->guided_step = 0U;
        af_editor_status(editor, "Guided project ready; follow the checklist");
    }
}

static void af_editor_render_welcome(AfEditorState *editor, int width, int height) {
    static const char *const labels[] = {"START GUIDED PROJECT", "BROWSE PROJECT TEMPLATES",
                                         "OPEN EXAMPLE BROWSER", "REFERENCE TOPICS"};
    static const char *const details[] = {
        "Create a ready-to-animate project and follow six steps",
        "Blank, vector, button, responsive, and particle starters",
        "Inspect feature-focused projects that compile and play",
        "Installation, API, Vulkan, formats, security, and more"};
    char version[160];
    const float x = ((float)width - AF_EDITOR_WELCOME_WIDTH) * 0.5F;
    const float y = ((float)height - AF_EDITOR_WELCOME_HEIGHT) * 0.5F;
    if (!editor->welcome_visible) {
        return;
    }
    (void)snprintf(version, sizeof(version), "VERSION %s  %s  PROJECT %u  PACKAGE %u  RUNTIME %u",
                   AF_VERSION_STRING, AF_RELEASE_CHANNEL, AF_PROJECT_FORMAT_VERSION,
                   AF_PACKAGE_FORMAT_VERSION, AF_RUNTIME_VERSION);
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.68F}, 249000);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                        (AfVec2){AF_EDITOR_WELCOME_WIDTH, AF_EDITOR_WELCOME_HEIGHT},
                        (AfColor){0.075F, 0.085F, 0.115F, 1.0F}, 10.0F, 249010);
    af_editor_add_text(editor, x + 30.0F, y + 25.0F, "WELCOME TO ARCFLOW", 1.65F,
                       AF_EDITOR_WELCOME_WIDTH - 60.0F, (AfColor){0.62F, 0.84F, 1.0F, 1.0F},
                       249020);
    af_editor_add_text(editor, x + 30.0F, y + 62.0F,
                       "Create, animate, compile, and play without leaving the editor.", 0.9F,
                       AF_EDITOR_WELCOME_WIDTH - 60.0F, (AfColor){0.72F, 0.77F, 0.87F, 1.0F},
                       249020);
    af_editor_add_text(editor, x + 30.0F, y + 88.0F, version, 0.64F,
                       AF_EDITOR_WELCOME_WIDTH - 60.0F, (AfColor){0.48F, 0.57F, 0.7F, 1.0F},
                       249020);
    for (uint32_t index = 0U; index < 4U; ++index) {
        const float row_y = y + 128.0F + (float)index * 74.0F;
        const bool hovered = editor->mouse_position.x >= x + 28.0F &&
                             editor->mouse_position.x < x + AF_EDITOR_WELCOME_WIDTH - 28.0F &&
                             editor->mouse_position.y >= row_y &&
                             editor->mouse_position.y < row_y + 60.0F;
        af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 28.0F, .ty = row_y},
                            (AfVec2){AF_EDITOR_WELCOME_WIDTH - 56.0F, 60.0F},
                            hovered ? (AfColor){0.18F, 0.38F, 0.64F, 1.0F}
                                    : (AfColor){0.12F, 0.17F, 0.25F, 1.0F},
                            6.0F, 249020);
        af_editor_add_text(editor, x + 48.0F, row_y + 11.0F, labels[index], 0.9F, 320.0F,
                           (AfColor){0.9F, 0.94F, 1.0F, 1.0F}, 249030);
        af_editor_add_text(editor, x + 48.0F, row_y + 36.0F, details[index], 0.63F, 620.0F,
                           (AfColor){0.58F, 0.67F, 0.8F, 1.0F}, 249030);
    }
    af_editor_add_text(editor, x + 30.0F, y + 452.0F, "ESC CLOSES. OPEN AGAIN FROM HELP > WELCOME.",
                       0.64F, AF_EDITOR_WELCOME_WIDTH - 60.0F, (AfColor){0.45F, 0.52F, 0.64F, 1.0F},
                       249030);
}

static void af_editor_render_guided_tour(AfEditorState *editor, int width, int height) {
    const AfOnboardingGuide *guide = af_onboarding_guide(AF_ONBOARDING_QUICK_START);
    const float box_width = 430.0F;
    const float box_height = 230.0F;
    const float x = (float)width - editor->right_width - box_width - 14.0F;
    const float y = AF_EDITOR_TOOLBAR_HEIGHT + 14.0F;
    char progress[64];
    (void)height;
    if (!editor->guided_tour_visible || guide == NULL || editor->guided_step >= guide->step_count) {
        return;
    }
    (void)snprintf(progress, sizeof(progress), "GUIDED FIRST PROJECT  %u / %u",
                   editor->guided_step + 1U, guide->step_count);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){box_width, box_height}, (AfColor){0.075F, 0.09F, 0.125F, 0.98F}, 8.0F, 247000);
    af_editor_add_text(editor, x + 18.0F, y + 16.0F, progress, 0.9F, box_width - 36.0F,
                       (AfColor){0.62F, 0.84F, 1.0F, 1.0F}, 247010);
    af_editor_add_text(editor, x + 18.0F, y + 53.0F, guide->steps[editor->guided_step], 0.86F,
                       box_width - 36.0F, (AfColor){0.86F, 0.89F, 0.95F, 1.0F}, 247010);
    af_editor_add_text(editor, x + 18.0F, y + 124.0F,
                       "The panel stays open while you work on the canvas.", 0.64F,
                       box_width - 36.0F, (AfColor){0.5F, 0.58F, 0.7F, 1.0F}, 247010);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 18.0F, .ty = y + 174.0F},
                        (AfVec2){112.0F, 36.0F}, (AfColor){0.15F, 0.2F, 0.3F, 1.0F}, 4.0F, 247010);
    af_editor_add_text(editor, x + 43.0F, y + 187.0F, "BACK", 0.7F, 80.0F,
                       (AfColor){0.8F, 0.84F, 0.92F, 1.0F}, 247020);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 144.0F, .ty = y + 174.0F},
                        (AfVec2){160.0F, 36.0F}, (AfColor){0.18F, 0.42F, 0.68F, 1.0F}, 4.0F,
                        247010);
    af_editor_add_text(editor, x + 180.0F, y + 187.0F,
                       editor->guided_step + 1U == guide->step_count ? "FINISH" : "NEXT STEP", 0.7F,
                       120.0F, (AfColor){0.92F, 0.96F, 1.0F, 1.0F}, 247020);
    af_editor_add_text(editor, x + 328.0F, y + 187.0F, "CLOSE", 0.66F, 78.0F,
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
    af_editor_add_text(editor, x + 22.0F, y + 444.0F, "UP / DOWN BROWSES ALL 16 TOPICS", 0.58F,
                       212.0F, (AfColor){0.46F, 0.53F, 0.65F, 1.0F}, 249030);
    af_editor_add_text(editor, x + 282.0F, y + 64.0F, guide->title, 1.2F, 600.0F,
                       (AfColor){0.9F, 0.94F, 1.0F, 1.0F}, 249030);
    af_editor_add_text(editor, x + 282.0F, y + 101.0F, guide->summary, 0.78F, 600.0F,
                       (AfColor){0.58F, 0.67F, 0.79F, 1.0F}, 249030);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 282.0F, .ty = y + 152.0F},
                        (AfVec2){604.0F, 260.0F}, (AfColor){0.105F, 0.13F, 0.18F, 1.0F}, 7.0F,
                        249025);
    (void)snprintf(step_label, sizeof(step_label), "STEP %u OF %u", editor->guide_step + 1U,
                   guide->step_count);
    af_editor_add_text(editor, x + 308.0F, y + 180.0F, step_label, 0.82F, 540.0F,
                       (AfColor){0.48F, 0.8F, 1.0F, 1.0F}, 249040);
    af_editor_add_text(editor, x + 308.0F, y + 236.0F, guide->steps[editor->guide_step], 1.0F,
                       550.0F, (AfColor){0.86F, 0.9F, 0.96F, 1.0F}, 249040);
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
    af_editor_add_text(editor, x + 282.0F, y + 522.0F,
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
        const AfOnboardingGuide *guide = af_onboarding_guide(AF_ONBOARDING_QUICK_START);
        const float box_x = (float)width - editor->right_width - 430.0F - 14.0F;
        const float box_y = AF_EDITOR_TOOLBAR_HEIGHT + 14.0F;
        if (x < box_x || x >= box_x + 430.0F || y < box_y || y >= box_y + 230.0F) {
            return false;
        }
        if (y >= box_y + 174.0F && y < box_y + 210.0F) {
            if (x >= box_x + 18.0F && x < box_x + 130.0F && editor->guided_step > 0U) {
                --editor->guided_step;
            } else if (x >= box_x + 144.0F && x < box_x + 304.0F && guide != NULL) {
                if (editor->guided_step + 1U < guide->step_count) {
                    ++editor->guided_step;
                } else {
                    editor->guided_tour_visible = false;
                    af_editor_status(editor, "Guided first project complete");
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
