#include "editor/editor_app.h"

#include "arcflow/arcflow_build_info.h"
#include "arcflow/arcflow_version.h"
#include "core/allocator.h"
#include "core/file_io.h"
#include "core/hash.h"
#include "core/log.h"
#include "editor/editor_actions.h"
#include "editor/editor_commands.h"
#include "editor/editor_gizmo.h"
#include "editor/editor_preferences.h"
#include "editor/editor_recovery.h"
#include "editor/editor_workflow.h"
#include "onboarding/onboarding.h"
#include "performance/metrics.h"
#include "platform/platform_sdl.h"
#include "project/image_import.h"
#include "project/interchange.h"
#include "project/project.h"
#include "project/project_templates.h"
#include "renderer/vk_renderer.h"
#include "scene/scene.h"
#include "text/text.h"
#include "update/update.h"

#include <SDL3/SDL.h>
#include <ctype.h>
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
#define AF_EDITOR_MENU_HEIGHT 28.0F
#define AF_EDITOR_TOOLBAR_HEIGHT 72.0F
#define AF_EDITOR_STATUS_HEIGHT 28.0F
#define AF_EDITOR_TIMELINE_DEFAULT_HEIGHT 300.0F
#define AF_EDITOR_TIMELINE_MIN_HEIGHT 200.0F
#define AF_EDITOR_TIMELINE_RULER_HEIGHT 72.0F
#define AF_EDITOR_TIMELINE_TRACK_HEIGHT 34.0F
#define AF_EDITOR_TIMELINE_HEADER_WIDTH 190.0F
#define AF_EDITOR_PATH_ACTION_Y 32.0F
#define AF_EDITOR_PATH_ACTION_HEIGHT 34.0F
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
#define AF_EDITOR_PARTICLE_PAGE_Y 510.0F
#define AF_EDITOR_PARTICLE_ROW_Y 540.0F
#define AF_EDITOR_PARTICLE_ROW_STEP 29.0F
#define AF_EDITOR_PARTICLE_ROW_COUNT 8U
#define AF_EDITOR_PARTICLE_PAGE_COUNT 7U
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
    AF_EDITOR_DRAG_GIZMO,
    AF_EDITOR_DRAG_PAN,
    AF_EDITOR_DRAG_LEFT_DOCK,
    AF_EDITOR_DRAG_RIGHT_DOCK,
    AF_EDITOR_DRAG_TIMELINE_HEIGHT,
    AF_EDITOR_DRAG_TIMELINE_PAN,
    AF_EDITOR_DRAG_PLAYHEAD,
    AF_EDITOR_DRAG_LOOP_START,
    AF_EDITOR_DRAG_LOOP_END,
    AF_EDITOR_DRAG_LOOP_RANGE,
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
    AF_EDITOR_CONTEXT_GROUP,
    AF_EDITOR_CONTEXT_IMAGE,
    AF_EDITOR_CONTEXT_ASSET,
    AF_EDITOR_CONTEXT_PARTICLE
} AfEditorContextMenu;

typedef enum AfEditorTopMenu {
    AF_EDITOR_MENU_NONE = 0,
    AF_EDITOR_MENU_FILE,
    AF_EDITOR_MENU_EDIT,
    AF_EDITOR_MENU_VIEW,
    AF_EDITOR_MENU_ANIMATION,
    AF_EDITOR_MENU_ASSETS,
    AF_EDITOR_MENU_WORKSPACE,
    AF_EDITOR_MENU_TOOLS,
    AF_EDITOR_MENU_HELP
} AfEditorTopMenu;

typedef enum AfEditorDialogAction {
    AF_EDITOR_DIALOG_NONE = 0,
    AF_EDITOR_DIALOG_OPEN_PROJECT,
    AF_EDITOR_DIALOG_SAVE_PROJECT_AS,
    AF_EDITOR_DIALOG_IMPORT_IMAGE,
    AF_EDITOR_DIALOG_REPAIR_IMAGE,
    AF_EDITOR_DIALOG_UPDATE_MANIFEST,
    AF_EDITOR_DIALOG_EXPORT_PACKAGE,
    AF_EDITOR_DIALOG_EXPORT_SVG,
    AF_EDITOR_DIALOG_EXPORT_LOTTIE,
    AF_EDITOR_DIALOG_EXPORT_DOTLOTTIE,
    AF_EDITOR_DIALOG_EXPORT_GIF,
    AF_EDITOR_DIALOG_EXPORT_MP4
} AfEditorDialogAction;

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
    uint32_t asset_scroll;
    uint64_t hierarchy_anchor_id;
    float left_width;
    float right_width;
    float zoom;
    AfVec2 pan;
    float grid_step;
    float color_hue;
    uint32_t particle_page;
    bool grid_visible;
    bool snapping;
    AfVec2 mouse_position;
    AfVec2 context_position;
    AfEditorContextMenu context_menu;
    AfEditorTopMenu top_menu;
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
    AfProjectKeyframeSnapshot path_key_drag_before;
    uint64_t path_drag_key_id;
    bool path_key_drag;
    bool timeline_focus;
    bool curve_visible;
    bool preview_playing;
    uint64_t previous_ticks;
    AfEditorDragMode drag_mode;
    AfVec2 drag_start_mouse;
    AfProjectNodeSnapshot drag_before;
    AfProjectNodeSnapshot selection_drag_before[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t selection_drag_count;
    uint64_t transform_drag_node_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint64_t transform_drag_key_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    float transform_drag_key_starts[AF_EDITOR_COMMAND_MAX_SNAPSHOTS][4];
    AfProjectKeyframeSnapshot transform_drag_key_before[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    bool transform_drag_key_created[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t transform_drag_key_count;
    AfPropertyType transform_drag_property;
    AfEditorGizmoHandle gizmo_handle;
    AfTransform2D gizmo_start_transform;
    float gizmo_start_angle;
    AfVec2 canvas_box_start;
    AfVec2 canvas_box_current;
    bool canvas_box_additive;
    bool rename_active;
    AfProjectNodeSnapshot rename_before;
    char rename_buffer[AF_PROJECT_NAME_CAPACITY];
    char project_path[AF_EDITOR_PATH_CAPACITY];
    char package_path[AF_EDITOR_PATH_CAPACITY];
    char output_path[AF_EDITOR_PATH_CAPACITY];
    char sequence_prefix[AF_EDITOR_PATH_CAPACITY];
    char status[AF_EDITOR_STATUS_CAPACITY];
    uint32_t dialog_event_type;
    AfEditorDialogAction dialog_action;
    uint32_t maximum_frames;
    uint32_t rendered_frames;
    bool resize_smoke;
    bool context_smoke;
    bool tooltip_smoke;
    bool selection_smoke;
    bool vfx_smoke;
    bool particle_view_smoke;
    bool vector_animation_smoke;
    bool image_smoke;
    bool menu_smoke;
    bool workflow_smoke;
    bool platform_smoke;
    bool release_smoke;
    bool onboarding_smoke;
    bool vector_animation_smoke_prepared;
    bool dialog_active;
    bool export_panel_visible;
    bool help_overlay;
    bool shortcuts_overlay;
    bool asset_browser_visible;
    bool hierarchy_search_active;
    bool profiler_visible;
    bool validation_visible;
    bool platform_panel_visible;
    bool update_panel_visible;
    bool update_checked;
    bool recovery_overlay;
    bool preset_panel_visible;
    bool template_panel_visible;
    bool recent_panel_visible;
    bool welcome_visible;
    bool guided_tour_visible;
    bool example_browser_visible;
    bool guide_browser_visible;
    bool smart_guides_visible;
    bool rulers_visible;
    bool measurement_visible;
    bool workflow_initialized;
    bool preferences_read_only;
    bool project_read_only;
    bool external_change_visible;
    uint64_t selected_asset_id;
    char hierarchy_search[AF_EDITOR_COMMAND_QUERY_CAPACITY];
    char preferences_path[AF_EDITOR_PATH_CAPACITY * 2U];
    char workspace_path[AF_EDITOR_PATH_CAPACITY * 2U];
    AfEditorAction action_storage[AF_EDITOR_MAX_ACTIONS];
    AfEditorActionRegistry actions;
    AfEditorCommandPalette command_palette;
    AfEditorPreferences preferences;
    AfEditorWorkspace workspace;
    AfEditorCommand object_clipboard;
    char object_clipboard_image_sources[AF_EDITOR_COMMAND_MAX_SNAPSHOTS][AF_IMAGE_PATH_CAPACITY];
    AfEditorRecoveryState recovery;
    AfEditorRecoveryCandidate recovery_candidate;
    AfPerformanceMetrics performance;
    AfPlatformCapabilities platform_capabilities;
    AfUpdateManifest update_manifest;
    AfUpdateStatus update_status;
    AfProjectLock project_lock;
    AfProjectFingerprint project_fingerprint;
    uint64_t external_check_ticks;
    char project_lock_owner[AF_PROJECT_LOCK_OWNER_CAPACITY];
    float last_frame_ms;
    int window_width;
    int window_height;
    uint32_t shortcut_selected_index;
    uint32_t template_selected_index;
    uint32_t recent_selected_index;
    uint32_t guided_step;
    uint32_t example_selected_index;
    uint32_t guide_step;
    AfOnboardingTopic guide_topic;
    bool shortcut_capture;
    bool running;
    bool dirty;
} AfEditorState;

static void af_editor_prepare_render_paths(AfEditorState *editor);
static void af_editor_render_top_menu(AfEditorState *editor, int width, int height);
static bool af_editor_top_menu_mouse_down(AfEditorState *editor, int width, int height, float x,
                                          float y);
static void af_editor_handle_dialog_event(AfEditorState *editor, const SDL_UserEvent *event);
static void af_editor_show_project_dialog(AfEditorState *editor, AfEditorDialogAction action);
static void af_editor_mark_dirty(AfEditorState *editor);
static void af_editor_after_project_saved(AfEditorState *editor);
static void af_editor_project_changed(AfEditorState *editor);
static void af_editor_refresh_recovery(AfEditorState *editor, bool show_available);
static AfResult af_editor_workflow_init(AfEditorState *editor, int width, int height);
static void af_editor_workflow_shutdown(AfEditorState *editor);
static void af_editor_workflow_update(AfEditorState *editor, uint64_t now_ns);
static void af_editor_render_workflow(AfEditorState *editor, int width, int height);
static void af_editor_render_canvas_workflow(AfEditorState *editor, int width, int height,
                                             AfAffine2 camera);
static void af_editor_render_onboarding(AfEditorState *editor, int width, int height);
static bool af_editor_workflow_mouse_down(AfEditorState *editor, int width, int height, float x,
                                          float y);
static bool af_editor_workflow_key(AfEditorState *editor, const SDL_KeyboardEvent *event);
static bool af_editor_workflow_text(AfEditorState *editor, const char *text);
static void af_editor_workflow_drop_file(AfEditorState *editor, const char *path);
static void af_editor_onboarding_start_guided(AfEditorState *editor);
static void af_editor_onboarding_show_welcome(AfEditorState *editor);
static void af_editor_onboarding_show_examples(AfEditorState *editor);
static void af_editor_onboarding_show_guides(AfEditorState *editor, AfOnboardingTopic topic);
static void af_editor_collaboration_release(AfEditorState *editor);
static void af_editor_collaboration_track_path(AfEditorState *editor);
static void af_editor_collaboration_saved(AfEditorState *editor);
static void af_editor_collaboration_update(AfEditorState *editor, uint64_t now_ns);
static bool af_editor_hierarchy_name_matches(const AfEditorState *editor, uint32_t node_index);

// clang-format off
#include "editor/editor_app_core.inc"
#include "editor/editor_app_collaboration.inc"
#include "editor/editor_app_images.inc"
#include "editor/editor_app_draw.inc"
#include "editor/editor_app_timeline.inc"
#include "editor/editor_app_preview.inc"
#include "editor/editor_app_panels.inc"
#include "editor/editor_app_tooltips.inc"
#include "editor/editor_app_canvas.inc"
#include "editor/editor_app_animation.inc"
#include "editor/editor_app_paths.inc"
#include "editor/editor_app_properties.inc"
#include "editor/editor_app_menus.inc"
#include "editor/editor_app_workflow_core.inc"
#include "editor/editor_app_onboarding.inc"
#include "editor/editor_app_workflow_render.inc"
#include "editor/editor_app_workflow_input.inc"
#include "editor/editor_app_input.inc"
// clang-format on

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
    uint32_t missing_image_count = 0U;
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
    if (editor.image_smoke) {
        result = af_editor_prepare_image_smoke(&editor);
        if (result != AF_RESULT_SUCCESS) {
            goto cleanup;
        }
    }
    result = af_image_build_atlases(&editor.project.image_data);
    if (result != AF_RESULT_SUCCESS) {
        af_log(AF_LOG_ERROR, "Cannot prepare project image atlases: %s", af_result_string(result));
        goto cleanup;
    }
    for (uint32_t index = 0U; index < editor.project.image_data.asset_count; ++index) {
        missing_image_count += editor.project.image_data.assets[index].missing ? 1U : 0U;
    }
    editor.asset_browser_visible = missing_image_count != 0U;
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
    editor.preview_nodes = af_allocate_array(&editor.allocator, AF_EDITOR_PROJECT_CAPACITY,
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
    if (result != AF_RESULT_SUCCESS) {
        char font_path[AF_EDITOR_PATH_CAPACITY];
        if (af_platform_resource_path("fonts/NotoSans-Bold.ttf", font_path, sizeof(font_path)) ==
            AF_RESULT_SUCCESS) {
            result = af_file_read_limited(font_path, 4U * 1024U * 1024U, &editor.allocator,
                                          &editor.font_file);
        }
    }
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
    af_scene_init(&editor.preview_scene, editor.preview_nodes, AF_EDITOR_PROJECT_CAPACITY);
    editor.render_scene.paths = &editor.render_path_data;
    editor.render_scene.images = &editor.project.image_data;
    editor.render_scene.particles = &editor.project.particle_data;
    editor.preview_scene.paths = &editor.project.path_data;
    editor.preview_scene.styles = &editor.project.style_data;
    editor.preview_scene.layouts = &editor.project.layout_data;
    editor.preview_scene.particles = &editor.project.particle_data;
    editor.preview_scene.images = &editor.project.image_data;
    result = af_platform_window_create("Arcflow Editor", 1440, 840, &editor.window);
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_renderer_create(&editor.window, &editor.renderer);
    }
    if (result != AF_RESULT_SUCCESS) {
        goto cleanup;
    }
    editor.dialog_event_type = SDL_RegisterEvents(1);
    if (editor.dialog_event_type == UINT32_MAX) {
        result = AF_RESULT_IO_ERROR;
        goto cleanup;
    }
    {
        AfPlatformWindowMetrics metrics = {0};
        result = af_platform_window_metrics(&editor.window, &metrics);
        if (result == AF_RESULT_SUCCESS) {
            result = af_editor_workflow_init(&editor, metrics.pixel_width, metrics.pixel_height);
        }
        if (result != AF_RESULT_SUCCESS) {
            goto cleanup;
        }
    }
    if (missing_image_count != 0U) {
        char status[96];
        (void)snprintf(status, sizeof(status), "%u image source%s missing", missing_image_count,
                       missing_image_count == 1U ? " is" : "s are");
        af_editor_status(&editor, status);
    } else {
        af_editor_status(&editor, loaded ? "Project loaded" : "New project");
    }
    editor.previous_ticks = SDL_GetTicksNS();
    while (editor.running) {
        SDL_Event event;
        int width = 0;
        int height = 0;
        uint64_t current_ticks = 0U;
        float delta_time = 0.0F;
        AfPlatformWindowMetrics metrics = {0};
        if (af_platform_window_metrics(&editor.window, &metrics) != AF_RESULT_SUCCESS ||
            metrics.pixel_width <= 0 || metrics.pixel_height <= 0) {
            continue;
        }
        width = metrics.pixel_width;
        height = metrics.pixel_height;
        editor.window_width = width;
        editor.window_height = height;
        while (SDL_PollEvent(&event)) {
            af_editor_handle_event(&editor, &event, width, height);
        }
        current_ticks = SDL_GetTicksNS();
        delta_time = (float)((double)(current_ticks - editor.previous_ticks) / 1000000000.0);
        editor.last_frame_ms = delta_time * 1000.0F;
        editor.previous_ticks = current_ticks;
        af_editor_workflow_update(&editor, current_ticks);
        af_editor_collaboration_update(&editor, current_ticks);
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
        if (editor.image_smoke) {
            editor.playhead = (float)editor.rendered_frames * 0.125F;
            editor.asset_browser_visible = true;
            af_editor_status(&editor, "Programmatic image atlas / flipbook / nine-slice capture");
        } else if (editor.vector_animation_smoke) {
            const AfProjectClip *clip = NULL;
            if (!editor.vector_animation_smoke_prepared) {
                result = af_editor_prepare_vector_animation_smoke(&editor, width, height);
                if (result != AF_RESULT_SUCCESS) {
                    goto cleanup;
                }
                editor.vector_animation_smoke_prepared = true;
            }
            clip = af_project_find_clip_const(&editor.project, editor.selected_clip_id);
            if (clip != NULL) {
                const uint32_t denominator =
                    editor.maximum_frames > 1U ? editor.maximum_frames - 1U : 1U;
                editor.playhead =
                    clip->duration * (float)editor.rendered_frames / (float)denominator;
            }
        }
        af_editor_update_preview(&editor);
        af_editor_build_hierarchy(&editor);
        if (editor.vector_animation_smoke) {
            editor.selected_property = AF_PROPERTY_PATH_CONTROL_OUT;
            af_editor_status(&editor, "Programmatic animated vector handles capture");
        } else if (editor.particle_view_smoke) {
            const uint32_t stage = editor.rendered_frames / 8U;
            for (uint32_t index = 0U; index < editor.project.particle_data.count; ++index) {
                af_editor_select_only(&editor,
                                      editor.project.particle_data.emitters[index].node_id);
                break;
            }
            if (stage == 0U) {
                editor.zoom = 0.65F;
                editor.pan = (AfVec2){48.0F, 42.0F};
            } else if (stage == 1U) {
                editor.zoom = 1.5F;
                editor.pan = (AfVec2){-350.0F, -220.0F};
            } else {
                editor.zoom = 0.4F;
                editor.pan = (AfVec2){850.0F, 300.0F};
            }
            af_editor_status(&editor, "Programmatic particle pan / zoom / clip capture");
        } else if (editor.vfx_smoke) {
            for (uint32_t index = 0U; index < editor.project.particle_data.count; ++index) {
                af_editor_select_only(&editor,
                                      editor.project.particle_data.emitters[index].node_id);
                break;
            }
            editor.particle_page = editor.rendered_frames % AF_EDITOR_PARTICLE_PAGE_COUNT;
            af_editor_status(&editor, "Programmatic VFX inspector capture");
        } else if (editor.selection_smoke) {
            af_editor_prepare_selection_smoke(&editor, width, height);
        } else if (editor.context_smoke) {
            const uint32_t context =
                (editor.rendered_frames / 2U) %
                (uint32_t)(AF_EDITOR_CONTEXT_PARTICLE - AF_EDITOR_CONTEXT_HIERARCHY + 1);
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
            } else if (editor.context_menu == AF_EDITOR_CONTEXT_PARTICLE) {
                editor.context_position =
                    (AfVec2){(float)width - editor.right_width + 22.0F, AF_EDITOR_PARTICLE_PAGE_Y};
            } else {
                editor.context_position = (AfVec2){620.0F, 190.0F};
            }
            editor.mouse_position =
                (AfVec2){editor.context_position.x + 52.0F,
                         editor.context_position.y + AF_EDITOR_CONTEXT_HEADER_HEIGHT +
                             AF_EDITOR_CONTEXT_ITEM_HEIGHT + 12.0F};
        } else if (editor.release_smoke) {
            editor.update_panel_visible = true;
            editor.update_checked = false;
            af_editor_status(&editor, "Programmatic release, version and update capture");
        } else if (editor.onboarding_smoke) {
            const uint32_t stage = editor.rendered_frames / 3U;
            editor.welcome_visible = false;
            editor.guided_tour_visible = false;
            editor.example_browser_visible = false;
            editor.guide_browser_visible = false;
            if (stage == 0U) {
                editor.welcome_visible = true;
            } else if (stage == 1U) {
                editor.guided_tour_visible = true;
                editor.guided_step = 2U;
            } else if (stage == 2U) {
                editor.example_browser_visible = true;
                editor.example_selected_index = AF_PROJECT_TEMPLATE_VECTOR_ANIMATION;
            } else {
                editor.guide_browser_visible = true;
                editor.guide_topic = stage == 3U   ? AF_ONBOARDING_RUNTIME_INTEGRATION
                                     : stage == 4U ? AF_ONBOARDING_SECURITY
                                                   : AF_ONBOARDING_LIMITATIONS;
            }
            af_editor_status(&editor, "Programmatic Milestone 28 onboarding capture");
        } else if (editor.platform_smoke) {
            editor.platform_panel_visible = true;
            (void)af_platform_query_capabilities(&editor.window, &editor.platform_capabilities);
            af_editor_status(&editor, "Programmatic display, DPI and input capability capture");
        } else if (editor.workflow_smoke) {
            const uint32_t stage = (editor.rendered_frames / 2U) % 10U;
            editor.command_palette.open = false;
            editor.profiler_visible = false;
            editor.validation_visible = false;
            editor.recovery_overlay = false;
            editor.template_panel_visible = false;
            editor.recent_panel_visible = false;
            editor.preset_panel_visible = false;
            editor.shortcuts_overlay = false;
            editor.help_overlay = false;
            editor.export_panel_visible = false;
            if (stage == 0U) {
                (void)af_editor_command_palette_open(&editor.command_palette,
                                                     AF_EDITOR_ACTION_CONTEXT_ALL);
                (void)af_editor_command_palette_set_query(&editor.command_palette, "ALIGN");
            } else if (stage == 1U) {
                editor.profiler_visible = true;
            } else if (stage == 2U) {
                editor.validation_visible = true;
            } else if (stage == 3U) {
                editor.recovery_overlay = true;
            } else if (stage == 4U) {
                editor.template_panel_visible = true;
                editor.template_selected_index = 1U;
            } else if (stage == 5U) {
                editor.recent_panel_visible = true;
                editor.recent_selected_index = 0U;
            } else if (stage == 6U) {
                editor.preset_panel_visible = true;
            } else if (stage == 7U) {
                editor.shortcuts_overlay = true;
            } else if (stage == 8U) {
                editor.help_overlay = true;
            } else {
                editor.export_panel_visible = true;
            }
            af_editor_status(&editor, "Programmatic workflow and export capture");
        } else if (editor.menu_smoke) {
            const uint32_t menu = (editor.rendered_frames / 2U) % (uint32_t)AF_EDITOR_MENU_HELP;
            editor.top_menu = (AfEditorTopMenu)(AF_EDITOR_MENU_FILE + menu);
            editor.mouse_position =
                (AfVec2){af_editor_top_menu_x(&editor, editor.top_menu) + 22.0F,
                         AF_EDITOR_MENU_HEIGHT + AF_EDITOR_CONTEXT_ITEM_HEIGHT + 12.0F};
        } else if (editor.tooltip_smoke) {
            editor.context_menu = AF_EDITOR_CONTEXT_NONE;
            editor.mouse_position = (AfVec2){editor.left_width + 31.0F, 20.0F};
        }
        af_editor_build_render_scene(&editor, width, height);
        af_editor_update_title(&editor);
        if (editor.sequence_prefix[0] != '\0') {
            char frame_path[AF_EDITOR_PATH_CAPACITY];
            (void)snprintf(frame_path, sizeof(frame_path), "%.480s_%04u.png",
                           editor.sequence_prefix, editor.rendered_frames + 1U);
            result = af_vk_renderer_capture_next(editor.renderer, frame_path);
        } else if (editor.output_path[0] != '\0' &&
                   editor.rendered_frames + 1U == editor.maximum_frames) {
            result = af_vk_renderer_capture_next(editor.renderer, editor.output_path);
        }
        if (result == AF_RESULT_SUCCESS) {
            float canvas_x = 0.0F;
            float canvas_y = 0.0F;
            float canvas_width = 0.0F;
            float canvas_height = 0.0F;
            af_editor_layout(&editor, width, height, &canvas_x, &canvas_y, &canvas_width,
                             &canvas_height);
            const AfVkRenderOptions render_options = {
                .particle_scene = &editor.preview_scene,
                .particle_transform = af_editor_camera(&editor, width, height),
                .particle_layer_order = 89980,
                .particle_clip_x = (uint32_t)fmaxf(canvas_x, 0.0F),
                .particle_clip_y = (uint32_t)fmaxf(canvas_y, 0.0F),
                .particle_clip_width = (uint32_t)fmaxf(canvas_width, 0.0F),
                .particle_clip_height = (uint32_t)fmaxf(canvas_height, 0.0F),
                .image_time = editor.playhead,
                .image_time_override = true,
                .particle_clip_enabled = true};
            result = af_vk_renderer_draw_timed_with_options(
                editor.renderer, &editor.render_scene, fminf(delta_time, 0.1F), &render_options);
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
    af_editor_workflow_shutdown(&editor);
    af_editor_collaboration_release(&editor);
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
