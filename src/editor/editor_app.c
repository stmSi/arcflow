#include "editor/editor_app.h"

#include "core/allocator.h"
#include "core/file_io.h"
#include "core/hash.h"
#include "core/log.h"
#include "editor/editor_commands.h"
#include "platform/platform_sdl.h"
#include "project/project.h"
#include "renderer/vk_renderer.h"
#include "scene/scene.h"
#include "text/text.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AF_EDITOR_UI_RESERVE 1200U
#define AF_EDITOR_PROJECT_CAPACITY (AF_MAX_NODES - AF_EDITOR_UI_RESERVE)
#define AF_EDITOR_PATH_CAPACITY 512U
#define AF_EDITOR_STATUS_CAPACITY 160U
#define AF_EDITOR_TOOLBAR_HEIGHT 44.0F
#define AF_EDITOR_STATUS_HEIGHT 28.0F
#define AF_EDITOR_TIMELINE_DEFAULT_HEIGHT 270.0F
#define AF_EDITOR_TIMELINE_MIN_HEIGHT 170.0F
#define AF_EDITOR_TIMELINE_RULER_HEIGHT 54.0F
#define AF_EDITOR_TIMELINE_TRACK_HEIGHT 26.0F
#define AF_EDITOR_TIMELINE_HEADER_WIDTH 154.0F
#define AF_EDITOR_ROW_HEIGHT 28.0F
#define AF_EDITOR_MIN_DOCK_WIDTH 150.0F
#define AF_EDITOR_MIN_VIEW_WIDTH 240.0F
#define AF_EDITOR_CONTEXT_WIDTH 240.0F
#define AF_EDITOR_CONTEXT_HEADER_HEIGHT 34.0F
#define AF_EDITOR_CONTEXT_ITEM_HEIGHT 30.0F
#define AF_EDITOR_TOOLTIP_WIDTH 292.0F
#define AF_EDITOR_TOOLTIP_DETAIL_WIDTH 420.0F
#define AF_EDITOR_TOOLTIP_DELAY_NS UINT64_C(350000000)
#define AF_EDITOR_COLOR_SV_Y 76.0F
#define AF_EDITOR_COLOR_SV_WIDTH 142.0F
#define AF_EDITOR_COLOR_SV_HEIGHT 86.0F
#define AF_EDITOR_COLOR_HUE_X 150.0F
#define AF_EDITOR_COLOR_HUE_WIDTH 16.0F
#define AF_EDITOR_COLOR_ALPHA_Y 170.0F
#define AF_EDITOR_COLOR_ALPHA_HEIGHT 14.0F
#define AF_EDITOR_INSPECTOR_SCALAR_Y 210.0F
#define AF_EDITOR_INSPECTOR_SCALAR_STEP 44.0F
#define AF_EDITOR_STYLE_BAR_Y 382.0F
#define AF_EDITOR_STYLE_LABEL_Y 400.0F
#define AF_EDITOR_STYLE_ROW_Y 414.0F
#define AF_EDITOR_LAYOUT_ROW_Y 450.0F
#define AF_EDITOR_PARTICLE_LABEL_Y 493.0F
#define AF_EDITOR_PARTICLE_ROW_Y 514.0F
#define AF_EDITOR_PARTICLE_ROW_STEP 30.0F
#define AF_EDITOR_PARTICLE_ROW_COUNT 10U
#define AF_EDITOR_TEXT_FONT_ID 1U
#define AF_EDITOR_TEXT_GLYPH_CAPACITY 512U
#define AF_EDITOR_TEXT_LINE_CAPACITY 8U
#define AF_EDITOR_TEXT_GLYPHS_PER_PATH 8U
#define AF_EDITOR_UI_NODE_ID_BASE UINT64_C(0xF000000000000000)

typedef enum AfEditorDragMode {
    AF_EDITOR_DRAG_NONE = 0,
    AF_EDITOR_DRAG_MOVE,
    AF_EDITOR_DRAG_X,
    AF_EDITOR_DRAG_Y,
    AF_EDITOR_DRAG_PAN,
    AF_EDITOR_DRAG_LEFT_DOCK,
    AF_EDITOR_DRAG_RIGHT_DOCK,
    AF_EDITOR_DRAG_TIMELINE_HEIGHT,
    AF_EDITOR_DRAG_TIMELINE_PAN,
    AF_EDITOR_DRAG_PLAYHEAD,
    AF_EDITOR_DRAG_LOOP_START,
    AF_EDITOR_DRAG_LOOP_END,
    AF_EDITOR_DRAG_KEYFRAMES,
    AF_EDITOR_DRAG_KEY_BOX,
    AF_EDITOR_DRAG_TANGENT_IN,
    AF_EDITOR_DRAG_TANGENT_OUT,
    AF_EDITOR_DRAG_PATH_POINT,
    AF_EDITOR_DRAG_PATH_CONTROL1,
    AF_EDITOR_DRAG_PATH_CONTROL2,
    AF_EDITOR_DRAG_CANVAS_BOX,
    AF_EDITOR_DRAG_COLOR_SV,
    AF_EDITOR_DRAG_COLOR_HUE,
    AF_EDITOR_DRAG_COLOR_ALPHA
} AfEditorDragMode;

typedef enum AfEditorContextMenu {
    AF_EDITOR_CONTEXT_NONE = 0,
    AF_EDITOR_CONTEXT_HIERARCHY,
    AF_EDITOR_CONTEXT_CANVAS,
    AF_EDITOR_CONTEXT_INSPECTOR,
    AF_EDITOR_CONTEXT_TIMELINE,
    AF_EDITOR_CONTEXT_TOOLBAR,
    AF_EDITOR_CONTEXT_RECTANGLE,
    AF_EDITOR_CONTEXT_ROUNDED_RECTANGLE,
    AF_EDITOR_CONTEXT_ELLIPSE,
    AF_EDITOR_CONTEXT_PATH,
    AF_EDITOR_CONTEXT_GROUP
} AfEditorContextMenu;

typedef struct AfEditorContextMenuInfo {
    const char *title;
    const char *items[7];
    uint32_t item_count;
} AfEditorContextMenuInfo;

typedef struct AfEditorTooltip {
    uint64_t id;
    const char *title;
    const char *summary;
    const char *details;
    const char *shortcut;
} AfEditorTooltip;

typedef struct AfEditorState {
    AfAllocator allocator;
    AfProject project;
    AfEditorHistory history;
    AfPlatformWindow window;
    AfVkRenderer *renderer;
    AfSceneNode *render_nodes;
    AfScene render_scene;
    AfPath *render_paths;
    AfPathCommand *render_path_commands;
    AfPathData render_path_data;
    AfSceneNode *preview_nodes;
    AfScene preview_scene;
    uint64_t selected_id;
    uint64_t selected_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t selected_count;
    uint32_t hierarchy_order[AF_EDITOR_PROJECT_CAPACITY];
    uint8_t hierarchy_depth[AF_EDITOR_PROJECT_CAPACITY];
    uint32_t hierarchy_count;
    uint32_t hierarchy_scroll;
    float left_width;
    float right_width;
    float zoom;
    AfVec2 pan;
    float grid_step;
    float color_hue;
    bool grid_visible;
    bool snapping;
    AfVec2 mouse_position;
    AfVec2 context_position;
    AfEditorContextMenu context_menu;
    uint64_t hovered_tooltip_id;
    uint64_t tooltip_started_ticks;
    float timeline_height;
    float timeline_zoom;
    float timeline_pan;
    float playhead;
    uint64_t selected_clip_id;
    AfPropertyType selected_property;
    uint64_t selected_keyframes[AF_EDITOR_COMMAND_MAX_KEYFRAMES];
    uint32_t selected_keyframe_count;
    AfProjectKeyframeSnapshot key_drag_before[AF_EDITOR_COMMAND_MAX_KEYFRAMES];
    uint32_t key_drag_count;
    AfProjectClipSnapshot clip_drag_before;
    AfVec2 key_box_start;
    AfVec2 key_box_current;
    AfProjectKeyframe key_clipboard[AF_EDITOR_COMMAND_MAX_KEYFRAMES];
    uint32_t key_clipboard_count;
    uint32_t *curve_order;
    AfPathTessellationScratch *path_scratch;
    AfTextEngine *text_engine;
    AfFileData font_file;
    AfGlyphPlacement *text_glyphs;
    AfTextLine *text_lines;
    uint32_t selected_path_command;
    AfProjectPathSnapshot path_drag_before;
    AfPathCommand path_drag_commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    bool timeline_focus;
    bool curve_visible;
    bool preview_playing;
    uint64_t previous_ticks;
    AfEditorDragMode drag_mode;
    AfVec2 drag_start_mouse;
    AfProjectNodeSnapshot drag_before;
    AfProjectNodeSnapshot selection_drag_before[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t selection_drag_count;
    AfVec2 canvas_box_start;
    AfVec2 canvas_box_current;
    bool canvas_box_additive;
    bool rename_active;
    AfProjectNodeSnapshot rename_before;
    char rename_buffer[AF_PROJECT_NAME_CAPACITY];
    char project_path[AF_EDITOR_PATH_CAPACITY];
    char package_path[AF_EDITOR_PATH_CAPACITY];
    char output_path[AF_EDITOR_PATH_CAPACITY];
    char status[AF_EDITOR_STATUS_CAPACITY];
    uint32_t maximum_frames;
    uint32_t rendered_frames;
    bool resize_smoke;
    bool context_smoke;
    bool tooltip_smoke;
    bool selection_smoke;
    bool running;
    bool dirty;
} AfEditorState;

static float af_editor_clamp(float value, float minimum, float maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static AfColor af_editor_hsv_color(float hue, float saturation, float value, float alpha) {
    const float wrapped = hue - floorf(hue);
    const float sector_value = wrapped * 6.0F;
    const int sector = (int)floorf(sector_value);
    const float fraction = sector_value - (float)sector;
    const float low = value * (1.0F - saturation);
    const float falling = value * (1.0F - saturation * fraction);
    const float rising = value * (1.0F - saturation * (1.0F - fraction));
    switch (sector % 6) {
    case 0:
        return (AfColor){value, rising, low, alpha};
    case 1:
        return (AfColor){falling, value, low, alpha};
    case 2:
        return (AfColor){low, value, rising, alpha};
    case 3:
        return (AfColor){low, falling, value, alpha};
    case 4:
        return (AfColor){rising, low, value, alpha};
    default:
        return (AfColor){value, low, falling, alpha};
    }
}

static void af_editor_color_hsv(AfColor color, float *out_hue, float *out_saturation,
                                float *out_value) {
    const float maximum = fmaxf(color.r, fmaxf(color.g, color.b));
    const float minimum = fminf(color.r, fminf(color.g, color.b));
    const float range = maximum - minimum;
    float hue = 0.0F;
    if (range > 0.00001F) {
        if (maximum == color.r) {
            hue = (color.g - color.b) / range;
        } else if (maximum == color.g) {
            hue = 2.0F + (color.b - color.r) / range;
        } else {
            hue = 4.0F + (color.r - color.g) / range;
        }
        hue /= 6.0F;
        if (hue < 0.0F) {
            hue += 1.0F;
        }
    }
    *out_hue = hue;
    *out_saturation = maximum <= 0.00001F ? 0.0F : range / maximum;
    *out_value = maximum;
}

static bool af_editor_selection_contains(const AfEditorState *editor, uint64_t id) {
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        if (editor->selected_ids[index] == id) {
            return true;
        }
    }
    return false;
}

static void af_editor_select_only(AfEditorState *editor, uint64_t id) {
    editor->selected_count = id == 0U ? 0U : 1U;
    editor->selected_id = id;
    if (id != 0U) {
        editor->selected_ids[0] = id;
    }
}

static void af_editor_selection_add(AfEditorState *editor, uint64_t id) {
    if (id == 0U || af_editor_selection_contains(editor, id)) {
        return;
    }
    if (editor->selected_count < AF_EDITOR_COMMAND_MAX_SNAPSHOTS) {
        editor->selected_ids[editor->selected_count++] = id;
        editor->selected_id = id;
    }
}

static void af_editor_selection_toggle(AfEditorState *editor, uint64_t id) {
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        if (editor->selected_ids[index] == id) {
            memmove(&editor->selected_ids[index], &editor->selected_ids[index + 1U],
                    (size_t)(editor->selected_count - index - 1U) *
                        sizeof(editor->selected_ids[0]));
            --editor->selected_count;
            editor->selected_id = editor->selected_count == 0U
                                      ? 0U
                                      : editor->selected_ids[editor->selected_count - 1U];
            return;
        }
    }
    af_editor_selection_add(editor, id);
}

static bool af_editor_selection_has_selected_ancestor(const AfEditorState *editor, uint64_t id) {
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, id);
    uint32_t depth = 0U;
    while (node != NULL && node->parent_id != AF_INVALID_NODE_ID &&
           depth++ < AF_MAX_HIERARCHY_DEPTH) {
        if (af_editor_selection_contains(editor, node->parent_id)) {
            return true;
        }
        node = af_scene_find_const(&editor->project.scene, node->parent_id);
    }
    return false;
}

static void af_editor_begin_selection_modify(AfEditorState *editor, bool top_level_only) {
    editor->selection_drag_count = 0U;
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        const uint64_t id = editor->selected_ids[index];
        if ((!top_level_only || !af_editor_selection_has_selected_ancestor(editor, id)) &&
            af_project_snapshot(&editor->project, id,
                                &editor->selection_drag_before[editor->selection_drag_count]) ==
                AF_RESULT_SUCCESS) {
            ++editor->selection_drag_count;
        }
    }
    if (editor->selection_drag_count == 0U && editor->selected_id != 0U &&
        af_project_snapshot(&editor->project, editor->selected_id,
                            &editor->selection_drag_before[0]) == AF_RESULT_SUCCESS) {
        editor->selection_drag_count = 1U;
    }
}

static AfResult af_editor_commit_selection_modify(AfEditorState *editor, AfEditorMergeKey merge_key,
                                                  bool allow_merge) {
    AfEditorCommand command = {.type = AF_EDITOR_COMMAND_MODIFY,
                               .merge_key = merge_key,
                               .target_id = editor->selected_id,
                               .before_count = editor->selection_drag_count,
                               .after_count = editor->selection_drag_count};
    AfResult result = AF_RESULT_SUCCESS;
    if (editor->selection_drag_count == 0U) {
        return AF_RESULT_NOT_FOUND;
    }
    memcpy(command.before, editor->selection_drag_before,
           (size_t)editor->selection_drag_count * sizeof(command.before[0]));
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        result = af_project_snapshot(&editor->project, editor->selection_drag_before[index].node.id,
                                     &command.after[index]);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    result = af_editor_history_execute(&editor->history, &editor->project, &command, true,
                                       allow_merge && editor->selection_drag_count == 1U);
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
    }
    return result;
}

static int32_t af_editor_clamp_layer(int32_t value) {
    if (value < -10000) {
        return -10000;
    }
    return value > 10000 ? 10000 : value;
}

static void af_editor_status(AfEditorState *editor, const char *message) {
    (void)snprintf(editor->status, sizeof(editor->status), "%s", message);
}

static AfSceneNode af_editor_default_node(AfNodeType type, uint64_t id, AfVec2 position) {
    const bool rounded = type == AF_NODE_ROUNDED_RECTANGLE;
    return (AfSceneNode){.id = id,
                         .parent_id = AF_INVALID_NODE_ID,
                         .type = type,
                         .local_transform = {.position = position, .scale = {1.0F, 1.0F}},
                         .bounds = type == AF_NODE_ELLIPSE ? (AfVec2){120.0F, 120.0F}
                                                           : (AfVec2){180.0F, 110.0F},
                         .fill = {0.22F, 0.58F, 0.96F, 1.0F},
                         .opacity = 1.0F,
                         .corner_radius = rounded ? 24.0F : 0.0F,
                         .visible = true};
}

static AfResult af_editor_default_project(AfEditorState *editor) {
    AfSceneNode root = {.id = 1U,
                        .parent_id = AF_INVALID_NODE_ID,
                        .type = AF_NODE_GROUP,
                        .local_transform = {.scale = {1.0F, 1.0F}},
                        .opacity = 1.0F,
                        .visible = true};
    AfSceneNode rectangle = af_editor_default_node(AF_NODE_RECTANGLE, 2U, (AfVec2){160.0F, 150.0F});
    AfSceneNode rounded =
        af_editor_default_node(AF_NODE_ROUNDED_RECTANGLE, 3U, (AfVec2){480.0F, 270.0F});
    AfSceneNode ellipse = af_editor_default_node(AF_NODE_ELLIPSE, 4U, (AfVec2){820.0F, 390.0F});
    AfResult result = AF_RESULT_SUCCESS;
    AfProjectClip clip = {.id = 1U,
                          .name = "Main Clip",
                          .duration = 4.0F,
                          .loop_start = 0.0F,
                          .loop_end = 4.0F,
                          .loop = true};
    rectangle.parent_id = root.id;
    rectangle.fill = (AfColor){0.18F, 0.56F, 0.96F, 1.0F};
    rounded.parent_id = root.id;
    rounded.fill = (AfColor){0.94F, 0.35F, 0.27F, 1.0F};
    rounded.layer_order = 1;
    ellipse.parent_id = root.id;
    ellipse.fill = (AfColor){0.32F, 0.86F, 0.54F, 1.0F};
    ellipse.layer_order = 2;
    result = af_project_add_node(&editor->project, &root, "Scene");
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(&editor->project, &rectangle, "Rectangle");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(&editor->project, &rounded, "Rounded Rectangle");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(&editor->project, &ellipse, "Ellipse");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_clip(&editor->project, &clip);
    }
    if (result == AF_RESULT_SUCCESS) {
        uint64_t emitter_id = 0U;
        result = af_project_allocate_animation_id(&editor->project, &emitter_id);
        if (result == AF_RESULT_SUCCESS) {
            AfParticleEmitter emitter = af_particle_emitter_default(emitter_id, ellipse.id);
            emitter.type = AF_PARTICLE_EMITTER_CIRCLE;
            emitter.output = AF_PARTICLE_OUTPUT_TRAIL;
            emitter.turbulence_strength = 120.0F;
            emitter.vortex_force = 70.0F;
            result = af_project_add_emitter(&editor->project, &emitter);
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_scene_evaluate_world(&editor->project.scene);
    }
    return result;
}

static void af_editor_derive_package_path(AfEditorState *editor) {
    const char *extension = strrchr(editor->project_path, '.');
    size_t prefix = strlen(editor->project_path);
    if (extension != NULL && strcmp(extension, ".afproj") == 0) {
        prefix = (size_t)(extension - editor->project_path);
    }
    if (prefix > sizeof(editor->package_path) - 5U) {
        prefix = sizeof(editor->package_path) - 5U;
    }
    memcpy(editor->package_path, editor->project_path, prefix);
    memcpy(editor->package_path + prefix, ".afp", 5U);
}

static AfResult af_editor_save(AfEditorState *editor) {
    AfResult result =
        af_project_save_file(&editor->project, editor->project_path, &editor->allocator);
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = false;
        af_editor_status(editor, "Project saved");
    } else {
        af_editor_status(editor, "Project save failed");
    }
    return result;
}

static AfResult af_editor_compile(AfEditorState *editor) {
    AfPackageBlob package = {0};
    AfResult result = af_project_compile(&editor->project, &editor->allocator, &package);
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write(editor->package_path, package.data, package.size);
    }
    af_package_blob_destroy(&editor->allocator, &package);
    af_editor_status(editor, result == AF_RESULT_SUCCESS ? "Runtime package compiled"
                                                         : "Runtime package compilation failed");
    return result;
}

static void af_editor_hierarchy_visit(AfEditorState *editor, uint64_t parent_id, uint8_t depth) {
    uint32_t index = 0U;
    for (index = 0U; index < editor->project.scene.count; ++index) {
        const AfSceneNode *node = &editor->project.scene.nodes[index];
        if (node->parent_id != parent_id || editor->hierarchy_count >= AF_EDITOR_PROJECT_CAPACITY) {
            continue;
        }
        editor->hierarchy_order[editor->hierarchy_count] = index;
        editor->hierarchy_depth[editor->hierarchy_count] = depth;
        ++editor->hierarchy_count;
        if (depth + 1U < AF_MAX_HIERARCHY_DEPTH) {
            af_editor_hierarchy_visit(editor, node->id, (uint8_t)(depth + 1U));
        }
    }
}

static void af_editor_build_hierarchy(AfEditorState *editor) {
    editor->hierarchy_count = 0U;
    af_editor_hierarchy_visit(editor, AF_INVALID_NODE_ID, 0U);
    if (editor->hierarchy_scroll > editor->hierarchy_count) {
        editor->hierarchy_scroll = editor->hierarchy_count;
    }
}

static void af_editor_add_shape(AfEditorState *editor, AfNodeType type, AfAffine2 transform,
                                AfVec2 bounds, AfColor color, float radius, int32_t layer) {
    AfSceneNode *node = NULL;
    if (editor->render_scene.count >= editor->render_scene.capacity) {
        return;
    }
    node = &editor->render_scene.nodes[editor->render_scene.count++];
    *node = (AfSceneNode){.id = AF_EDITOR_UI_NODE_ID_BASE + editor->render_scene.count,
                          .parent_id = AF_INVALID_NODE_ID,
                          .type = type,
                          .bounds = bounds,
                          .resolved_bounds = bounds,
                          .fill = color,
                          .opacity = 1.0F,
                          .corner_radius = radius,
                          .layer_order = layer,
                          .visible = true,
                          .world_transform = transform,
                          .world_opacity = 1.0F,
                          .world_visible = true};
}

static void af_editor_add_rect(AfEditorState *editor, float x, float y, float width, float height,
                               AfColor color, int32_t layer) {
    af_editor_add_shape(editor, AF_NODE_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                        (AfVec2){width, height}, color, 0.0F, layer);
}

static void af_editor_add_line(AfEditorState *editor, AfVec2 start, AfVec2 end, float thickness,
                               AfColor color, int32_t layer) {
    const float x = end.x - start.x;
    const float y = end.y - start.y;
    const float length = sqrtf(x * x + y * y);
    const float cosine = length > 0.0F ? x / length : 1.0F;
    const float sine = length > 0.0F ? y / length : 0.0F;
    af_editor_add_shape(editor, AF_NODE_RECTANGLE,
                        (AfAffine2){.m00 = cosine,
                                    .m01 = -sine,
                                    .m10 = sine,
                                    .m11 = cosine,
                                    .tx = start.x + sine * thickness * 0.5F,
                                    .ty = start.y - cosine * thickness * 0.5F},
                        (AfVec2){length, thickness}, color, 0.0F, layer);
}

static const uint8_t *af_editor_glyph(char character) {
    static const uint8_t letters[26][5] = {
        {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU}, {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U},
        {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U}, {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU},
        {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}, {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U},
        {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU}, {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU},
        {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U}, {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U},
        {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U}, {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U},
        {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}, {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU},
        {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU}, {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U},
        {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU}, {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U},
        {0x46U, 0x49U, 0x49U, 0x49U, 0x31U}, {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U},
        {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU}, {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU},
        {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU}, {0x63U, 0x14U, 0x08U, 0x14U, 0x63U},
        {0x07U, 0x08U, 0x70U, 0x08U, 0x07U}, {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}};
    static const uint8_t digits[10][5] = {
        {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}, {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U},
        {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}, {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U},
        {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}, {0x27U, 0x45U, 0x45U, 0x45U, 0x39U},
        {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}, {0x01U, 0x71U, 0x09U, 0x05U, 0x03U},
        {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}, {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}};
    static const uint8_t unknown[5] = {0x02U, 0x01U, 0x51U, 0x09U, 0x06U};
    static const uint8_t dash[5] = {0x08U, 0x08U, 0x08U, 0x08U, 0x08U};
    static const uint8_t dot[5] = {0x00U, 0x60U, 0x60U, 0x00U, 0x00U};
    static const uint8_t colon[5] = {0x00U, 0x36U, 0x36U, 0x00U, 0x00U};
    static const uint8_t slash[5] = {0x20U, 0x10U, 0x08U, 0x04U, 0x02U};
    static const uint8_t plus[5] = {0x08U, 0x08U, 0x3EU, 0x08U, 0x08U};
    static const uint8_t percent[5] = {0x63U, 0x13U, 0x08U, 0x64U, 0x63U};
    static const uint8_t equals[5] = {0x14U, 0x14U, 0x14U, 0x14U, 0x14U};
    static const uint8_t underscore[5] = {0x40U, 0x40U, 0x40U, 0x40U, 0x40U};
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (character >= 'A' && character <= 'Z') {
        return letters[(uint32_t)(character - 'A')];
    }
    if (character >= '0' && character <= '9') {
        return digits[(uint32_t)(character - '0')];
    }
    switch (character) {
    case ' ':
        return NULL;
    case '-':
        return dash;
    case '.':
    case ',':
        return dot;
    case ':':
        return colon;
    case '/':
        return slash;
    case '+':
        return plus;
    case '%':
        return percent;
    case '=':
        return equals;
    case '_':
        return underscore;
    default:
        return unknown;
    }
}

static void af_editor_add_bitmap_text(AfEditorState *editor, float x, float y, const char *text,
                                      float scale, float maximum_width, AfColor color,
                                      int32_t layer) {
    float cursor = x;
    if (text == NULL || scale <= 0.0F || maximum_width <= 0.0F) {
        return;
    }
    for (size_t index = 0U; text[index] != '\0' && cursor + 5.0F * scale <= x + maximum_width;
         ++index) {
        const uint8_t *columns = af_editor_glyph(text[index]);
        if (columns != NULL) {
            uint8_t rows[7] = {0};
            for (uint32_t row = 0U; row < 7U; ++row) {
                for (uint32_t column = 0U; column < 5U; ++column) {
                    if ((columns[column] & (1U << row)) != 0U) {
                        rows[row] |= (uint8_t)(1U << column);
                    }
                }
            }
            for (;;) {
                uint32_t best_area = 0U;
                uint32_t best_row = 0U;
                uint32_t best_column = 0U;
                uint32_t best_width = 0U;
                uint32_t best_height = 0U;
                for (uint32_t row = 0U; row < 7U; ++row) {
                    for (uint32_t first = 0U; first < 5U; ++first) {
                        uint8_t mask = 0U;
                        if ((rows[row] & (1U << first)) == 0U) {
                            continue;
                        }
                        for (uint32_t last = first; last < 5U; ++last) {
                            if ((rows[row] & (1U << last)) == 0U) {
                                break;
                            }
                            mask |= (uint8_t)(1U << last);
                            for (uint32_t bottom = row; bottom < 7U; ++bottom) {
                                const uint32_t area = (last - first + 1U) * (bottom - row + 1U);
                                if ((rows[bottom] & mask) != mask) {
                                    break;
                                }
                                if (area > best_area) {
                                    best_area = area;
                                    best_row = row;
                                    best_column = first;
                                    best_width = last - first + 1U;
                                    best_height = bottom - row + 1U;
                                }
                            }
                        }
                    }
                }
                if (best_area == 0U) {
                    break;
                }
                af_editor_add_rect(editor, cursor + (float)best_column * scale,
                                   y + (float)best_row * scale, (float)best_width * scale,
                                   (float)best_height * scale, color, layer);
                const uint8_t clear_mask = (uint8_t)(((1U << best_width) - 1U) << best_column);
                for (uint32_t row = best_row; row < best_row + best_height; ++row) {
                    rows[row] &= (uint8_t)~clear_mask;
                }
            }
        }
        cursor += 6.0F * scale;
    }
}

static void af_editor_finish_text_path(AfEditorState *editor, uint32_t command_start, AfColor color,
                                       int32_t layer) {
    const uint32_t command_count = editor->render_path_data.command_count - command_start;
    AfSceneNode *node = NULL;
    AfPath *path = NULL;
    uint64_t generation = 0U;
    if (command_count == 0U || command_count > AF_MAX_PATH_COMMANDS_PER_PATH ||
        editor->render_scene.count >= editor->render_scene.capacity ||
        editor->render_path_data.path_count >= editor->render_path_data.path_capacity) {
        editor->render_path_data.command_count = command_start;
        return;
    }
    node = &editor->render_scene.nodes[editor->render_scene.count++];
    *node = (AfSceneNode){.id = AF_EDITOR_UI_NODE_ID_BASE + editor->render_scene.count,
                          .parent_id = AF_INVALID_NODE_ID,
                          .type = AF_NODE_PATH,
                          .bounds = {1.0F, 1.0F},
                          .resolved_bounds = {1.0F, 1.0F},
                          .fill = color,
                          .opacity = 1.0F,
                          .layer_order = layer,
                          .visible = true,
                          .world_transform = {.m00 = 1.0F, .m11 = 1.0F},
                          .world_opacity = 1.0F,
                          .world_visible = true};
    generation = af_hash_bytes(&editor->render_path_data.commands[command_start],
                               (size_t)command_count * sizeof(AfPathCommand));
    path = &editor->render_path_data.paths[editor->render_path_data.path_count++];
    *path = (AfPath){.id = node->id,
                     .node_id = node->id,
                     .command_start = command_start,
                     .command_count = command_count,
                     .fill_rule = AF_PATH_FILL_NON_ZERO,
                     .geometry_generation = generation == 0U ? 1U : generation};
}

static void af_editor_add_text(AfEditorState *editor, float x, float y, const char *text,
                               float scale, float maximum_width, AfColor color, int32_t layer) {
    AfTextLayout layout = {.glyphs = editor->text_glyphs,
                           .glyph_capacity = AF_EDITOR_TEXT_GLYPH_CAPACITY,
                           .lines = editor->text_lines,
                           .line_capacity = AF_EDITOR_TEXT_LINE_CAPACITY};
    const size_t byte_count = text == NULL ? 0U : strlen(text);
    float font_size = ceilf(8.0F + scale * 4.0F);
    AfTextRequest request = {.utf8 = text,
                             .byte_count = (uint32_t)byte_count,
                             .font_ids = {AF_EDITOR_TEXT_FONT_ID},
                             .font_count = 1U,
                             .font_size = font_size,
                             .maximum_width = maximum_width,
                             .line_spacing = 1.0F,
                             .maximum_lines = 1U,
                             .direction = AF_TEXT_DIRECTION_LTR,
                             .alignment = AF_TEXT_ALIGN_START,
                             .wrap = AF_TEXT_WRAP_NONE,
                             .kerning = true,
                             .ligatures = true,
                             .truncate = true};
    uint32_t command_start = editor->render_path_data.command_count;
    uint32_t batch_glyph_count = 0U;
    if (editor->text_engine == NULL || editor->text_glyphs == NULL || editor->text_lines == NULL ||
        byte_count == 0U || byte_count > UINT32_MAX || scale <= 0.0F || maximum_width <= 0.0F) {
        af_editor_add_bitmap_text(editor, x, y, text, scale, maximum_width, color, layer);
        return;
    }
    for (;;) {
        if (af_text_shape(editor->text_engine, &request, &layout) != AF_RESULT_SUCCESS) {
            af_editor_add_bitmap_text(editor, x, y, text, scale, maximum_width, color, layer);
            return;
        }
        if (layout.bounds.x <= maximum_width || font_size <= 6.0F) {
            break;
        }
        {
            float fitted_size = floorf(font_size * maximum_width / layout.bounds.x);
            if (fitted_size >= font_size) {
                fitted_size = font_size - 1.0F;
            }
            font_size = fmaxf(6.0F, fitted_size);
            request.font_size = font_size;
        }
    }
    for (uint32_t index = 0U; index < layout.glyph_count; ++index) {
        const AfGlyphPlacement *glyph = &layout.glyphs[index];
        const float glyph_width =
            fmaxf(fabsf(glyph->advance.x), glyph->bearing.x + glyph->bitmap_size.x);
        uint32_t written = 0U;
        uint32_t batch_commands = editor->render_path_data.command_count - command_start;
        uint32_t available = AF_MAX_PATH_COMMANDS_PER_PATH - batch_commands;
        AfResult result = AF_RESULT_SUCCESS;
        if (glyph->position.x < 0.0F || glyph->position.x + glyph_width > maximum_width + 0.25F) {
            break;
        }
        if (batch_glyph_count >= AF_EDITOR_TEXT_GLYPHS_PER_PATH) {
            af_editor_finish_text_path(editor, command_start, color, layer);
            command_start = editor->render_path_data.command_count;
            batch_glyph_count = 0U;
            available = AF_MAX_PATH_COMMANDS_PER_PATH;
        }
        if (available >
            editor->render_path_data.command_capacity - editor->render_path_data.command_count) {
            available =
                editor->render_path_data.command_capacity - editor->render_path_data.command_count;
        }
        if (available == 0U) {
            break;
        }
        result = af_text_glyph_outline(
            editor->text_engine, glyph->font_id, glyph->glyph_id, font_size,
            (AfVec2){x + glyph->position.x, y + glyph->position.y},
            &editor->render_path_data.commands[editor->render_path_data.command_count], available,
            &written);
        if (result == AF_RESULT_LIMIT_EXCEEDED && batch_commands != 0U) {
            af_editor_finish_text_path(editor, command_start, color, layer);
            command_start = editor->render_path_data.command_count;
            batch_glyph_count = 0U;
            available =
                editor->render_path_data.command_capacity - editor->render_path_data.command_count;
            if (available > AF_MAX_PATH_COMMANDS_PER_PATH) {
                available = AF_MAX_PATH_COMMANDS_PER_PATH;
            }
            result = available == 0U
                         ? AF_RESULT_LIMIT_EXCEEDED
                         : af_text_glyph_outline(
                               editor->text_engine, glyph->font_id, glyph->glyph_id, font_size,
                               (AfVec2){x + glyph->position.x, y + glyph->position.y},
                               &editor->render_path_data
                                    .commands[editor->render_path_data.command_count],
                               available, &written);
        }
        if (result != AF_RESULT_SUCCESS) {
            break;
        }
        editor->render_path_data.command_count += written;
        if (written != 0U) {
            ++batch_glyph_count;
        }
    }
    af_editor_finish_text_path(editor, command_start, color, layer);
}

static const AfEditorContextMenuInfo *af_editor_context_info(AfEditorContextMenu menu) {
    static const AfEditorContextMenuInfo menus[] = {
        {0},
        {.title = "SCENE",
         .items = {"DUPLICATE", "RENAME", "UNPARENT", "DELETE"},
         .item_count = 4U},
        {.title = "CANVAS",
         .items = {"ADD RECTANGLE", "ADD ROUNDED RECT", "ADD ELLIPSE", "ADD PATH", "TOGGLE GRID",
                   "TOGGLE SNAP"},
         .item_count = 6U},
        {.title = "INSPECTOR",
         .items = {"RESET TRANSFORM", "RESET APPEARANCE", "BRING FORWARD", "SEND BACKWARD",
                   "DUPLICATE", "DELETE"},
         .item_count = 6U},
        {.title = "TIMELINE",
         .items = {"ADD KEYFRAME", "DELETE KEYFRAMES", "COPY KEYFRAMES", "PASTE KEYFRAMES",
                   "TOGGLE CURVES", "NEW CLIP"},
         .item_count = 6U},
        {.title = "TOOLS",
         .items = {"SAVE PROJECT", "COMPILE PACKAGE", "TOGGLE GRID", "TOGGLE SNAP", "UNDO", "REDO"},
         .item_count = 6U},
        {.title = "RECTANGLE",
         .items = {"DUPLICATE", "RENAME", "MAKE ROUNDED", "FIT TO CANVAS", "TOGGLE PARTICLES",
                   "BRING FORWARD", "DELETE"},
         .item_count = 7U},
        {.title = "ROUNDED RECTANGLE",
         .items = {"DUPLICATE", "RENAME", "INCREASE RADIUS", "MAKE RECTANGLE", "TOGGLE PARTICLES",
                   "BRING FORWARD", "DELETE"},
         .item_count = 7U},
        {.title = "ELLIPSE",
         .items = {"DUPLICATE", "RENAME", "MAKE CIRCLE", "FIT TO CANVAS", "TOGGLE PARTICLES",
                   "BRING FORWARD", "DELETE"},
         .item_count = 7U},
        {.title = "VECTOR PATH",
         .items = {"DUPLICATE", "RENAME", "TOGGLE FILL RULE", "EDIT POINTS", "TOGGLE PARTICLES",
                   "REVERSE CONTOUR", "DELETE"},
         .item_count = 7U},
        {.title = "GROUP",
         .items = {"DUPLICATE", "RENAME", "ADD RECT CHILD", "UNPARENT", "TOGGLE PARTICLES",
                   "DELETE"},
         .item_count = 6U}};
    return menu > AF_EDITOR_CONTEXT_NONE && menu <= AF_EDITOR_CONTEXT_GROUP ? &menus[menu] : NULL;
}

static AfEditorContextMenu af_editor_context_for_node(const AfSceneNode *node) {
    if (node == NULL) {
        return AF_EDITOR_CONTEXT_CANVAS;
    }
    switch (node->type) {
    case AF_NODE_RECTANGLE:
        return AF_EDITOR_CONTEXT_RECTANGLE;
    case AF_NODE_ROUNDED_RECTANGLE:
        return AF_EDITOR_CONTEXT_ROUNDED_RECTANGLE;
    case AF_NODE_ELLIPSE:
        return AF_EDITOR_CONTEXT_ELLIPSE;
    case AF_NODE_PATH:
        return AF_EDITOR_CONTEXT_PATH;
    case AF_NODE_GROUP:
        return AF_EDITOR_CONTEXT_GROUP;
    default:
        return AF_EDITOR_CONTEXT_CANVAS;
    }
}

static void af_editor_layout(const AfEditorState *editor, int width, int height, float *canvas_x,
                             float *canvas_y, float *canvas_width, float *canvas_height) {
    *canvas_x = editor->left_width;
    *canvas_y = AF_EDITOR_TOOLBAR_HEIGHT;
    *canvas_width = (float)width - editor->left_width - editor->right_width;
    *canvas_height = (float)height - AF_EDITOR_TOOLBAR_HEIGHT - AF_EDITOR_STATUS_HEIGHT -
                     editor->timeline_height;
}

static float af_editor_timeline_top(const AfEditorState *editor, int height) {
    return (float)height - AF_EDITOR_STATUS_HEIGHT - editor->timeline_height;
}

static float af_editor_timeline_ruler_x(const AfEditorState *editor) {
    return editor->left_width + AF_EDITOR_TIMELINE_HEADER_WIDTH;
}

static float af_editor_time_to_x(const AfEditorState *editor, float time) {
    return af_editor_timeline_ruler_x(editor) +
           (time - editor->timeline_pan) * editor->timeline_zoom;
}

static float af_editor_x_to_time(const AfEditorState *editor, float x) {
    return editor->timeline_pan + (x - af_editor_timeline_ruler_x(editor)) / editor->timeline_zoom;
}

static const AfProjectTrack *af_editor_selected_track(const AfEditorState *editor) {
    uint32_t index = 0U;
    for (index = 0U; index < editor->project.track_count; ++index) {
        const AfProjectTrack *track = &editor->project.tracks[index];
        if (track->clip_id == editor->selected_clip_id && track->node_id == editor->selected_id &&
            track->property == editor->selected_property) {
            return track;
        }
    }
    return NULL;
}

static bool af_editor_keyframe_selected(const AfEditorState *editor, uint64_t id) {
    uint32_t index = 0U;
    for (index = 0U; index < editor->selected_keyframe_count; ++index) {
        if (editor->selected_keyframes[index] == id) {
            return true;
        }
    }
    return false;
}

static AfColor af_editor_property_color(AfPropertyType property) {
    static const AfColor colors[] = {{0.3F, 0.72F, 1.0F, 1.0F},  {1.0F, 0.56F, 0.3F, 1.0F},
                                     {0.45F, 0.9F, 0.46F, 1.0F}, {0.78F, 0.58F, 1.0F, 1.0F},
                                     {1.0F, 0.35F, 0.55F, 1.0F}, {0.96F, 0.8F, 0.28F, 1.0F}};
    return property <= AF_PROPERTY_CORNER_RADIUS ? colors[property]
                                                 : (AfColor){1.0F, 1.0F, 1.0F, 1.0F};
}

static const char *af_editor_property_name(AfPropertyType property) {
    static const char *const names[] = {"POSITION", "ROTATION", "SCALE",
                                        "OPACITY",  "FILL",     "RADIUS"};
    return property <= AF_PROPERTY_CORNER_RADIUS ? names[property] : "UNKNOWN";
}

static uint32_t af_editor_track_row(const AfEditorState *editor, uint64_t track_id) {
    uint32_t index = 0U;
    uint32_t row = 0U;
    for (index = 0U; index < editor->project.track_count; ++index) {
        if (editor->project.tracks[index].clip_id != editor->selected_clip_id) {
            continue;
        }
        if (editor->project.tracks[index].id == track_id) {
            return row;
        }
        ++row;
    }
    return UINT32_MAX;
}

static float af_editor_curve_value(const AfProjectTrack *track, const AfProjectKeyframe *key) {
    float value = key->keyframe.value[0];
    switch (track->property) {
    case AF_PROPERTY_POSITION:
        value /= 1280.0F;
        break;
    case AF_PROPERTY_ROTATION:
        value = 0.5F + value / 6.28318530718F;
        break;
    case AF_PROPERTY_SCALE:
        value *= 0.5F;
        break;
    case AF_PROPERTY_CORNER_RADIUS:
        value *= 0.01F;
        break;
    case AF_PROPERTY_OPACITY:
    case AF_PROPERTY_FILL_COLOR:
    default:
        break;
    }
    return af_editor_clamp(value, 0.0F, 1.0F);
}

static bool af_editor_curve_key_less(const AfProject *project, uint32_t left, uint32_t right) {
    const AfProjectKeyframe *left_key = &project->keyframes[left];
    const AfProjectKeyframe *right_key = &project->keyframes[right];
    return left_key->keyframe.time < right_key->keyframe.time ||
           (left_key->keyframe.time == right_key->keyframe.time && left_key->id < right_key->id);
}

static void af_editor_curve_sift(const AfProject *project, uint32_t *order, uint32_t root,
                                 uint32_t count) {
    for (;;) {
        uint32_t child = root * 2U + 1U;
        uint32_t largest = root;
        if (child < count && af_editor_curve_key_less(project, order[largest], order[child])) {
            largest = child;
        }
        if (child + 1U < count &&
            af_editor_curve_key_less(project, order[largest], order[child + 1U])) {
            largest = child + 1U;
        }
        if (largest == root) {
            return;
        }
        {
            const uint32_t swap = order[root];
            order[root] = order[largest];
            order[largest] = swap;
        }
        root = largest;
    }
}

static void af_editor_curve_sort(const AfProject *project, uint32_t *order, uint32_t count) {
    uint32_t index = count / 2U;
    while (index > 0U) {
        --index;
        af_editor_curve_sift(project, order, index, count);
    }
    index = count;
    while (index > 1U) {
        const uint32_t swap = order[0];
        order[0] = order[index - 1U];
        order[index - 1U] = swap;
        --index;
        af_editor_curve_sift(project, order, 0U, index);
    }
}

static float af_editor_curve_bezier(float first, float second, float parameter) {
    const float inverse = 1.0F - parameter;
    return 3.0F * inverse * inverse * parameter * first +
           3.0F * inverse * parameter * parameter * second + parameter * parameter * parameter;
}

static void af_editor_render_curve(AfEditorState *editor, int width, int height) {
    const AfProjectTrack *track = af_editor_selected_track(editor);
    const float top = af_editor_timeline_top(editor, height) + AF_EDITOR_TIMELINE_RULER_HEIGHT;
    const float bottom = (float)height - AF_EDITOR_STATUS_HEIGHT - 12.0F;
    const float left = af_editor_timeline_ruler_x(editor);
    const float right = (float)width - editor->right_width;
    uint32_t curve_count = 0U;
    uint32_t index = 0U;
    if (track == NULL || bottom <= top || editor->curve_order == NULL) {
        return;
    }
    for (index = 0U; index <= 4U; ++index) {
        const float y = top + (bottom - top) * (float)index * 0.25F;
        af_editor_add_rect(editor, left, y, right - left, 1.0F, (AfColor){0.2F, 0.22F, 0.27F, 1.0F},
                           100115);
    }
    for (index = 0U; index < editor->project.keyframe_count; ++index) {
        if (editor->project.keyframes[index].track_id == track->id) {
            editor->curve_order[curve_count++] = index;
        }
    }
    af_editor_curve_sort(&editor->project, editor->curve_order, curve_count);
    for (index = 1U; index < curve_count; ++index) {
        const AfProjectKeyframe *previous =
            &editor->project.keyframes[editor->curve_order[index - 1U]];
        const AfProjectKeyframe *key = &editor->project.keyframes[editor->curve_order[index]];
        const float first_value = af_editor_curve_value(track, previous);
        const float second_value = af_editor_curve_value(track, key);
        AfVec2 start = {af_editor_time_to_x(editor, previous->keyframe.time),
                        bottom - first_value * (bottom - top)};
        if (previous->keyframe.interpolation == AF_INTERPOLATION_STEP) {
            const AfVec2 corner = {af_editor_time_to_x(editor, key->keyframe.time), start.y};
            const AfVec2 end = {corner.x, bottom - second_value * (bottom - top)};
            af_editor_add_line(editor, start, corner, 2.0F,
                               af_editor_property_color(track->property), 100130);
            af_editor_add_line(editor, corner, end, 2.0F, af_editor_property_color(track->property),
                               100130);
        } else if (previous->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER) {
            for (uint32_t sample = 1U; sample <= 16U; ++sample) {
                const float parameter = (float)sample / 16.0F;
                const float time_progress = af_editor_curve_bezier(
                    previous->keyframe.easing[0], previous->keyframe.easing[2], parameter);
                const float value_progress = af_editor_curve_bezier(
                    previous->keyframe.easing[1], previous->keyframe.easing[3], parameter);
                const AfVec2 end = {
                    af_editor_time_to_x(editor, previous->keyframe.time +
                                                    (key->keyframe.time - previous->keyframe.time) *
                                                        time_progress),
                    bottom - (first_value + (second_value - first_value) * value_progress) *
                                 (bottom - top)};
                af_editor_add_line(editor, start, end, 2.0F,
                                   af_editor_property_color(track->property), 100130);
                start = end;
            }
        } else {
            const AfVec2 end = {af_editor_time_to_x(editor, key->keyframe.time),
                                bottom - second_value * (bottom - top)};
            af_editor_add_line(editor, start, end, 2.0F, af_editor_property_color(track->property),
                               100130);
        }
    }
    if (editor->selected_keyframe_count == 1U) {
        const AfProjectKeyframe *key =
            af_project_find_keyframe_const(&editor->project, editor->selected_keyframes[0]);
        if (key != NULL && key->track_id == track->id &&
            key->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER) {
            const AfVec2 origin = {left + 30.0F, bottom - 30.0F};
            const AfVec2 out = {origin.x + key->keyframe.easing[0] * 100.0F,
                                origin.y - key->keyframe.easing[1] * 80.0F};
            const AfVec2 in = {right - 130.0F + key->keyframe.easing[2] * 100.0F,
                               bottom - key->keyframe.easing[3] * 80.0F};
            const AfVec2 in_origin = {right - 30.0F, top + 30.0F};
            af_editor_add_line(editor, origin, out, 2.0F, (AfColor){0.96F, 0.58F, 0.25F, 1.0F},
                               100140);
            af_editor_add_line(editor, in_origin, in, 2.0F, (AfColor){0.35F, 0.74F, 1.0F, 1.0F},
                               100140);
            af_editor_add_rect(editor, out.x - 5.0F, out.y - 5.0F, 10.0F, 10.0F,
                               (AfColor){0.96F, 0.58F, 0.25F, 1.0F}, 100150);
            af_editor_add_rect(editor, in.x - 5.0F, in.y - 5.0F, 10.0F, 10.0F,
                               (AfColor){0.35F, 0.74F, 1.0F, 1.0F}, 100150);
        }
    }
}

static void af_editor_render_timeline(AfEditorState *editor, int width, int height) {
    const float top = af_editor_timeline_top(editor, height);
    const float center_left = editor->left_width;
    const float center_right = (float)width - editor->right_width;
    const float ruler_x = af_editor_timeline_ruler_x(editor);
    const float bottom = (float)height - AF_EDITOR_STATUS_HEIGHT;
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    float tick_time = 0.0F;
    uint32_t index = 0U;
    uint32_t row = 0U;
    af_editor_add_rect(editor, center_left, top, center_right - center_left,
                       editor->timeline_height, (AfColor){0.075F, 0.082F, 0.102F, 1.0F}, 100000);
    af_editor_add_rect(editor, center_left, top - 2.0F, center_right - center_left, 4.0F,
                       (AfColor){0.27F, 0.29F, 0.35F, 1.0F}, 100110);
    af_editor_add_rect(editor, center_left + 10.0F, top + 9.0F, 26.0F, 26.0F,
                       editor->preview_playing ? (AfColor){0.3F, 0.82F, 0.46F, 1.0F}
                                               : (AfColor){0.24F, 0.27F, 0.33F, 1.0F},
                       100120);
    af_editor_add_shape(
        editor, AF_NODE_ELLIPSE,
        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = center_left + 47.0F, .ty = top + 14.0F},
        (AfVec2){16.0F, 16.0F}, (AfColor){0.9F, 0.34F, 0.3F, 1.0F}, 0.0F, 100120);
    af_editor_add_rect(editor, center_left + 73.0F, top + 9.0F, 34.0F, 26.0F,
                       editor->curve_visible ? (AfColor){0.48F, 0.4F, 0.82F, 1.0F}
                                             : (AfColor){0.24F, 0.27F, 0.33F, 1.0F},
                       100120);
    af_editor_add_text(editor, center_left + 16.0F, top + 19.0F, "PLAY", 0.5F, 20.0F,
                       (AfColor){0.9F, 0.92F, 0.96F, 1.0F}, 100135);
    af_editor_add_text(editor, center_left + 47.0F, top + 19.0F, "STOP", 0.5F, 20.0F,
                       (AfColor){0.9F, 0.92F, 0.96F, 1.0F}, 100135);
    af_editor_add_text(editor, center_left + 76.0F, top + 19.0F, "CURVE", 0.45F, 31.0F,
                       (AfColor){0.9F, 0.92F, 0.96F, 1.0F}, 100135);
    for (index = 0U; index < 6U; ++index) {
        const float x = center_left + 116.0F + (float)index * 20.0F;
        static const char *const property_icons[] = {"P", "R", "S", "O", "F", "C"};
        af_editor_add_rect(editor, x, top + 12.0F, 15.0F, 18.0F,
                           index == (uint32_t)editor->selected_property
                               ? af_editor_property_color((AfPropertyType)index)
                               : (AfColor){0.22F, 0.24F, 0.29F, 1.0F},
                           100120);
        af_editor_add_text(editor, x + 5.0F, top + 18.0F, property_icons[index], 0.85F, 6.0F,
                           (AfColor){0.94F, 0.95F, 0.98F, 1.0F}, 100135);
    }
    af_editor_add_text(editor, center_left + 242.0F, top + 18.0F, "TIMELINE", 0.9F, 58.0F,
                       (AfColor){0.62F, 0.65F, 0.72F, 1.0F}, 100130);
    for (index = 0U; index < editor->project.clip_count && index < 8U; ++index) {
        const float x = center_right - 74.0F * (float)(index + 1U);
        af_editor_add_rect(editor, x, top + 10.0F, 68.0F, 22.0F,
                           editor->project.clips[index].id == editor->selected_clip_id
                               ? (AfColor){0.2F, 0.42F, 0.68F, 1.0F}
                               : (AfColor){0.18F, 0.2F, 0.25F, 1.0F},
                           100120);
        af_editor_add_text(editor, x + 4.0F, top + 17.0F, editor->project.clips[index].name, 0.75F,
                           60.0F, (AfColor){0.92F, 0.94F, 0.98F, 1.0F}, 100135);
    }
    af_editor_add_rect(editor, center_left, top + 40.0F, center_right - center_left, 1.0F,
                       (AfColor){0.25F, 0.27F, 0.32F, 1.0F}, 100115);
    if (clip == NULL) {
        return;
    }
    tick_time = ceilf(editor->timeline_pan * 2.0F) * 0.5F;
    while (af_editor_time_to_x(editor, tick_time) < center_right && tick_time <= clip->duration) {
        const float x = af_editor_time_to_x(editor, tick_time);
        if (x >= ruler_x) {
            char tick_label[16];
            af_editor_add_rect(editor, x, top + 40.0F, 1.0F, 11.0F,
                               (AfColor){0.48F, 0.5F, 0.58F, 1.0F}, 100120);
            (void)snprintf(tick_label, sizeof(tick_label), "%.1f", (double)tick_time);
            af_editor_add_text(editor, x + 3.0F, top + 42.0F, tick_label, 0.72F, 24.0F,
                               (AfColor){0.58F, 0.61F, 0.69F, 1.0F}, 100125);
        }
        tick_time += 0.5F;
    }
    if (clip->loop) {
        const float loop_start = af_editor_time_to_x(editor, clip->loop_start);
        const float loop_end = af_editor_time_to_x(editor, clip->loop_end);
        af_editor_add_rect(editor, loop_start, top + 43.0F, loop_end - loop_start, 5.0F,
                           (AfColor){0.25F, 0.7F, 0.42F, 1.0F}, 100125);
        af_editor_add_rect(editor, loop_start - 3.0F, top + 40.0F, 6.0F, 15.0F,
                           (AfColor){0.35F, 0.9F, 0.5F, 1.0F}, 100130);
        af_editor_add_rect(editor, loop_end - 3.0F, top + 40.0F, 6.0F, 15.0F,
                           (AfColor){0.35F, 0.9F, 0.5F, 1.0F}, 100130);
    }
    if (editor->curve_visible) {
        af_editor_render_curve(editor, width, height);
    } else {
        for (index = 0U; index < editor->project.track_count; ++index) {
            const AfProjectTrack *track = &editor->project.tracks[index];
            float y = 0.0F;
            uint32_t key_index = 0U;
            if (track->clip_id != editor->selected_clip_id) {
                continue;
            }
            y = top + AF_EDITOR_TIMELINE_RULER_HEIGHT +
                (float)row * AF_EDITOR_TIMELINE_TRACK_HEIGHT;
            af_editor_add_rect(editor, center_left, y, AF_EDITOR_TIMELINE_HEADER_WIDTH,
                               AF_EDITOR_TIMELINE_TRACK_HEIGHT - 1.0F,
                               track->node_id == editor->selected_id &&
                                       track->property == editor->selected_property
                                   ? (AfColor){0.16F, 0.27F, 0.42F, 1.0F}
                                   : (AfColor){0.11F, 0.12F, 0.15F, 1.0F},
                               100115);
            af_editor_add_rect(editor, center_left + 10.0F, y + 7.0F, 5.0F, 12.0F,
                               af_editor_property_color(track->property), 100125);
            af_editor_add_text(editor, center_left + 23.0F, y + 8.0F,
                               af_editor_property_name(track->property), 1.0F,
                               AF_EDITOR_TIMELINE_HEADER_WIDTH - 30.0F,
                               (AfColor){0.72F, 0.75F, 0.82F, 1.0F}, 100125);
            af_editor_add_rect(editor, ruler_x, y, center_right - ruler_x,
                               AF_EDITOR_TIMELINE_TRACK_HEIGHT - 1.0F,
                               (row & 1U) != 0U ? (AfColor){0.095F, 0.105F, 0.13F, 1.0F}
                                                : (AfColor){0.105F, 0.115F, 0.14F, 1.0F},
                               100110);
            for (key_index = 0U; key_index < editor->project.keyframe_count; ++key_index) {
                const AfProjectKeyframe *key = &editor->project.keyframes[key_index];
                const float key_x = af_editor_time_to_x(editor, key->keyframe.time);
                AfAffine2 diamond = {0};
                if (key->track_id != track->id || key_x < ruler_x - 8.0F ||
                    key_x > center_right + 8.0F) {
                    continue;
                }
                diamond = (AfAffine2){.m00 = 0.70710678F,
                                      .m01 = -0.70710678F,
                                      .m10 = 0.70710678F,
                                      .m11 = 0.70710678F,
                                      .tx = key_x,
                                      .ty = y + 7.0F};
                af_editor_add_shape(editor, AF_NODE_RECTANGLE, diamond, (AfVec2){10.0F, 10.0F},
                                    af_editor_keyframe_selected(editor, key->id)
                                        ? (AfColor){1.0F, 0.82F, 0.26F, 1.0F}
                                        : af_editor_property_color(track->property),
                                    0.0F, 100135);
            }
            ++row;
        }
    }
    {
        const float playhead_x = af_editor_time_to_x(editor, editor->playhead);
        if (playhead_x >= ruler_x && playhead_x <= center_right) {
            af_editor_add_rect(editor, playhead_x - 1.0F, top + 40.0F, 2.0F, bottom - top - 40.0F,
                               (AfColor){1.0F, 0.32F, 0.28F, 1.0F}, 100170);
            af_editor_add_shape(
                editor, AF_NODE_ELLIPSE,
                (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = playhead_x - 5.0F, .ty = top + 37.0F},
                (AfVec2){10.0F, 10.0F}, (AfColor){1.0F, 0.32F, 0.28F, 1.0F}, 0.0F, 100180);
        }
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_KEY_BOX) {
        const float x = fminf(editor->key_box_start.x, editor->key_box_current.x);
        const float y = fminf(editor->key_box_start.y, editor->key_box_current.y);
        const float box_width = fabsf(editor->key_box_current.x - editor->key_box_start.x);
        const float box_height = fabsf(editor->key_box_current.y - editor->key_box_start.y);
        af_editor_add_rect(editor, x, y, box_width, 1.0F, (AfColor){0.35F, 0.72F, 1.0F, 1.0F},
                           100190);
        af_editor_add_rect(editor, x, y + box_height, box_width, 1.0F,
                           (AfColor){0.35F, 0.72F, 1.0F, 1.0F}, 100190);
        af_editor_add_rect(editor, x, y, 1.0F, box_height, (AfColor){0.35F, 0.72F, 1.0F, 1.0F},
                           100190);
        af_editor_add_rect(editor, x + box_width, y, 1.0F, box_height,
                           (AfColor){0.35F, 0.72F, 1.0F, 1.0F}, 100190);
    }
}

static AfAffine2 af_editor_camera(const AfEditorState *editor, int width, int height) {
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    (void)canvas_width;
    (void)canvas_height;
    return (AfAffine2){.m00 = editor->zoom,
                       .m11 = editor->zoom,
                       .tx = canvas_x + editor->pan.x,
                       .ty = canvas_y + editor->pan.y};
}

static AfVec2 af_editor_screen_to_project(const AfEditorState *editor, int width, int height,
                                          AfVec2 screen) {
    AfAffine2 inverse = {0};
    if (!af_affine_inverse(af_editor_camera(editor, width, height), &inverse)) {
        return (AfVec2){0};
    }
    return af_affine_transform_point(inverse, screen);
}

static void af_editor_update_preview(AfEditorState *editor) {
    editor->preview_scene.count = editor->project.scene.count;
    editor->preview_scene.paths = &editor->project.path_data;
    editor->preview_scene.styles = &editor->project.style_data;
    editor->preview_scene.layouts = &editor->project.layout_data;
    editor->preview_scene.particles = &editor->project.particle_data;
    if (editor->project.scene.count != 0U) {
        memcpy(editor->preview_nodes, editor->project.scene.nodes,
               (size_t)editor->project.scene.count * sizeof(*editor->preview_nodes));
    }
    if (af_project_find_clip_const(&editor->project, editor->selected_clip_id) != NULL) {
        (void)af_project_animation_apply(&editor->project, editor->selected_clip_id,
                                         editor->playhead, &editor->preview_scene);
    } else {
        (void)af_scene_evaluate_world(&editor->preview_scene);
    }
}

static bool af_editor_point_in_node(const AfSceneNode *node, AfVec2 project_point) {
    AfAffine2 inverse = {0};
    AfVec2 point = {0};
    if (node == NULL || node->type == AF_NODE_GROUP || !node->world_visible ||
        !af_affine_inverse(node->world_transform, &inverse)) {
        return false;
    }
    point = af_affine_transform_point(inverse, project_point);
    if (point.x < 0.0F || point.y < 0.0F || point.x > node->bounds.x || point.y > node->bounds.y) {
        return false;
    }
    if (node->type == AF_NODE_ELLIPSE && node->bounds.x > 0.0F && node->bounds.y > 0.0F) {
        const float x = point.x / node->bounds.x * 2.0F - 1.0F;
        const float y = point.y / node->bounds.y * 2.0F - 1.0F;
        return x * x + y * y <= 1.0F;
    }
    return true;
}

static uint64_t af_editor_hit_test(AfEditorState *editor, AfVec2 project_point) {
    uint64_t hit = 0U;
    int32_t hit_layer = INT32_MIN;
    uint32_t index = 0U;
    for (index = 0U; index < editor->preview_scene.count; ++index) {
        const AfSceneNode *node = &editor->preview_scene.nodes[index];
        bool contains = false;
        if (node->type == AF_NODE_PATH) {
            AfAffine2 inverse = {0};
            const AfPath *path = af_path_find_node_const(&editor->project.path_data, node->id);
            contains = path != NULL && editor->path_scratch != NULL &&
                       af_affine_inverse(node->world_transform, &inverse) &&
                       af_path_hit_test(&editor->project.path_data, path,
                                        af_affine_transform_point(inverse, project_point), 0.25F,
                                        editor->path_scratch);
        } else {
            contains = af_editor_point_in_node(node, project_point);
        }
        if (contains && node->layer_order >= hit_layer) {
            hit = node->id;
            hit_layer = node->layer_order;
        }
    }
    return hit;
}

static void af_editor_render_grid(AfEditorState *editor, int width, int height, AfAffine2 camera) {
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    float spacing = editor->grid_step * editor->zoom;
    float x = 0.0F;
    float y = 0.0F;
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    if (!editor->grid_visible || spacing < 5.0F) {
        return;
    }
    x = camera.tx + floorf((canvas_x - camera.tx) / spacing) * spacing;
    for (; x < canvas_x + canvas_width; x += spacing) {
        af_editor_add_rect(editor, x, canvas_y, 1.0F, canvas_height,
                           (AfColor){0.19F, 0.21F, 0.25F, 1.0F}, -90000);
    }
    y = camera.ty + floorf((canvas_y - camera.ty) / spacing) * spacing;
    for (; y < canvas_y + canvas_height; y += spacing) {
        af_editor_add_rect(editor, canvas_x, y, canvas_width, 1.0F,
                           (AfColor){0.19F, 0.21F, 0.25F, 1.0F}, -90000);
    }
}

static void af_editor_render_hierarchy(AfEditorState *editor, int height) {
    uint32_t visible_rows = 0U;
    uint32_t row = 0U;
    const float top = AF_EDITOR_TOOLBAR_HEIGHT + 8.0F;
    af_editor_add_text(editor, 14.0F, 15.0F, "SCENE HIERARCHY", 1.5F, editor->left_width - 28.0F,
                       (AfColor){0.78F, 0.81F, 0.88F, 1.0F}, 100125);
    af_editor_add_text(editor, 14.0F, 30.0F, "HOVER HELP / ALT DETAILS / RIGHT CLICK ACTIONS", 0.6F,
                       editor->left_width - 28.0F, (AfColor){0.43F, 0.47F, 0.56F, 1.0F}, 100125);
    if ((float)height > top + AF_EDITOR_STATUS_HEIGHT) {
        visible_rows =
            (uint32_t)(((float)height - top - AF_EDITOR_STATUS_HEIGHT - editor->timeline_height) /
                       AF_EDITOR_ROW_HEIGHT);
    }
    for (row = 0U; row < visible_rows && editor->hierarchy_scroll + row < editor->hierarchy_count;
         ++row) {
        const uint32_t hierarchy_index = editor->hierarchy_scroll + row;
        const uint32_t node_index = editor->hierarchy_order[hierarchy_index];
        const AfSceneNode *node = &editor->project.scene.nodes[node_index];
        const float y = top + (float)row * AF_EDITOR_ROW_HEIGHT;
        const float indent = 12.0F + (float)editor->hierarchy_depth[hierarchy_index] * 14.0F;
        AfColor row_color = (row & 1U) != 0U ? (AfColor){0.105F, 0.115F, 0.14F, 1.0F}
                                             : (AfColor){0.12F, 0.13F, 0.155F, 1.0F};
        if (node->id == editor->selected_id) {
            row_color = (AfColor){0.16F, 0.34F, 0.58F, 1.0F};
        } else if (af_editor_selection_contains(editor, node->id)) {
            row_color = (AfColor){0.12F, 0.25F, 0.4F, 1.0F};
        }
        af_editor_add_rect(editor, 6.0F, y, editor->left_width - 12.0F, AF_EDITOR_ROW_HEIGHT - 2.0F,
                           row_color, 100110);
        af_editor_add_shape(
            editor,
            node->type == AF_NODE_GROUP || node->type == AF_NODE_PATH ? AF_NODE_ROUNDED_RECTANGLE
                                                                      : node->type,
            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = indent, .ty = y + 6.0F},
            (AfVec2){15.0F, 15.0F},
            node->type == AF_NODE_GROUP ? (AfColor){0.72F, 0.58F, 0.22F, 1.0F} : node->fill,
            node->type == AF_NODE_GROUP ? 3.0F : node->corner_radius, 100120);
        af_editor_add_text(editor, indent + 23.0F, y + 10.0F, editor->project.names[node_index],
                           1.2F, editor->left_width - indent - 34.0F,
                           (AfColor){0.78F, 0.81F, 0.88F, 1.0F}, 100125);
    }
}

static void af_editor_render_toolbar(AfEditorState *editor) {
    const float start = editor->left_width + 14.0F;
    const AfColor button = {0.18F, 0.2F, 0.25F, 1.0F};
    af_editor_add_rect(editor, start, 6.0F, 34.0F, 32.0F, button, 100110);
    af_editor_add_rect(editor, start + 8.0F, 13.0F, 18.0F, 17.0F,
                       (AfColor){0.22F, 0.58F, 0.96F, 1.0F}, 100120);
    af_editor_add_rect(editor, start + 42.0F, 6.0F, 34.0F, 32.0F, button, 100110);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = start + 50.0F, .ty = 13.0F},
                        (AfVec2){18.0F, 17.0F}, (AfColor){0.94F, 0.35F, 0.27F, 1.0F}, 5.0F, 100120);
    af_editor_add_rect(editor, start + 84.0F, 6.0F, 34.0F, 32.0F, button, 100110);
    af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = start + 92.0F, .ty = 13.0F},
                        (AfVec2){18.0F, 17.0F}, (AfColor){0.32F, 0.86F, 0.54F, 1.0F}, 0.0F, 100120);
    af_editor_add_rect(editor, start + 126.0F, 6.0F, 34.0F, 32.0F, button, 100110);
    af_editor_add_line(editor, (AfVec2){start + 133.0F, 28.0F}, (AfVec2){start + 141.0F, 14.0F},
                       3.0F, (AfColor){0.78F, 0.42F, 0.96F, 1.0F}, 100120);
    af_editor_add_line(editor, (AfVec2){start + 141.0F, 14.0F}, (AfVec2){start + 153.0F, 26.0F},
                       3.0F, (AfColor){0.78F, 0.42F, 0.96F, 1.0F}, 100120);
    af_editor_add_text(editor, start + 5.0F, 30.0F, "RECT", 0.65F, 26.0F,
                       (AfColor){0.8F, 0.82F, 0.88F, 1.0F}, 100135);
    af_editor_add_text(editor, start + 45.0F, 30.0F, "ROUND", 0.58F, 28.0F,
                       (AfColor){0.8F, 0.82F, 0.88F, 1.0F}, 100135);
    af_editor_add_text(editor, start + 89.0F, 30.0F, "OVAL", 0.65F, 26.0F,
                       (AfColor){0.8F, 0.82F, 0.88F, 1.0F}, 100135);
    af_editor_add_text(editor, start + 130.0F, 30.0F, "PATH", 0.65F, 27.0F,
                       (AfColor){0.8F, 0.82F, 0.88F, 1.0F}, 100135);
    af_editor_add_rect(editor, start + 172.0F, 12.0F, 48.0F, 5.0F,
                       editor->grid_visible ? (AfColor){0.35F, 0.7F, 0.98F, 1.0F}
                                            : (AfColor){0.4F, 0.42F, 0.48F, 1.0F},
                       100120);
    af_editor_add_rect(editor, start + 172.0F, 25.0F, 48.0F, 5.0F,
                       editor->snapping ? (AfColor){0.96F, 0.55F, 0.25F, 1.0F}
                                        : (AfColor){0.4F, 0.42F, 0.48F, 1.0F},
                       100120);
    af_editor_add_text(editor, start + 225.0F, 10.0F, "GRID", 0.8F, 34.0F,
                       (AfColor){0.69F, 0.72F, 0.8F, 1.0F}, 100130);
    af_editor_add_text(editor, start + 225.0F, 24.0F, "SNAP", 0.8F, 34.0F,
                       (AfColor){0.69F, 0.72F, 0.8F, 1.0F}, 100130);
}

static void af_editor_render_color_picker(AfEditorState *editor, float x,
                                          const AfSceneNode *selected) {
    const AfColor color = selected == NULL ? (AfColor){0.5F, 0.5F, 0.5F, 1.0F} : selected->fill;
    float hue = editor->color_hue;
    float saturation = 0.0F;
    float value = 0.0F;
    char first_values[32];
    char second_values[32];
    af_editor_color_hsv(color, &hue, &saturation, &value);
    if (saturation > 0.0001F) {
        editor->color_hue = hue;
    } else {
        hue = editor->color_hue;
    }
    af_editor_add_text(editor, x, 58.0F, "COLOR PICKER", 0.9F, 100.0F,
                       (AfColor){0.61F, 0.65F, 0.73F, 1.0F}, 100125);
    af_editor_add_text(editor, x + 106.0F, 59.0F, "SV / HUE / ALPHA", 0.55F, 118.0F,
                       (AfColor){0.46F, 0.5F, 0.58F, 1.0F}, 100125);
    for (uint32_t row = 0U; row < 8U; ++row) {
        for (uint32_t column = 0U; column < 12U; ++column) {
            const float cell_width = AF_EDITOR_COLOR_SV_WIDTH / 12.0F;
            const float cell_height = AF_EDITOR_COLOR_SV_HEIGHT / 8.0F;
            const float cell_saturation = ((float)column + 0.5F) / 12.0F;
            const float cell_value = 1.0F - ((float)row + 0.5F) / 8.0F;
            af_editor_add_rect(editor, x + (float)column * cell_width,
                               AF_EDITOR_COLOR_SV_Y + (float)row * cell_height, cell_width + 0.4F,
                               cell_height + 0.4F,
                               af_editor_hsv_color(hue, cell_saturation, cell_value, 1.0F), 100120);
        }
    }
    for (uint32_t row = 0U; row < 12U; ++row) {
        const float segment_height = AF_EDITOR_COLOR_SV_HEIGHT / 12.0F;
        af_editor_add_rect(
            editor, x + AF_EDITOR_COLOR_HUE_X, AF_EDITOR_COLOR_SV_Y + (float)row * segment_height,
            AF_EDITOR_COLOR_HUE_WIDTH, segment_height + 0.4F,
            af_editor_hsv_color(((float)row + 0.5F) / 12.0F, 1.0F, 1.0F, 1.0F), 100120);
    }
    for (uint32_t column = 0U; column < 12U; ++column) {
        const float segment_width = (AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH) / 12.0F;
        const AfColor checker = (column & 1U) != 0U ? (AfColor){0.18F, 0.19F, 0.22F, 1.0F}
                                                    : (AfColor){0.38F, 0.4F, 0.44F, 1.0F};
        AfColor alpha_color = color;
        alpha_color.a = ((float)column + 0.5F) / 12.0F;
        af_editor_add_rect(editor, x + (float)column * segment_width, AF_EDITOR_COLOR_ALPHA_Y,
                           segment_width + 0.4F, AF_EDITOR_COLOR_ALPHA_HEIGHT, checker, 100120);
        af_editor_add_rect(editor, x + (float)column * segment_width, AF_EDITOR_COLOR_ALPHA_Y,
                           segment_width + 0.4F, AF_EDITOR_COLOR_ALPHA_HEIGHT, alpha_color, 100125);
    }
    {
        const float marker_x = x + saturation * AF_EDITOR_COLOR_SV_WIDTH;
        const float marker_y = AF_EDITOR_COLOR_SV_Y + (1.0F - value) * AF_EDITOR_COLOR_SV_HEIGHT;
        const AfColor marker = value > 0.55F ? (AfColor){0.03F, 0.04F, 0.06F, 1.0F}
                                             : (AfColor){0.96F, 0.97F, 1.0F, 1.0F};
        af_editor_add_rect(editor, marker_x - 5.0F, marker_y - 5.0F, 10.0F, 2.0F, marker, 100140);
        af_editor_add_rect(editor, marker_x - 5.0F, marker_y + 3.0F, 10.0F, 2.0F, marker, 100140);
        af_editor_add_rect(editor, marker_x - 5.0F, marker_y - 5.0F, 2.0F, 10.0F, marker, 100140);
        af_editor_add_rect(editor, marker_x + 3.0F, marker_y - 5.0F, 2.0F, 10.0F, marker, 100140);
    }
    af_editor_add_rect(editor, x + AF_EDITOR_COLOR_HUE_X - 2.0F,
                       AF_EDITOR_COLOR_SV_Y + hue * AF_EDITOR_COLOR_SV_HEIGHT - 1.0F,
                       AF_EDITOR_COLOR_HUE_WIDTH + 4.0F, 3.0F, (AfColor){0.96F, 0.97F, 1.0F, 1.0F},
                       100140);
    af_editor_add_rect(editor,
                       x + color.a * (AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH) - 1.0F,
                       AF_EDITOR_COLOR_ALPHA_Y - 2.0F, 3.0F, AF_EDITOR_COLOR_ALPHA_HEIGHT + 4.0F,
                       (AfColor){0.96F, 0.97F, 1.0F, 1.0F}, 100140);
    af_editor_add_rect(editor, x + 176.0F, AF_EDITOR_COLOR_SV_Y, 48.0F, 36.0F,
                       (AfColor){0.16F, 0.17F, 0.2F, 1.0F}, 100120);
    af_editor_add_rect(editor, x + 180.0F, AF_EDITOR_COLOR_SV_Y + 4.0F, 40.0F, 28.0F, color,
                       100130);
    (void)snprintf(first_values, sizeof(first_values), "R%03u G%03u",
                   (uint32_t)roundf(color.r * 255.0F), (uint32_t)roundf(color.g * 255.0F));
    (void)snprintf(second_values, sizeof(second_values), "B%03u A%03u",
                   (uint32_t)roundf(color.b * 255.0F), (uint32_t)roundf(color.a * 255.0F));
    af_editor_add_text(editor, x + 176.0F, 121.0F, first_values, 0.58F, 56.0F,
                       (AfColor){0.69F, 0.72F, 0.8F, 1.0F}, 100130);
    af_editor_add_text(editor, x + 176.0F, 140.0F, second_values, 0.58F, 56.0F,
                       (AfColor){0.69F, 0.72F, 0.8F, 1.0F}, 100130);
    af_editor_add_text(editor, x + 176.0F, 173.0F, "DRAG", 0.58F, 48.0F,
                       (AfColor){0.46F, 0.5F, 0.58F, 1.0F}, 100130);
}

static void af_editor_render_inspector(AfEditorState *editor, int width) {
    const float x = (float)width - editor->right_width + 18.0F;
    const AfSceneNode *selected = af_scene_find_const(&editor->project.scene, editor->selected_id);
    const char *selected_name = af_project_name(&editor->project, editor->selected_id);
    char selection_name[64];
    uint32_t index = 0U;
    if (editor->selected_count > 1U) {
        (void)snprintf(selection_name, sizeof(selection_name), "%u OBJECTS SELECTED",
                       editor->selected_count);
        selected_name = selection_name;
    }
    af_editor_add_text(editor, x, 15.0F, "INSPECTOR", 1.5F, editor->right_width - 36.0F,
                       (AfColor){0.78F, 0.81F, 0.88F, 1.0F}, 100125);
    af_editor_add_text(editor, x, 35.0F, selected_name == NULL ? "NO SELECTION" : selected_name,
                       1.0F, editor->right_width - 36.0F, (AfColor){0.5F, 0.55F, 0.65F, 1.0F},
                       100125);
    af_editor_render_color_picker(editor, x, selected);
    for (index = 0U; index < 4U; ++index) {
        const float y =
            AF_EDITOR_INSPECTOR_SCALAR_Y + (float)index * AF_EDITOR_INSPECTOR_SCALAR_STEP;
        char value[64];
        af_editor_add_rect(editor, x, y, 30.0F, 26.0F, (AfColor){0.24F, 0.26F, 0.32F, 1.0F},
                           100120);
        af_editor_add_rect(editor, x + 40.0F, y, 30.0F, 26.0F, (AfColor){0.24F, 0.26F, 0.32F, 1.0F},
                           100120);
        af_editor_add_rect(editor, x + 8.0F, y + 12.0F, 14.0F, 2.0F,
                           (AfColor){0.75F, 0.78F, 0.84F, 1.0F}, 100130);
        af_editor_add_rect(editor, x + 48.0F, y + 12.0F, 14.0F, 2.0F,
                           (AfColor){0.75F, 0.78F, 0.84F, 1.0F}, 100130);
        af_editor_add_rect(editor, x + 54.0F, y + 6.0F, 2.0F, 14.0F,
                           (AfColor){0.75F, 0.78F, 0.84F, 1.0F}, 100130);
        if (selected == NULL) {
            (void)snprintf(value, sizeof(value), "%s",
                           index == 0U   ? "OPACITY"
                           : index == 1U ? "RADIUS"
                           : index == 2U ? "LAYER"
                                         : "ROTATION");
        } else if (index == 0U) {
            (void)snprintf(value, sizeof(value), "OPACITY %.0f%%",
                           (double)(selected->opacity * 100.0F));
        } else if (index == 1U) {
            (void)snprintf(value, sizeof(value), "RADIUS %.1f", (double)selected->corner_radius);
        } else if (index == 2U) {
            (void)snprintf(value, sizeof(value), "LAYER %d", selected->layer_order);
        } else {
            (void)snprintf(value, sizeof(value), "ROTATION %.2f",
                           (double)selected->local_transform.rotation);
        }
        af_editor_add_text(editor, x + 82.0F, y + 8.0F, value, 1.0F, editor->right_width - 118.0F,
                           (AfColor){0.7F, 0.73F, 0.81F, 1.0F}, 100130);
    }
    if (selected != NULL) {
        const AfStyle *style = af_style_find_node_const(&editor->project.style_data, selected->id);
        const AfParticleEmitter *emitter =
            af_project_find_node_emitter(&editor->project, selected->id);
        af_editor_add_rect(editor, x, AF_EDITOR_STYLE_BAR_Y, editor->right_width - 36.0F, 12.0F,
                           (AfColor){0.18F, 0.2F, 0.24F, 1.0F}, 100120);
        af_editor_add_rect(editor, x, AF_EDITOR_STYLE_BAR_Y,
                           (editor->right_width - 36.0F) * selected->opacity, 12.0F, selected->fill,
                           100130);
        for (index = 0U; index < 6U; ++index) {
            static const char *const style_labels[] = {"ON", "-W", "+W", "FILL", "CAP", "JOIN"};
            static const char *const layout_labels[] = {"ANCH", "WIDTH", "HEIGHT",
                                                        "FLOW", "ALIGN", "ASPECT"};
            AfColor button = {0.24F, 0.26F, 0.32F, 1.0F};
            if (style != NULL && ((index == 0U && style->stroke.enabled) ||
                                  (index == 3U && style->fill.type != AF_PAINT_SOLID))) {
                button = (AfColor){0.22F, 0.58F, 0.96F, 1.0F};
            }
            af_editor_add_rect(editor, x + (float)index * 38.0F, AF_EDITOR_STYLE_ROW_Y, 32.0F,
                               28.0F, button, 100120);
            af_editor_add_rect(editor, x + (float)index * 38.0F, AF_EDITOR_LAYOUT_ROW_Y, 32.0F,
                               28.0F, (AfColor){0.28F, 0.3F, 0.37F, 1.0F}, 100120);
            af_editor_add_text(editor, x + (float)index * 38.0F + 4.0F,
                               AF_EDITOR_STYLE_ROW_Y + 9.0F, style_labels[index], 0.65F, 26.0F,
                               (AfColor){0.82F, 0.84F, 0.9F, 1.0F}, 100135);
            af_editor_add_text(editor, x + (float)index * 38.0F + 3.0F,
                               AF_EDITOR_LAYOUT_ROW_Y + 9.0F, layout_labels[index], 0.58F, 27.0F,
                               (AfColor){0.82F, 0.84F, 0.9F, 1.0F}, 100135);
        }
        af_editor_add_text(editor, x, AF_EDITOR_STYLE_LABEL_Y, "STYLE", 0.8F, 48.0F,
                           (AfColor){0.55F, 0.59F, 0.68F, 1.0F}, 100125);
        af_editor_add_text(editor, x + 62.0F, AF_EDITOR_STYLE_LABEL_Y, "LAYOUT CONTROLS BELOW",
                           0.55F, editor->right_width - 98.0F, (AfColor){0.45F, 0.49F, 0.58F, 1.0F},
                           100125);
        af_editor_add_text(editor, x, AF_EDITOR_PARTICLE_LABEL_Y, "PARTICLE EMITTER", 0.85F,
                           editor->right_width - 36.0F,
                           emitter == NULL ? (AfColor){0.45F, 0.49F, 0.58F, 1.0F}
                                           : (AfColor){0.45F, 0.78F, 1.0F, 1.0F},
                           100125);
        if (emitter == NULL) {
            af_editor_add_text(editor, x, AF_EDITOR_PARTICLE_LABEL_Y + 21.0F,
                               "RIGHT CLICK > TOGGLE PARTICLES", 0.62F, editor->right_width - 36.0F,
                               (AfColor){0.52F, 0.56F, 0.64F, 1.0F}, 100125);
        } else {
            static const char *const type_names[] = {"POINT", "LINE", "RECT", "CIRCLE", "PATH"};
            static const char *const output_names[] = {"BILLBOARD", "UI",   "FLIPBOOK", "TRAIL",
                                                       "RIBBON",    "MESH", "DECAL",    "HEAT",
                                                       "REFRACT",   "SOFT", "LIGHT"};
            char labels[AF_EDITOR_PARTICLE_ROW_COUNT][64];
            const AfVkVfxMetrics metrics = af_vk_renderer_vfx_metrics(editor->renderer);
            (void)snprintf(labels[0], sizeof(labels[0]), "RATE %.0f / S",
                           (double)emitter->spawn_rate);
            (void)snprintf(labels[1], sizeof(labels[1]), "LIFE %.2f - %.2f",
                           (double)emitter->lifetime_min, (double)emitter->lifetime_max);
            (void)snprintf(labels[2], sizeof(labels[2]), "CAPACITY %u", emitter->capacity);
            (void)snprintf(labels[3], sizeof(labels[3]), "SHAPE %s", type_names[emitter->type]);
            (void)snprintf(labels[4], sizeof(labels[4]), "OUTPUT %s",
                           output_names[emitter->output]);
            (void)snprintf(labels[5], sizeof(labels[5]), "TURBULENCE %.0f",
                           (double)emitter->turbulence_strength);
            (void)snprintf(labels[6], sizeof(labels[6]), "RADIAL %.0f",
                           (double)emitter->radial_force);
            (void)snprintf(labels[7], sizeof(labels[7]), "VORTEX %.0f",
                           (double)emitter->vortex_force);
            (void)snprintf(labels[8], sizeof(labels[8]), "PLANE COLLISION %s",
                           emitter->collision_plane_enabled ? "ON" : "OFF");
            (void)snprintf(labels[9], sizeof(labels[9]), "EVENT BURST %u",
                           emitter->event_burst_count);
            for (uint32_t row = 0U; row < AF_EDITOR_PARTICLE_ROW_COUNT; ++row) {
                const float row_y =
                    AF_EDITOR_PARTICLE_ROW_Y + (float)row * AF_EDITOR_PARTICLE_ROW_STEP;
                af_editor_add_rect(editor, x, row_y, 28.0F, 24.0F,
                                   (AfColor){0.24F, 0.26F, 0.32F, 1.0F}, 100120);
                af_editor_add_rect(editor, x + 34.0F, row_y, 28.0F, 24.0F,
                                   (AfColor){0.24F, 0.26F, 0.32F, 1.0F}, 100120);
                af_editor_add_text(editor, x + 9.0F, row_y + 6.0F, "-", 0.8F, 10.0F,
                                   (AfColor){0.85F, 0.87F, 0.92F, 1.0F}, 100135);
                af_editor_add_text(editor, x + 42.0F, row_y + 6.0F, "+", 0.8F, 12.0F,
                                   (AfColor){0.85F, 0.87F, 0.92F, 1.0F}, 100135);
                af_editor_add_text(editor, x + 72.0F, row_y + 7.0F, labels[row], 0.72F,
                                   editor->right_width - 108.0F,
                                   (AfColor){0.7F, 0.74F, 0.82F, 1.0F}, 100130);
            }
            {
                char timing[96];
                (void)snprintf(timing, sizeof(timing), "VFX CPU %.2f MS  GPU %.2f MS  LOD %.0f%%",
                               (double)metrics.cpu_milliseconds, (double)metrics.gpu_milliseconds,
                               (double)(metrics.lod_factor * 100.0F));
                af_editor_add_text(editor, x,
                                   AF_EDITOR_PARTICLE_ROW_Y +
                                       AF_EDITOR_PARTICLE_ROW_COUNT * AF_EDITOR_PARTICLE_ROW_STEP +
                                       2.0F,
                                   timing, 0.58F, editor->right_width - 36.0F,
                                   (AfColor){0.48F, 0.66F, 0.76F, 1.0F}, 100130);
            }
        }
    }
}

static AfVec2 af_editor_context_origin(const AfEditorState *editor, int width, int height) {
    const AfEditorContextMenuInfo *info = af_editor_context_info(editor->context_menu);
    const float menu_height =
        info == NULL ? 0.0F
                     : AF_EDITOR_CONTEXT_HEADER_HEIGHT +
                           (float)info->item_count * AF_EDITOR_CONTEXT_ITEM_HEIGHT + 8.0F;
    return (AfVec2){
        af_editor_clamp(editor->context_position.x, 6.0F,
                        (float)width - AF_EDITOR_CONTEXT_WIDTH - 6.0F),
        af_editor_clamp(editor->context_position.y, 6.0F, (float)height - menu_height - 6.0F)};
}

static void af_editor_render_context_icon(AfEditorState *editor, const char *label, float x,
                                          float y) {
    const AfColor color = {0.56F, 0.78F, 1.0F, 1.0F};
    const int32_t layer = 200030;
    if (strcmp(label, "ADD RECTANGLE") == 0 || strcmp(label, "ADD RECT CHILD") == 0 ||
        strcmp(label, "MAKE RECTANGLE") == 0) {
        af_editor_add_rect(editor, x, y + 2.0F, 14.0F, 10.0F, color, layer);
    } else if (strcmp(label, "ADD ROUNDED RECT") == 0 || strcmp(label, "MAKE ROUNDED") == 0 ||
               strstr(label, "RADIUS") != NULL) {
        af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y + 2.0F},
                            (AfVec2){14.0F, 10.0F}, color, 3.0F, layer);
    } else if (strcmp(label, "ADD ELLIPSE") == 0 || strcmp(label, "MAKE CIRCLE") == 0) {
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 1.0F, .ty = y},
                            (AfVec2){12.0F, 12.0F}, color, 0.0F, layer);
    } else if (strstr(label, "FIT") != NULL) {
        af_editor_add_line(editor, (AfVec2){x, y + 4.0F}, (AfVec2){x, y}, 2.0F, color, layer);
        af_editor_add_line(editor, (AfVec2){x, y}, (AfVec2){x + 4.0F, y}, 2.0F, color, layer);
        af_editor_add_line(editor, (AfVec2){x + 10.0F, y}, (AfVec2){x + 14.0F, y}, 2.0F, color,
                           layer);
        af_editor_add_line(editor, (AfVec2){x + 14.0F, y}, (AfVec2){x + 14.0F, y + 4.0F}, 2.0F,
                           color, layer);
        af_editor_add_line(editor, (AfVec2){x, y + 9.0F}, (AfVec2){x, y + 13.0F}, 2.0F, color,
                           layer);
        af_editor_add_line(editor, (AfVec2){x, y + 13.0F}, (AfVec2){x + 4.0F, y + 13.0F}, 2.0F,
                           color, layer);
        af_editor_add_line(editor, (AfVec2){x + 10.0F, y + 13.0F}, (AfVec2){x + 14.0F, y + 13.0F},
                           2.0F, color, layer);
        af_editor_add_line(editor, (AfVec2){x + 14.0F, y + 13.0F}, (AfVec2){x + 14.0F, y + 9.0F},
                           2.0F, color, layer);
    } else if (strstr(label, "FILL RULE") != NULL) {
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                            (AfVec2){13.0F, 13.0F}, color, 0.0F, layer);
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 4.0F, .ty = y + 4.0F},
                            (AfVec2){5.0F, 5.0F}, (AfColor){0.12F, 0.18F, 0.26F, 1.0F}, 0.0F,
                            layer + 1);
    } else if (strstr(label, "EDIT POINTS") != NULL) {
        af_editor_add_line(editor, (AfVec2){x + 1.0F, y + 11.0F}, (AfVec2){x + 7.0F, y + 2.0F},
                           1.5F, color, layer);
        af_editor_add_line(editor, (AfVec2){x + 7.0F, y + 2.0F}, (AfVec2){x + 13.0F, y + 9.0F},
                           1.5F, color, layer);
        af_editor_add_rect(editor, x, y + 10.0F, 3.0F, 3.0F, color, layer + 1);
        af_editor_add_rect(editor, x + 6.0F, y + 1.0F, 3.0F, 3.0F, color, layer + 1);
        af_editor_add_rect(editor, x + 12.0F, y + 8.0F, 3.0F, 3.0F, color, layer + 1);
    } else if (strstr(label, "REVERSE") != NULL) {
        af_editor_add_line(editor, (AfVec2){x + 13.0F, y + 3.0F}, (AfVec2){x + 3.0F, y + 3.0F},
                           2.0F, color, layer);
        af_editor_add_line(editor, (AfVec2){x + 3.0F, y + 3.0F}, (AfVec2){x + 7.0F, y}, 2.0F, color,
                           layer);
        af_editor_add_line(editor, (AfVec2){x + 3.0F, y + 3.0F}, (AfVec2){x + 7.0F, y + 6.0F}, 2.0F,
                           color, layer);
    } else if (strstr(label, "PATH") != NULL || strstr(label, "CURVE") != NULL) {
        af_editor_add_line(editor, (AfVec2){x, y + 11.0F}, (AfVec2){x + 5.0F, y + 3.0F}, 2.0F,
                           color, layer);
        af_editor_add_line(editor, (AfVec2){x + 5.0F, y + 3.0F}, (AfVec2){x + 14.0F, y + 9.0F},
                           2.0F, color, layer);
    } else if (strstr(label, "GRID") != NULL) {
        for (uint32_t index = 0U; index < 3U; ++index) {
            af_editor_add_rect(editor, x + (float)index * 5.0F, y, 1.0F, 13.0F, color, layer);
            af_editor_add_rect(editor, x, y + (float)index * 5.0F, 13.0F, 1.0F, color, layer);
        }
    } else if (strstr(label, "SNAP") != NULL) {
        af_editor_add_rect(editor, x, y, 3.0F, 10.0F, color, layer);
        af_editor_add_rect(editor, x + 10.0F, y, 3.0F, 10.0F, color, layer);
        af_editor_add_rect(editor, x + 2.0F, y + 8.0F, 9.0F, 3.0F, color, layer);
    } else if (strstr(label, "DELETE") != NULL) {
        const AfColor danger = {1.0F, 0.45F, 0.42F, 1.0F};
        af_editor_add_line(editor, (AfVec2){x + 1.0F, y + 1.0F}, (AfVec2){x + 12.0F, y + 12.0F},
                           2.0F, danger, layer);
        af_editor_add_line(editor, (AfVec2){x + 12.0F, y + 1.0F}, (AfVec2){x + 1.0F, y + 12.0F},
                           2.0F, danger, layer);
    } else if (strstr(label, "DUPLICATE") != NULL || strstr(label, "COPY") != NULL) {
        af_editor_add_rect(editor, x, y, 10.0F, 9.0F, (AfColor){0.34F, 0.48F, 0.66F, 1.0F}, layer);
        af_editor_add_rect(editor, x + 4.0F, y + 4.0F, 10.0F, 9.0F, color, layer + 1);
    } else if (strstr(label, "RENAME") != NULL) {
        af_editor_add_line(editor, (AfVec2){x + 1.0F, y + 12.0F}, (AfVec2){x + 12.0F, y + 1.0F},
                           3.0F, color, layer);
    } else if (strstr(label, "UNPARENT") != NULL) {
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y + 3.0F},
                            (AfVec2){7.0F, 7.0F}, color, 0.0F, layer);
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 7.0F, .ty = y + 3.0F},
                            (AfVec2){7.0F, 7.0F}, color, 0.0F, layer);
        af_editor_add_line(editor, (AfVec2){x, y + 13.0F}, (AfVec2){x + 14.0F, y}, 2.0F,
                           (AfColor){1.0F, 0.45F, 0.42F, 1.0F}, layer + 1);
    } else if (strstr(label, "KEYFRAME") != NULL) {
        af_editor_add_shape(editor, AF_NODE_RECTANGLE,
                            (AfAffine2){.m00 = 0.70710678F,
                                        .m01 = -0.70710678F,
                                        .m10 = 0.70710678F,
                                        .m11 = 0.70710678F,
                                        .tx = x + 7.0F,
                                        .ty = y},
                            (AfVec2){9.0F, 9.0F}, (AfColor){1.0F, 0.76F, 0.28F, 1.0F}, 0.0F, layer);
    } else if (strstr(label, "FORWARD") != NULL || strstr(label, "BACKWARD") != NULL) {
        const bool forward = strstr(label, "FORWARD") != NULL;
        const float tip = forward ? y : y + 13.0F;
        const float base = forward ? y + 13.0F : y;
        af_editor_add_line(editor, (AfVec2){x + 7.0F, tip}, (AfVec2){x + 1.0F, base}, 2.0F, color,
                           layer);
        af_editor_add_line(editor, (AfVec2){x + 7.0F, tip}, (AfVec2){x + 13.0F, base}, 2.0F, color,
                           layer);
    } else if (strstr(label, "UNDO") != NULL || strstr(label, "REDO") != NULL) {
        const float direction = strstr(label, "UNDO") != NULL ? -1.0F : 1.0F;
        const float center = x + 7.0F;
        af_editor_add_line(editor, (AfVec2){center - direction * 5.0F, y + 6.0F},
                           (AfVec2){center + direction * 5.0F, y + 2.0F}, 2.0F, color, layer);
        af_editor_add_line(editor, (AfVec2){center - direction * 5.0F, y + 6.0F},
                           (AfVec2){center + direction * 2.0F, y + 12.0F}, 2.0F, color, layer);
    } else if (strstr(label, "SAVE") != NULL || strstr(label, "PASTE") != NULL) {
        af_editor_add_rect(editor, x + 1.0F, y, 12.0F, 13.0F, color, layer);
        af_editor_add_rect(editor, x + 4.0F, y + 2.0F, 6.0F, 4.0F,
                           (AfColor){0.12F, 0.18F, 0.26F, 1.0F}, layer + 1);
    } else {
        af_editor_add_rect(editor, x + 5.0F, y, 3.0F, 13.0F, color, layer);
        af_editor_add_rect(editor, x, y + 5.0F, 13.0F, 3.0F, color, layer);
    }
}

static void af_editor_render_context_menu(AfEditorState *editor, int width, int height) {
    const AfEditorContextMenuInfo *info = af_editor_context_info(editor->context_menu);
    const AfVec2 origin = af_editor_context_origin(editor, width, height);
    if (info == NULL) {
        return;
    }
    const float menu_height = AF_EDITOR_CONTEXT_HEADER_HEIGHT +
                              (float)info->item_count * AF_EDITOR_CONTEXT_ITEM_HEIGHT + 8.0F;
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE,
        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = origin.x + 5.0F, .ty = origin.y + 6.0F},
        (AfVec2){AF_EDITOR_CONTEXT_WIDTH, menu_height}, (AfColor){0.0F, 0.0F, 0.0F, 0.45F}, 7.0F,
        200000);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = origin.x, .ty = origin.y},
                        (AfVec2){AF_EDITOR_CONTEXT_WIDTH, menu_height},
                        (AfColor){0.105F, 0.115F, 0.14F, 1.0F}, 7.0F, 200010);
    af_editor_add_rect(editor, origin.x, origin.y + AF_EDITOR_CONTEXT_HEADER_HEIGHT - 1.0F,
                       AF_EDITOR_CONTEXT_WIDTH, 1.0F, (AfColor){0.28F, 0.3F, 0.36F, 1.0F}, 200020);
    af_editor_add_text(editor, origin.x + 13.0F, origin.y + 10.0F, info->title, 1.2F,
                       AF_EDITOR_CONTEXT_WIDTH - 26.0F, (AfColor){0.57F, 0.72F, 0.94F, 1.0F},
                       200030);
    for (uint32_t index = 0U; index < info->item_count; ++index) {
        const float item_y = origin.y + AF_EDITOR_CONTEXT_HEADER_HEIGHT +
                             (float)index * AF_EDITOR_CONTEXT_ITEM_HEIGHT + 4.0F;
        const bool hovered = editor->mouse_position.x >= origin.x &&
                             editor->mouse_position.x <= origin.x + AF_EDITOR_CONTEXT_WIDTH &&
                             editor->mouse_position.y >= item_y &&
                             editor->mouse_position.y < item_y + AF_EDITOR_CONTEXT_ITEM_HEIGHT;
        if (hovered) {
            af_editor_add_rect(editor, origin.x + 4.0F, item_y, AF_EDITOR_CONTEXT_WIDTH - 8.0F,
                               AF_EDITOR_CONTEXT_ITEM_HEIGHT - 1.0F,
                               (AfColor){0.18F, 0.34F, 0.56F, 1.0F}, 200020);
        }
        af_editor_render_context_icon(editor, info->items[index], origin.x + 11.0F, item_y + 7.0F);
        af_editor_add_text(editor, origin.x + 34.0F, item_y + 9.0F, info->items[index], 1.05F,
                           AF_EDITOR_CONTEXT_WIDTH - 45.0F, (AfColor){0.85F, 0.87F, 0.92F, 1.0F},
                           200035);
    }
}

static bool af_editor_point_in_rect(AfVec2 point, float x, float y, float width, float height) {
    return point.x >= x && point.x < x + width && point.y >= y && point.y < y + height;
}

static AfEditorTooltip af_editor_context_tooltip(const char *label, uint64_t id) {
    AfEditorTooltip tooltip = {.id = id,
                               .title = label,
                               .summary = "Run this context action.",
                               .details = "The action applies to the current editor context.",
                               .shortcut = "Right click opens context actions"};
    if (strstr(label, "DUPLICATE") != NULL) {
        tooltip.summary = "Create an independent copy of the selection.";
        tooltip.details = "Copies the object or selected keyframes and assigns stable new IDs.";
        tooltip.shortcut = "Shortcut: Ctrl+D";
    } else if (strstr(label, "RENAME") != NULL) {
        tooltip.summary = "Rename the selected scene object.";
        tooltip.details = "Starts inline name editing; Enter confirms and Escape cancels.";
        tooltip.shortcut = "Shortcut: F2";
    } else if (strstr(label, "DELETE") != NULL) {
        tooltip.summary = "Delete the selected item safely.";
        tooltip.details = "Deletes the current object or selected keys through undo history.";
        tooltip.shortcut = "Shortcut: Delete";
    } else if (strstr(label, "PARTICLES") != NULL) {
        tooltip.summary = "Attach or remove a GPU particle emitter.";
        tooltip.details = "Uses this object's shape as the emitter source with bounded GPU memory.";
        tooltip.shortcut = "Edit emitter values in the Inspector";
    } else if (strstr(label, "ADD KEYFRAME") != NULL) {
        tooltip.summary = "Key the selected property at the playhead.";
        tooltip.details =
            "Creates a key on the active clip and property track at the current time.";
        tooltip.shortcut = "Shortcut: K";
    } else if (strstr(label, "KEYFRAME") != NULL) {
        tooltip.summary = "Edit the currently selected animation keys.";
        tooltip.details = "The operation is bounded, undoable, and keeps stable keyframe IDs.";
        tooltip.shortcut = "Ctrl+C / Ctrl+V / Delete";
    } else if (strstr(label, "ADD RECTANGLE") != NULL || strstr(label, "RECT CHILD") != NULL) {
        tooltip.summary = "Create a new rectangle object.";
        tooltip.details = "Adds an editable rectangle at the canvas center or under this group.";
        tooltip.shortcut = "Shortcut: 1";
    } else if (strstr(label, "ADD ROUNDED") != NULL) {
        tooltip.summary = "Create a rounded rectangle object.";
        tooltip.details = "Adds a rectangle with an editable corner radius at the canvas center.";
        tooltip.shortcut = "Shortcut: 2";
    } else if (strstr(label, "ADD ELLIPSE") != NULL) {
        tooltip.summary = "Create a new ellipse object.";
        tooltip.details =
            "Adds an editable ellipse at the canvas center; equal bounds make a circle.";
        tooltip.shortcut = "Shortcut: 3";
    } else if (strstr(label, "ADD PATH") != NULL) {
        tooltip.summary = "Create a new vector path.";
        tooltip.details =
            "Adds an editable Bézier path with direct point and control-handle editing.";
        tooltip.shortcut = "Shortcut: 4";
    } else if (strstr(label, "GRID") != NULL) {
        tooltip.summary = "Show or hide the canvas grid.";
        tooltip.details = "The grid is a visual guide and is not included in runtime packages.";
        tooltip.shortcut = "Shortcut: G";
    } else if (strstr(label, "SNAP") != NULL) {
        tooltip.summary = "Toggle position and timeline snapping.";
        tooltip.details = "Rounds object moves and timeline edits to the active grid increments.";
        tooltip.shortcut = "Shortcut: N";
    } else if (strstr(label, "SAVE") != NULL) {
        tooltip.summary = "Save the editable Arcflow project.";
        tooltip.details =
            "Writes scene, animation, style, layout, path, and particle authoring data.";
        tooltip.shortcut = "Shortcut: Ctrl+S";
    } else if (strstr(label, "COMPILE") != NULL) {
        tooltip.summary = "Compile the project into a runtime package.";
        tooltip.details = "Validates authoring data and writes the bounded package used by Player.";
        tooltip.shortcut = "Shortcut: F5";
    } else if (strstr(label, "UNDO") != NULL) {
        tooltip.summary = "Undo the most recent editor command.";
        tooltip.details = "Restores the previous bounded command snapshot without changing IDs.";
        tooltip.shortcut = "Shortcut: Ctrl+Z";
    } else if (strstr(label, "REDO") != NULL) {
        tooltip.summary = "Redo the most recently undone command.";
        tooltip.details = "Reapplies the command at the current history position.";
        tooltip.shortcut = "Shortcut: Ctrl+Y";
    } else if (strstr(label, "FORWARD") != NULL || strstr(label, "BACKWARD") != NULL) {
        tooltip.summary = "Change the object's drawing order.";
        tooltip.details = "Moves the object one layer while preserving its scene hierarchy.";
        tooltip.shortcut = "Shortcuts: [ and ]";
    } else if (strstr(label, "UNPARENT") != NULL) {
        tooltip.summary = "Move the object out of its parent group.";
        tooltip.details = "Reparents to the scene root while rejecting cyclic hierarchies.";
        tooltip.shortcut = "Shortcut: U";
    } else if (strstr(label, "CURVE") != NULL) {
        tooltip.summary = "Show or hide the animation curve editor.";
        tooltip.details = "Displays easing curves and tangent handles for selected cubic keys.";
        tooltip.shortcut = "Shortcut: F6";
    } else if (strstr(label, "CLIP") != NULL) {
        tooltip.summary = "Create or manage an animation clip.";
        tooltip.details = "Clips contain bounded tracks, keys, duration, looping, and events.";
        tooltip.shortcut = "New clip: Alt+C";
    } else if (strstr(label, "RESET") != NULL) {
        tooltip.summary = "Restore default values for this section.";
        tooltip.details = "Creates one undoable command for the selected object's values.";
        tooltip.shortcut = "Undo available with Ctrl+Z";
    } else if (strstr(label, "PATH") != NULL || strstr(label, "POINTS") != NULL ||
               strstr(label, "CONTOUR") != NULL || strstr(label, "FILL RULE") != NULL) {
        tooltip.summary = "Modify this vector path.";
        tooltip.details = "Edits bounded path commands while invalidating only affected geometry.";
        tooltip.shortcut = "Path shortcuts: I, C, O, F, Shift+R";
    }
    return tooltip;
}

static AfEditorTooltip af_editor_find_tooltip(AfEditorState *editor, int width, int height) {
    const AfVec2 mouse = editor->mouse_position;
    const float panel_x = (float)width - editor->right_width + 18.0F;
    const float timeline_top = af_editor_timeline_top(editor, height);
    const float center_right = (float)width - editor->right_width;
    AfEditorTooltip tooltip = {0};
    if (editor->drag_mode != AF_EDITOR_DRAG_NONE || editor->rename_active) {
        return tooltip;
    }
    if (editor->context_menu != AF_EDITOR_CONTEXT_NONE) {
        const AfEditorContextMenuInfo *info = af_editor_context_info(editor->context_menu);
        const AfVec2 origin = af_editor_context_origin(editor, width, height);
        const float first_y = origin.y + AF_EDITOR_CONTEXT_HEADER_HEIGHT + 4.0F;
        if (info != NULL && mouse.x >= origin.x && mouse.x < origin.x + AF_EDITOR_CONTEXT_WIDTH &&
            mouse.y >= first_y) {
            const uint32_t item = (uint32_t)((mouse.y - first_y) / AF_EDITOR_CONTEXT_ITEM_HEIGHT);
            if (item < info->item_count) {
                return af_editor_context_tooltip(info->items[item],
                                                 UINT64_C(0x10000) +
                                                     (uint64_t)editor->context_menu * 16U + item);
            }
        }
        return tooltip;
    }
    if (fabsf(mouse.x - editor->left_width) <= 5.0F) {
        return (AfEditorTooltip){.id = 1U,
                                 .title = "Resize Scene Hierarchy",
                                 .summary = "Drag to change the left panel width.",
                                 .details =
                                     "The canvas and timeline resize around the docked panel.",
                                 .shortcut = "Drag the vertical divider"};
    }
    if (fabsf(mouse.x - center_right) <= 5.0F) {
        return (AfEditorTooltip){.id = 2U,
                                 .title = "Resize Inspector",
                                 .summary = "Drag to change the right panel width.",
                                 .details =
                                     "Inspector text and controls adapt to the available width.",
                                 .shortcut = "Drag the vertical divider"};
    }
    if (fabsf(mouse.y - timeline_top) <= 5.0F && mouse.x >= editor->left_width &&
        mouse.x < center_right) {
        return (AfEditorTooltip){.id = 3U,
                                 .title = "Resize Timeline",
                                 .summary = "Drag to change the timeline height.",
                                 .details =
                                     "Expands the dope sheet or curve editor without hiding docks.",
                                 .shortcut = "Drag the horizontal divider"};
    }
    if (mouse.y < AF_EDITOR_TOOLBAR_HEIGHT && mouse.x >= editor->left_width) {
        const float local_x = mouse.x - editor->left_width - 14.0F;
        static const AfEditorTooltip tools[] = {
            {.id = 10U,
             .title = "Rectangle Tool",
             .summary = "Create a rectangle at the canvas center.",
             .details = "The new object is selected immediately and every edit is undoable.",
             .shortcut = "Shortcut: 1"},
            {.id = 11U,
             .title = "Rounded Rectangle Tool",
             .summary = "Create a rectangle with rounded corners.",
             .details = "Change its radius later with the Inspector or object context menu.",
             .shortcut = "Shortcut: 2"},
            {.id = 12U,
             .title = "Ellipse Tool",
             .summary = "Create an ellipse at the canvas center.",
             .details = "Use equal width and height for a circle; its fill remains editable.",
             .shortcut = "Shortcut: 3"},
            {.id = 13U,
             .title = "Vector Path Tool",
             .summary = "Create an editable Bézier path.",
             .details = "Select it to edit anchors and controls or use its object context menu.",
             .shortcut = "Shortcut: 4"},
            {.id = 14U,
             .title = "Canvas Grid",
             .summary = "Show or hide the canvas grid.",
             .details = "The guide follows canvas zoom and is excluded from exported packages.",
             .shortcut = "Shortcut: G"},
            {.id = 15U,
             .title = "Snapping",
             .summary = "Enable or disable editing snap increments.",
             .details = "Affects object movement and timeline edits for predictable alignment.",
             .shortcut = "Shortcut: N"}};
        if (local_x >= 0.0F && local_x < 34.0F) {
            return tools[0];
        }
        if (local_x >= 42.0F && local_x < 76.0F) {
            return tools[1];
        }
        if (local_x >= 84.0F && local_x < 118.0F) {
            return tools[2];
        }
        if (local_x >= 126.0F && local_x < 160.0F) {
            return tools[3];
        }
        if (local_x >= 172.0F && local_x < 260.0F) {
            return mouse.y < 22.0F ? tools[4] : tools[5];
        }
        return tooltip;
    }
    if (mouse.x >= center_right) {
        if (af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_COLOR_SV_Y, AF_EDITOR_COLOR_SV_WIDTH,
                                    AF_EDITOR_COLOR_SV_HEIGHT)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x20000),
                .title = "Saturation / Value",
                .summary = "Drag to choose color intensity and brightness.",
                .details =
                    "Horizontal movement changes saturation; vertical movement changes value.",
                .shortcut = "One drag creates one undoable fill edit"};
        }
        if (af_editor_point_in_rect(mouse, panel_x + AF_EDITOR_COLOR_HUE_X, AF_EDITOR_COLOR_SV_Y,
                                    AF_EDITOR_COLOR_HUE_WIDTH, AF_EDITOR_COLOR_SV_HEIGHT)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x20001),
                .title = "Hue",
                .summary = "Drag to choose the base color family.",
                .details = "Hue is preserved while saturation, value, and alpha stay unchanged.",
                .shortcut = "Drag the vertical rainbow strip"};
        }
        if (af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_COLOR_ALPHA_Y,
                                    AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH,
                                    AF_EDITOR_COLOR_ALPHA_HEIGHT)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x20002),
                .title = "Fill Alpha",
                .summary = "Drag to set color transparency.",
                .details = "Alpha changes the fill color while object opacity remains independent.",
                .shortcut = "Left is transparent; right is opaque"};
        }
        for (uint32_t row = 0U; row < 4U; ++row) {
            const float row_y =
                AF_EDITOR_INSPECTOR_SCALAR_Y + (float)row * AF_EDITOR_INSPECTOR_SCALAR_STEP;
            static const char *const titles[] = {"Opacity", "Corner Radius", "Layer Order",
                                                 "Rotation"};
            static const char *const summaries[] = {
                "Decrease or increase object opacity.", "Decrease or increase corner rounding.",
                "Move the object backward or forward.", "Rotate the object in small increments."};
            static const char *const details[] = {
                "Opacity is clamped from fully transparent to fully visible.",
                "Radius is bounded by the selected shape and remains animatable.",
                "Layer order changes drawing order without changing the parent hierarchy.",
                "Rotation edits merge while repeated and can be animated on the timeline."};
            static const char *const shortcuts[] = {"Value range: 0% to 100%",
                                                    "Inspector or object context menu",
                                                    "Shortcuts: [ and ]", "Shortcuts: Q and E"};
            if (af_editor_point_in_rect(mouse, panel_x, row_y, 70.0F, 26.0F)) {
                return (AfEditorTooltip){.id = UINT64_C(0x21000) + row,
                                         .title = titles[row],
                                         .summary = summaries[row],
                                         .details = details[row],
                                         .shortcut = shortcuts[row]};
            }
        }
        if (mouse.y >= AF_EDITOR_STYLE_ROW_Y && mouse.y < AF_EDITOR_STYLE_ROW_Y + 28.0F &&
            mouse.x >= panel_x) {
            const uint32_t item = (uint32_t)((mouse.x - panel_x) / 38.0F);
            static const char *const titles[] = {"Stroke",    "Stroke Width -", "Stroke Width +",
                                                 "Fill Mode", "Line Cap",       "Line Join"};
            static const char *const summaries[] = {
                "Toggle the vector stroke.",       "Make the stroke thinner.",
                "Make the stroke thicker.",        "Cycle the fill paint mode.",
                "Cycle the stroke end-cap style.", "Cycle the stroke corner-join style."};
            if (item < 6U) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x22000) + item,
                    .title = titles[item],
                    .summary = summaries[item],
                    .details = "Applies to the selected vector style and is undoable.",
                    .shortcut = "Style data is compiled into the runtime package"};
            }
        }
        if (mouse.y >= AF_EDITOR_LAYOUT_ROW_Y && mouse.y < AF_EDITOR_LAYOUT_ROW_Y + 28.0F &&
            mouse.x >= panel_x) {
            const uint32_t item = (uint32_t)((mouse.x - panel_x) / 38.0F);
            static const char *const titles[] = {"Anchors",         "Width Sizing",
                                                 "Height Sizing",   "Flow Direction",
                                                 "Child Alignment", "Aspect Constraint"};
            static const char *const summaries[] = {"Cycle responsive anchor presets.",
                                                    "Cycle fixed, percent, and content width.",
                                                    "Cycle fixed, percent, and content height.",
                                                    "Cycle free, row, and column flow.",
                                                    "Cycle how child objects align.",
                                                    "Cycle aspect-ratio constraints."};
            if (item < 6U) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x23000) + item,
                    .title = titles[item],
                    .summary = summaries[item],
                    .details = "Responsive layout is evaluated against the current canvas.",
                    .shortcut = "Click repeatedly to cycle available modes"};
            }
        }
        if (mouse.y >= AF_EDITOR_PARTICLE_ROW_Y &&
            mouse.y < AF_EDITOR_PARTICLE_ROW_Y +
                          AF_EDITOR_PARTICLE_ROW_COUNT * AF_EDITOR_PARTICLE_ROW_STEP &&
            mouse.x >= panel_x && mouse.x < panel_x + 65.0F) {
            const uint32_t row =
                (uint32_t)((mouse.y - AF_EDITOR_PARTICLE_ROW_Y) / AF_EDITOR_PARTICLE_ROW_STEP);
            static const char *const titles[] = {
                "Particle Spawn Rate", "Particle Lifetime", "Particle Capacity", "Emitter Shape",
                "Particle Output",     "Curl Turbulence",   "Radial Force",      "Vortex Force",
                "Collision Plane",     "Event Burst"};
            static const char *const summaries[] = {
                "Change particles emitted per second.",
                "Change the particle lifetime range.",
                "Change this emitter's bounded GPU capacity.",
                "Cycle the particle source shape.",
                "Cycle the VFX renderer type.",
                "Change deterministic curl-like turbulence.",
                "Push particles toward or away from the force center.",
                "Rotate particles around the force center.",
                "Enable or disable the local collision plane.",
                "Set the count used by a host-triggered burst."};
            static const char *const details[] = {
                "Spawn uses a fixed-step accumulator and deterministic per-emitter random seed.",
                "Color, size, position, velocity, and rotation evolve over this lifetime.",
                "Total project capacity cannot exceed the fixed runtime GPU particle pool.",
                "Sources include point, line, rectangle, circle, and vector path.",
                "Outputs include trails, ribbons, mesh proxies, decals, distortion, soft and "
                "light.",
                "The force is bounded and sampled analytically without per-frame allocation.",
                "Positive values repel; negative values pull toward the local center.",
                "The force is perpendicular to the radial direction and remains bounded.",
                "Failed or non-finite collisions recycle particles without corrupting the pool.",
                "Runtime events are sequence-numbered, one-shot, and capacity bounded."};
            if (row < AF_EDITOR_PARTICLE_ROW_COUNT) {
                return (AfEditorTooltip){.id = UINT64_C(0x24000) + row,
                                         .title = titles[row],
                                         .summary = summaries[row],
                                         .details = details[row],
                                         .shortcut = "Use - and + to change the value"};
            }
        }
        return (AfEditorTooltip){
            .id = UINT64_C(0x25000),
            .title = "Inspector",
            .summary = "Edit the selected object's properties.",
            .details = "Right click for reset, ordering, duplication, and delete actions.",
            .shortcut = "Changes are stored in bounded undo history"};
    }
    if (mouse.y >= timeline_top) {
        const float local_x = mouse.x - editor->left_width;
        if (mouse.y < timeline_top + 40.0F) {
            if (local_x >= 10.0F && local_x < 36.0F) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x30000),
                    .title = "Play / Pause",
                    .summary = "Preview the active animation clip.",
                    .details = "Playback advances the playhead and respects the clip loop region.",
                    .shortcut = "Shortcut: Space"};
            }
            if (local_x >= 42.0F && local_x < 68.0F) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x30001),
                    .title = "Stop",
                    .summary = "Stop preview and return to time zero.",
                    .details =
                        "The scene updates immediately to the first evaluated animation frame.",
                    .shortcut = "Shortcut: Home moves the playhead to zero"};
            }
            if (local_x >= 73.0F && local_x < 107.0F) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x30002),
                    .title = "Curve Editor",
                    .summary = "Toggle curve and dope-sheet views.",
                    .details = "Curve view exposes cubic Bézier tangent handles for selected keys.",
                    .shortcut = "Shortcut: F6"};
            }
            if (local_x >= 116.0F && local_x < 236.0F) {
                const uint32_t property = (uint32_t)((local_x - 116.0F) / 20.0F);
                static const char *const titles[] = {"Position Track", "Rotation Track",
                                                     "Scale Track",    "Opacity Track",
                                                     "Fill Track",     "Corner Radius Track"};
                if (property < 6U) {
                    return (AfEditorTooltip){
                        .id = UINT64_C(0x30100) + property,
                        .title = titles[property],
                        .summary = "Choose the property used by new keyframes.",
                        .details =
                            "The active object, clip, and property identify the animation track.",
                        .shortcut = "Shortcut: K adds a key at the playhead"};
                }
            }
            for (uint32_t index = 0U; index < editor->project.clip_count && index < 8U; ++index) {
                const float x = center_right - 74.0F * (float)(index + 1U);
                if (mouse.x >= x && mouse.x < x + 68.0F) {
                    return (AfEditorTooltip){
                        .id = UINT64_C(0x30200) + index,
                        .title = editor->project.clips[index].name,
                        .summary = "Select this animation clip.",
                        .details =
                            "Switches the visible tracks and returns its playhead to time zero.",
                        .shortcut = "New clip: Alt+C"};
                }
            }
        }
        if (mouse.y < timeline_top + AF_EDITOR_TIMELINE_RULER_HEIGHT) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x30300),
                .title = "Timeline Ruler",
                .summary = "Click or drag to scrub the playhead.",
                .details = "Drag green loop handles to set the bounded playback region.",
                .shortcut = "Wheel zooms; Shift+wheel pans"};
        }
        return (AfEditorTooltip){
            .id = UINT64_C(0x30400),
            .title = editor->curve_visible ? "Animation Curves" : "Dope Sheet",
            .summary = editor->curve_visible ? "Edit easing curves and tangent handles."
                                             : "Select, box-select, and move keyframes.",
            .details = editor->curve_visible
                           ? "Cubic keys expose bounded in and out tangent controls."
                           : "Ctrl toggles key selection; drag moves keys with optional snapping.",
            .shortcut = "Right click for keyframe actions"};
    }
    if (mouse.x < editor->left_width && mouse.y >= AF_EDITOR_TOOLBAR_HEIGHT + 8.0F) {
        const uint32_t row =
            (uint32_t)((mouse.y - AF_EDITOR_TOOLBAR_HEIGHT - 8.0F) / AF_EDITOR_ROW_HEIGHT);
        const uint32_t hierarchy_index = editor->hierarchy_scroll + row;
        if (hierarchy_index < editor->hierarchy_count) {
            const uint32_t node_index = editor->hierarchy_order[hierarchy_index];
            const AfSceneNode *node = &editor->project.scene.nodes[node_index];
            return (AfEditorTooltip){
                .id = UINT64_C(0x4000000000000000) | node->id,
                .title = editor->project.names[node_index],
                .summary = "Select this object in the scene hierarchy.",
                .details = "Shift-click a group to reparent; right click for object actions.",
                .shortcut = "F2 rename / Ctrl+D duplicate / Delete"};
        }
        return tooltip;
    }
    return tooltip;
}

static void af_editor_render_tooltip(AfEditorState *editor, int width, int height) {
    const AfEditorTooltip tooltip = af_editor_find_tooltip(editor, width, height);
    const uint64_t ticks = SDL_GetTicksNS();
    const bool detailed = editor->tooltip_smoke || (SDL_GetModState() & SDL_KMOD_ALT) != 0U;
    const float popup_width = detailed ? AF_EDITOR_TOOLTIP_DETAIL_WIDTH : AF_EDITOR_TOOLTIP_WIDTH;
    const float popup_height = detailed ? 142.0F : 78.0F;
    float x = editor->mouse_position.x + 16.0F;
    float y = editor->mouse_position.y + 18.0F;
    if (tooltip.id == 0U || tooltip.title == NULL) {
        editor->hovered_tooltip_id = 0U;
        return;
    }
    if (editor->hovered_tooltip_id != tooltip.id) {
        editor->hovered_tooltip_id = tooltip.id;
        editor->tooltip_started_ticks = ticks;
        if (!detailed) {
            return;
        }
    }
    if (!detailed && ticks - editor->tooltip_started_ticks < AF_EDITOR_TOOLTIP_DELAY_NS) {
        return;
    }
    x = af_editor_clamp(x, 8.0F, (float)width - popup_width - 8.0F);
    y = af_editor_clamp(y, 8.0F, (float)height - popup_height - 8.0F);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 5.0F, .ty = y + 6.0F},
                        (AfVec2){popup_width, popup_height}, (AfColor){0.0F, 0.0F, 0.0F, 0.5F},
                        8.0F, 300000);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){popup_width, popup_height}, (AfColor){0.085F, 0.095F, 0.12F, 0.99F}, 8.0F, 300010);
    af_editor_add_rect(editor, x, y, 4.0F, popup_height,
                       detailed ? (AfColor){0.72F, 0.46F, 1.0F, 1.0F}
                                : (AfColor){0.28F, 0.68F, 1.0F, 1.0F},
                       300020);
    af_editor_add_text(editor, x + 14.0F, y + 12.0F, tooltip.title, 1.25F, popup_width - 28.0F,
                       (AfColor){0.92F, 0.94F, 0.98F, 1.0F}, 300030);
    af_editor_add_text(editor, x + 14.0F, y + 34.0F, tooltip.summary, 0.82F, popup_width - 28.0F,
                       (AfColor){0.7F, 0.75F, 0.84F, 1.0F}, 300030);
    if (detailed) {
        af_editor_add_rect(editor, x + 14.0F, y + 57.0F, popup_width - 28.0F, 1.0F,
                           (AfColor){0.25F, 0.28F, 0.35F, 1.0F}, 300025);
        af_editor_add_text(editor, x + 14.0F, y + 67.0F, "MORE DETAILS", 0.7F, popup_width - 28.0F,
                           (AfColor){0.72F, 0.54F, 0.98F, 1.0F}, 300030);
        af_editor_add_text(editor, x + 14.0F, y + 87.0F, tooltip.details, 0.78F,
                           popup_width - 28.0F, (AfColor){0.76F, 0.8F, 0.87F, 1.0F}, 300030);
        af_editor_add_text(editor, x + 14.0F, y + 114.0F, tooltip.shortcut, 0.75F,
                           popup_width - 28.0F, (AfColor){0.42F, 0.76F, 1.0F, 1.0F}, 300030);
    } else {
        af_editor_add_text(editor, x + 14.0F, y + 58.0F, "HOLD ALT FOR MORE DETAILS", 0.62F,
                           popup_width - 28.0F, (AfColor){0.48F, 0.66F, 0.9F, 1.0F}, 300030);
    }
}

static bool af_editor_screen_bounds(const AfSceneNode *node, AfAffine2 camera, float *minimum_x,
                                    float *minimum_y, float *maximum_x, float *maximum_y) {
    AfAffine2 world;
    AfVec2 corners[4] = {0};
    if (node == NULL || node->type == AF_NODE_GROUP || !node->world_visible ||
        node->bounds.x <= 0.0F || node->bounds.y <= 0.0F) {
        return false;
    }
    world = af_affine_multiply(camera, node->world_transform);
    corners[0] = af_affine_transform_point(world, (AfVec2){0.0F, 0.0F});
    corners[1] = af_affine_transform_point(world, (AfVec2){node->bounds.x, 0.0F});
    corners[2] = af_affine_transform_point(world, node->bounds);
    corners[3] = af_affine_transform_point(world, (AfVec2){0.0F, node->bounds.y});
    *minimum_x = *maximum_x = corners[0].x;
    *minimum_y = *maximum_y = corners[0].y;
    for (uint32_t index = 1U; index < 4U; ++index) {
        *minimum_x = fminf(*minimum_x, corners[index].x);
        *minimum_y = fminf(*minimum_y, corners[index].y);
        *maximum_x = fmaxf(*maximum_x, corners[index].x);
        *maximum_y = fmaxf(*maximum_y, corners[index].y);
    }
    return true;
}

static void af_editor_render_outline(AfEditorState *editor, const AfSceneNode *node,
                                     AfAffine2 camera, AfColor color, int32_t layer) {
    float minimum_x = 0.0F;
    float minimum_y = 0.0F;
    float maximum_x = 0.0F;
    float maximum_y = 0.0F;
    if (!af_editor_screen_bounds(node, camera, &minimum_x, &minimum_y, &maximum_x, &maximum_y)) {
        return;
    }
    af_editor_add_rect(editor, minimum_x - 2.0F, minimum_y - 2.0F, maximum_x - minimum_x + 4.0F,
                       2.0F, color, layer);
    af_editor_add_rect(editor, minimum_x - 2.0F, maximum_y, maximum_x - minimum_x + 4.0F, 2.0F,
                       color, layer);
    af_editor_add_rect(editor, minimum_x - 2.0F, minimum_y, 2.0F, maximum_y - minimum_y, color,
                       layer);
    af_editor_add_rect(editor, maximum_x, minimum_y, 2.0F, maximum_y - minimum_y, color, layer);
}

static void af_editor_render_selection_outlines(AfEditorState *editor, AfAffine2 camera) {
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        const uint64_t id = editor->selected_ids[index];
        if (id != editor->selected_id) {
            af_editor_render_outline(editor, af_scene_find_const(&editor->preview_scene, id),
                                     camera, (AfColor){0.25F, 0.72F, 1.0F, 0.95F}, 89990);
        }
    }
}

static void af_editor_render_gizmo(AfEditorState *editor, AfAffine2 camera) {
    const AfSceneNode *selected = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    AfVec2 origin;
    if (selected == NULL || selected->type == AF_NODE_GROUP) {
        return;
    }
    af_editor_render_outline(editor, selected, camera, (AfColor){0.98F, 0.8F, 0.22F, 1.0F}, 90000);
    origin = af_affine_transform_point(af_affine_multiply(camera, selected->world_transform),
                                       (AfVec2){0});
    af_editor_add_rect(editor, origin.x, origin.y - 2.0F, 58.0F, 4.0F,
                       (AfColor){0.96F, 0.25F, 0.28F, 1.0F}, 90010);
    af_editor_add_rect(editor, origin.x - 2.0F, origin.y, 4.0F, 58.0F,
                       (AfColor){0.28F, 0.86F, 0.38F, 1.0F}, 90010);
    af_editor_add_rect(editor, origin.x + 50.0F, origin.y - 6.0F, 12.0F, 12.0F,
                       (AfColor){0.96F, 0.25F, 0.28F, 1.0F}, 90020);
    af_editor_add_rect(editor, origin.x - 6.0F, origin.y + 50.0F, 12.0F, 12.0F,
                       (AfColor){0.28F, 0.86F, 0.38F, 1.0F}, 90020);
}

static void af_editor_render_canvas_box(AfEditorState *editor) {
    const float minimum_x = fminf(editor->canvas_box_start.x, editor->canvas_box_current.x);
    const float minimum_y = fminf(editor->canvas_box_start.y, editor->canvas_box_current.y);
    const float maximum_x = fmaxf(editor->canvas_box_start.x, editor->canvas_box_current.x);
    const float maximum_y = fmaxf(editor->canvas_box_start.y, editor->canvas_box_current.y);
    const AfColor border = {0.28F, 0.74F, 1.0F, 1.0F};
    if (editor->drag_mode != AF_EDITOR_DRAG_CANVAS_BOX) {
        return;
    }
    af_editor_add_rect(editor, minimum_x, minimum_y, maximum_x - minimum_x, maximum_y - minimum_y,
                       (AfColor){0.2F, 0.62F, 1.0F, 0.16F}, 90030);
    af_editor_add_rect(editor, minimum_x, minimum_y, maximum_x - minimum_x, 2.0F, border, 90040);
    af_editor_add_rect(editor, minimum_x, maximum_y - 2.0F, maximum_x - minimum_x, 2.0F, border,
                       90040);
    af_editor_add_rect(editor, minimum_x, minimum_y, 2.0F, maximum_y - minimum_y, border, 90040);
    af_editor_add_rect(editor, maximum_x - 2.0F, minimum_y, 2.0F, maximum_y - minimum_y, border,
                       90040);
}

static void af_editor_render_path_points(AfEditorState *editor, AfAffine2 camera) {
    const AfSceneNode *selected = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const AfPath *path = af_path_find_node_const(&editor->project.path_data, editor->selected_id);
    AfAffine2 transform = {0};
    AfVec2 previous = {0};
    bool have_previous = false;
    if (selected == NULL || selected->type != AF_NODE_PATH || path == NULL) {
        return;
    }
    transform = af_affine_multiply(camera, selected->world_transform);
    for (uint32_t index = 0U; index < path->command_count; ++index) {
        const AfPathCommand *command =
            &editor->project.path_data.commands[path->command_start + index];
        const bool chosen = index == editor->selected_path_command;
        const AfColor point_color =
            chosen ? (AfColor){1.0F, 0.82F, 0.2F, 1.0F} : (AfColor){0.88F, 0.9F, 0.96F, 1.0F};
        if (command->type == AF_PATH_MOVE || command->type == AF_PATH_LINE ||
            command->type == AF_PATH_CUBIC) {
            const AfVec2 point = af_affine_transform_point(transform, command->point);
            af_editor_add_rect(editor, point.x - 4.0F, point.y - 4.0F, 8.0F, 8.0F, point_color,
                               90040);
            if (chosen && command->type == AF_PATH_CUBIC && have_previous) {
                const AfVec2 first = af_affine_transform_point(transform, command->control1);
                const AfVec2 second = af_affine_transform_point(transform, command->control2);
                const AfVec2 start = af_affine_transform_point(transform, previous);
                af_editor_add_line(editor, start, first, 1.5F, (AfColor){0.54F, 0.72F, 1.0F, 1.0F},
                                   90030);
                af_editor_add_line(editor, second, point, 1.5F, (AfColor){0.54F, 0.72F, 1.0F, 1.0F},
                                   90030);
                af_editor_add_rect(editor, first.x - 4.0F, first.y - 4.0F, 8.0F, 8.0F,
                                   (AfColor){0.34F, 0.7F, 1.0F, 1.0F}, 90040);
                af_editor_add_rect(editor, second.x - 4.0F, second.y - 4.0F, 8.0F, 8.0F,
                                   (AfColor){0.34F, 0.7F, 1.0F, 1.0F}, 90040);
            }
            previous = command->point;
            have_previous = true;
        } else if (command->type == AF_PATH_CLOSE) {
            have_previous = false;
        }
    }
}

static void af_editor_prepare_render_paths(AfEditorState *editor) {
    editor->render_path_data.path_count = editor->project.path_data.path_count;
    editor->render_path_data.command_count = editor->project.path_data.command_count;
    if (editor->project.path_data.path_count != 0U) {
        memcpy(editor->render_path_data.paths, editor->project.path_data.paths,
               (size_t)editor->project.path_data.path_count * sizeof(*editor->render_paths));
    }
    if (editor->project.path_data.command_count != 0U) {
        memcpy(editor->render_path_data.commands, editor->project.path_data.commands,
               (size_t)editor->project.path_data.command_count *
                   sizeof(*editor->render_path_commands));
    }
}

static void af_editor_build_render_scene(AfEditorState *editor, int width, int height) {
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    AfAffine2 camera = af_editor_camera(editor, width, height);
    uint32_t index = 0U;
    editor->render_scene.count = 0U;
    af_editor_prepare_render_paths(editor);
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    af_editor_add_rect(editor, canvas_x, canvas_y, canvas_width, canvas_height,
                       (AfColor){0.145F, 0.155F, 0.18F, 1.0F}, -100000);
    af_editor_add_text(editor, canvas_x + 12.0F, canvas_y + 12.0F, "CANVAS", 1.0F, 54.0F,
                       (AfColor){0.46F, 0.5F, 0.58F, 1.0F}, -89990);
    af_editor_render_grid(editor, width, height, camera);
    for (index = 0U; index < editor->preview_scene.count; ++index) {
        const AfSceneNode *source = &editor->preview_scene.nodes[index];
        AfSceneNode *target = NULL;
        if (editor->render_scene.count >= AF_MAX_NODES) {
            continue;
        }
        target = &editor->render_scene.nodes[editor->render_scene.count++];
        *target = *source;
        target->world_transform = af_affine_multiply(camera, source->world_transform);
        target->layer_order = af_editor_clamp_layer(source->layer_order);
    }
    af_editor_render_selection_outlines(editor, camera);
    af_editor_render_gizmo(editor, camera);
    if (editor->selected_count == 1U) {
        af_editor_render_path_points(editor, camera);
    }
    af_editor_render_canvas_box(editor);
    af_editor_add_rect(editor, 0.0F, 0.0F, editor->left_width, (float)height,
                       (AfColor){0.085F, 0.092F, 0.115F, 1.0F}, 100000);
    af_editor_add_rect(editor, (float)width - editor->right_width, 0.0F, editor->right_width,
                       (float)height, (AfColor){0.095F, 0.102F, 0.125F, 1.0F}, 100000);
    af_editor_add_rect(editor, editor->left_width, 0.0F,
                       (float)width - editor->left_width - editor->right_width,
                       AF_EDITOR_TOOLBAR_HEIGHT, (AfColor){0.11F, 0.12F, 0.145F, 1.0F}, 100000);
    af_editor_add_rect(editor, editor->left_width, (float)height - AF_EDITOR_STATUS_HEIGHT,
                       (float)width - editor->left_width - editor->right_width,
                       AF_EDITOR_STATUS_HEIGHT, (AfColor){0.08F, 0.087F, 0.105F, 1.0F}, 100000);
    af_editor_add_rect(editor, editor->left_width - 2.0F, 0.0F, 4.0F, (float)height,
                       (AfColor){0.23F, 0.25F, 0.3F, 1.0F}, 100100);
    af_editor_add_rect(editor, (float)width - editor->right_width - 2.0F, 0.0F, 4.0F, (float)height,
                       (AfColor){0.23F, 0.25F, 0.3F, 1.0F}, 100100);
    af_editor_render_hierarchy(editor, height);
    af_editor_render_toolbar(editor);
    af_editor_render_inspector(editor, width);
    af_editor_render_timeline(editor, width, height);
    af_editor_add_text(editor, editor->left_width + 12.0F,
                       (float)height - AF_EDITOR_STATUS_HEIGHT + 9.0F, editor->status, 1.0F,
                       (float)width - editor->left_width - editor->right_width - 24.0F,
                       (AfColor){0.62F, 0.67F, 0.76F, 1.0F}, 100140);
    af_editor_render_context_menu(editor, width, height);
    af_editor_render_tooltip(editor, width, height);
}

static void af_editor_update_title(AfEditorState *editor) {
    char title[768] = {0};
    const AfSceneNode *selected = af_scene_find_const(&editor->project.scene, editor->selected_id);
    const char *name = af_project_name(&editor->project, editor->selected_id);
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    static const char *const properties[] = {"position", "rotation", "scale",
                                             "opacity",  "fill",     "radius"};
    if (editor->rename_active) {
        (void)snprintf(title, sizeof(title),
                       "Arcflow Editor | Rename: %s | Enter commit, Esc cancel",
                       editor->rename_buffer);
    } else if (editor->selected_count > 1U) {
        (void)snprintf(title, sizeof(title),
                       "Arcflow Editor%s | %u objects selected | drag, arrows, color, style, "
                       "rotate, scale, layer, duplicate and delete apply to all | %s",
                       editor->dirty ? " *" : "", editor->selected_count, editor->status);
    } else if (selected != NULL && name != NULL) {
        (void)snprintf(
            title, sizeof(title),
            "Arcflow Editor%s | %s | pos %.1f,%.1f scale %.2f,%.2f rot %.2f opacity %.2f "
            "radius %.1f layer %d | %s %.2fs %s (%u keys) | %s",
            editor->dirty ? " *" : "", name, selected->local_transform.position.x,
            selected->local_transform.position.y, selected->local_transform.scale.x,
            selected->local_transform.scale.y, selected->local_transform.rotation,
            selected->opacity, selected->corner_radius, selected->layer_order,
            clip != NULL ? clip->name : "No clip", editor->playhead,
            properties[editor->selected_property], editor->selected_keyframe_count, editor->status);
    } else {
        (void)snprintf(
            title, sizeof(title),
            "Arcflow Editor%s | 1/2/3/4 create, Ctrl+D duplicate, Del delete, F2 rename, "
            "Shift-click parent | %s",
            editor->dirty ? " *" : "", editor->status);
    }
    (void)SDL_SetWindowTitle(editor->window.window, title);
}

static AfResult af_editor_commit_modify(AfEditorState *editor, const AfProjectNodeSnapshot *before,
                                        AfEditorMergeKey merge_key, bool merge) {
    AfProjectNodeSnapshot after = {0};
    AfEditorCommand command = {0};
    AfResult result = af_project_snapshot(&editor->project, before->node.id, &after);
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_command_modify(before, &after, merge_key, &command);
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, true, merge);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
    }
    return result;
}

static void af_editor_clear_key_selection(AfEditorState *editor) {
    editor->selected_keyframe_count = 0U;
}

static void af_editor_select_keyframe(AfEditorState *editor, uint64_t id, bool toggle) {
    uint32_t index = 0U;
    for (index = 0U; index < editor->selected_keyframe_count; ++index) {
        if (editor->selected_keyframes[index] == id) {
            if (toggle) {
                memmove(&editor->selected_keyframes[index], &editor->selected_keyframes[index + 1U],
                        (size_t)(editor->selected_keyframe_count - index - 1U) *
                            sizeof(editor->selected_keyframes[0]));
                --editor->selected_keyframe_count;
            }
            return;
        }
    }
    if (!toggle) {
        editor->selected_keyframe_count = 0U;
    }
    if (editor->selected_keyframe_count < AF_EDITOR_COMMAND_MAX_KEYFRAMES) {
        editor->selected_keyframes[editor->selected_keyframe_count++] = id;
    }
}

static void af_editor_property_value(const AfSceneNode *node, AfPropertyType property,
                                     float value[4]) {
    switch (property) {
    case AF_PROPERTY_POSITION:
        value[0] = node->local_transform.position.x;
        value[1] = node->local_transform.position.y;
        break;
    case AF_PROPERTY_ROTATION:
        value[0] = node->local_transform.rotation;
        break;
    case AF_PROPERTY_SCALE:
        value[0] = node->local_transform.scale.x;
        value[1] = node->local_transform.scale.y;
        break;
    case AF_PROPERTY_OPACITY:
        value[0] = node->opacity;
        break;
    case AF_PROPERTY_FILL_COLOR:
        value[0] = node->fill.r;
        value[1] = node->fill.g;
        value[2] = node->fill.b;
        value[3] = node->fill.a;
        break;
    case AF_PROPERTY_CORNER_RADIUS:
        value[0] = node->corner_radius;
        break;
    default:
        break;
    }
}

static AfResult af_editor_add_keyframe(AfEditorState *editor) {
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    const AfProjectTrack *existing = af_editor_selected_track(editor);
    AfProjectTrack new_track = {0};
    AfProjectKeyframe new_key = {0};
    AfEditorCommand command = {0};
    uint64_t track_id = 0U;
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (node == NULL || clip == NULL) {
        return result;
    }
    result = af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_id, &command);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    if (existing == NULL) {
        result = af_project_allocate_animation_id(&editor->project, &track_id);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        new_track = (AfProjectTrack){.id = track_id,
                                     .clip_id = clip->id,
                                     .node_id = node->id,
                                     .property = editor->selected_property};
        command.track_after[0] =
            (AfProjectTrackSnapshot){.track = new_track, .index = editor->project.track_count};
        command.track_after_count = 1U;
    } else {
        track_id = existing->id;
    }
    result = af_project_allocate_animation_id(&editor->project, &new_key.id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    new_key.track_id = track_id;
    new_key.keyframe.time = af_editor_clamp(editor->playhead, 0.0F, clip->duration);
    if (editor->snapping) {
        new_key.keyframe.time = roundf(new_key.keyframe.time * 10.0F) * 0.1F;
    }
    new_key.keyframe.interpolation = AF_INTERPOLATION_LINEAR;
    new_key.keyframe.easing[0] = 0.42F;
    new_key.keyframe.easing[2] = 0.58F;
    new_key.keyframe.easing[3] = 1.0F;
    af_editor_property_value(node, editor->selected_property, new_key.keyframe.value);
    command.keyframe_after[0] =
        (AfProjectKeyframeSnapshot){.keyframe = new_key, .index = editor->project.keyframe_count};
    command.keyframe_after_count = 1U;
    result = af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    if (result == AF_RESULT_SUCCESS) {
        editor->selected_keyframe_count = 1U;
        editor->selected_keyframes[0] = new_key.id;
        editor->timeline_focus = true;
        editor->dirty = true;
        af_editor_status(editor, "Keyframe added");
    }
    return result;
}

static AfResult af_editor_delete_keyframes(AfEditorState *editor) {
    AfEditorCommand command = {0};
    uint32_t index = 0U;
    AfResult result = AF_RESULT_NOT_FOUND;
    if (editor->selected_keyframe_count == 0U) {
        return result;
    }
    result =
        af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_keyframes[0], &command);
    for (index = 0U; result == AF_RESULT_SUCCESS && index < editor->selected_keyframe_count;
         ++index) {
        result = af_project_keyframe_snapshot(&editor->project, editor->selected_keyframes[index],
                                              &command.keyframe_before[index]);
    }
    if (result == AF_RESULT_SUCCESS) {
        command.keyframe_before_count = editor->selected_keyframe_count;
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_clear_key_selection(editor);
        editor->dirty = true;
        af_editor_status(editor, "Keyframes deleted");
    }
    return result;
}

static void af_editor_copy_keyframes(AfEditorState *editor) {
    uint32_t index = 0U;
    editor->key_clipboard_count = 0U;
    for (index = 0U; index < editor->selected_keyframe_count; ++index) {
        const AfProjectKeyframe *key =
            af_project_find_keyframe_const(&editor->project, editor->selected_keyframes[index]);
        if (key != NULL) {
            editor->key_clipboard[editor->key_clipboard_count++] = *key;
        }
    }
    af_editor_status(editor, "Keyframes copied");
}

static AfResult af_editor_paste_keyframes(AfEditorState *editor, bool duplicate) {
    AfEditorCommand command = {0};
    float earliest = INFINITY;
    uint32_t index = 0U;
    AfResult result = AF_RESULT_NOT_FOUND;
    if (duplicate) {
        af_editor_copy_keyframes(editor);
    }
    if (editor->key_clipboard_count == 0U) {
        return result;
    }
    result = af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_clip_id, &command);
    for (index = 0U; index < editor->key_clipboard_count; ++index) {
        earliest = fminf(earliest, editor->key_clipboard[index].keyframe.time);
    }
    editor->selected_keyframe_count = 0U;
    for (index = 0U; result == AF_RESULT_SUCCESS && index < editor->key_clipboard_count; ++index) {
        AfProjectKeyframe copy = editor->key_clipboard[index];
        const AfProjectTrack *track = af_project_find_track_const(&editor->project, copy.track_id);
        const AfProjectClip *clip =
            track == NULL ? NULL : af_project_find_clip_const(&editor->project, track->clip_id);
        if (clip == NULL || clip->id != editor->selected_clip_id) {
            result = AF_RESULT_INVALID_ARGUMENT;
            break;
        }
        result = af_project_allocate_animation_id(&editor->project, &copy.id);
        copy.keyframe.time =
            af_editor_clamp(editor->playhead + copy.keyframe.time - earliest, 0.0F, clip->duration);
        if (duplicate) {
            copy.keyframe.time = af_editor_clamp(copy.keyframe.time + 0.1F, 0.0F, clip->duration);
        }
        command.keyframe_after[index] = (AfProjectKeyframeSnapshot){
            .keyframe = copy, .index = editor->project.keyframe_count + index};
        editor->selected_keyframes[editor->selected_keyframe_count++] = copy.id;
    }
    if (result == AF_RESULT_SUCCESS) {
        command.keyframe_after_count = editor->key_clipboard_count;
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
        af_editor_status(editor, duplicate ? "Keyframes duplicated" : "Keyframes pasted");
    }
    return result;
}

static AfResult af_editor_modify_selected_keys(AfEditorState *editor, int operation,
                                               AfInterpolation interpolation) {
    AfEditorCommand command = {0};
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    float minimum = INFINITY;
    float maximum = -INFINITY;
    uint32_t index = 0U;
    AfResult result = AF_RESULT_NOT_FOUND;
    if (clip == NULL || editor->selected_keyframe_count == 0U) {
        return result;
    }
    result =
        af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_keyframes[0], &command);
    for (index = 0U; result == AF_RESULT_SUCCESS && index < editor->selected_keyframe_count;
         ++index) {
        result = af_project_keyframe_snapshot(&editor->project, editor->selected_keyframes[index],
                                              &command.keyframe_before[index]);
        if (result == AF_RESULT_SUCCESS) {
            const float time = command.keyframe_before[index].keyframe.keyframe.time;
            minimum = fminf(minimum, time);
            maximum = fmaxf(maximum, time);
        }
    }
    command.keyframe_before_count = editor->selected_keyframe_count;
    for (index = 0U; result == AF_RESULT_SUCCESS && index < editor->selected_keyframe_count;
         ++index) {
        AfProjectKeyframeSnapshot snapshot = command.keyframe_before[index];
        if (operation == 0) {
            snapshot.keyframe.keyframe.interpolation = interpolation;
        } else if (operation == 1) {
            snapshot.keyframe.keyframe.time =
                minimum + (snapshot.keyframe.keyframe.time - minimum) * 1.1F;
        } else if (operation == 2) {
            snapshot.keyframe.keyframe.time =
                minimum + (snapshot.keyframe.keyframe.time - minimum) * 0.9F;
        } else if (operation == 3) {
            snapshot.keyframe.keyframe.time = minimum + maximum - snapshot.keyframe.keyframe.time;
        } else if (operation >= 4 && operation <= 6) {
            snapshot.keyframe.keyframe.interpolation = AF_INTERPOLATION_CUBIC_BEZIER;
            snapshot.keyframe.keyframe.easing[0] = operation == 6 ? 0.0F : 0.42F;
            snapshot.keyframe.keyframe.easing[1] = 0.0F;
            snapshot.keyframe.keyframe.easing[2] = operation == 5 ? 1.0F : 0.58F;
            snapshot.keyframe.keyframe.easing[3] = 1.0F;
        }
        snapshot.keyframe.keyframe.time =
            af_editor_clamp(snapshot.keyframe.keyframe.time, 0.0F, clip->duration);
        command.keyframe_after[index] = snapshot;
    }
    command.keyframe_after_count = editor->selected_keyframe_count;
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
        af_editor_status(editor, "Keyframes edited");
    }
    return result;
}

static AfResult af_editor_new_clip(AfEditorState *editor) {
    AfEditorCommand command = {0};
    AfProjectClip clip = {.duration = 4.0F, .loop_start = 0.0F, .loop_end = 4.0F, .loop = true};
    AfResult result = af_project_allocate_animation_id(&editor->project, &clip.id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    (void)snprintf(clip.name, sizeof(clip.name), "Clip %llu", (unsigned long long)clip.id);
    result = af_editor_command_animation(AF_EDITOR_MERGE_NONE, clip.id, &command);
    command.clip_after[0] =
        (AfProjectClipSnapshot){.clip = clip, .index = editor->project.clip_count};
    command.clip_after_count = 1U;
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->selected_clip_id = clip.id;
        editor->playhead = 0.0F;
        editor->dirty = true;
        af_editor_status(editor, "Animation clip created");
    }
    return result;
}

static AfResult af_editor_delete_clip(AfEditorState *editor) {
    AfEditorCommand command = {0};
    uint32_t index = 0U;
    AfResult result = AF_RESULT_NOT_FOUND;
    if (editor->project.clip_count <= 1U || editor->selected_clip_id == 0U) {
        return result;
    }
    result = af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_clip_id, &command);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_clip_snapshot(&editor->project, editor->selected_clip_id,
                                          &command.clip_before[0]);
        command.clip_before_count = 1U;
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < editor->project.track_count; ++index) {
        const AfProjectTrack *track = &editor->project.tracks[index];
        uint32_t key_index = 0U;
        if (track->clip_id != editor->selected_clip_id) {
            continue;
        }
        if (command.track_before_count >= AF_EDITOR_COMMAND_MAX_TRACKS) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        command.track_before[command.track_before_count++] =
            (AfProjectTrackSnapshot){.track = *track, .index = index};
        for (key_index = 0U; key_index < editor->project.keyframe_count; ++key_index) {
            if (editor->project.keyframes[key_index].track_id == track->id) {
                if (command.keyframe_before_count >= AF_EDITOR_COMMAND_MAX_KEYFRAMES) {
                    return AF_RESULT_LIMIT_EXCEEDED;
                }
                command.keyframe_before[command.keyframe_before_count++] =
                    (AfProjectKeyframeSnapshot){.keyframe = editor->project.keyframes[key_index],
                                                .index = key_index};
            }
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->selected_clip_id = editor->project.clips[0].id;
        editor->playhead = 0.0F;
        af_editor_clear_key_selection(editor);
        editor->dirty = true;
        af_editor_status(editor, "Animation clip deleted");
    }
    return result;
}

static AfVec2 af_editor_visible_center(const AfEditorState *editor, int width, int height) {
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    return af_editor_screen_to_project(
        editor, width, height,
        (AfVec2){canvas_x + canvas_width * 0.5F, canvas_y + canvas_height * 0.5F});
}

static AfResult af_editor_create_primitive(AfEditorState *editor, AfNodeType type, int width,
                                           int height) {
    AfProjectNodeSnapshot snapshot = {0};
    AfEditorCommand command = {0};
    uint64_t id = 0U;
    const char *name = type == AF_NODE_RECTANGLE
                           ? "Rectangle"
                           : (type == AF_NODE_ROUNDED_RECTANGLE ? "Rounded Rectangle" : "Ellipse");
    AfResult result = af_project_allocate_id(&editor->project, &id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    snapshot = (AfProjectNodeSnapshot){
        .node = af_editor_default_node(type, id, af_editor_visible_center(editor, width, height)),
        .index = editor->project.scene.count};
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "%s %llu", name, (unsigned long long)id);
    result = af_editor_command_create(&snapshot, &command);
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, id);
        editor->selected_path_command = UINT32_MAX;
        editor->dirty = true;
        af_editor_status(editor, "Primitive created");
    }
    return result;
}

static AfPath *af_editor_selected_path(AfEditorState *editor) {
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    return node != NULL && node->type == AF_NODE_PATH
               ? af_project_find_node_path(&editor->project, node->id)
               : NULL;
}

static AfResult af_editor_create_path(AfEditorState *editor, int width, int height) {
    static const AfPathCommand commands[] = {{.type = AF_PATH_MOVE, .point = {0.0F, 90.0F}},
                                             {.type = AF_PATH_CUBIC,
                                              .point = {110.0F, 0.0F},
                                              .control1 = {20.0F, 25.0F},
                                              .control2 = {72.0F, 0.0F}},
                                             {.type = AF_PATH_CUBIC,
                                              .point = {220.0F, 90.0F},
                                              .control1 = {148.0F, 0.0F},
                                              .control2 = {200.0F, 25.0F}},
                                             {.type = AF_PATH_LINE, .point = {110.0F, 180.0F}},
                                             {.type = AF_PATH_CLOSE}};
    AfProjectNodeSnapshot snapshot = {0};
    AfEditorCommand command = {0};
    uint64_t id = 0U;
    AfResult result = af_project_allocate_id(&editor->project, &id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    snapshot = (AfProjectNodeSnapshot){
        .node = af_editor_default_node(AF_NODE_PATH, id,
                                       af_editor_visible_center(editor, width, height)),
        .index = editor->project.scene.count};
    snapshot.node.bounds = (AfVec2){220.0F, 180.0F};
    snapshot.node.fill = (AfColor){0.78F, 0.42F, 0.96F, 1.0F};
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "Path %llu", (unsigned long long)id);
    result = af_editor_command_create(&snapshot, &command);
    if (result == AF_RESULT_SUCCESS) {
        command.path_after_count = 1U;
        command.path_command_after_count = (uint32_t)(sizeof(commands) / sizeof(commands[0]));
        command.path_after[0] = (AfProjectPathSnapshot){
            .path = {.id = id,
                     .node_id = id,
                     .command_start = editor->project.path_data.command_count,
                     .command_count = command.path_command_after_count,
                     .fill_rule = AF_PATH_FILL_EVEN_ODD,
                     .geometry_generation = 1U},
            .index = editor->project.path_data.path_count};
        memcpy(command.path_command_after, commands, sizeof(commands));
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, id);
        editor->selected_path_command = 1U;
        editor->dirty = true;
        af_editor_status(editor, "Editable path created");
    }
    return result;
}

static AfResult af_editor_apply_path_edit(AfEditorState *editor, const AfPathCommand *new_commands,
                                          uint32_t new_command_count, AfPathFillRule fill_rule,
                                          const char *status) {
    AfPath *path = af_editor_selected_path(editor);
    AfEditorCommand command = {0};
    AfResult result = AF_RESULT_NOT_FOUND;
    if (path == NULL || new_commands == NULL || new_command_count == 0U) {
        return result;
    }
    if (path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        new_command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        af_editor_status(editor, "Path edit exceeds bounded undo capacity");
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (path->geometry_generation == UINT64_MAX) {
        af_editor_status(editor, "Path geometry generation exhausted");
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    result = af_editor_command_path(AF_EDITOR_MERGE_NONE, path->id, &command);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_path_snapshot(&editor->project, path->id, &command.path_before[0],
                                          command.path_command_before,
                                          AF_EDITOR_COMMAND_MAX_PATH_COMMANDS);
    }
    if (result == AF_RESULT_SUCCESS) {
        command.path_before_count = 1U;
        command.path_command_before_count = path->command_count;
        command.path_before[0].command_offset = 0U;
        command.path_after_count = 1U;
        command.path_after[0] = command.path_before[0];
        command.path_after[0].path.command_count = new_command_count;
        command.path_after[0].path.fill_rule = fill_rule;
        command.path_after[0].path.geometry_generation =
            command.path_before[0].path.geometry_generation + 1U;
        command.path_after[0].command_offset = 0U;
        command.path_command_after_count = new_command_count;
        memcpy(command.path_command_after, new_commands,
               (size_t)new_command_count * sizeof(*new_commands));
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
        af_editor_status(editor, status);
    } else {
        af_editor_status(editor, "Path edit rejected");
    }
    return result;
}

static bool af_editor_path_contour(const AfPathCommand *commands, uint32_t count, uint32_t selected,
                                   uint32_t *out_start, uint32_t *out_end) {
    uint32_t start = selected;
    uint32_t end = selected;
    if (commands == NULL || selected >= count || out_start == NULL || out_end == NULL) {
        return false;
    }
    while (start > 0U && commands[start].type != AF_PATH_MOVE) {
        --start;
    }
    if (commands[start].type != AF_PATH_MOVE) {
        return false;
    }
    end = start + 1U;
    while (end < count && commands[end].type != AF_PATH_MOVE) {
        ++end;
    }
    *out_start = start;
    *out_end = end;
    return true;
}

static AfVec2 af_editor_vec_midpoint(AfVec2 first, AfVec2 second) {
    return (AfVec2){(first.x + second.x) * 0.5F, (first.y + second.y) * 0.5F};
}

static AfResult af_editor_insert_path_point(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t selected = editor->selected_path_command;
    if (path == NULL || path->command_count >= AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        selected == 0U || selected >= path->command_count) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if (commands[selected].type != AF_PATH_LINE && commands[selected].type != AF_PATH_CUBIC) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memmove(&commands[selected + 1U], &commands[selected],
            (size_t)(path->command_count - selected) * sizeof(*commands));
    if (commands[selected + 1U].type == AF_PATH_LINE) {
        commands[selected] =
            (AfPathCommand){.type = AF_PATH_LINE,
                            .point = af_editor_vec_midpoint(commands[selected - 1U].point,
                                                            commands[selected + 1U].point)};
    } else {
        const AfVec2 start = commands[selected - 1U].point;
        const AfPathCommand original = commands[selected + 1U];
        const AfVec2 first = af_editor_vec_midpoint(start, original.control1);
        const AfVec2 middle = af_editor_vec_midpoint(original.control1, original.control2);
        const AfVec2 last = af_editor_vec_midpoint(original.control2, original.point);
        const AfVec2 left = af_editor_vec_midpoint(first, middle);
        const AfVec2 right = af_editor_vec_midpoint(middle, last);
        const AfVec2 point = af_editor_vec_midpoint(left, right);
        commands[selected] = (AfPathCommand){
            .type = AF_PATH_CUBIC, .point = point, .control1 = first, .control2 = left};
        commands[selected + 1U] = (AfPathCommand){
            .type = AF_PATH_CUBIC, .point = original.point, .control1 = right, .control2 = last};
    }
    if (af_editor_apply_path_edit(editor, commands, path->command_count + 1U, path->fill_rule,
                                  "Path point inserted") == AF_RESULT_SUCCESS) {
        editor->selected_path_command = selected;
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_delete_path_point(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t start = 0U;
    uint32_t end = 0U;
    uint32_t segments = 0U;
    const uint32_t selected = editor->selected_path_command;
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        path->geometry_generation == UINT64_MAX || selected >= path->command_count) {
        return AF_RESULT_NOT_FOUND;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if ((commands[selected].type != AF_PATH_LINE && commands[selected].type != AF_PATH_CUBIC) ||
        !af_editor_path_contour(commands, path->command_count, selected, &start, &end)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = start + 1U; index < end; ++index) {
        segments += commands[index].type == AF_PATH_LINE || commands[index].type == AF_PATH_CUBIC;
    }
    if (segments <= 1U) {
        af_editor_status(editor, "A contour must retain a segment");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memmove(&commands[selected], &commands[selected + 1U],
            (size_t)(path->command_count - selected - 1U) * sizeof(*commands));
    if (af_editor_apply_path_edit(editor, commands, path->command_count - 1U, path->fill_rule,
                                  "Path point deleted") == AF_RESULT_SUCCESS) {
        editor->selected_path_command = selected > 0U ? selected - 1U : 0U;
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_convert_path_point(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    const uint32_t selected = editor->selected_path_command;
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        selected == 0U || selected >= path->command_count) {
        return AF_RESULT_NOT_FOUND;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if (commands[selected].type == AF_PATH_LINE) {
        const AfVec2 first = commands[selected - 1U].point;
        const AfVec2 last = commands[selected].point;
        commands[selected].type = AF_PATH_CUBIC;
        commands[selected].control1 =
            (AfVec2){first.x + (last.x - first.x) / 3.0F, first.y + (last.y - first.y) / 3.0F};
        commands[selected].control2 = (AfVec2){first.x + (last.x - first.x) * 2.0F / 3.0F,
                                               first.y + (last.y - first.y) * 2.0F / 3.0F};
    } else if (commands[selected].type == AF_PATH_CUBIC) {
        commands[selected].type = AF_PATH_LINE;
        commands[selected].control1 = (AfVec2){0};
        commands[selected].control2 = (AfVec2){0};
    } else {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return af_editor_apply_path_edit(editor, commands, path->command_count, path->fill_rule,
                                     "Path point converted");
}

static AfResult af_editor_toggle_path_closed(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t start = 0U;
    uint32_t end = 0U;
    const uint32_t selected = editor->selected_path_command;
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        !af_editor_path_contour(&editor->project.path_data.commands[path->command_start],
                                path->command_count, selected < path->command_count ? selected : 0U,
                                &start, &end)) {
        return AF_RESULT_NOT_FOUND;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if (end > start && commands[end - 1U].type == AF_PATH_CLOSE) {
        memmove(&commands[end - 1U], &commands[end],
                (size_t)(path->command_count - end) * sizeof(*commands));
        editor->selected_path_command = end > start + 1U ? end - 2U : start;
        return af_editor_apply_path_edit(editor, commands, path->command_count - 1U,
                                         path->fill_rule, "Path contour opened");
    }
    if (path->command_count >= AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memmove(&commands[end + 1U], &commands[end],
            (size_t)(path->command_count - end) * sizeof(*commands));
    commands[end] = (AfPathCommand){.type = AF_PATH_CLOSE};
    editor->selected_path_command = end;
    return af_editor_apply_path_edit(editor, commands, path->command_count + 1U, path->fill_rule,
                                     "Path contour closed");
}

static AfResult af_editor_reverse_path_contour(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    AfPathCommand reversed[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t start = 0U;
    uint32_t end = 0U;
    uint32_t write = 0U;
    uint32_t segment_end = 0U;
    bool closed = false;
    const uint32_t selected = editor->selected_path_command;
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        return AF_RESULT_NOT_FOUND;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if (!af_editor_path_contour(commands, path->command_count,
                                selected < path->command_count ? selected : 0U, &start, &end)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    closed = end > start && commands[end - 1U].type == AF_PATH_CLOSE;
    segment_end = closed ? end - 1U : end;
    if (segment_end <= start + 1U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    reversed[write++] =
        (AfPathCommand){.type = AF_PATH_MOVE, .point = commands[segment_end - 1U].point};
    for (uint32_t index = segment_end; index > start + 1U;) {
        AfPathCommand segment = commands[--index];
        segment.point = index == start + 1U ? commands[start].point : commands[index - 1U].point;
        if (segment.type == AF_PATH_CUBIC) {
            const AfVec2 control = segment.control1;
            segment.control1 = segment.control2;
            segment.control2 = control;
        }
        reversed[write++] = segment;
    }
    if (closed) {
        reversed[write++] = (AfPathCommand){.type = AF_PATH_CLOSE};
    }
    memcpy(&commands[start], reversed, (size_t)write * sizeof(*reversed));
    editor->selected_path_command = start + (write > 1U ? 1U : 0U);
    return af_editor_apply_path_edit(editor, commands, path->command_count, path->fill_rule,
                                     "Path contour reversed");
}

static AfResult af_editor_add_path_contour(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t count = 0U;
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - 3U) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    count = path->command_count;
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)count * sizeof(*commands));
    commands[count] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {55.0F, 65.0F}};
    commands[count + 1U] = (AfPathCommand){.type = AF_PATH_LINE, .point = {165.0F, 65.0F}};
    commands[count + 2U] = (AfPathCommand){.type = AF_PATH_LINE, .point = {110.0F, 130.0F}};
    if (af_editor_apply_path_edit(editor, commands, count + 3U, path->fill_rule,
                                  "Open contour added") == AF_RESULT_SUCCESS) {
        editor->selected_path_command = count + 1U;
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_toggle_path_fill_rule(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        return AF_RESULT_NOT_FOUND;
    }
    return af_editor_apply_path_edit(
        editor, &editor->project.path_data.commands[path->command_start], path->command_count,
        path->fill_rule == AF_PATH_FILL_EVEN_ODD ? AF_PATH_FILL_NON_ZERO : AF_PATH_FILL_EVEN_ODD,
        path->fill_rule == AF_PATH_FILL_EVEN_ODD ? "Non-zero fill rule" : "Even-odd fill rule");
}

static AfResult af_editor_delete_selection(AfEditorState *editor) {
    uint64_t roots[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t root_count = 0U;
    AfEditorCommand command = {0};
    AfResult result = AF_RESULT_NOT_FOUND;
    if (editor->selected_id == 0U) {
        return result;
    }
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        const uint64_t id = editor->selected_ids[index];
        if (!af_editor_selection_has_selected_ancestor(editor, id)) {
            roots[root_count++] = id;
        }
    }
    if (root_count == 0U) {
        roots[root_count++] = editor->selected_id;
    }
    result = af_editor_command_delete_many(&editor->project, roots, root_count, &command);
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, 0U);
        editor->selected_path_command = UINT32_MAX;
        editor->dirty = true;
        af_editor_status(editor, root_count == 1U ? "Object deleted" : "Objects deleted");
    }
    return result;
}

static void af_editor_reconcile_selection(AfEditorState *editor) {
    uint32_t selection_write = 0U;
    uint32_t write_index = 0U;
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        if (af_scene_find_const(&editor->project.scene, editor->selected_ids[index]) != NULL) {
            editor->selected_ids[selection_write++] = editor->selected_ids[index];
        }
    }
    editor->selected_count = selection_write;
    if (af_scene_find_const(&editor->project.scene, editor->selected_id) == NULL) {
        editor->selected_id =
            editor->selected_count == 0U ? 0U : editor->selected_ids[editor->selected_count - 1U];
        editor->selected_path_command = UINT32_MAX;
    } else if (!af_editor_selection_contains(editor, editor->selected_id)) {
        af_editor_selection_add(editor, editor->selected_id);
    }
    if (editor->selected_id != 0U) {
        const AfPath *path =
            af_path_find_node_const(&editor->project.path_data, editor->selected_id);
        if (path == NULL) {
            editor->selected_path_command = UINT32_MAX;
        } else if (editor->selected_path_command >= path->command_count) {
            editor->selected_path_command = 0U;
        }
    } else {
        editor->selected_path_command = UINT32_MAX;
    }
    if (clip == NULL) {
        editor->selected_clip_id =
            editor->project.clip_count == 0U ? 0U : editor->project.clips[0].id;
        clip = af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    }
    if (clip != NULL) {
        editor->playhead = af_editor_clamp(editor->playhead, 0.0F, clip->duration);
    }
    for (uint32_t index = 0U; index < editor->selected_keyframe_count; ++index) {
        const AfProjectKeyframe *key =
            af_project_find_keyframe_const(&editor->project, editor->selected_keyframes[index]);
        const AfProjectTrack *track =
            key == NULL ? NULL : af_project_find_track_const(&editor->project, key->track_id);
        if (track != NULL && track->clip_id == editor->selected_clip_id) {
            editor->selected_keyframes[write_index++] = key->id;
        }
    }
    editor->selected_keyframe_count = write_index;
}

static AfResult af_editor_duplicate_selection(AfEditorState *editor) {
    AfProjectNodeSnapshot originals[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    AfProjectNodeSnapshot copies[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint64_t roots[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint64_t new_roots[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint64_t old_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint64_t new_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t root_count = 0U;
    uint32_t count = 0U;
    uint32_t index = 0U;
    AfEditorCommand command = {0};
    AfResult result = AF_RESULT_NOT_FOUND;
    if (editor->selected_id == 0U) {
        return result;
    }
    for (index = 0U; index < editor->selected_count; ++index) {
        const uint64_t id = editor->selected_ids[index];
        if (!af_editor_selection_has_selected_ancestor(editor, id)) {
            roots[root_count++] = id;
        }
    }
    if (root_count == 0U) {
        roots[root_count++] = editor->selected_id;
    }
    result = AF_RESULT_SUCCESS;
    for (index = 0U; result == AF_RESULT_SUCCESS && index < root_count; ++index) {
        uint32_t subtree_count = 0U;
        result =
            af_project_collect_subtree(&editor->project, roots[index], &originals[count],
                                       AF_EDITOR_COMMAND_MAX_SNAPSHOTS - count, &subtree_count);
        count += subtree_count;
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < count; ++index) {
        old_ids[index] = originals[index].node.id;
        result = af_project_allocate_id(&editor->project, &new_ids[index]);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < count; ++index) {
        uint32_t parent_index = 0U;
        copies[index] = originals[index];
        copies[index].node.id = new_ids[index];
        copies[index].style.id = new_ids[index];
        copies[index].style.node_id = new_ids[index];
        copies[index].layout.node_id = new_ids[index];
        copies[index].index = editor->project.scene.count + index;
        bool is_root = false;
        for (uint32_t root_index = 0U; root_index < root_count; ++root_index) {
            if (originals[index].node.id == roots[root_index]) {
                new_roots[root_index] = new_ids[index];
                is_root = true;
                break;
            }
        }
        if (is_root) {
            copies[index].node.local_transform.position.x += 16.0F;
            copies[index].node.local_transform.position.y += 16.0F;
        }
        for (parent_index = 0U; parent_index < count; ++parent_index) {
            if (copies[index].node.parent_id == old_ids[parent_index]) {
                copies[index].node.parent_id = new_ids[parent_index];
                break;
            }
        }
        (void)snprintf(copies[index].name, sizeof(copies[index].name), "Copy %.56s",
                       originals[index].name);
        if (copies[index].has_emitter) {
            result = af_project_allocate_animation_id(&editor->project, &copies[index].emitter.id);
            copies[index].emitter.node_id = new_ids[index];
            if (copies[index].emitter.type == AF_PARTICLE_EMITTER_VECTOR_PATH) {
                copies[index].emitter.path_id = new_ids[index];
            }
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_command_duplicate(copies, count, &command);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < count; ++index) {
        const AfPath *source =
            af_path_find_node_const(&editor->project.path_data, originals[index].node.id);
        AfProjectPathSnapshot *path_copy = NULL;
        if (source == NULL) {
            continue;
        }
        if (command.path_after_count >= AF_EDITOR_COMMAND_MAX_PATHS ||
            source->command_count >
                AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - command.path_command_after_count) {
            result = AF_RESULT_LIMIT_EXCEEDED;
            break;
        }
        path_copy = &command.path_after[command.path_after_count];
        *path_copy = (AfProjectPathSnapshot){.path = *source,
                                             .index = editor->project.path_data.path_count +
                                                      command.path_after_count,
                                             .command_offset = command.path_command_after_count};
        path_copy->path.id = new_ids[index];
        path_copy->path.node_id = new_ids[index];
        path_copy->path.command_start =
            editor->project.path_data.command_count + command.path_command_after_count;
        path_copy->path.geometry_generation = 1U;
        memcpy(&command.path_command_after[command.path_command_after_count],
               &editor->project.path_data.commands[source->command_start],
               (size_t)source->command_count * sizeof(*command.path_command_after));
        command.path_command_after_count += source->command_count;
        ++command.path_after_count;
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, 0U);
        for (index = 0U; index < root_count; ++index) {
            af_editor_selection_add(editor, new_roots[index]);
        }
        editor->selected_path_command =
            af_path_find_node_const(&editor->project.path_data, editor->selected_id) != NULL
                ? 0U
                : UINT32_MAX;
        editor->dirty = true;
        af_editor_status(editor, root_count == 1U ? "Object duplicated" : "Objects duplicated");
    }
    return result;
}

static void af_editor_begin_rename(AfEditorState *editor) {
    const char *name = af_project_name(&editor->project, editor->selected_id);
    if (name == NULL || af_project_snapshot(&editor->project, editor->selected_id,
                                            &editor->rename_before) != AF_RESULT_SUCCESS) {
        return;
    }
    (void)snprintf(editor->rename_buffer, sizeof(editor->rename_buffer), "%s", name);
    editor->rename_active = true;
    (void)SDL_StartTextInput(editor->window.window);
}

static void af_editor_finish_rename(AfEditorState *editor, bool commit) {
    if (!editor->rename_active) {
        return;
    }
    if (commit && editor->rename_buffer[0] != '\0' &&
        af_project_set_name(&editor->project, editor->selected_id, editor->rename_buffer) ==
            AF_RESULT_SUCCESS) {
        (void)af_editor_commit_modify(editor, &editor->rename_before, AF_EDITOR_MERGE_RENAME,
                                      false);
        af_editor_status(editor, "Node renamed");
    }
    editor->rename_active = false;
    SDL_StopTextInput(editor->window.window);
}

static void af_editor_backspace_utf8(char *text) {
    size_t length = strlen(text);
    if (length == 0U) {
        return;
    }
    --length;
    while (length > 0U && ((unsigned char)text[length] & 0xC0U) == 0x80U) {
        --length;
    }
    text[length] = '\0';
}

static void af_editor_append_text(AfEditorState *editor, const char *text) {
    const size_t current = strlen(editor->rename_buffer);
    const size_t incoming = text == NULL ? 0U : strlen(text);
    if (incoming != 0U && current + incoming < sizeof(editor->rename_buffer) &&
        strchr(text, '\n') == NULL && strchr(text, '\r') == NULL) {
        memcpy(editor->rename_buffer + current, text, incoming + 1U);
    }
}

static void af_editor_modify_scalar(AfEditorState *editor, AfEditorMergeKey merge_key, float amount,
                                    int property) {
    if (editor->selected_id == 0U) {
        return;
    }
    af_editor_begin_selection_modify(editor, false);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        AfSceneNode *node =
            af_scene_find(&editor->project.scene, editor->selection_drag_before[index].node.id);
        if (node == NULL) {
            continue;
        }
        switch (property) {
        case 0:
            node->opacity = af_editor_clamp(node->opacity + amount, 0.0F, 1.0F);
            break;
        case 1:
            node->corner_radius = fmaxf(0.0F, node->corner_radius + amount);
            break;
        case 2:
            if ((amount > 0.0F && node->layer_order < INT32_MAX) ||
                (amount < 0.0F && node->layer_order > INT32_MIN)) {
                node->layer_order += amount > 0.0F ? 1 : -1;
            }
            break;
        case 3:
            node->local_transform.rotation += amount;
            break;
        case 4:
            node->local_transform.scale.x = fmaxf(0.05F, node->local_transform.scale.x + amount);
            node->local_transform.scale.y = fmaxf(0.05F, node->local_transform.scale.y + amount);
            break;
        default:
            return;
        }
    }
    (void)af_scene_evaluate_world(&editor->project.scene);
    (void)af_editor_commit_selection_modify(editor, merge_key, true);
}

static void af_editor_apply_fill(AfEditorState *editor, AfColor color) {
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_ids[index]);
        if (node == NULL) {
            continue;
        }
        node->fill = color;
        AfStyle *style = af_project_find_node_style(&editor->project, node->id);
        if (style != NULL) {
            style->fill = af_paint_solid(color);
            (void)af_style_mark_paint_changed(style);
        }
    }
    editor->dirty = true;
}

static void af_editor_update_color_picker(AfEditorState *editor, int width, float x, float y) {
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    const float panel_x = (float)width - editor->right_width + 18.0F;
    float hue = editor->color_hue;
    float saturation = 0.0F;
    float value = 0.0F;
    AfColor color;
    if (node == NULL) {
        return;
    }
    color = node->fill;
    af_editor_color_hsv(color, &hue, &saturation, &value);
    if (saturation <= 0.0001F) {
        hue = editor->color_hue;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_SV) {
        saturation = af_editor_clamp((x - panel_x) / AF_EDITOR_COLOR_SV_WIDTH, 0.0F, 1.0F);
        value = 1.0F -
                af_editor_clamp((y - AF_EDITOR_COLOR_SV_Y) / AF_EDITOR_COLOR_SV_HEIGHT, 0.0F, 1.0F);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_HUE) {
        hue = af_editor_clamp((y - AF_EDITOR_COLOR_SV_Y) / AF_EDITOR_COLOR_SV_HEIGHT, 0.0F, 1.0F);
        editor->color_hue = hue;
    } else if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_ALPHA) {
        color.a = af_editor_clamp(
            (x - panel_x) / (AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH), 0.0F, 1.0F);
    }
    {
        AfColor result = af_editor_hsv_color(hue, saturation, value, color.a);
        af_editor_apply_fill(editor, result);
    }
}

static void af_editor_apply_style_action(AfSceneNode *node, AfStyle *style, uint32_t action) {
    if (action == 0U) {
        style->stroke.enabled = !style->stroke.enabled;
        (void)af_style_mark_stroke_geometry_changed(style);
    } else if (action == 1U || action == 2U) {
        const float amount = action == 1U ? -1.0F : 1.0F;
        style->stroke.width = af_editor_clamp(style->stroke.width + amount, 0.0F, 1000.0F);
        style->stroke.end_width = style->stroke.width;
        (void)af_style_mark_stroke_geometry_changed(style);
    } else if (action == 3U) {
        const AfPaintType next = (AfPaintType)(((uint32_t)style->fill.type + 1U) % 4U);
        if (next == AF_PAINT_SOLID) {
            style->fill = af_paint_solid(node->fill);
        } else {
            style->fill = (AfPaint){.type = next,
                                    .solid = node->fill,
                                    .start = {0.0F, 0.0F},
                                    .end = {node->bounds.x, node->bounds.y},
                                    .end_radius = fmaxf(node->bounds.x, node->bounds.y),
                                    .transform = {.m00 = 1.0F, .m11 = 1.0F},
                                    .stops = {{.offset = 0.0F, .color = node->fill},
                                              {.offset = 1.0F,
                                               .color = {1.0F - node->fill.r, 1.0F - node->fill.g,
                                                         1.0F - node->fill.b, node->fill.a}}},
                                    .stop_count = 2U};
        }
        (void)af_style_mark_paint_changed(style);
    } else if (action == 4U) {
        style->stroke.cap = (AfStrokeCap)(((uint32_t)style->stroke.cap + 1U) % 3U);
        (void)af_style_mark_stroke_geometry_changed(style);
    } else if (action == 5U) {
        style->stroke.join = (AfStrokeJoin)(((uint32_t)style->stroke.join + 1U) % 3U);
        (void)af_style_mark_stroke_geometry_changed(style);
    }
}

static void af_editor_modify_style(AfEditorState *editor, uint32_t action) {
    if (editor->selected_id == 0U) {
        return;
    }
    af_editor_begin_selection_modify(editor, false);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        const uint64_t id = editor->selection_drag_before[index].node.id;
        AfSceneNode *node = af_scene_find(&editor->project.scene, id);
        AfStyle *style = af_project_find_node_style(&editor->project, id);
        if (node != NULL && style != NULL) {
            af_editor_apply_style_action(node, style, action);
        }
    }
    (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
}

static AfLayoutNode af_editor_next_layout(const AfSceneNode *node, const AfLayoutNode *existing,
                                          uint32_t action) {
    AfLayoutNode layout = existing == NULL ? af_layout_node_default(node->id) : *existing;
    if (action == 0U) {
        static const AfVec2 anchors[3] = {{0.0F, 0.0F}, {0.5F, 0.5F}, {1.0F, 1.0F}};
        uint32_t next = 0U;
        if (layout.anchor_min.x == 0.0F) {
            next = 1U;
        } else if (layout.anchor_min.x == 0.5F) {
            next = 2U;
        }
        layout.anchor_min = anchors[next];
        layout.anchor_max = anchors[next];
        layout.pivot = anchors[next];
    } else if (action == 1U) {
        layout.width.mode = (AfLayoutValueMode)(((uint32_t)layout.width.mode + 1U) % 3U);
        layout.width.value = layout.width.mode == AF_LAYOUT_VALUE_PERCENT ? 0.5F : node->bounds.x;
    } else if (action == 2U) {
        layout.height.mode = (AfLayoutValueMode)(((uint32_t)layout.height.mode + 1U) % 3U);
        layout.height.value = layout.height.mode == AF_LAYOUT_VALUE_PERCENT ? 0.5F : node->bounds.y;
    } else if (action == 3U) {
        layout.direction = (AfLayoutDirection)(((uint32_t)layout.direction + 1U) % 3U);
        layout.spacing = layout.direction == AF_LAYOUT_DIRECTION_NONE ? 0.0F : 8.0F;
    } else if (action == 4U) {
        layout.child_alignment = (AfLayoutAlignment)(((uint32_t)layout.child_alignment + 1U) % 4U);
    } else if (action == 5U) {
        layout.aspect = (AfAspectConstraint)(((uint32_t)layout.aspect + 1U) % 4U);
        layout.aspect_ratio = node->bounds.y > 0.0F ? node->bounds.x / node->bounds.y : 1.0F;
    }
    return layout;
}

static void af_editor_modify_layout(AfEditorState *editor, uint32_t action) {
    if (editor->selected_id == 0U) {
        return;
    }
    af_editor_begin_selection_modify(editor, false);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        const uint64_t id = editor->selection_drag_before[index].node.id;
        const AfSceneNode *node = af_scene_find_const(&editor->project.scene, id);
        const AfLayoutNode *existing = af_layout_find_const(&editor->project.layout_data, id);
        if (node != NULL) {
            const AfLayoutNode layout = af_editor_next_layout(node, existing, action);
            if (af_project_set_layout(&editor->project, &layout) != AF_RESULT_SUCCESS) {
                return;
            }
        }
    }
    (void)af_scene_evaluate_world(&editor->project.scene);
    (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
}

static void af_editor_nudge(AfEditorState *editor, float x, float y) {
    if (editor->selected_id == 0U) {
        return;
    }
    af_editor_begin_selection_modify(editor, true);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        AfSceneNode *node =
            af_scene_find(&editor->project.scene, editor->selection_drag_before[index].node.id);
        if (node == NULL) {
            continue;
        }
        node->local_transform.position.x += x;
        node->local_transform.position.y += y;
        if (editor->snapping) {
            node->local_transform.position.x =
                roundf(node->local_transform.position.x / editor->grid_step) * editor->grid_step;
            node->local_transform.position.y =
                roundf(node->local_transform.position.y / editor->grid_step) * editor->grid_step;
        }
    }
    (void)af_scene_evaluate_world(&editor->project.scene);
    (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_TRANSFORM, true);
}

static void af_editor_reparent(AfEditorState *editor, uint64_t parent_id) {
    AfProjectNodeSnapshot before = {0};
    AfSceneNode *parent = af_scene_find(&editor->project.scene, parent_id);
    if (editor->selected_id == 0U || editor->selected_id == parent_id ||
        (parent_id != AF_INVALID_NODE_ID && (parent == NULL || parent->type != AF_NODE_GROUP)) ||
        af_project_snapshot(&editor->project, editor->selected_id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    if (af_scene_reparent(&editor->project.scene, editor->selected_id, parent_id) ==
        AF_RESULT_SUCCESS) {
        (void)af_scene_evaluate_world(&editor->project.scene);
        (void)af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_PARENT, false);
        af_editor_status(editor,
                         parent_id == AF_INVALID_NODE_ID ? "Node unparented" : "Node parented");
    }
}

static void af_editor_handle_inspector(AfEditorState *editor, int width, float x, float y) {
    const float panel_x = (float)width - editor->right_width + 18.0F;
    AfParticleEmitter *emitter =
        af_project_find_node_emitter(&editor->project, editor->selected_id);
    if (af_scene_find_const(&editor->project.scene, editor->selected_id) != NULL &&
        ((x >= panel_x && x <= panel_x + AF_EDITOR_COLOR_SV_WIDTH && y >= AF_EDITOR_COLOR_SV_Y &&
          y <= AF_EDITOR_COLOR_SV_Y + AF_EDITOR_COLOR_SV_HEIGHT) ||
         (x >= panel_x + AF_EDITOR_COLOR_HUE_X &&
          x <= panel_x + AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH &&
          y >= AF_EDITOR_COLOR_SV_Y && y <= AF_EDITOR_COLOR_SV_Y + AF_EDITOR_COLOR_SV_HEIGHT) ||
         (x >= panel_x && x <= panel_x + AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH &&
          y >= AF_EDITOR_COLOR_ALPHA_Y &&
          y <= AF_EDITOR_COLOR_ALPHA_Y + AF_EDITOR_COLOR_ALPHA_HEIGHT))) {
        af_editor_begin_selection_modify(editor, false);
        if (editor->selection_drag_count == 0U) {
            return;
        }
        if (y >= AF_EDITOR_COLOR_ALPHA_Y) {
            editor->drag_mode = AF_EDITOR_DRAG_COLOR_ALPHA;
        } else if (x >= panel_x + AF_EDITOR_COLOR_HUE_X) {
            editor->drag_mode = AF_EDITOR_DRAG_COLOR_HUE;
        } else {
            editor->drag_mode = AF_EDITOR_DRAG_COLOR_SV;
        }
        af_editor_update_color_picker(editor, width, x, y);
        return;
    }
    if (emitter != NULL && y >= AF_EDITOR_PARTICLE_ROW_Y &&
        y < AF_EDITOR_PARTICLE_ROW_Y + AF_EDITOR_PARTICLE_ROW_COUNT * AF_EDITOR_PARTICLE_ROW_STEP) {
        AfProjectNodeSnapshot before = {0};
        const uint32_t row =
            (uint32_t)((y - AF_EDITOR_PARTICLE_ROW_Y) / AF_EDITOR_PARTICLE_ROW_STEP);
        const bool decrease = x < panel_x + 31.0F;
        if (x <= panel_x + 65.0F && row < AF_EDITOR_PARTICLE_ROW_COUNT &&
            af_project_snapshot(&editor->project, editor->selected_id, &before) ==
                AF_RESULT_SUCCESS) {
            if (row == 0U) {
                emitter->spawn_rate = fmaxf(0.0F, emitter->spawn_rate + (decrease ? -8.0F : 8.0F));
            } else if (row == 1U) {
                const float amount = decrease ? -0.1F : 0.1F;
                emitter->lifetime_min = fmaxf(0.1F, emitter->lifetime_min + amount);
                emitter->lifetime_max =
                    fmaxf(emitter->lifetime_min, emitter->lifetime_max + amount);
            } else if (row == 2U) {
                const uint32_t amount = 256U;
                if (decrease) {
                    emitter->capacity =
                        emitter->capacity > amount ? emitter->capacity - amount : 1U;
                } else {
                    const uint32_t total =
                        af_particle_total_capacity(&editor->project.particle_data);
                    emitter->capacity = total <= AF_MAX_PARTICLES - amount
                                            ? emitter->capacity + amount
                                            : emitter->capacity;
                }
                emitter->burst_count = emitter->burst_count > emitter->capacity
                                           ? emitter->capacity
                                           : emitter->burst_count;
            } else if (row == 3U) {
                int type = (int)emitter->type + (decrease ? -1 : 1);
                type = type < 0 ? (int)AF_PARTICLE_EMITTER_VECTOR_PATH
                                : (type > (int)AF_PARTICLE_EMITTER_VECTOR_PATH ? 0 : type);
                if (type == (int)AF_PARTICLE_EMITTER_VECTOR_PATH &&
                    af_path_find_node_const(&editor->project.path_data, editor->selected_id) ==
                        NULL) {
                    type =
                        decrease ? (int)AF_PARTICLE_EMITTER_CIRCLE : (int)AF_PARTICLE_EMITTER_POINT;
                }
                emitter->type = (AfParticleEmitterType)type;
                emitter->path_id =
                    emitter->type == AF_PARTICLE_EMITTER_VECTOR_PATH
                        ? af_path_find_node_const(&editor->project.path_data, editor->selected_id)
                              ->id
                        : 0U;
            } else if (row == 4U) {
                int output = (int)emitter->output + (decrease ? -1 : 1);
                output = output < 0 ? (int)AF_PARTICLE_OUTPUT_LIGHT_PROXY
                                    : (output > (int)AF_PARTICLE_OUTPUT_LIGHT_PROXY ? 0 : output);
                emitter->output = (AfParticleOutputType)output;
            } else if (row == 5U) {
                emitter->turbulence_strength =
                    af_editor_clamp(emitter->turbulence_strength + (decrease ? -20.0F : 20.0F),
                                    -AF_MAX_PARTICLE_FORCE, AF_MAX_PARTICLE_FORCE);
            } else if (row == 6U) {
                emitter->radial_force =
                    af_editor_clamp(emitter->radial_force + (decrease ? -25.0F : 25.0F),
                                    -AF_MAX_PARTICLE_FORCE, AF_MAX_PARTICLE_FORCE);
            } else if (row == 7U) {
                emitter->vortex_force =
                    af_editor_clamp(emitter->vortex_force + (decrease ? -25.0F : 25.0F),
                                    -AF_MAX_PARTICLE_FORCE, AF_MAX_PARTICLE_FORCE);
            } else if (row == 8U) {
                const AfSceneNode *node =
                    af_scene_find_const(&editor->project.scene, editor->selected_id);
                emitter->collision_plane_enabled = !emitter->collision_plane_enabled;
                emitter->collision_plane_normal = (AfVec2){0.0F, -1.0F};
                if (node != NULL) {
                    emitter->collision_plane_offset = -(node->bounds.y + 80.0F);
                }
            } else {
                if (decrease) {
                    emitter->event_burst_count =
                        emitter->event_burst_count > 16U ? emitter->event_burst_count - 16U : 0U;
                } else {
                    emitter->event_burst_count =
                        emitter->event_burst_count <=
                                emitter->capacity -
                                    (emitter->capacity < 16U ? emitter->capacity : 16U)
                            ? emitter->event_burst_count +
                                  (emitter->capacity < 16U ? emitter->capacity : 16U)
                            : emitter->capacity;
                }
                emitter->event_sequence =
                    emitter->event_sequence == UINT32_MAX ? 1U : emitter->event_sequence + 1U;
            }
            (void)af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_PARTICLE, true);
            editor->dirty = true;
            af_editor_status(editor, "Particle emitter updated");
        }
        return;
    }
    if (y >= AF_EDITOR_STYLE_ROW_Y && y <= AF_EDITOR_STYLE_ROW_Y + 28.0F) {
        const int action = (int)((x - panel_x) / 38.0F);
        if (action >= 0 && action < 6) {
            af_editor_modify_style(editor, (uint32_t)action);
        }
        return;
    }
    if (y >= AF_EDITOR_LAYOUT_ROW_Y && y <= AF_EDITOR_LAYOUT_ROW_Y + 28.0F) {
        const int action = (int)((x - panel_x) / 38.0F);
        if (action >= 0 && action < 6) {
            af_editor_modify_layout(editor, (uint32_t)action);
        }
        return;
    }
    for (int row = 0; row < 4; ++row) {
        const float row_y =
            AF_EDITOR_INSPECTOR_SCALAR_Y + (float)row * AF_EDITOR_INSPECTOR_SCALAR_STEP;
        if (y >= row_y && y <= row_y + 26.0F) {
            const float amount = x < panel_x + 35.0F ? -1.0F : 1.0F;
            if (row == 0) {
                af_editor_modify_scalar(editor, AF_EDITOR_MERGE_OPACITY, amount * 0.05F, 0);
            } else if (row == 1) {
                af_editor_modify_scalar(editor, AF_EDITOR_MERGE_RADIUS, amount * 2.0F, 1);
            } else if (row == 2) {
                af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, amount, 2);
            } else {
                af_editor_modify_scalar(editor, AF_EDITOR_MERGE_TRANSFORM, amount * 0.05F, 3);
            }
            return;
        }
    }
}

static void af_editor_reset_selected(AfEditorState *editor, bool appearance) {
    if (editor->selected_id == 0U) {
        return;
    }
    af_editor_begin_selection_modify(editor, false);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        AfSceneNode *node =
            af_scene_find(&editor->project.scene, editor->selection_drag_before[index].node.id);
        if (node == NULL) {
            continue;
        }
        if (appearance) {
            AfStyle *style = af_project_find_node_style(&editor->project, node->id);
            const AfSceneNode defaults = af_editor_default_node(node->type, node->id, (AfVec2){0});
            node->opacity = 1.0F;
            node->fill = defaults.fill;
            node->corner_radius = defaults.corner_radius;
            if (style != NULL) {
                *style = af_style_default(style->id, node->id, defaults.fill);
            }
        } else {
            node->local_transform = (AfTransform2D){.scale = {1.0F, 1.0F}};
        }
    }
    (void)af_scene_evaluate_world(&editor->project.scene);
    if (af_editor_commit_selection_modify(
            editor, appearance ? AF_EDITOR_MERGE_STYLE : AF_EDITOR_MERGE_TRANSFORM, false) ==
        AF_RESULT_SUCCESS) {
        af_editor_status(editor, appearance ? "Appearance reset" : "Transform reset");
    }
}

static void af_editor_convert_selected(AfEditorState *editor, AfNodeType type) {
    AfProjectNodeSnapshot before = {0};
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    if (node == NULL || node->type == AF_NODE_GROUP || node->type == AF_NODE_PATH ||
        af_project_snapshot(&editor->project, node->id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    node->type = type;
    node->corner_radius =
        type == AF_NODE_ROUNDED_RECTANGLE ? fmaxf(node->corner_radius, 18.0F) : 0.0F;
    if (af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_STYLE, false) ==
        AF_RESULT_SUCCESS) {
        af_editor_status(editor, type == AF_NODE_ROUNDED_RECTANGLE
                                     ? "Converted to rounded rectangle"
                                     : "Converted to rectangle");
    }
}

static void af_editor_make_selected_circle(AfEditorState *editor) {
    AfProjectNodeSnapshot before = {0};
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    if (node == NULL || node->type != AF_NODE_ELLIPSE ||
        af_project_snapshot(&editor->project, node->id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    const float size = fminf(node->bounds.x, node->bounds.y);
    node->local_transform.position.x += (node->bounds.x - size) * 0.5F;
    node->local_transform.position.y += (node->bounds.y - size) * 0.5F;
    node->bounds = (AfVec2){size, size};
    (void)af_scene_evaluate_world(&editor->project.scene);
    if (af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_TRANSFORM, false) ==
        AF_RESULT_SUCCESS) {
        af_editor_status(editor, "Ellipse made circular");
    }
}

static void af_editor_fit_selected_to_canvas(AfEditorState *editor, int width, int height) {
    AfProjectNodeSnapshot before = {0};
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    AfAffine2 parent_inverse = {.m00 = 1.0F, .m11 = 1.0F};
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    AfVec2 corners[4];
    float minimum_x = 0.0F;
    float minimum_y = 0.0F;
    float maximum_x = 0.0F;
    float maximum_y = 0.0F;
    if (node == NULL || (node->type != AF_NODE_RECTANGLE && node->type != AF_NODE_ELLIPSE) ||
        af_project_snapshot(&editor->project, node->id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    if (node->parent_id != AF_INVALID_NODE_ID) {
        const AfSceneNode *parent = af_scene_find_const(&editor->project.scene, node->parent_id);
        if (parent == NULL || !af_affine_inverse(parent->world_transform, &parent_inverse)) {
            return;
        }
    }
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    corners[0] = af_editor_screen_to_project(editor, width, height,
                                             (AfVec2){canvas_x + 24.0F, canvas_y + 24.0F});
    corners[1] = af_editor_screen_to_project(
        editor, width, height, (AfVec2){canvas_x + canvas_width - 24.0F, canvas_y + 24.0F});
    corners[2] = af_editor_screen_to_project(
        editor, width, height,
        (AfVec2){canvas_x + canvas_width - 24.0F, canvas_y + canvas_height - 24.0F});
    corners[3] = af_editor_screen_to_project(
        editor, width, height, (AfVec2){canvas_x + 24.0F, canvas_y + canvas_height - 24.0F});
    for (uint32_t index = 0U; index < 4U; ++index) {
        corners[index] = af_affine_transform_point(parent_inverse, corners[index]);
        if (index == 0U) {
            minimum_x = maximum_x = corners[index].x;
            minimum_y = maximum_y = corners[index].y;
        } else {
            minimum_x = fminf(minimum_x, corners[index].x);
            minimum_y = fminf(minimum_y, corners[index].y);
            maximum_x = fmaxf(maximum_x, corners[index].x);
            maximum_y = fmaxf(maximum_y, corners[index].y);
        }
    }
    node->local_transform =
        (AfTransform2D){.position = {minimum_x, minimum_y}, .scale = {1.0F, 1.0F}};
    node->bounds = (AfVec2){maximum_x - minimum_x, maximum_y - minimum_y};
    (void)af_scene_evaluate_world(&editor->project.scene);
    if (af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_TRANSFORM, false) ==
        AF_RESULT_SUCCESS) {
        af_editor_status(editor, "Object fitted to visible canvas");
    }
}

static void af_editor_toggle_selected_particles(AfEditorState *editor) {
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    AfParticleEmitter *existing =
        af_project_find_node_emitter(&editor->project, editor->selected_id);
    AfProjectNodeSnapshot before = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (node == NULL ||
        af_project_snapshot(&editor->project, node->id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    if (existing != NULL) {
        result = af_project_remove_emitter(&editor->project, existing->id);
        af_editor_status(editor, result == AF_RESULT_SUCCESS ? "Particle emitter removed"
                                                             : "Particle emitter removal failed");
    } else {
        uint64_t id = 0U;
        result = af_project_allocate_animation_id(&editor->project, &id);
        if (result == AF_RESULT_SUCCESS) {
            AfParticleEmitter emitter = af_particle_emitter_default(id, node->id);
            if (node->type == AF_NODE_RECTANGLE || node->type == AF_NODE_ROUNDED_RECTANGLE) {
                emitter.type = AF_PARTICLE_EMITTER_RECTANGLE;
            } else if (node->type == AF_NODE_ELLIPSE) {
                emitter.type = AF_PARTICLE_EMITTER_CIRCLE;
            } else if (node->type == AF_NODE_PATH) {
                const AfPath *path = af_path_find_node_const(&editor->project.path_data, node->id);
                emitter.type = AF_PARTICLE_EMITTER_VECTOR_PATH;
                emitter.path_id = path == NULL ? 0U : path->id;
            }
            emitter.output = AF_PARTICLE_OUTPUT_UI_BILLBOARD;
            result = af_project_add_emitter(&editor->project, &emitter);
        }
        af_editor_status(editor, result == AF_RESULT_SUCCESS ? "Particle emitter added"
                                                             : "Particle emitter creation failed");
    }
    if (result == AF_RESULT_SUCCESS) {
        (void)af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_PARTICLE, false);
        editor->dirty = true;
    }
}

static void af_editor_execute_object_context_item(AfEditorState *editor, uint32_t item, int width,
                                                  int height) {
    const AfEditorContextMenu menu = editor->context_menu;
    if (item == 0U) {
        (void)af_editor_duplicate_selection(editor);
        return;
    }
    if (item == 1U) {
        af_editor_begin_rename(editor);
        return;
    }
    if (item == 4U) {
        af_editor_toggle_selected_particles(editor);
        return;
    }
    if (menu == AF_EDITOR_CONTEXT_RECTANGLE) {
        if (item == 2U) {
            af_editor_convert_selected(editor, AF_NODE_ROUNDED_RECTANGLE);
        } else if (item == 3U) {
            af_editor_fit_selected_to_canvas(editor, width, height);
        } else if (item == 5U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, 1.0F, 2);
        } else if (item == 6U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (menu == AF_EDITOR_CONTEXT_ROUNDED_RECTANGLE) {
        if (item == 2U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_RADIUS, 4.0F, 1);
        } else if (item == 3U) {
            af_editor_convert_selected(editor, AF_NODE_RECTANGLE);
        } else if (item == 5U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, 1.0F, 2);
        } else if (item == 6U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (menu == AF_EDITOR_CONTEXT_ELLIPSE) {
        if (item == 2U) {
            af_editor_make_selected_circle(editor);
        } else if (item == 3U) {
            af_editor_fit_selected_to_canvas(editor, width, height);
        } else if (item == 5U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, 1.0F, 2);
        } else if (item == 6U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (menu == AF_EDITOR_CONTEXT_PATH) {
        if (item == 2U) {
            (void)af_editor_toggle_path_fill_rule(editor);
        } else if (item == 3U) {
            editor->selected_path_command = 0U;
            af_editor_status(editor, "Path points ready for editing");
        } else if (item == 5U) {
            (void)af_editor_reverse_path_contour(editor);
        } else if (item == 6U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (menu == AF_EDITOR_CONTEXT_GROUP) {
        if (item == 2U) {
            const uint64_t parent_id = editor->selected_id;
            if (af_editor_create_primitive(editor, AF_NODE_RECTANGLE, width, height) ==
                AF_RESULT_SUCCESS) {
                af_editor_reparent(editor, parent_id);
                af_editor_status(editor, "Rectangle child added to group");
            }
        } else if (item == 3U) {
            af_editor_reparent(editor, AF_INVALID_NODE_ID);
        } else if (item == 5U) {
            (void)af_editor_delete_selection(editor);
        }
    }
}

static void af_editor_execute_context_item(AfEditorState *editor, uint32_t item, int width,
                                           int height) {
    if (editor->context_menu == AF_EDITOR_CONTEXT_HIERARCHY) {
        if (item == 0U) {
            (void)af_editor_duplicate_selection(editor);
        } else if (item == 1U) {
            af_editor_begin_rename(editor);
        } else if (item == 2U) {
            af_editor_reparent(editor, AF_INVALID_NODE_ID);
        } else if (item == 3U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (editor->context_menu == AF_EDITOR_CONTEXT_CANVAS) {
        if (item <= 2U) {
            static const AfNodeType types[] = {AF_NODE_RECTANGLE, AF_NODE_ROUNDED_RECTANGLE,
                                               AF_NODE_ELLIPSE};
            (void)af_editor_create_primitive(editor, types[item], width, height);
        } else if (item == 3U) {
            (void)af_editor_create_path(editor, width, height);
        } else if (item == 4U) {
            editor->grid_visible = !editor->grid_visible;
            af_editor_status(editor, editor->grid_visible ? "Grid enabled" : "Grid disabled");
        } else if (item == 5U) {
            editor->snapping = !editor->snapping;
            af_editor_status(editor, editor->snapping ? "Snapping enabled" : "Snapping disabled");
        }
    } else if (editor->context_menu == AF_EDITOR_CONTEXT_INSPECTOR) {
        if (item == 0U) {
            af_editor_reset_selected(editor, false);
        } else if (item == 1U) {
            af_editor_reset_selected(editor, true);
        } else if (item == 2U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, 1.0F, 2);
        } else if (item == 3U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, -1.0F, 2);
        } else if (item == 4U) {
            (void)af_editor_duplicate_selection(editor);
        } else if (item == 5U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (editor->context_menu == AF_EDITOR_CONTEXT_TIMELINE) {
        editor->timeline_focus = true;
        if (item == 0U) {
            (void)af_editor_add_keyframe(editor);
        } else if (item == 1U) {
            (void)af_editor_delete_keyframes(editor);
        } else if (item == 2U) {
            af_editor_copy_keyframes(editor);
        } else if (item == 3U) {
            (void)af_editor_paste_keyframes(editor, false);
        } else if (item == 4U) {
            editor->curve_visible = !editor->curve_visible;
        } else if (item == 5U) {
            (void)af_editor_new_clip(editor);
        }
    } else if (editor->context_menu == AF_EDITOR_CONTEXT_TOOLBAR) {
        if (item == 0U) {
            (void)af_editor_save(editor);
        } else if (item == 1U) {
            (void)af_editor_compile(editor);
        } else if (item == 2U) {
            editor->grid_visible = !editor->grid_visible;
        } else if (item == 3U) {
            editor->snapping = !editor->snapping;
        } else if (item == 4U) {
            if (af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
                editor->dirty = true;
                af_editor_reconcile_selection(editor);
                af_editor_status(editor, "Undo");
            }
        } else if (item == 5U && af_editor_history_redo(&editor->history, &editor->project) ==
                                     AF_RESULT_SUCCESS) {
            editor->dirty = true;
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Redo");
        }
    } else {
        af_editor_execute_object_context_item(editor, item, width, height);
    }
}

static int32_t af_editor_context_item_at(const AfEditorState *editor, int width, int height,
                                         float x, float y) {
    const AfEditorContextMenuInfo *info = af_editor_context_info(editor->context_menu);
    const AfVec2 origin = af_editor_context_origin(editor, width, height);
    const float first_y = origin.y + AF_EDITOR_CONTEXT_HEADER_HEIGHT + 4.0F;
    if (info == NULL || x < origin.x || x > origin.x + AF_EDITOR_CONTEXT_WIDTH || y < first_y) {
        return -1;
    }
    const uint32_t item = (uint32_t)((y - first_y) / AF_EDITOR_CONTEXT_ITEM_HEIGHT);
    return item < info->item_count ? (int32_t)item : -1;
}

static void af_editor_open_context_menu(AfEditorState *editor, int width, int height, float x,
                                        float y) {
    const float timeline_top = af_editor_timeline_top(editor, height);
    editor->context_position = (AfVec2){x, y};
    editor->mouse_position = editor->context_position;
    if (x < editor->left_width) {
        const float hierarchy_top = AF_EDITOR_TOOLBAR_HEIGHT + 8.0F;
        editor->context_menu = AF_EDITOR_CONTEXT_HIERARCHY;
        if (y >= hierarchy_top) {
            const uint32_t row = (uint32_t)((y - hierarchy_top) / AF_EDITOR_ROW_HEIGHT);
            const uint32_t hierarchy_index = editor->hierarchy_scroll + row;
            if (hierarchy_index < editor->hierarchy_count) {
                const uint64_t clicked =
                    editor->project.scene.nodes[editor->hierarchy_order[hierarchy_index]].id;
                if (!af_editor_selection_contains(editor, clicked)) {
                    af_editor_select_only(editor, clicked);
                } else {
                    editor->selected_id = clicked;
                }
                af_editor_reconcile_selection(editor);
                editor->context_menu = af_editor_context_for_node(
                    af_scene_find_const(&editor->project.scene, editor->selected_id));
            }
        }
    } else if (x >= (float)width - editor->right_width) {
        editor->context_menu = AF_EDITOR_CONTEXT_INSPECTOR;
    } else if (y < AF_EDITOR_TOOLBAR_HEIGHT) {
        editor->context_menu = AF_EDITOR_CONTEXT_TOOLBAR;
    } else if (y >= timeline_top) {
        editor->context_menu = AF_EDITOR_CONTEXT_TIMELINE;
        editor->timeline_focus = true;
    } else {
        const uint64_t hit = af_editor_hit_test(
            editor, af_editor_screen_to_project(editor, width, height, (AfVec2){x, y}));
        if (hit != 0U) {
            if (!af_editor_selection_contains(editor, hit)) {
                af_editor_select_only(editor, hit);
            } else {
                editor->selected_id = hit;
            }
            af_editor_reconcile_selection(editor);
            editor->context_menu = af_editor_context_for_node(
                af_scene_find_const(&editor->project.scene, editor->selected_id));
        } else {
            editor->context_menu = AF_EDITOR_CONTEXT_CANVAS;
        }
    }
}

static void af_editor_begin_selected_key_drag(AfEditorState *editor) {
    uint32_t index = 0U;
    editor->key_drag_count = 0U;
    for (index = 0U; index < editor->selected_keyframe_count; ++index) {
        if (af_project_keyframe_snapshot(&editor->project, editor->selected_keyframes[index],
                                         &editor->key_drag_before[editor->key_drag_count]) ==
            AF_RESULT_SUCCESS) {
            ++editor->key_drag_count;
        }
    }
}

static void af_editor_timeline_commit_keys(AfEditorState *editor, AfEditorMergeKey merge_key) {
    AfEditorCommand command = {0};
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (editor->key_drag_count == 0U ||
        af_editor_command_animation(merge_key, editor->key_drag_before[0].keyframe.id, &command) !=
            AF_RESULT_SUCCESS) {
        return;
    }
    command.keyframe_before_count = editor->key_drag_count;
    command.keyframe_after_count = editor->key_drag_count;
    for (index = 0U; index < editor->key_drag_count; ++index) {
        command.keyframe_before[index] = editor->key_drag_before[index];
        result = af_project_keyframe_snapshot(&editor->project,
                                              editor->key_drag_before[index].keyframe.id,
                                              &command.keyframe_after[index]);
        if (result != AF_RESULT_SUCCESS) {
            return;
        }
    }
    result = af_editor_history_execute(&editor->history, &editor->project, &command, true, false);
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
        af_editor_status(editor, "Timeline edit committed");
    }
}

static void af_editor_timeline_commit_clip(AfEditorState *editor) {
    AfEditorCommand command = {0};
    if (af_editor_command_animation(AF_EDITOR_MERGE_LOOP_REGION, editor->clip_drag_before.clip.id,
                                    &command) != AF_RESULT_SUCCESS) {
        return;
    }
    command.clip_before[0] = editor->clip_drag_before;
    command.clip_before_count = 1U;
    if (af_project_clip_snapshot(&editor->project, editor->clip_drag_before.clip.id,
                                 &command.clip_after[0]) != AF_RESULT_SUCCESS) {
        return;
    }
    command.clip_after_count = 1U;
    if (af_editor_history_execute(&editor->history, &editor->project, &command, true, false) ==
        AF_RESULT_SUCCESS) {
        editor->dirty = true;
        af_editor_status(editor, "Loop region edited");
    }
}

static bool af_editor_timeline_mouse_down(AfEditorState *editor, int width, int height,
                                          const SDL_MouseButtonEvent *event, SDL_Keymod modifiers) {
    const float top = af_editor_timeline_top(editor, height);
    const float ruler_x = af_editor_timeline_ruler_x(editor);
    const float center_right = (float)width - editor->right_width;
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    uint32_t index = 0U;
    if (event->y < top - 5.0F || event->x < editor->left_width || event->x > center_right) {
        return false;
    }
    editor->timeline_focus = true;
    if (fabsf(event->y - top) <= 5.0F && event->button == SDL_BUTTON_LEFT) {
        editor->drag_mode = AF_EDITOR_DRAG_TIMELINE_HEIGHT;
        return true;
    }
    if (event->button == SDL_BUTTON_MIDDLE) {
        editor->drag_mode = AF_EDITOR_DRAG_TIMELINE_PAN;
        editor->drag_start_mouse = (AfVec2){event->x, event->y};
        return true;
    }
    if (event->button != SDL_BUTTON_LEFT) {
        return true;
    }
    if (event->y < top + 40.0F) {
        const float local_x = event->x - editor->left_width;
        if (local_x >= 10.0F && local_x < 36.0F) {
            editor->preview_playing = !editor->preview_playing;
        } else if (local_x >= 42.0F && local_x < 68.0F) {
            editor->preview_playing = false;
            editor->playhead = 0.0F;
        } else if (local_x >= 73.0F && local_x < 107.0F) {
            editor->curve_visible = !editor->curve_visible;
        } else if (local_x >= 116.0F && local_x < 236.0F) {
            const uint32_t property = (uint32_t)((local_x - 116.0F) / 20.0F);
            if (property <= AF_PROPERTY_CORNER_RADIUS) {
                editor->selected_property = (AfPropertyType)property;
                af_editor_clear_key_selection(editor);
            }
        } else {
            for (index = 0U; index < editor->project.clip_count && index < 8U; ++index) {
                const float x = center_right - 74.0F * (float)(index + 1U);
                if (event->x >= x && event->x <= x + 68.0F) {
                    editor->selected_clip_id = editor->project.clips[index].id;
                    editor->playhead = 0.0F;
                    af_editor_clear_key_selection(editor);
                    break;
                }
            }
        }
        return true;
    }
    if (clip == NULL) {
        return true;
    }
    if (event->y < top + AF_EDITOR_TIMELINE_RULER_HEIGHT) {
        const float start_x = af_editor_time_to_x(editor, clip->loop_start);
        const float end_x = af_editor_time_to_x(editor, clip->loop_end);
        if (clip->loop && fabsf(event->x - start_x) <= 7.0F) {
            editor->drag_mode = AF_EDITOR_DRAG_LOOP_START;
            (void)af_project_clip_snapshot(&editor->project, clip->id, &editor->clip_drag_before);
        } else if (clip->loop && fabsf(event->x - end_x) <= 7.0F) {
            editor->drag_mode = AF_EDITOR_DRAG_LOOP_END;
            (void)af_project_clip_snapshot(&editor->project, clip->id, &editor->clip_drag_before);
        } else if (event->x >= ruler_x) {
            editor->drag_mode = AF_EDITOR_DRAG_PLAYHEAD;
            editor->preview_playing = false;
            editor->playhead =
                af_editor_clamp(af_editor_x_to_time(editor, event->x), 0.0F, clip->duration);
        }
        return true;
    }
    if (editor->curve_visible) {
        if (editor->selected_keyframe_count == 1U) {
            AfProjectKeyframe *key =
                af_project_find_keyframe(&editor->project, editor->selected_keyframes[0]);
            if (key == NULL) {
                af_editor_clear_key_selection(editor);
                return true;
            }
            const float curve_top = top + AF_EDITOR_TIMELINE_RULER_HEIGHT;
            const float bottom = (float)height - AF_EDITOR_STATUS_HEIGHT - 12.0F;
            const float out_x = ruler_x + 30.0F + key->keyframe.easing[0] * 100.0F;
            const float out_y = bottom - 30.0F - key->keyframe.easing[1] * 80.0F;
            const float in_x = center_right - 130.0F + key->keyframe.easing[2] * 100.0F;
            const float in_y = bottom - key->keyframe.easing[3] * 80.0F;
            (void)curve_top;
            if (fabsf(event->x - out_x) <= 10.0F && fabsf(event->y - out_y) <= 10.0F) {
                editor->drag_mode = AF_EDITOR_DRAG_TANGENT_OUT;
                af_editor_begin_selected_key_drag(editor);
            } else if (fabsf(event->x - in_x) <= 10.0F && fabsf(event->y - in_y) <= 10.0F) {
                editor->drag_mode = AF_EDITOR_DRAG_TANGENT_IN;
                af_editor_begin_selected_key_drag(editor);
            }
        }
        return true;
    }
    {
        uint64_t hit_id = 0U;
        float hit_distance = 10.0F;
        for (index = 0U; index < editor->project.keyframe_count; ++index) {
            const AfProjectKeyframe *key = &editor->project.keyframes[index];
            const uint32_t row = af_editor_track_row(editor, key->track_id);
            const float x = af_editor_time_to_x(editor, key->keyframe.time);
            const float y = top + AF_EDITOR_TIMELINE_RULER_HEIGHT +
                            (float)row * AF_EDITOR_TIMELINE_TRACK_HEIGHT + 12.0F;
            const float distance = fabsf(event->x - x) + fabsf(event->y - y);
            if (row != UINT32_MAX && distance < hit_distance) {
                hit_id = key->id;
                hit_distance = distance;
            }
        }
        if (hit_id != 0U) {
            af_editor_select_keyframe(editor, hit_id, (modifiers & SDL_KMOD_CTRL) != 0U);
            if (af_editor_keyframe_selected(editor, hit_id)) {
                editor->drag_mode = AF_EDITOR_DRAG_KEYFRAMES;
                editor->drag_start_mouse = (AfVec2){event->x, event->y};
                af_editor_begin_selected_key_drag(editor);
            }
        } else {
            if ((modifiers & SDL_KMOD_CTRL) == 0U) {
                af_editor_clear_key_selection(editor);
            }
            editor->drag_mode = AF_EDITOR_DRAG_KEY_BOX;
            editor->key_box_start = (AfVec2){event->x, event->y};
            editor->key_box_current = editor->key_box_start;
        }
    }
    return true;
}

static void af_editor_timeline_box_select(AfEditorState *editor, int height) {
    const float top = af_editor_timeline_top(editor, height);
    const float left = fminf(editor->key_box_start.x, editor->key_box_current.x);
    const float right = fmaxf(editor->key_box_start.x, editor->key_box_current.x);
    const float box_top = fminf(editor->key_box_start.y, editor->key_box_current.y);
    const float bottom = fmaxf(editor->key_box_start.y, editor->key_box_current.y);
    uint32_t index = 0U;
    for (index = 0U; index < editor->project.keyframe_count; ++index) {
        const AfProjectKeyframe *key = &editor->project.keyframes[index];
        const uint32_t row = af_editor_track_row(editor, key->track_id);
        const float x = af_editor_time_to_x(editor, key->keyframe.time);
        const float y = top + AF_EDITOR_TIMELINE_RULER_HEIGHT +
                        (float)row * AF_EDITOR_TIMELINE_TRACK_HEIGHT + 12.0F;
        if (row != UINT32_MAX && x >= left && x <= right && y >= box_top && y <= bottom &&
            editor->selected_keyframe_count < AF_EDITOR_COMMAND_MAX_KEYFRAMES) {
            editor->selected_keyframes[editor->selected_keyframe_count++] = key->id;
        }
    }
}

static bool af_editor_begin_path_drag(AfEditorState *editor, AfPath *path, AfEditorDragMode mode) {
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        path->geometry_generation == UINT64_MAX ||
        editor->selected_path_command >= path->command_count ||
        af_project_path_snapshot(&editor->project, path->id, &editor->path_drag_before,
                                 editor->path_drag_commands,
                                 AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) != AF_RESULT_SUCCESS) {
        af_editor_status(editor, "Path is too large for bounded drag undo");
        return false;
    }
    editor->path_drag_before.command_offset = 0U;
    editor->drag_mode = mode;
    return true;
}

static bool af_editor_path_mouse_down(AfEditorState *editor, int width, int height, AfVec2 mouse) {
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    AfPath *path = af_editor_selected_path(editor);
    AfAffine2 transform = {0};
    const float threshold_squared = 100.0F;
    if (node == NULL || path == NULL) {
        return false;
    }
    transform = af_affine_multiply(af_editor_camera(editor, width, height), node->world_transform);
    if (editor->selected_path_command < path->command_count) {
        const AfPathCommand *selected =
            &editor->project.path_data
                 .commands[path->command_start + editor->selected_path_command];
        if (selected->type == AF_PATH_CUBIC) {
            const AfVec2 first = af_affine_transform_point(transform, selected->control1);
            const AfVec2 second = af_affine_transform_point(transform, selected->control2);
            const float first_x = mouse.x - first.x;
            const float first_y = mouse.y - first.y;
            const float second_x = mouse.x - second.x;
            const float second_y = mouse.y - second.y;
            if (first_x * first_x + first_y * first_y <= threshold_squared) {
                return af_editor_begin_path_drag(editor, path, AF_EDITOR_DRAG_PATH_CONTROL1);
            }
            if (second_x * second_x + second_y * second_y <= threshold_squared) {
                return af_editor_begin_path_drag(editor, path, AF_EDITOR_DRAG_PATH_CONTROL2);
            }
        }
    }
    for (uint32_t index = path->command_count; index > 0U;) {
        const uint32_t command_index = --index;
        const AfPathCommand *command =
            &editor->project.path_data.commands[path->command_start + command_index];
        AfVec2 point = {0};
        float x = 0.0F;
        float y = 0.0F;
        if (command->type == AF_PATH_CLOSE) {
            continue;
        }
        point = af_affine_transform_point(transform, command->point);
        x = mouse.x - point.x;
        y = mouse.y - point.y;
        if (x * x + y * y <= threshold_squared) {
            editor->selected_path_command = command_index;
            return af_editor_begin_path_drag(editor, path, AF_EDITOR_DRAG_PATH_POINT);
        }
    }
    return false;
}

static void af_editor_drag_path_handle(AfEditorState *editor, int width, int height, AfVec2 mouse) {
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    AfPath *path = af_editor_selected_path(editor);
    AfAffine2 inverse = {0};
    AfVec2 point = {0};
    AfPathCommand *command = NULL;
    const AfPathCommand *before = NULL;
    const uint32_t selected = editor->selected_path_command;
    if (node == NULL || path == NULL || selected >= path->command_count ||
        path->command_count != editor->path_drag_before.path.command_count ||
        !af_affine_inverse(node->world_transform, &inverse)) {
        return;
    }
    point = af_affine_transform_point(inverse,
                                      af_editor_screen_to_project(editor, width, height, mouse));
    if (!isfinite(point.x) || !isfinite(point.y)) {
        return;
    }
    point.x = af_editor_clamp(point.x, -1000000.0F, 1000000.0F);
    point.y = af_editor_clamp(point.y, -1000000.0F, 1000000.0F);
    if (editor->snapping) {
        point.x = roundf(point.x / editor->grid_step) * editor->grid_step;
        point.y = roundf(point.y / editor->grid_step) * editor->grid_step;
    }
    command = &editor->project.path_data.commands[path->command_start + selected];
    before = &editor->path_drag_commands[selected];
    if (editor->drag_mode == AF_EDITOR_DRAG_PATH_POINT && command->type != AF_PATH_CLOSE) {
        const AfVec2 delta = {point.x - before->point.x, point.y - before->point.y};
        command->point = point;
        if (command->type == AF_PATH_CUBIC) {
            command->control2 =
                (AfVec2){before->control2.x + delta.x, before->control2.y + delta.y};
        }
        if (selected + 1U < path->command_count) {
            AfPathCommand *next = command + 1U;
            const AfPathCommand *next_before = before + 1U;
            if (next->type == AF_PATH_CUBIC) {
                next->control1 =
                    (AfVec2){next_before->control1.x + delta.x, next_before->control1.y + delta.y};
            }
        }
    } else if (editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL1 &&
               command->type == AF_PATH_CUBIC) {
        command->control1 = point;
    } else if (editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL2 &&
               command->type == AF_PATH_CUBIC) {
        command->control2 = point;
    } else {
        return;
    }
    command->point.x = af_editor_clamp(command->point.x, -1000000.0F, 1000000.0F);
    command->point.y = af_editor_clamp(command->point.y, -1000000.0F, 1000000.0F);
    command->control1.x = af_editor_clamp(command->control1.x, -1000000.0F, 1000000.0F);
    command->control1.y = af_editor_clamp(command->control1.y, -1000000.0F, 1000000.0F);
    command->control2.x = af_editor_clamp(command->control2.x, -1000000.0F, 1000000.0F);
    command->control2.y = af_editor_clamp(command->control2.y, -1000000.0F, 1000000.0F);
    if (selected + 1U < path->command_count) {
        AfPathCommand *next = command + 1U;
        next->control1.x = af_editor_clamp(next->control1.x, -1000000.0F, 1000000.0F);
        next->control1.y = af_editor_clamp(next->control1.y, -1000000.0F, 1000000.0F);
    }
    (void)af_path_mark_geometry_changed(path);
}

static void af_editor_commit_path_drag(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfEditorCommand command = {0};
    AfResult result = AF_RESULT_NOT_FOUND;
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        path->id != editor->path_drag_before.path.id) {
        return;
    }
    result = af_editor_command_path(AF_EDITOR_MERGE_PATH_POINT, path->id, &command);
    if (result == AF_RESULT_SUCCESS) {
        command.path_before_count = 1U;
        command.path_before[0] = editor->path_drag_before;
        command.path_before[0].command_offset = 0U;
        command.path_command_before_count = editor->path_drag_before.path.command_count;
        memcpy(command.path_command_before, editor->path_drag_commands,
               (size_t)command.path_command_before_count * sizeof(*command.path_command_before));
        command.path_after_count = 1U;
        result = af_project_path_snapshot(&editor->project, path->id, &command.path_after[0],
                                          command.path_command_after,
                                          AF_EDITOR_COMMAND_MAX_PATH_COMMANDS);
    }
    if (result == AF_RESULT_SUCCESS) {
        command.path_after[0].command_offset = 0U;
        command.path_command_after_count = path->command_count;
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, true, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
        af_editor_status(editor, "Path point edited");
    }
}

static void af_editor_canvas_box_select(AfEditorState *editor, int width, int height) {
    const AfAffine2 camera = af_editor_camera(editor, width, height);
    const float box_minimum_x = fminf(editor->canvas_box_start.x, editor->canvas_box_current.x);
    const float box_minimum_y = fminf(editor->canvas_box_start.y, editor->canvas_box_current.y);
    const float box_maximum_x = fmaxf(editor->canvas_box_start.x, editor->canvas_box_current.x);
    const float box_maximum_y = fmaxf(editor->canvas_box_start.y, editor->canvas_box_current.y);
    if (!editor->canvas_box_additive) {
        af_editor_select_only(editor, 0U);
    }
    for (uint32_t index = 0U; index < editor->preview_scene.count; ++index) {
        const AfSceneNode *node = &editor->preview_scene.nodes[index];
        float minimum_x = 0.0F;
        float minimum_y = 0.0F;
        float maximum_x = 0.0F;
        float maximum_y = 0.0F;
        if (af_editor_screen_bounds(node, camera, &minimum_x, &minimum_y, &maximum_x, &maximum_y) &&
            maximum_x >= box_minimum_x && minimum_x <= box_maximum_x &&
            maximum_y >= box_minimum_y && minimum_y <= box_maximum_y) {
            af_editor_selection_add(editor, node->id);
        }
    }
    editor->selected_path_command =
        af_path_find_node_const(&editor->project.path_data, editor->selected_id) != NULL
            ? 0U
            : UINT32_MAX;
    if (editor->selected_count == 0U) {
        af_editor_status(editor, "Selection cleared");
    } else {
        char status[64];
        (void)snprintf(status, sizeof(status), "%u objects selected", editor->selected_count);
        af_editor_status(editor, status);
    }
}

static void af_editor_prepare_selection_smoke(AfEditorState *editor, int width, int height) {
    const AfAffine2 camera = af_editor_camera(editor, width, height);
    bool found = false;
    float box_minimum_x = 0.0F;
    float box_minimum_y = 0.0F;
    float box_maximum_x = 0.0F;
    float box_maximum_y = 0.0F;
    for (uint32_t index = 0U; index < editor->preview_scene.count; ++index) {
        float minimum_x = 0.0F;
        float minimum_y = 0.0F;
        float maximum_x = 0.0F;
        float maximum_y = 0.0F;
        if (!af_editor_screen_bounds(&editor->preview_scene.nodes[index], camera, &minimum_x,
                                     &minimum_y, &maximum_x, &maximum_y)) {
            continue;
        }
        if (!found) {
            box_minimum_x = minimum_x;
            box_minimum_y = minimum_y;
            box_maximum_x = maximum_x;
            box_maximum_y = maximum_y;
            found = true;
        } else {
            box_minimum_x = fminf(box_minimum_x, minimum_x);
            box_minimum_y = fminf(box_minimum_y, minimum_y);
            box_maximum_x = fmaxf(box_maximum_x, maximum_x);
            box_maximum_y = fmaxf(box_maximum_y, maximum_y);
        }
    }
    if (found) {
        editor->canvas_box_start = (AfVec2){box_minimum_x - 8.0F, box_minimum_y - 8.0F};
        editor->canvas_box_current = (AfVec2){box_maximum_x + 8.0F, box_maximum_y + 8.0F};
        editor->canvas_box_additive = false;
        af_editor_canvas_box_select(editor, width, height);
        editor->drag_mode = AF_EDITOR_DRAG_CANVAS_BOX;
    }
}

static void af_editor_mouse_down(AfEditorState *editor, int width, int height,
                                 const SDL_MouseButtonEvent *event, SDL_Keymod modifiers) {
    const float x = event->x;
    const float y = event->y;
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    editor->mouse_position = (AfVec2){x, y};
    if (event->button == SDL_BUTTON_RIGHT) {
        af_editor_open_context_menu(editor, width, height, x, y);
        return;
    }
    if (editor->context_menu != AF_EDITOR_CONTEXT_NONE) {
        const int32_t item = af_editor_context_item_at(editor, width, height, x, y);
        if (event->button == SDL_BUTTON_LEFT && item >= 0) {
            af_editor_execute_context_item(editor, (uint32_t)item, width, height);
        }
        editor->context_menu = AF_EDITOR_CONTEXT_NONE;
        return;
    }
    if (af_editor_timeline_mouse_down(editor, width, height, event, modifiers)) {
        return;
    }
    editor->timeline_focus = false;
    if (event->button == SDL_BUTTON_MIDDLE) {
        editor->drag_mode = AF_EDITOR_DRAG_PAN;
        editor->drag_start_mouse = (AfVec2){x, y};
        return;
    }
    if (event->button != SDL_BUTTON_LEFT) {
        return;
    }
    if (fabsf(x - editor->left_width) <= 5.0F) {
        editor->drag_mode = AF_EDITOR_DRAG_LEFT_DOCK;
        return;
    }
    if (fabsf(x - ((float)width - editor->right_width)) <= 5.0F) {
        editor->drag_mode = AF_EDITOR_DRAG_RIGHT_DOCK;
        return;
    }
    if (x < editor->left_width && y >= AF_EDITOR_TOOLBAR_HEIGHT + 8.0F) {
        const uint32_t row =
            (uint32_t)((y - AF_EDITOR_TOOLBAR_HEIGHT - 8.0F) / AF_EDITOR_ROW_HEIGHT);
        const uint32_t hierarchy_index = editor->hierarchy_scroll + row;
        if (hierarchy_index < editor->hierarchy_count) {
            const uint64_t clicked =
                editor->project.scene.nodes[editor->hierarchy_order[hierarchy_index]].id;
            if ((modifiers & SDL_KMOD_SHIFT) != 0U && editor->selected_id != 0U) {
                af_editor_reparent(editor, clicked);
            } else if ((modifiers & SDL_KMOD_CTRL) != 0U) {
                af_editor_selection_toggle(editor, clicked);
            } else {
                af_editor_select_only(editor, clicked);
            }
            editor->selected_path_command =
                af_path_find_node_const(&editor->project.path_data, editor->selected_id) != NULL
                    ? 0U
                    : UINT32_MAX;
        }
        return;
    }
    if (y < AF_EDITOR_TOOLBAR_HEIGHT && x >= editor->left_width) {
        const float toolbar_x = x - editor->left_width - 14.0F;
        if (toolbar_x >= 0.0F && toolbar_x < 34.0F) {
            (void)af_editor_create_primitive(editor, AF_NODE_RECTANGLE, width, height);
        } else if (toolbar_x >= 42.0F && toolbar_x < 76.0F) {
            (void)af_editor_create_primitive(editor, AF_NODE_ROUNDED_RECTANGLE, width, height);
        } else if (toolbar_x >= 84.0F && toolbar_x < 118.0F) {
            (void)af_editor_create_primitive(editor, AF_NODE_ELLIPSE, width, height);
        } else if (toolbar_x >= 126.0F && toolbar_x < 160.0F) {
            (void)af_editor_create_path(editor, width, height);
        } else if (toolbar_x >= 172.0F && toolbar_x < 260.0F) {
            if (y < 22.0F) {
                editor->grid_visible = !editor->grid_visible;
            } else {
                editor->snapping = !editor->snapping;
            }
        }
        return;
    }
    if (x >= (float)width - editor->right_width) {
        af_editor_handle_inspector(editor, width, x, y);
        return;
    }
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    if (x >= canvas_x && x < canvas_x + canvas_width && y >= canvas_y &&
        y < canvas_y + canvas_height) {
        AfAffine2 camera = af_editor_camera(editor, width, height);
        const AfSceneNode *selected =
            af_scene_find_const(&editor->project.scene, editor->selected_id);
        AfVec2 origin = {0};
        if (editor->selected_count <= 1U &&
            af_editor_path_mouse_down(editor, width, height, (AfVec2){x, y})) {
            return;
        }
        if (selected != NULL) {
            origin = af_affine_transform_point(
                af_affine_multiply(camera, selected->world_transform), (AfVec2){0});
            if (fabsf(x - (origin.x + 56.0F)) <= 10.0F && fabsf(y - origin.y) <= 10.0F) {
                editor->drag_mode = AF_EDITOR_DRAG_X;
            } else if (fabsf(x - origin.x) <= 10.0F && fabsf(y - (origin.y + 56.0F)) <= 10.0F) {
                editor->drag_mode = AF_EDITOR_DRAG_Y;
            }
        }
        if (editor->drag_mode == AF_EDITOR_DRAG_NONE) {
            const uint64_t hit = af_editor_hit_test(
                editor, af_editor_screen_to_project(editor, width, height, (AfVec2){x, y}));
            const bool control = (modifiers & SDL_KMOD_CTRL) != 0U;
            const bool additive = (modifiers & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT)) != 0U;
            if (hit != 0U) {
                if (control) {
                    af_editor_selection_toggle(editor, hit);
                } else if ((modifiers & SDL_KMOD_SHIFT) != 0U) {
                    af_editor_selection_add(editor, hit);
                } else if (!af_editor_selection_contains(editor, hit)) {
                    af_editor_select_only(editor, hit);
                } else {
                    editor->selected_id = hit;
                }
                editor->selected_path_command =
                    af_path_find_node_const(&editor->project.path_data, editor->selected_id) != NULL
                        ? 0U
                        : UINT32_MAX;
                if (af_editor_selection_contains(editor, hit)) {
                    editor->drag_mode = AF_EDITOR_DRAG_MOVE;
                }
            } else {
                if (!additive) {
                    af_editor_select_only(editor, 0U);
                    editor->selected_path_command = UINT32_MAX;
                }
                editor->canvas_box_start = (AfVec2){x, y};
                editor->canvas_box_current = editor->canvas_box_start;
                editor->canvas_box_additive = additive;
                editor->drag_mode = AF_EDITOR_DRAG_CANVAS_BOX;
            }
        }
        if (editor->drag_mode == AF_EDITOR_DRAG_MOVE || editor->drag_mode == AF_EDITOR_DRAG_X ||
            editor->drag_mode == AF_EDITOR_DRAG_Y) {
            editor->drag_start_mouse =
                af_editor_screen_to_project(editor, width, height, (AfVec2){x, y});
            af_editor_begin_selection_modify(editor, true);
        }
    }
}

static void af_editor_mouse_motion(AfEditorState *editor, int width, int height,
                                   const SDL_MouseMotionEvent *event) {
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    editor->mouse_position = (AfVec2){event->x, event->y};
    if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_SV ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_HUE ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_ALPHA) {
        af_editor_update_color_picker(editor, width, event->x, event->y);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_CANVAS_BOX) {
        editor->canvas_box_current = (AfVec2){event->x, event->y};
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_TIMELINE_HEIGHT) {
        editor->timeline_height = af_editor_clamp(
            (float)height - AF_EDITOR_STATUS_HEIGHT - event->y, AF_EDITOR_TIMELINE_MIN_HEIGHT,
            (float)height - AF_EDITOR_TOOLBAR_HEIGHT - 150.0F);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_TIMELINE_PAN) {
        editor->timeline_pan =
            fmaxf(0.0F, editor->timeline_pan - event->xrel / editor->timeline_zoom);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_PLAYHEAD && clip != NULL) {
        editor->playhead =
            af_editor_clamp(af_editor_x_to_time(editor, event->x), 0.0F, clip->duration);
        return;
    }
    if ((editor->drag_mode == AF_EDITOR_DRAG_LOOP_START ||
         editor->drag_mode == AF_EDITOR_DRAG_LOOP_END) &&
        clip != NULL) {
        AfProjectClip *mutable_clip = af_project_find_clip(&editor->project, clip->id);
        float time = af_editor_clamp(af_editor_x_to_time(editor, event->x), 0.0F, clip->duration);
        if (editor->snapping) {
            time = roundf(time * 10.0F) * 0.1F;
        }
        if (editor->drag_mode == AF_EDITOR_DRAG_LOOP_START) {
            mutable_clip->loop_start = fminf(time, mutable_clip->loop_end);
        } else {
            mutable_clip->loop_end = fmaxf(time, mutable_clip->loop_start);
        }
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_KEYFRAMES && clip != NULL) {
        const float delta = (event->x - editor->drag_start_mouse.x) / editor->timeline_zoom;
        uint32_t index = 0U;
        for (index = 0U; index < editor->key_drag_count; ++index) {
            AfProjectKeyframe *key = af_project_find_keyframe(
                &editor->project, editor->key_drag_before[index].keyframe.id);
            float time = editor->key_drag_before[index].keyframe.keyframe.time + delta;
            if (key == NULL) {
                continue;
            }
            time = af_editor_clamp(time, 0.0F, clip->duration);
            if (editor->snapping) {
                time = roundf(time * 10.0F) * 0.1F;
            }
            key->keyframe.time = time;
        }
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_KEY_BOX) {
        editor->key_box_current = (AfVec2){event->x, event->y};
        return;
    }
    if ((editor->drag_mode == AF_EDITOR_DRAG_TANGENT_IN ||
         editor->drag_mode == AF_EDITOR_DRAG_TANGENT_OUT) &&
        editor->key_drag_count == 1U) {
        AfProjectKeyframe *key =
            af_project_find_keyframe(&editor->project, editor->key_drag_before[0].keyframe.id);
        const float top = af_editor_timeline_top(editor, height) + AF_EDITOR_TIMELINE_RULER_HEIGHT;
        const float bottom = (float)height - AF_EDITOR_STATUS_HEIGHT - 12.0F;
        const float right = (float)width - editor->right_width;
        if (key == NULL) {
            return;
        }
        key->keyframe.interpolation = AF_INTERPOLATION_CUBIC_BEZIER;
        if (editor->drag_mode == AF_EDITOR_DRAG_TANGENT_OUT) {
            key->keyframe.easing[0] = af_editor_clamp(
                (event->x - af_editor_timeline_ruler_x(editor) - 30.0F) / 100.0F, 0.0F, 1.0F);
            key->keyframe.easing[1] =
                af_editor_clamp((bottom - 30.0F - event->y) / 80.0F, -2.0F, 2.0F);
        } else {
            key->keyframe.easing[2] =
                af_editor_clamp((event->x - (right - 130.0F)) / 100.0F, 0.0F, 1.0F);
            key->keyframe.easing[3] = af_editor_clamp((bottom - event->y) / 80.0F, -2.0F, 2.0F);
        }
        (void)top;
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_PATH_POINT ||
        editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL1 ||
        editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL2) {
        af_editor_drag_path_handle(editor, width, height, (AfVec2){event->x, event->y});
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_PAN) {
        editor->pan.x += event->xrel;
        editor->pan.y += event->yrel;
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_LEFT_DOCK) {
        editor->left_width =
            af_editor_clamp(event->x, AF_EDITOR_MIN_DOCK_WIDTH,
                            (float)width - editor->right_width - AF_EDITOR_MIN_VIEW_WIDTH);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_RIGHT_DOCK) {
        editor->right_width =
            af_editor_clamp((float)width - event->x, AF_EDITOR_MIN_DOCK_WIDTH,
                            (float)width - editor->left_width - AF_EDITOR_MIN_VIEW_WIDTH);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_MOVE || editor->drag_mode == AF_EDITOR_DRAG_X ||
        editor->drag_mode == AF_EDITOR_DRAG_Y) {
        const AfVec2 project_current =
            af_editor_screen_to_project(editor, width, height, (AfVec2){event->x, event->y});
        for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
            const AfProjectNodeSnapshot *before = &editor->selection_drag_before[index];
            AfSceneNode *node = af_scene_find(&editor->project.scene, before->node.id);
            AfVec2 current = project_current;
            AfVec2 start = editor->drag_start_mouse;
            if (node == NULL) {
                continue;
            }
            if (before->node.parent_id != AF_INVALID_NODE_ID) {
                const AfSceneNode *parent =
                    af_scene_find_const(&editor->project.scene, before->node.parent_id);
                AfAffine2 parent_inverse = af_affine_identity();
                if (parent != NULL && af_affine_inverse(parent->world_transform, &parent_inverse)) {
                    current = af_affine_transform_point(parent_inverse, current);
                    start = af_affine_transform_point(parent_inverse, start);
                }
            }
            node->local_transform.position = before->node.local_transform.position;
            if (editor->drag_mode != AF_EDITOR_DRAG_Y) {
                node->local_transform.position.x += current.x - start.x;
            }
            if (editor->drag_mode != AF_EDITOR_DRAG_X) {
                node->local_transform.position.y += current.y - start.y;
            }
            if (editor->snapping) {
                node->local_transform.position.x =
                    roundf(node->local_transform.position.x / editor->grid_step) *
                    editor->grid_step;
                node->local_transform.position.y =
                    roundf(node->local_transform.position.y / editor->grid_step) *
                    editor->grid_step;
            }
        }
        (void)af_scene_evaluate_world(&editor->project.scene);
    }
}

static void af_editor_mouse_up(AfEditorState *editor, int width, int height) {
    if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_SV ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_HUE ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_ALPHA) {
        (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_FILL, false);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_MOVE || editor->drag_mode == AF_EDITOR_DRAG_X ||
               editor->drag_mode == AF_EDITOR_DRAG_Y) {
        (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_TRANSFORM, false);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_CANVAS_BOX) {
        af_editor_canvas_box_select(editor, width, height);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_KEYFRAMES) {
        af_editor_timeline_commit_keys(editor, AF_EDITOR_MERGE_KEYFRAME_TIME);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_LOOP_START ||
               editor->drag_mode == AF_EDITOR_DRAG_LOOP_END) {
        af_editor_timeline_commit_clip(editor);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_TANGENT_IN ||
               editor->drag_mode == AF_EDITOR_DRAG_TANGENT_OUT) {
        af_editor_timeline_commit_keys(editor, AF_EDITOR_MERGE_TANGENT);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_KEY_BOX) {
        af_editor_timeline_box_select(editor, height);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_PATH_POINT ||
               editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL1 ||
               editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL2) {
        af_editor_commit_path_drag(editor);
    }
    editor->drag_mode = AF_EDITOR_DRAG_NONE;
}

static void af_editor_mouse_wheel(AfEditorState *editor, int width, int height,
                                  const SDL_MouseWheelEvent *event) {
    if (event->mouse_x < editor->left_width) {
        if (event->y > 0.0F && editor->hierarchy_scroll > 0U) {
            --editor->hierarchy_scroll;
        } else if (event->y < 0.0F && editor->hierarchy_scroll < editor->hierarchy_count) {
            ++editor->hierarchy_scroll;
        }
        return;
    }
    if (event->mouse_y >= af_editor_timeline_top(editor, height)) {
        const float anchor_time = af_editor_x_to_time(editor, event->mouse_x);
        if ((SDL_GetModState() & SDL_KMOD_SHIFT) != 0U) {
            editor->timeline_pan = fmaxf(0.0F, editor->timeline_pan - event->y * 0.25F);
        } else {
            editor->timeline_zoom = af_editor_clamp(
                editor->timeline_zoom * (event->y > 0.0F ? 1.12F : 0.89F), 24.0F, 800.0F);
            editor->timeline_pan =
                fmaxf(0.0F, anchor_time - (event->mouse_x - af_editor_timeline_ruler_x(editor)) /
                                              editor->timeline_zoom);
        }
        return;
    }
    if (event->mouse_x >= (float)width - editor->right_width) {
        return;
    }
    {
        const AfVec2 mouse = {event->mouse_x, event->mouse_y};
        const AfVec2 project = af_editor_screen_to_project(editor, width, height, mouse);
        float canvas_x = 0.0F;
        float canvas_y = 0.0F;
        float canvas_width = 0.0F;
        float canvas_height = 0.0F;
        af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width,
                         &canvas_height);
        (void)canvas_width;
        (void)canvas_height;
        editor->zoom =
            af_editor_clamp(editor->zoom * (event->y > 0.0F ? 1.12F : 0.89F), 0.1F, 8.0F);
        editor->pan.x = mouse.x - canvas_x - project.x * editor->zoom;
        editor->pan.y = mouse.y - canvas_y - project.y * editor->zoom;
    }
}

static void af_editor_key(AfEditorState *editor, int width, int height,
                          const SDL_KeyboardEvent *event) {
    const bool control = (event->mod & SDL_KMOD_CTRL) != 0U;
    const bool alt = (event->mod & SDL_KMOD_ALT) != 0U;
    const bool shift = (event->mod & SDL_KMOD_SHIFT) != 0U;
    const float nudge = (event->mod & SDL_KMOD_SHIFT) != 0U ? 10.0F : 1.0F;
    if (!event->down) {
        return;
    }
    if (editor->context_menu != AF_EDITOR_CONTEXT_NONE && event->key == SDLK_ESCAPE) {
        editor->context_menu = AF_EDITOR_CONTEXT_NONE;
        return;
    }
    if (editor->rename_active) {
        if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            af_editor_finish_rename(editor, true);
        } else if (event->key == SDLK_ESCAPE) {
            af_editor_finish_rename(editor, false);
        } else if (event->key == SDLK_BACKSPACE) {
            af_editor_backspace_utf8(editor->rename_buffer);
        }
        return;
    }
    if (control &&
        (event->key == SDLK_Y || ((event->mod & SDL_KMOD_SHIFT) != 0U && event->key == SDLK_Z))) {
        if (af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
            editor->dirty = true;
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Redo");
        }
    } else if (control && event->key == SDLK_Z) {
        if (af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
            editor->dirty = true;
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Undo");
        }
    } else if (control && event->key == SDLK_S) {
        (void)af_editor_save(editor);
    } else if (control && event->key == SDLK_C && editor->timeline_focus) {
        af_editor_copy_keyframes(editor);
    } else if (control && event->key == SDLK_V && editor->timeline_focus) {
        (void)af_editor_paste_keyframes(editor, false);
    } else if (control && event->key == SDLK_D) {
        if (editor->timeline_focus && editor->selected_keyframe_count != 0U) {
            (void)af_editor_paste_keyframes(editor, true);
        } else {
            (void)af_editor_duplicate_selection(editor);
        }
    } else if (event->key == SDLK_F5) {
        (void)af_editor_compile(editor);
    } else if (alt && event->key == SDLK_C) {
        (void)af_editor_new_clip(editor);
    } else if (alt && event->key == SDLK_DELETE) {
        (void)af_editor_delete_clip(editor);
    } else if (event->key == SDLK_DELETE || event->key == SDLK_BACKSPACE) {
        if (editor->timeline_focus && editor->selected_keyframe_count != 0U) {
            (void)af_editor_delete_keyframes(editor);
        } else if (!shift && af_editor_selected_path(editor) != NULL &&
                   editor->selected_path_command != UINT32_MAX) {
            (void)af_editor_delete_path_point(editor);
        } else {
            (void)af_editor_delete_selection(editor);
        }
    } else if (event->key == SDLK_F2) {
        af_editor_begin_rename(editor);
    } else if (event->key == SDLK_1) {
        (void)af_editor_create_primitive(editor, AF_NODE_RECTANGLE, width, height);
    } else if (event->key == SDLK_2) {
        (void)af_editor_create_primitive(editor, AF_NODE_ROUNDED_RECTANGLE, width, height);
    } else if (event->key == SDLK_3) {
        (void)af_editor_create_primitive(editor, AF_NODE_ELLIPSE, width, height);
    } else if (event->key == SDLK_4) {
        (void)af_editor_create_path(editor, width, height);
    } else if (!control && !alt && event->key == SDLK_I && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_insert_path_point(editor);
    } else if (!control && !alt && event->key == SDLK_C && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_convert_path_point(editor);
    } else if (!control && !alt && event->key == SDLK_O && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_toggle_path_closed(editor);
    } else if (!control && !alt && event->key == SDLK_F && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_toggle_path_fill_rule(editor);
    } else if (!control && !alt && shift && event->key == SDLK_M && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_add_path_contour(editor);
    } else if (!control && !alt && shift && event->key == SDLK_R && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_reverse_path_contour(editor);
    } else if (event->key == SDLK_LEFT) {
        af_editor_nudge(editor, -nudge, 0.0F);
    } else if (event->key == SDLK_RIGHT) {
        af_editor_nudge(editor, nudge, 0.0F);
    } else if (event->key == SDLK_UP) {
        af_editor_nudge(editor, 0.0F, -nudge);
    } else if (event->key == SDLK_DOWN) {
        af_editor_nudge(editor, 0.0F, nudge);
    } else if (event->key == SDLK_LEFTBRACKET) {
        if (control && editor->timeline_focus) {
            (void)af_editor_modify_selected_keys(editor, 2, AF_INTERPOLATION_LINEAR);
        } else {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, -1.0F, 2);
        }
    } else if (event->key == SDLK_RIGHTBRACKET) {
        if (control && editor->timeline_focus) {
            (void)af_editor_modify_selected_keys(editor, 1, AF_INTERPOLATION_LINEAR);
        } else {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, 1.0F, 2);
        }
    } else if (event->key == SDLK_Q) {
        af_editor_modify_scalar(editor, AF_EDITOR_MERGE_TRANSFORM, -0.05F, 3);
    } else if (event->key == SDLK_E) {
        af_editor_modify_scalar(editor, AF_EDITOR_MERGE_TRANSFORM, 0.05F, 3);
    } else if (event->key == SDLK_MINUS) {
        af_editor_modify_scalar(editor, AF_EDITOR_MERGE_TRANSFORM, -0.05F, 4);
    } else if (event->key == SDLK_EQUALS || event->key == SDLK_PLUS) {
        af_editor_modify_scalar(editor, AF_EDITOR_MERGE_TRANSFORM, 0.05F, 4);
    } else if (event->key == SDLK_G) {
        editor->grid_visible = !editor->grid_visible;
    } else if (event->key == SDLK_N) {
        editor->snapping = !editor->snapping;
    } else if (event->key == SDLK_U) {
        af_editor_reparent(editor, AF_INVALID_NODE_ID);
    } else if (event->key == SDLK_SPACE) {
        editor->preview_playing = !editor->preview_playing;
    } else if (event->key == SDLK_K) {
        (void)af_editor_add_keyframe(editor);
    } else if (event->key == SDLK_S && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 0, AF_INTERPOLATION_STEP);
    } else if (event->key == SDLK_L && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 0, AF_INTERPOLATION_LINEAR);
    } else if (event->key == SDLK_B && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 0, AF_INTERPOLATION_CUBIC_BEZIER);
    } else if ((event->mod & SDL_KMOD_SHIFT) != 0U && event->key == SDLK_R &&
               editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 3, AF_INTERPOLATION_LINEAR);
    } else if (event->key == SDLK_F6) {
        editor->curve_visible = !editor->curve_visible;
        editor->timeline_focus = true;
    } else if (event->key == SDLK_F7 && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 4, AF_INTERPOLATION_CUBIC_BEZIER);
    } else if (event->key == SDLK_F8 && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 5, AF_INTERPOLATION_CUBIC_BEZIER);
    } else if (event->key == SDLK_F9 && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 6, AF_INTERPOLATION_CUBIC_BEZIER);
    } else if (event->key == SDLK_HOME && editor->timeline_focus) {
        editor->playhead = 0.0F;
    } else if (event->key == SDLK_END && editor->timeline_focus) {
        const AfProjectClip *clip =
            af_project_find_clip_const(&editor->project, editor->selected_clip_id);
        if (clip != NULL) {
            editor->playhead = clip->duration;
        }
    } else if (event->key == SDLK_ESCAPE) {
        if (editor->timeline_focus) {
            af_editor_clear_key_selection(editor);
        } else {
            af_editor_select_only(editor, 0U);
            editor->selected_path_command = UINT32_MAX;
        }
    }
}

static void af_editor_mouse_scale(const AfEditorState *editor, float *x, float *y) {
    int logical_width = 0;
    int logical_height = 0;
    int pixel_width = 0;
    int pixel_height = 0;
    if (SDL_GetWindowSize(editor->window.window, &logical_width, &logical_height) &&
        SDL_GetWindowSizeInPixels(editor->window.window, &pixel_width, &pixel_height) &&
        logical_width > 0 && logical_height > 0) {
        *x *= (float)pixel_width / (float)logical_width;
        *y *= (float)pixel_height / (float)logical_height;
    }
}

static void af_editor_handle_event(AfEditorState *editor, SDL_Event *event, int width, int height) {
    if (event->type == SDL_EVENT_QUIT) {
        editor->running = false;
    } else if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
               event->type == SDL_EVENT_WINDOW_RESIZED) {
        af_vk_renderer_request_resize(editor->renderer);
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        af_editor_key(editor, width, height, &event->key);
    } else if (event->type == SDL_EVENT_TEXT_INPUT) {
        af_editor_append_text(editor, event->text.text);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        SDL_MouseButtonEvent button = event->button;
        af_editor_mouse_scale(editor, &button.x, &button.y);
        af_editor_mouse_down(editor, width, height, &button, SDL_GetModState());
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        af_editor_mouse_up(editor, width, height);
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        SDL_MouseMotionEvent motion = event->motion;
        af_editor_mouse_scale(editor, &motion.x, &motion.y);
        af_editor_mouse_scale(editor, &motion.xrel, &motion.yrel);
        af_editor_mouse_motion(editor, width, height, &motion);
    } else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        SDL_MouseWheelEvent wheel = event->wheel;
        af_editor_mouse_scale(editor, &wheel.mouse_x, &wheel.mouse_y);
        af_editor_mouse_wheel(editor, width, height, &wheel);
    }
}

static void af_editor_parse_arguments(AfEditorState *editor, int argument_count, char **arguments) {
    int index = 1;
    (void)snprintf(editor->project_path, sizeof(editor->project_path), "arcflow_project.afproj");
    for (index = 1; index < argument_count; ++index) {
        if (strcmp(arguments[index], "--frames") == 0 && index + 1 < argument_count) {
            editor->maximum_frames = (uint32_t)strtoul(arguments[++index], NULL, 10);
        } else if (strcmp(arguments[index], "--resize-smoke") == 0) {
            editor->resize_smoke = true;
        } else if (strcmp(arguments[index], "--context-smoke") == 0) {
            editor->context_smoke = true;
        } else if (strcmp(arguments[index], "--tooltip-smoke") == 0) {
            editor->tooltip_smoke = true;
        } else if (strcmp(arguments[index], "--selection-smoke") == 0) {
            editor->selection_smoke = true;
        } else if (strcmp(arguments[index], "--output") == 0 && index + 1 < argument_count) {
            (void)snprintf(editor->output_path, sizeof(editor->output_path), "%s",
                           arguments[++index]);
        } else {
            (void)snprintf(editor->project_path, sizeof(editor->project_path), "%s",
                           arguments[index]);
        }
    }
    if (editor->output_path[0] != '\0' && editor->maximum_frames == 0U) {
        editor->maximum_frames = editor->tooltip_smoke ? 2U : 1U;
    }
    af_editor_derive_package_path(editor);
}

int af_editor_run(int argument_count, char **arguments) {
    AfEditorState editor = {.selected_id = 0U,
                            .selected_path_command = UINT32_MAX,
                            .left_width = 230.0F,
                            .right_width = 286.0F,
                            .zoom = 0.65F,
                            .pan = {48.0F, 42.0F},
                            .grid_step = 20.0F,
                            .color_hue = 0.58F,
                            .grid_visible = true,
                            .snapping = true,
                            .timeline_height = AF_EDITOR_TIMELINE_DEFAULT_HEIGHT,
                            .timeline_zoom = 120.0F,
                            .running = true};
    AfResult result = AF_RESULT_SUCCESS;
    bool loaded = false;
    int exit_code = 1;
    af_allocator_init_default(&editor.allocator);
    af_editor_parse_arguments(&editor, argument_count, arguments);
    result = af_project_load_file(editor.project_path, &editor.allocator, &editor.project);
    if (result == AF_RESULT_SUCCESS) {
        loaded = true;
        if (editor.project.scene.count > AF_EDITOR_PROJECT_CAPACITY) {
            af_log(AF_LOG_ERROR, "Project exceeds the editor node limit");
            goto cleanup;
        }
    } else if (result == AF_RESULT_IO_ERROR) {
        result = af_project_create(&editor.project, &editor.allocator, AF_EDITOR_PROJECT_CAPACITY);
        if (result == AF_RESULT_SUCCESS) {
            result = af_editor_default_project(&editor);
        }
    }
    if (result != AF_RESULT_SUCCESS) {
        af_log(AF_LOG_ERROR, "Cannot open editable project '%s': %s", editor.project_path,
               af_result_string(result));
        goto cleanup;
    }
    if (editor.project.clip_count == 0U) {
        AfProjectClip clip = {.id = 1U,
                              .name = "Main Clip",
                              .duration = 4.0F,
                              .loop_start = 0.0F,
                              .loop_end = 4.0F,
                              .loop = true};
        result = af_project_add_clip(&editor.project, &clip);
        editor.dirty = loaded;
        if (result != AF_RESULT_SUCCESS) {
            goto cleanup;
        }
    }
    editor.selected_clip_id = editor.project.clips[0].id;
    result = af_editor_history_create(&editor.history, &editor.allocator,
                                      AF_EDITOR_HISTORY_DEFAULT_BYTES);
    if (result != AF_RESULT_SUCCESS) {
        goto cleanup;
    }
    editor.render_nodes = af_allocate_array(&editor.allocator, AF_MAX_NODES,
                                            sizeof(*editor.render_nodes), _Alignof(AfSceneNode));
    editor.render_paths = af_allocate_array(&editor.allocator, AF_MAX_PATHS,
                                            sizeof(*editor.render_paths), _Alignof(AfPath));
    editor.render_path_commands =
        af_allocate_array(&editor.allocator, AF_MAX_PATH_COMMANDS,
                          sizeof(*editor.render_path_commands), _Alignof(AfPathCommand));
    editor.preview_nodes = af_allocate_array(&editor.allocator, editor.project.scene.capacity,
                                             sizeof(*editor.preview_nodes), _Alignof(AfSceneNode));
    editor.curve_order = af_allocate_array(&editor.allocator, AF_PROJECT_MAX_KEYFRAMES,
                                           sizeof(*editor.curve_order), _Alignof(uint32_t));
    editor.path_scratch = af_allocate(&editor.allocator, sizeof(*editor.path_scratch),
                                      _Alignof(AfPathTessellationScratch));
    editor.text_glyphs = af_allocate_array(&editor.allocator, AF_EDITOR_TEXT_GLYPH_CAPACITY,
                                           sizeof(*editor.text_glyphs), _Alignof(AfGlyphPlacement));
    editor.text_lines = af_allocate_array(&editor.allocator, AF_EDITOR_TEXT_LINE_CAPACITY,
                                          sizeof(*editor.text_lines), _Alignof(AfTextLine));
    if (editor.render_nodes == NULL || editor.render_paths == NULL ||
        editor.render_path_commands == NULL || editor.preview_nodes == NULL ||
        editor.curve_order == NULL || editor.path_scratch == NULL || editor.text_glyphs == NULL ||
        editor.text_lines == NULL) {
        result = AF_RESULT_OUT_OF_MEMORY;
        goto cleanup;
    }
    editor.render_path_data = (AfPathData){.paths = editor.render_paths,
                                           .path_capacity = AF_MAX_PATHS,
                                           .commands = editor.render_path_commands,
                                           .command_capacity = AF_MAX_PATH_COMMANDS};
    result = af_file_read_limited(ARCFLOW_EDITOR_FONT_PATH, 4U * 1024U * 1024U, &editor.allocator,
                                  &editor.font_file);
    if (result == AF_RESULT_SUCCESS) {
        result = af_text_engine_create(&editor.allocator, &editor.text_engine);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_text_font_load_memory(editor.text_engine, AF_EDITOR_TEXT_FONT_ID,
                                          editor.font_file.data, editor.font_file.size, 0U);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_log(AF_LOG_ERROR, "Cannot load the project-local editor font: %s",
               af_result_string(result));
        goto cleanup;
    }
    af_scene_init(&editor.render_scene, editor.render_nodes, AF_MAX_NODES);
    af_scene_init(&editor.preview_scene, editor.preview_nodes, editor.project.scene.capacity);
    editor.render_scene.paths = &editor.render_path_data;
    editor.render_scene.particles = &editor.project.particle_data;
    editor.preview_scene.paths = &editor.project.path_data;
    editor.preview_scene.styles = &editor.project.style_data;
    editor.preview_scene.layouts = &editor.project.layout_data;
    editor.preview_scene.particles = &editor.project.particle_data;
    result = af_platform_window_create("Arcflow Editor", 1440, 840, &editor.window);
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_renderer_create(&editor.window, &editor.renderer);
    }
    if (result != AF_RESULT_SUCCESS) {
        goto cleanup;
    }
    af_editor_status(&editor, loaded ? "Project loaded" : "New project");
    editor.previous_ticks = SDL_GetTicksNS();
    while (editor.running) {
        SDL_Event event;
        int width = 0;
        int height = 0;
        uint64_t current_ticks = 0U;
        float delta_time = 0.0F;
        if (!SDL_GetWindowSizeInPixels(editor.window.window, &width, &height) || width <= 0 ||
            height <= 0) {
            continue;
        }
        while (SDL_PollEvent(&event)) {
            af_editor_handle_event(&editor, &event, width, height);
        }
        current_ticks = SDL_GetTicksNS();
        delta_time = (float)((double)(current_ticks - editor.previous_ticks) / 1000000000.0);
        editor.previous_ticks = current_ticks;
        if (editor.preview_playing) {
            const AfProjectClip *clip =
                af_project_find_clip_const(&editor.project, editor.selected_clip_id);
            if (clip != NULL) {
                editor.playhead += fminf(delta_time, 0.1F);
                if (clip->loop && editor.playhead > clip->loop_end) {
                    const float length = clip->loop_end - clip->loop_start;
                    editor.playhead =
                        length > 0.0F
                            ? clip->loop_start + fmodf(editor.playhead - clip->loop_start, length)
                            : clip->loop_start;
                } else if (editor.playhead > clip->duration) {
                    editor.playhead = clip->duration;
                    editor.preview_playing = false;
                }
            }
        }
        af_editor_update_preview(&editor);
        af_editor_build_hierarchy(&editor);
        if (editor.selection_smoke) {
            af_editor_prepare_selection_smoke(&editor, width, height);
        } else if (editor.context_smoke) {
            const uint32_t context = (editor.rendered_frames / 2U) % 10U;
            editor.context_menu = (AfEditorContextMenu)(AF_EDITOR_CONTEXT_HIERARCHY + context);
            if (editor.context_menu == AF_EDITOR_CONTEXT_HIERARCHY) {
                editor.context_position = (AfVec2){54.0F, 92.0F};
            } else if (editor.context_menu == AF_EDITOR_CONTEXT_CANVAS) {
                editor.context_position = (AfVec2){620.0F, 190.0F};
            } else if (editor.context_menu == AF_EDITOR_CONTEXT_INSPECTOR) {
                editor.context_position =
                    (AfVec2){(float)width - editor.right_width + 22.0F, 90.0F};
            } else if (editor.context_menu == AF_EDITOR_CONTEXT_TIMELINE) {
                editor.context_position = (AfVec2){editor.left_width + 260.0F,
                                                   af_editor_timeline_top(&editor, height) + 58.0F};
            } else if (editor.context_menu == AF_EDITOR_CONTEXT_TOOLBAR) {
                editor.context_position = (AfVec2){editor.left_width + 280.0F, 18.0F};
            } else {
                editor.context_position = (AfVec2){620.0F, 190.0F};
            }
            editor.mouse_position =
                (AfVec2){editor.context_position.x + 52.0F,
                         editor.context_position.y + AF_EDITOR_CONTEXT_HEADER_HEIGHT +
                             AF_EDITOR_CONTEXT_ITEM_HEIGHT + 12.0F};
        } else if (editor.tooltip_smoke) {
            editor.context_menu = AF_EDITOR_CONTEXT_NONE;
            editor.mouse_position = (AfVec2){editor.left_width + 31.0F, 20.0F};
        }
        af_editor_build_render_scene(&editor, width, height);
        af_editor_update_title(&editor);
        if (editor.output_path[0] != '\0' && editor.rendered_frames + 1U == editor.maximum_frames) {
            result = af_vk_renderer_capture_next(editor.renderer, editor.output_path);
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_vk_renderer_draw_timed(editor.renderer, &editor.render_scene,
                                               fminf(delta_time, 0.1F));
        }
        if (result != AF_RESULT_SUCCESS) {
            af_log(AF_LOG_ERROR, "Editor rendering failed: %s", af_result_string(result));
            goto cleanup;
        }
        ++editor.rendered_frames;
        if (editor.resize_smoke && editor.rendered_frames == 2U) {
            (void)SDL_SetWindowSize(editor.window.window, 1120, 680);
            af_vk_renderer_request_resize(editor.renderer);
        }
        if (editor.maximum_frames != 0U && editor.rendered_frames >= editor.maximum_frames) {
            editor.running = false;
        }
    }
    if (af_vk_renderer_has_validation_errors(editor.renderer)) {
        af_log(AF_LOG_ERROR, "Vulkan validation reported errors in Arcflow Editor");
        goto cleanup;
    }
    exit_code = 0;

cleanup:
    if (result != AF_RESULT_SUCCESS) {
        af_log(AF_LOG_ERROR, "Arcflow Editor: %s", af_result_string(result));
    }
    af_vk_renderer_destroy(editor.renderer);
    af_platform_window_destroy(&editor.window);
    af_text_engine_destroy(editor.text_engine);
    af_file_data_destroy(&editor.allocator, &editor.font_file);
    af_deallocate(&editor.allocator, editor.text_lines);
    af_deallocate(&editor.allocator, editor.text_glyphs);
    af_deallocate(&editor.allocator, editor.path_scratch);
    af_deallocate(&editor.allocator, editor.curve_order);
    af_deallocate(&editor.allocator, editor.preview_nodes);
    af_deallocate(&editor.allocator, editor.render_path_commands);
    af_deallocate(&editor.allocator, editor.render_paths);
    af_deallocate(&editor.allocator, editor.render_nodes);
    af_editor_history_destroy(&editor.history);
    af_project_destroy(&editor.project);
    return exit_code;
}
