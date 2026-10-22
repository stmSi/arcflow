#include "editor/editor_app.h"

#include "af/core/allocator.h"
#include "af/core/file_io.h"
#include "af/core/hash.h"
#include "af/core/log.h"
#include "af/performance/metrics.h"
#include "af/platform/platform_sdl.h"
#include "af/scene/scene.h"
#include "af/text/text.h"
#include "af/ui/draw.h"
#include "arcflow/arcflow_build_info.h"
#include "arcflow/arcflow_version.h"
#include "editor/editor_actions.h"
#include "editor/editor_commands.h"
#include "editor/editor_export.h"
#include "editor/editor_fonts.h"
#include "editor/editor_frames.h"
#include "editor/editor_gizmo.h"
#include "editor/editor_inspector.h"
#include "editor/editor_preferences.h"
#include "editor/editor_recovery.h"
#include "editor/editor_snapping.h"
#include "editor/editor_text.h"
#include "editor/editor_theme.h"
#include "editor/editor_vector_tools.h"
#include "editor/editor_workflow.h"
#include "onboarding/onboarding.h"
#include "project/image_import.h"
#include "project/interchange.h"
#include "project/project.h"
#include "project/project_templates.h"
#include "renderer/vk_renderer.h"
#include "update/update.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include "web/browser_bridge.h"
#include <emscripten.h>
#endif

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
#define AF_EDITOR_HIERARCHY_TOP 80.0F
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
#define AF_EDITOR_COLOR_TARGET_Y 52.0F
#define AF_EDITOR_COLOR_TARGET_HEIGHT 20.0F
#define AF_EDITOR_COLOR_ALPHA_Y 170.0F
#define AF_EDITOR_COLOR_ALPHA_HEIGHT 14.0F
#define AF_EDITOR_COLOR_HEX_Y 190.0F
#define AF_EDITOR_COLOR_COMPONENT_Y 218.0F
#define AF_EDITOR_INSPECTOR_LABEL_Y 279.0F
#define AF_EDITOR_INSPECTOR_FIELD_Y 294.0F
#define AF_EDITOR_INSPECTOR_FIELD_HEIGHT 26.0F
#define AF_EDITOR_INSPECTOR_FIELD_STEP 30.0F
#define AF_EDITOR_SHAPE_SETTINGS_Y 446.0F
#define AF_EDITOR_CONVERT_PATH_Y 478.0F
#define AF_EDITOR_STYLE_BAR_Y 510.0F
#define AF_EDITOR_STYLE_ROW_Y 542.0F
#define AF_EDITOR_LAYOUT_ROW_Y 578.0F
#define AF_EDITOR_PARTICLE_LABEL_Y 621.0F
#define AF_EDITOR_PARTICLE_PAGE_Y 638.0F
#define AF_EDITOR_PARTICLE_ROW_Y 668.0F
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
    AF_EDITOR_DRAG_ASSET_HEIGHT,
    AF_EDITOR_DRAG_TOOLBAR_HEIGHT,
    AF_EDITOR_DRAG_STATUS_HEIGHT,
    AF_EDITOR_DRAG_TIMELINE_HEADER,
    AF_EDITOR_DRAG_HIERARCHY,
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
    AF_EDITOR_DRAG_PATH_SEGMENT,
    AF_EDITOR_DRAG_CANVAS_BOX,
    AF_EDITOR_DRAG_COLOR_SV,
    AF_EDITOR_DRAG_COLOR_HUE,
    AF_EDITOR_DRAG_COLOR_ALPHA,
    AF_EDITOR_DRAG_INSPECTOR_SCRUB,
    AF_EDITOR_DRAG_TOOL_CREATE
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
    AF_EDITOR_MENU_SHAPE,
    AF_EDITOR_MENU_HELP
} AfEditorTopMenu;

typedef enum AfEditorGradientDrag {
    AF_EDITOR_GRADIENT_DRAG_NONE = 0,
    AF_EDITOR_GRADIENT_DRAG_START,
    AF_EDITOR_GRADIENT_DRAG_END,
    AF_EDITOR_GRADIENT_DRAG_CANVAS_STOP,
    AF_EDITOR_GRADIENT_DRAG_TOOLBAR_STOP,
    AF_EDITOR_GRADIENT_DRAG_DRAW
} AfEditorGradientDrag;

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

typedef enum AfEditorInspectorField {
    AF_EDITOR_INSPECTOR_NONE = 0,
    AF_EDITOR_INSPECTOR_COLOR_HEX,
    AF_EDITOR_INSPECTOR_COLOR_R,
    AF_EDITOR_INSPECTOR_COLOR_G,
    AF_EDITOR_INSPECTOR_COLOR_B,
    AF_EDITOR_INSPECTOR_COLOR_A,
    AF_EDITOR_INSPECTOR_POSITION_X,
    AF_EDITOR_INSPECTOR_POSITION_Y,
    AF_EDITOR_INSPECTOR_WIDTH,
    AF_EDITOR_INSPECTOR_HEIGHT,
    AF_EDITOR_INSPECTOR_SCALE_X,
    AF_EDITOR_INSPECTOR_SCALE_Y,
    AF_EDITOR_INSPECTOR_ROTATION,
    AF_EDITOR_INSPECTOR_OPACITY,
    AF_EDITOR_INSPECTOR_RADIUS,
    AF_EDITOR_INSPECTOR_LAYER,
    AF_EDITOR_INSPECTOR_OUTLINE_WIDTH,
    AF_EDITOR_INSPECTOR_PARTICLE_FIRST,
    AF_EDITOR_INSPECTOR_PARTICLE_LAST = AF_EDITOR_INSPECTOR_PARTICLE_FIRST + 7 * 9 - 1
} AfEditorInspectorField;

typedef struct AfEditorContextMenuInfo {
    const char *title;
    const char *items[8];
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
    AfStyleData preview_style_data;
    AfStyle *preview_styles;
    AfScene preview_scene;
    uint64_t selected_id;
    uint64_t selected_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t selected_count;
    uint32_t hierarchy_order[AF_EDITOR_PROJECT_CAPACITY];
    uint32_t hierarchy_sorted[AF_EDITOR_PROJECT_CAPACITY];
    uint8_t hierarchy_depth[AF_EDITOR_PROJECT_CAPACITY];
    uint32_t hierarchy_count;
    uint32_t hierarchy_scroll;
    uint32_t asset_scroll;
    uint64_t hierarchy_anchor_id;
    float left_width;
    float right_width;
    float asset_height;
    float toolbar_height;
    float toolbar_scroll;
    float status_height;
    float timeline_header_width;
    SDL_Cursor *resize_horizontal_cursor;
    SDL_Cursor *resize_vertical_cursor;
    uint64_t hierarchy_drag_id;
    uint64_t hierarchy_scroll_ticks;
    bool hierarchy_drag_started;
    float zoom;
    AfVec2 pan;
    float grid_step;
    float color_hue;
    bool color_target_outline;
    uint32_t particle_page;
    bool grid_visible;
    bool snapping;
    AfEditorSnapTarget snap_targets[AF_MAX_NODES];
    uint32_t snap_target_count;
    AfEditorSnapBounds snap_drag_bounds;
    AfEditorSnapResult snap_result;

    AfVec2 mouse_position;
    AfVec2 context_position;
    AfEditorContextMenu context_menu;
    AfEditorTopMenu top_menu;
    uint32_t top_menu_item;
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
    uint32_t curve_component;
    float curve_drag_min, curve_drag_max;
    AfPathTessellationScratch *path_scratch;
    AfTextEngine *text_engine;
    AfFileData document_font_files[12];
    AfEditorFontCatalog font_catalog;
    AfTextEngine *system_font_engine;
    AfFileData system_font_file;
    const AfEditorFontFace *system_font_face;
    AfEditorCommand *text_preview;
    AfStyleData text_preview_styles;
    uint64_t text_preview_hash;
    bool text_preview_valid;
    size_t text_caret, text_anchor;
    char font_search[128];
    uint32_t font_scroll;
    AfVec2 text_carets[AF_PROJECT_TEXT_BYTES];
    AfVec2 text_caret_tops[AF_PROJECT_TEXT_BYTES];
    bool text_caret_valid[AF_PROJECT_TEXT_BYTES];
    float text_caret_height;

    AfFileData font_file;
    AfGlyphPlacement *text_glyphs;
    AfTextLine *text_lines;
    uint32_t selected_path_command;
    uint64_t path_command_selection_node_id;
    uint32_t selected_path_commands[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
    uint32_t selected_path_command_count;
    AfProjectPathSnapshot path_drag_before;
    AfPathCommand path_drag_commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    AfProjectKeyframeSnapshot path_key_drag_before;
    uint64_t path_drag_key_id;
    bool path_key_drag;
    float path_segment_t;
    AfVec2 path_segment_start;
    bool timeline_focus;
    bool curve_visible;
    bool frames_visible;
    bool onion_skin_disabled;
    bool preview_playing;
    uint64_t previous_ticks;
    AfEditorDragMode drag_mode;
    AfVec2 drag_start_mouse;
    AfProjectNodeSnapshot drag_before;
    AfProjectNodeSnapshot selection_drag_before[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t selection_drag_count;
    uint64_t transform_drag_node_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint64_t transform_drag_key_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    AfPropertyType transform_drag_key_properties[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    float transform_drag_key_starts[AF_EDITOR_COMMAND_MAX_SNAPSHOTS][4];
    AfProjectKeyframeSnapshot transform_drag_key_before[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    bool transform_drag_key_created[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t transform_drag_key_count;
    AfPropertyType transform_drag_property;
    AfEditorGizmoHandle gizmo_handle;
    AfTransform2D gizmo_start_transform;
    AfAffine2 gizmo_start_parent_affine;
    AfVec2 gizmo_start_pivot;
    AfVec2 gizmo_start_pivot_parent;
    AfLayoutNode gizmo_start_layout;
    bool gizmo_start_has_layout;
    bool gizmo_position_changed;
    float gizmo_start_angle;
    AfVec2 canvas_box_start;
    AfVec2 canvas_box_current;
    bool canvas_box_additive;
    AfEditorVectorTool active_tool;
    AfVec2 tool_start;
    AfVec2 tool_current;
    AfVec2 tool_points[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
    uint32_t tool_point_count;
    AfPathCommand tool_commands[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U];
    uint32_t tool_command_count;
    AfVec2 tool_pen_outgoing;
    bool tool_pen_has_outgoing;
    uint64_t gradient_target;
    bool gradient_outline;
    uint32_t gradient_stop;
    AfEditorGradientDrag gradient_drag;
    AfPaintType gradient_tool_type;
    AfAffine2 gradient_drag_frame;
    AfVec2 gradient_drag_origin;
    AfVec2 gradient_drag_pointer;
    float gradient_drag_stop_offset;
    uint32_t gradient_drag_stop_before;
    bool gradient_drag_changed;
    bool gradient_drag_was_dirty;
    bool gradient_stop_focus;
    uint32_t choice_menu; /* 1: fill type, 2: animation property. */
    uint32_t choice_index;
    uint64_t choice_target;
    AfVec2 choice_origin;
    bool text_dragging;
    bool text_panel_open;
    bool text_panel_replace;
    uint32_t text_panel_field;
    uint64_t text_panel_id;
    AfVec2 text_panel_position;
    AfProjectText text_panel_draft;
    char text_panel_numbers[6][32];
    char text_panel_original_color[32];
    bool offset_panel_open;
    bool offset_replace;
    char offset_buffer[32];
    uint64_t tool_pen_resume_id;
    uint64_t tool_pen_resume_generation;
    uint32_t tool_pen_resume_start;
    uint32_t tool_pen_resume_end;
    uint32_t tool_pen_resume_count;
    AfAffine2 tool_pen_resume_inverse;
    AfEditorInspectorField inspector_field;
    bool inspector_replace;
    bool inspector_ratio_locked;
    AfEditorInspectorField inspector_scrub_field;
    float inspector_scrub_start_value;
    float inspector_scroll;
    double inspector_particle_scrub_start_value;
    bool inspector_scrub_changed;
    char inspector_buffer[48];
    bool rename_active;
    AfProjectNodeSnapshot rename_before;
    char rename_buffer[AF_PROJECT_NAME_CAPACITY];
    char project_path[AF_EDITOR_PATH_CAPACITY];
    char package_path[AF_EDITOR_PATH_CAPACITY];
    char output_path[AF_EDITOR_PATH_CAPACITY];
    char sequence_prefix[AF_EDITOR_PATH_CAPACITY];
    char status[AF_EDITOR_STATUS_CAPACITY];
    AfUiContext ui;
    uint64_t ui_previous_ticks;
    uint32_t dialog_event_type;
    AfEditorDialogAction dialog_action;
    uint32_t maximum_frames;
    uint32_t rendered_frames;
    bool resize_smoke;
    bool context_smoke;
    bool tooltip_smoke;
    bool selection_smoke;
    bool outline_smoke;
    bool inspector_smoke;
    bool studio_smoke;
    bool template_smoke;
    bool vector_tools_smoke;
    bool pan_smoke;
    uint32_t pan_sample_count;
    uint32_t pan_gpu_samples;
    double pan_gpu_ms;
    double pan_draw_batches;
    uint64_t pan_path_rebuilds;
    double pan_build_ms;
    double pan_upload_ms;
    double pan_frame_ms;
    double pan_max_frame_ms;
    bool vector_preview_smoke;
    bool pen_close_smoke;
    bool compound_smoke;
    bool boolean_smoke;
    bool vector_package_smoke;
    bool gradient_drag_smoke;
    bool editing_smoke;
    bool text_panel_smoke;
    bool vfx_smoke;
    bool particle_view_smoke;
    bool particle_inputs_smoke;
    bool vector_animation_smoke;
    bool image_smoke;
    bool menu_smoke;
    bool snapping_smoke;
    bool workflow_smoke;
    bool platform_smoke;
    bool release_smoke;
    bool onboarding_smoke;
    bool vector_animation_smoke_prepared;
    bool outline_smoke_prepared;
    uint32_t inspector_smoke_stage;
    uint32_t vector_tools_smoke_stage;
    uint32_t vector_tools_smoke_scene_start;
    uint32_t vector_tools_smoke_path_start;
    bool dialog_active;
    bool export_panel_visible;
    AfEditorExportOptions export_options;
    uint32_t export_format;
    uint32_t export_field;
    bool export_replace;
    char export_buffer[16];
    bool export_ready;
    bool export_smoke;
    bool export_preview_dirty;
    bool export_preview_valid;
    bool export_preview_draw[AF_EDITOR_PROJECT_CAPACITY];
    uint32_t export_visible_count;
    uint32_t export_outside_count;
    AfInterchangeDiagnostics export_diagnostics;

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
    AfEditorPalette palette;
    bool appearance_visible;
    bool appearance_hex_active;
    bool appearance_hex_replace;
    bool appearance_drag;
    bool appearance_drag_changed;
    uint32_t appearance_color;
    uint32_t appearance_drag_rgb;
    AfEditorTheme appearance_drag_theme;
    uint32_t appearance_drag_colors[3];
    float appearance_drag_x;
    char appearance_hex[9];
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
static AfColor af_editor_inspector_color(const AfEditorState *editor, const AfSceneNode *node);
static void af_editor_collaboration_track_path(AfEditorState *editor);
static void af_editor_collaboration_saved(AfEditorState *editor);
static void af_editor_collaboration_update(AfEditorState *editor, uint64_t now_ns);
static bool af_editor_hierarchy_name_matches(const AfEditorState *editor, uint32_t node_index);
static AfVec2 af_editor_tool_line_end(AfVec2 start, AfVec2 end, SDL_Keymod modifiers);
static void af_editor_open_export(AfEditorState *editor);
static void af_editor_render_export_panel(AfEditorState *editor, int width, int height);
static AfResult af_editor_export_with_options(AfEditorState *editor, const char *path,
                                              AfEditorDialogAction action);
static void af_editor_export_panel_mouse(AfEditorState *editor, int width, int height, float x,
                                         float y);
static void af_editor_export_panel_key(AfEditorState *editor, const SDL_KeyboardEvent *event);
static void af_editor_export_panel_text(AfEditorState *editor, const char *text);
static AfVec2 af_editor_tool_snap_point(AfEditorState *editor, AfVec2 point);
static bool af_editor_pen_closes_at(const AfEditorState *editor, AfAffine2 camera, AfVec2 screen);
static AfResult af_editor_knife_path(AfEditorState *editor, AfVec2 project_start,
                                     AfVec2 project_end);
static void af_editor_convert_selected_to_path(AfEditorState *editor);
static void af_editor_cancel_vector_tool(AfEditorState *editor, const char *status);
static bool af_editor_selected_path_has_animation(const AfEditorState *editor);
static bool af_editor_path_segment_hit(const AfEditorState *editor, int width, int height,
                                       AfVec2 mouse, uint32_t *out_command, float *out_t);
static void af_editor_group_selection(AfEditorState *editor);
static void af_editor_choice_open(AfEditorState *editor, uint32_t menu, int width, int height,
                                  float x, float y);
static void af_editor_render_gradient_handles(AfEditorState *editor, int width, int height);
static void af_editor_save_preferences(AfEditorState *editor);
static void af_editor_render_appearance(AfEditorState *editor, int width, int height);
static uint32_t af_editor_gradient_stop(const AfEditorState *editor, const AfPaint *paint);
static void af_editor_text_preview_update(AfEditorState *editor);
static void af_editor_text_panel_mouse(AfEditorState *editor, int width, int height, float x,
                                       float y);
static void af_editor_render_text_canvas(AfEditorState *editor, int width, int height);
static void af_editor_open_text_panel(AfEditorState *editor, int width, int height);
static void af_editor_finish_text_tool(AfEditorState *editor, int width, int height);
static bool af_editor_is_text(const AfEditorState *editor, uint64_t id);
static AfPathCommand af_editor_pen_closing_segment(const AfEditorState *editor);
static AfResult af_editor_commit_pen_continuation(AfEditorState *editor, uint32_t count,
                                                  bool closed);

static void af_editor_render_frames(AfEditorState *editor, int width, int height);
static void af_editor_toggle_frames(AfEditorState *editor);
static void af_editor_frame_seek(AfEditorState *editor, uint32_t frame);
static uint32_t af_editor_current_frame(const AfEditorState *editor);
static bool af_editor_frame_node_active(const AfEditorState *editor, uint64_t node_id);
static bool af_editor_frames_mouse(AfEditorState *editor, int width, int height, float x, float y);
static AfResult af_editor_frame_parent(AfEditorState *editor, uint64_t *parent);
static void af_editor_render_onion(AfEditorState *editor, AfAffine2 camera);

// clang-format off
#include "editor/editor_app_core.inc"
#include "editor/editor_app_collaboration.inc"
#include "editor/editor_app_images.inc"
#include "editor/editor_app_draw.inc"
#include "editor/editor_app_splitters.inc"
#include "editor/editor_app_gradient_target.inc"
#include "editor/editor_app_timeline.inc"
#include "editor/editor_app_preview.inc"
#include "editor/editor_app_particle_inputs.inc"
#include "editor/editor_app_panels.inc"
#include "editor/editor_app_tooltips.inc"
#include "editor/editor_app_canvas.inc"
#include "editor/editor_app_snapping.inc"
#include "editor/editor_app_animation.inc"
#include "editor/editor_app_frames.inc"
#include "editor/editor_app_paths.inc"
#include "editor/editor_app_text.inc"
#include "editor/editor_app_properties.inc"
#include "editor/editor_app_gradient.inc"
#include "editor/editor_app_appearance.inc"
#include "editor/editor_app_menus.inc"
#include "editor/editor_app_export.inc"
#include "editor/editor_app_workflow_core.inc"
#include "editor/editor_app_onboarding.inc"
#include "editor/editor_app_studio.inc"
#include "editor/editor_app_workflow_render.inc"
#include "editor/editor_app_workflow_input.inc"
#include "editor/editor_app_input.inc"
#include "editor/editor_app_particle_smoke.inc"
#include "editor/editor_app_menu_smoke.inc"
#include "editor/editor_app_snap_smoke.inc"
#include "editor/editor_app_export_smoke.inc"
#include "editor/editor_app_gradient_smoke.inc"
#include "editor/editor_app_gradient_tool_smoke.inc"
#include "editor/editor_app_graph_smoke.inc"
#include "editor/editor_app_editing_smoke.inc"
#include "editor/editor_app_studio_smoke.inc"
#include "editor/editor_app_template_smoke.inc"
#include "editor/editor_app_color_input_smoke.inc"
// clang-format on

int af_editor_run(int argument_count, char **arguments) {
    AfEditorState editor = {.selected_id = 0U,
                            .selected_path_command = UINT32_MAX,
                            .left_width = 230.0F,
                            .right_width = 286.0F,
                            .asset_height = 190.0F,
                            .toolbar_height = AF_EDITOR_TOOLBAR_HEIGHT,
                            .status_height = AF_EDITOR_STATUS_HEIGHT,
                            .timeline_header_width = AF_EDITOR_TIMELINE_HEADER_WIDTH,
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
    result = af_editor_reserve_authoring_nodes(&editor, &editor.project);
    if (result != AF_RESULT_SUCCESS)
        goto cleanup;
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
    editor.preview_styles =
        af_allocate_array(&editor.allocator, AF_MAX_STYLES, sizeof(AfStyle), _Alignof(AfStyle));
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
        editor.preview_styles == NULL || editor.curve_order == NULL ||
        editor.path_scratch == NULL || editor.text_glyphs == NULL || editor.text_lines == NULL) {
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
        if (af_platform_resource_path("fonts/LiberationSans-Regular.ttf", font_path,
                                      sizeof(font_path)) == AF_RESULT_SUCCESS) {
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
    editor.render_scene.styles = &editor.project.style_data;
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
#ifdef __EMSCRIPTEN__
        emscripten_sleep(16);
#endif
        SDL_Event event;
        int width = editor.window_width;
        int height = editor.window_height;
        uint64_t current_ticks = 0U;
        float delta_time = 0.0F;
        AfPlatformWindowMetrics metrics = {0};
        /* Process close/restore events even when no drawable is available. */
        while (SDL_PollEvent(&event)) {
            af_editor_handle_event(&editor, &event, width, height);
        }
        if (!editor.running) {
            break;
        }
        current_ticks = SDL_GetTicksNS();
        delta_time = (float)((double)(current_ticks - editor.previous_ticks) / 1000000000.0);
        editor.last_frame_ms = delta_time * 1000.0F;
        editor.previous_ticks = current_ticks;
        af_editor_workflow_update(&editor, current_ticks);
        af_editor_collaboration_update(&editor, current_ticks);
        if (af_platform_window_metrics(&editor.window, &metrics) != AF_RESULT_SUCCESS ||
            metrics.pixel_width <= 0 || metrics.pixel_height <= 0 ||
            (SDL_GetWindowFlags(editor.window.window) &
             (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0U) {
            /* Keep recovery/lock timers alive without rebuilding or submitting GPU work. */
            if (SDL_WaitEventTimeout(&event, 100)) {
                af_editor_handle_event(&editor, &event, width, height);
            }
            editor.previous_ticks = SDL_GetTicksNS();
            continue;
        }
        width = metrics.pixel_width;
        height = metrics.pixel_height;
        editor.window_width = width;
        editor.window_height = height;
        if (editor.preview_playing) {
            const AfProjectClip *clip =
                af_project_find_clip_const(&editor.project, editor.selected_clip_id);
            if (clip != NULL) {
                editor.playhead += fminf(delta_time, 0.1F);
                if (clip->loop && editor.playhead >= clip->loop_end) {
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
        } else if (editor.outline_smoke && !editor.outline_smoke_prepared) {
            af_editor_prepare_outline_smoke(&editor);
            editor.outline_smoke_prepared = true;
        }
        if (editor.vector_preview_smoke) {
            result = af_editor_vector_preview_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
        }
        if (editor.pen_close_smoke && editor.rendered_frames == 0U) {
            result = af_editor_pen_close_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            af_editor_status(&editor, "Pen closure smoke: connections, handles and undo verified");
        }
        if (editor.compound_smoke && editor.rendered_frames == 0U) {
            result = af_editor_compound_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
        }
        if (editor.boolean_smoke && editor.rendered_frames == 0U) {
            result = af_editor_boolean_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
        }
        if (editor.snapping_smoke && editor.rendered_frames == 0U) {
            result = af_editor_snapping_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
        }
        if (editor.export_smoke && editor.rendered_frames == 0U) {
            result = af_editor_export_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
        }
        if (editor.template_smoke && editor.rendered_frames == 0U) {
            result = af_editor_template_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
        }
        if (editor.studio_smoke && editor.rendered_frames == 0U) {
            result = af_editor_studio_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                break;
        }
        if (editor.particle_inputs_smoke && editor.rendered_frames == 0U) {
            result = af_editor_particle_inputs_smoke(&editor, width);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
        }
        if (editor.menu_smoke && editor.rendered_frames == 0U) {
            result = af_editor_menu_interaction_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
        }
        if (editor.vector_package_smoke && editor.rendered_frames == 0U) {
            result = af_editor_vector_package_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
        }
        if (editor.editing_smoke && editor.rendered_frames == 0U) {
            result = af_editor_editing_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
        }
        if (editor.gradient_drag_smoke && editor.rendered_frames == 0U) {
            result = af_editor_gradient_drag_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
        }
        if (editor.vector_tools_smoke && editor.vector_tools_smoke_stage < 8U) {
            const float toolbar_start = editor.left_width + 14.0F;
            const float canvas_left = editor.left_width + 90.0F;
            const float canvas_top = editor.toolbar_height + 90.0F;
            SDL_MouseButtonEvent click = {.button = SDL_BUTTON_LEFT, .down = true, .clicks = 1U};
            if (editor.vector_tools_smoke_stage == 0U) {
                editor.vector_tools_smoke_scene_start = editor.project.scene.count;
                editor.vector_tools_smoke_path_start = editor.project.path_data.path_count;
            }
            if (editor.vector_tools_smoke_stage == 2U &&
                editor.drag_mode == AF_EDITOR_DRAG_TOOL_CREATE) {
                af_editor_mouse_up(&editor, width, height);
            }
            if (editor.vector_tools_smoke_stage == 4U && editor.active_tool == AF_EDITOR_TOOL_PEN &&
                editor.drag_mode == AF_EDITOR_DRAG_TOOL_CREATE) {
                af_editor_mouse_up(&editor, width, height);
                if (!af_editor_finish_pen_path(&editor)) {
                    result = AF_RESULT_INVALID_ARGUMENT;
                    goto cleanup;
                }
                af_editor_update_preview(&editor);
            }
            {
                static const AfEditorVectorTool tools[8] = {
                    AF_EDITOR_TOOL_RECTANGLE, AF_EDITOR_TOOL_STAR,
                    AF_EDITOR_TOOL_PENCIL,    AF_EDITOR_TOOL_PEN,
                    AF_EDITOR_TOOL_SCISSORS,  AF_EDITOR_TOOL_SEGMENT_DELETE,
                    AF_EDITOR_TOOL_KNIFE,     AF_EDITOR_TOOL_NODE};
                click.x =
                    toolbar_start + (float)tools[editor.vector_tools_smoke_stage] * 42.0F + 16.0F;
                click.y = AF_EDITOR_MENU_HEIGHT + 16.0F;
                af_editor_mouse_down(&editor, width, height, &click, SDL_KMOD_NONE);
            }
            if (editor.vector_tools_smoke_stage == 0U || editor.vector_tools_smoke_stage == 1U) {
                SDL_MouseMotionEvent motion = {0};
                click.x = canvas_left + (float)editor.vector_tools_smoke_stage * 220.0F;
                click.y = canvas_top;
                af_editor_mouse_down(&editor, width, height, &click, SDL_KMOD_NONE);
                motion.x = click.x + 170.0F;
                motion.y = click.y + 120.0F;
                motion.xrel = 170.0F;
                motion.yrel = 120.0F;
                af_editor_mouse_motion(&editor, width, height, &motion);
                if (editor.vector_tools_smoke_stage == 0U) {
                    af_editor_mouse_up(&editor, width, height);
                }
            } else if (editor.vector_tools_smoke_stage == 2U) {
                click.x = canvas_left + 80.0F;
                click.y = canvas_top + 220.0F;
                af_editor_mouse_down(&editor, width, height, &click, SDL_KMOD_NONE);
                for (uint32_t point = 1U; point <= 12U; ++point) {
                    SDL_MouseMotionEvent motion = {.x = click.x + (float)point * 18.0F,
                                                   .y = click.y + sinf((float)point * 0.7F) * 34.0F,
                                                   .xrel = 18.0F};
                    af_editor_mouse_motion(&editor, width, height, &motion);
                }
                af_editor_mouse_up(&editor, width, height);
            } else if (editor.vector_tools_smoke_stage == 3U) {
                static const AfVec2 points[3] = {
                    {520.0F, 170.0F}, {660.0F, 270.0F}, {500.0F, 350.0F}};
                for (uint32_t point = 0U; point < 3U; ++point) {
                    click.x = editor.left_width + points[point].x;
                    click.y = points[point].y;
                    af_editor_mouse_down(&editor, width, height, &click, SDL_KMOD_NONE);
                    if (point >= 1U) {
                        const float xrel = point == 1U ? 34.0F : -26.0F;
                        const float yrel = point == 1U ? -28.0F : 22.0F;
                        SDL_MouseMotionEvent motion = {
                            .x = click.x + xrel, .y = click.y + yrel, .xrel = xrel, .yrel = yrel};
                        af_editor_mouse_motion(&editor, width, height, &motion);
                    }
                    if (point < 2U) {
                        af_editor_mouse_up(&editor, width, height);
                    }
                }
            } else if (editor.vector_tools_smoke_stage == 4U) {
                const AfSceneNode *node =
                    af_scene_find_const(&editor.preview_scene, editor.selected_id);
                const AfPath *path =
                    af_path_find_node_const(&editor.render_path_data, editor.selected_id);
                if (node == NULL || path == NULL || path->command_count <= 1U) {
                    result = AF_RESULT_INVALID_ARGUMENT;
                    goto cleanup;
                }
                {
                    const AfPathCommand *commands =
                        &editor.render_path_data.commands[path->command_start];
                    const AfVec2 local =
                        af_editor_path_segment_point(commands[0].point, &commands[1], 0.5F);
                    const AfVec2 screen = af_affine_transform_point(
                        af_affine_multiply(af_editor_camera(&editor, width, height),
                                           node->world_transform),
                        local);
                    click.x = screen.x;
                    click.y = screen.y;
                    af_editor_mouse_down(&editor, width, height, &click, SDL_KMOD_NONE);
                }
            } else if (editor.vector_tools_smoke_stage == 5U) {
                const AfSceneNode *node =
                    af_scene_find_const(&editor.preview_scene, editor.selected_id);
                const AfPath *path =
                    af_path_find_node_const(&editor.render_path_data, editor.selected_id);
                if (node == NULL || path == NULL || path->command_count <= 3U) {
                    result = AF_RESULT_INVALID_ARGUMENT;
                    goto cleanup;
                }
                {
                    const AfPathCommand *commands =
                        &editor.render_path_data.commands[path->command_start];
                    const AfVec2 local =
                        af_editor_path_segment_point(commands[2].point, &commands[3], 0.5F);
                    const AfVec2 screen = af_affine_transform_point(
                        af_affine_multiply(af_editor_camera(&editor, width, height),
                                           node->world_transform),
                        local);
                    click.x = screen.x;
                    click.y = screen.y;
                    af_editor_mouse_down(&editor, width, height, &click, SDL_KMOD_NONE);
                }
            } else if (editor.vector_tools_smoke_stage == 6U) {
                const AfSceneNode *node =
                    af_scene_find_const(&editor.preview_scene, editor.selected_id);
                const AfPath *path =
                    af_path_find_node_const(&editor.render_path_data, editor.selected_id);
                if (node == NULL || path == NULL || path->command_count <= 3U) {
                    result = AF_RESULT_INVALID_ARGUMENT;
                    goto cleanup;
                }
                {
                    const AfPathCommand *commands =
                        &editor.render_path_data.commands[path->command_start];
                    const AfAffine2 transform = af_affine_multiply(
                        af_editor_camera(&editor, width, height), node->world_transform);
                    const AfVec2 first = af_affine_transform_point(transform, commands[2].point);
                    const AfVec2 second = af_affine_transform_point(transform, commands[3].point);
                    const AfVec2 middle = {(first.x + second.x) * 0.5F,
                                           (first.y + second.y) * 0.5F};
                    const float dx = second.x - first.x;
                    const float dy = second.y - first.y;
                    const float length = sqrtf(dx * dx + dy * dy);
                    SDL_MouseMotionEvent motion = {0};
                    if (length <= 0.0001F) {
                        result = AF_RESULT_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    click.x = middle.x - dy / length * 48.0F;
                    click.y = middle.y + dx / length * 48.0F;
                    motion.x = middle.x + dy / length * 48.0F;
                    motion.y = middle.y - dx / length * 48.0F;
                    motion.xrel = motion.x - click.x;
                    motion.yrel = motion.y - click.y;
                    af_editor_mouse_down(&editor, width, height, &click, SDL_KMOD_NONE);
                    af_editor_mouse_motion(&editor, width, height, &motion);
                    af_editor_mouse_up(&editor, width, height);
                }
            } else {
                const AfPath *path =
                    af_path_find_node_const(&editor.project.path_data, editor.selected_id);
                if (path != NULL && path->command_count > 2U) {
                    float action_canvas_x = 0.0F;
                    float action_canvas_y = 0.0F;
                    float action_canvas_width = 0.0F;
                    float action_canvas_height = 0.0F;
                    af_editor_path_selection_clear(&editor);
                    for (uint32_t command_index = 0U; command_index < path->command_count;
                         ++command_index) {
                        if (editor.project.path_data.commands[path->command_start + command_index]
                                .type != AF_PATH_CLOSE) {
                            (void)af_editor_path_selection_add(&editor, command_index);
                        }
                    }
                    af_editor_layout(&editor, width, height, &action_canvas_x, &action_canvas_y,
                                     &action_canvas_width, &action_canvas_height);
                    (void)action_canvas_width;
                    (void)action_canvas_height;
                    if (!af_editor_path_action_mouse_down(
                            &editor, width, height,
                            (AfVec2){action_canvas_x + 12.0F + 692.0F + 35.0F,
                                     action_canvas_y + AF_EDITOR_PATH_ACTION_Y + 16.0F},
                            SDL_KMOD_NONE)) {
                        result = AF_RESULT_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    editor.selected_path_command = 1U;
                    af_editor_path_selection_only(&editor, editor.selected_path_command);
                    editor.selected_property = AF_PROPERTY_PATH_CONTROL_IN;
                    if (af_editor_add_keyframe(&editor) != AF_RESULT_SUCCESS) {
                        result = AF_RESULT_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    for (uint32_t command_index = 0U; command_index < path->command_count;
                         ++command_index) {
                        if (editor.project.path_data.commands[path->command_start + command_index]
                                .type != AF_PATH_CLOSE) {
                            (void)af_editor_path_selection_add(&editor, command_index);
                        }
                    }
                    editor.selected_path_command = 1U;
                }
            }
            af_editor_update_preview(&editor);
            {
                const uint32_t stage = editor.vector_tools_smoke_stage;
                const uint32_t expected_nodes = editor.vector_tools_smoke_scene_start +
                                                (stage < 2U ? 1U : (stage < 4U ? 3U : 4U));
                const uint32_t expected_paths = editor.vector_tools_smoke_path_start +
                                                (stage <= 1U ? 0U : (stage < 4U ? 2U : 3U));
                bool pen_curve = stage < 3U;
                bool animated_handle = stage < 7U;
                bool scissors_cut = stage < 4U;
                bool segment_deleted = stage < 5U;
                bool knife_cut = stage < 6U;
                const AfPath *selected_path =
                    af_path_find_node_const(&editor.project.path_data, editor.selected_id);
                if (stage == 3U) {
                    for (uint32_t command = 0U; command < editor.tool_command_count; ++command) {
                        if (editor.tool_commands[command].type == AF_PATH_CUBIC) {
                            pen_curve = true;
                            break;
                        }
                    }
                } else if (stage > 3U && selected_path != NULL) {
                    for (uint32_t command = 0U; command < selected_path->command_count; ++command) {
                        if (editor.project.path_data
                                .commands[selected_path->command_start + command]
                                .type == AF_PATH_CUBIC) {
                            pen_curve = true;
                            break;
                        }
                    }
                }
                if (stage >= 4U && selected_path != NULL) {
                    uint32_t move_count = 0U;
                    for (uint32_t command = 0U; command < selected_path->command_count; ++command) {
                        move_count += editor.project.path_data
                                          .commands[selected_path->command_start + command]
                                          .type == AF_PATH_MOVE;
                    }
                    scissors_cut = move_count >= 2U;
                    if (stage == 5U) {
                        segment_deleted = selected_path->command_count == 4U;
                    } else if (stage >= 6U) {
                        segment_deleted = selected_path->command_count >= 6U && move_count >= 3U;
                        knife_cut = move_count >= 3U && selected_path->command_count >= 6U;
                    }
                }
                if (stage >= 7U) {
                    for (uint32_t track = 0U; track < editor.project.track_count; ++track) {
                        const AfProjectTrack *candidate = &editor.project.tracks[track];
                        if (candidate->node_id == editor.selected_id &&
                            candidate->property == AF_PROPERTY_PATH_CONTROL_IN &&
                            candidate->element_index == 1U) {
                            animated_handle = true;
                            break;
                        }
                    }
                }
                const bool valid = editor.project.scene.count == expected_nodes &&
                                   editor.project.path_data.path_count == expected_paths &&
                                   pen_curve && scissors_cut && segment_deleted && knife_cut &&
                                   animated_handle &&
                                   ((stage != 1U && stage != 3U) ||
                                    editor.drag_mode == AF_EDITOR_DRAG_TOOL_CREATE) &&
                                   af_project_validate(&editor.project) == AF_RESULT_SUCCESS &&
                                   (stage != 7U || (editor.active_tool == AF_EDITOR_TOOL_NODE &&
                                                    editor.selected_path_command_count >= 3U));
                if (!valid) {
                    result = AF_RESULT_INVALID_ARGUMENT;
                    goto cleanup;
                }
            }
            if (editor.vector_tools_smoke_stage == 7U) {
                result = af_editor_knife_smoke(&editor, width, height);
                if (result == AF_RESULT_SUCCESS) {
                    result = af_editor_compound_smoke(&editor, width, height);
                }
                if (result == AF_RESULT_SUCCESS) {
                    result = af_editor_boolean_smoke(&editor, width, height);
                }
                if (result == AF_RESULT_SUCCESS) {
                    result = af_editor_interaction_smoke(&editor, width, height);
                }
                if (result == AF_RESULT_SUCCESS) {
                    result = af_editor_pen_close_smoke(&editor, width, height);
                }
                if (result != AF_RESULT_SUCCESS) {
                    af_log(AF_LOG_ERROR, "Vector interaction workflow smoke failed");
                    goto cleanup;
                }
            }
            ++editor.vector_tools_smoke_stage;
        }
        if (editor.inspector_smoke && editor.inspector_smoke_stage < 7U) {
            const float panel_x = (float)width - editor.right_width + 18.0F;
            const float available = editor.right_width - 36.0F;
            SDL_MouseButtonEvent field_click = {.button = SDL_BUTTON_LEFT, .down = true};
            float initial_aspect = 0.0F;
            bool finish_text_edit = true;
            if (editor.selected_id == 0U) {
                for (uint32_t index = 0U; index < editor.project.scene.count; ++index) {
                    if (editor.project.scene.nodes[index].type != AF_NODE_GROUP) {
                        af_editor_select_only(&editor, editor.project.scene.nodes[index].id);
                        break;
                    }
                }
            }
            af_editor_update_preview(&editor);
            if (editor.inspector_smoke_stage == 0U) {
                field_click.x = panel_x + 20.0F;
                field_click.y = AF_EDITOR_INSPECTOR_FIELD_Y + 12.0F;
                af_editor_mouse_down(&editor, width, height, &field_click, SDL_KMOD_NONE);
                af_editor_mouse_up(&editor, width, height);
                af_editor_append_inspector_text(&editor, "275.5");
            } else if (editor.inspector_smoke_stage == 1U) {
                field_click.x = panel_x + 40.0F;
                field_click.y = AF_EDITOR_COLOR_HEX_Y + 12.0F;
                af_editor_mouse_down(&editor, width, height, &field_click, SDL_KMOD_NONE);
                af_editor_mouse_up(&editor, width, height);
                af_editor_append_inspector_text(&editor, "#3366CC80");
            } else if (editor.inspector_smoke_stage == 2U) {
                const AfSceneNode *before =
                    af_scene_find_const(&editor.preview_scene, editor.selected_id);
                SDL_MouseButtonEvent ratio_click = {.button = SDL_BUTTON_LEFT,
                                                    .down = true,
                                                    .x = panel_x + available - 38.0F,
                                                    .y = AF_EDITOR_INSPECTOR_LABEL_Y + 2.0F};
                if (before != NULL && before->resolved_bounds.y > 0.0001F) {
                    initial_aspect = before->resolved_bounds.x / before->resolved_bounds.y;
                }
                af_editor_mouse_down(&editor, width, height, &ratio_click, SDL_KMOD_NONE);
                field_click.x = panel_x + 20.0F;
                field_click.y =
                    AF_EDITOR_INSPECTOR_FIELD_Y + AF_EDITOR_INSPECTOR_FIELD_STEP + 12.0F;
                af_editor_mouse_down(&editor, width, height, &field_click, SDL_KMOD_NONE);
                af_editor_mouse_up(&editor, width, height);
                af_editor_append_inspector_text(&editor, "310");
            } else if (editor.inspector_smoke_stage == 3U) {
                field_click.x = panel_x + 40.0F;
                field_click.y = AF_EDITOR_STYLE_BAR_Y + 12.0F;
                af_editor_mouse_down(&editor, width, height, &field_click, SDL_KMOD_NONE);
                af_editor_mouse_up(&editor, width, height);
                af_editor_append_inspector_text(&editor, "12.5");
            } else if (editor.inspector_smoke_stage == 4U) {
                field_click.x = panel_x + available - 10.0F;
                field_click.y = AF_EDITOR_STYLE_BAR_Y + 12.0F;
                af_editor_mouse_down(&editor, width, height, &field_click, SDL_KMOD_NONE);
                finish_text_edit = false;
            } else if (editor.inspector_smoke_stage == 5U) {
                field_click.x = panel_x + 20.0F;
                field_click.y =
                    AF_EDITOR_INSPECTOR_FIELD_Y + 2.0F * AF_EDITOR_INSPECTOR_FIELD_STEP + 12.0F;
                af_editor_mouse_down(&editor, width, height, &field_click, SDL_KMOD_NONE);
                af_editor_mouse_up(&editor, width, height);
                af_editor_append_inspector_text(&editor, "1.5");
            } else {
                SDL_MouseMotionEvent motion = {
                    .x = panel_x + 60.0F, .y = AF_EDITOR_INSPECTOR_FIELD_Y + 12.0F, .xrel = 40.0F};
                field_click.x = panel_x + 20.0F;
                field_click.y = AF_EDITOR_INSPECTOR_FIELD_Y + 12.0F;
                af_editor_mouse_down(&editor, width, height, &field_click, SDL_KMOD_NONE);
                af_editor_mouse_motion(&editor, width, height, &motion);
                af_editor_mouse_up(&editor, width, height);
                finish_text_edit = false;
            }
            if (finish_text_edit && !af_editor_finish_inspector_edit(&editor, true)) {
                result = AF_RESULT_INVALID_ARGUMENT;
                goto cleanup;
            }
            {
                const AfSceneNode *inspected =
                    af_scene_find_const(&editor.preview_scene, editor.selected_id);
                const bool valid_position =
                    editor.inspector_smoke_stage != 0U ||
                    (inspected != NULL &&
                     fabsf(inspected->local_transform.position.x - 275.5F) < 0.001F);
                const bool valid_color =
                    editor.inspector_smoke_stage != 1U ||
                    (inspected != NULL && fabsf(inspected->fill.r - 0.2F) < 0.001F &&
                     fabsf(inspected->fill.g - 0.4F) < 0.001F &&
                     fabsf(inspected->fill.b - 0.8F) < 0.001F &&
                     fabsf(inspected->fill.a - 128.0F / 255.0F) < 0.001F);
                const bool valid_width =
                    editor.inspector_smoke_stage != 2U ||
                    (inspected != NULL && initial_aspect > 0.0F &&
                     fabsf(inspected->resolved_bounds.x - 310.0F) < 0.001F &&
                     fabsf(inspected->resolved_bounds.x / inspected->resolved_bounds.y -
                           initial_aspect) < 0.001F);
                const AfStyle *style =
                    af_style_find_node_const(&editor.project.style_data, editor.selected_id);
                const bool valid_stroke =
                    (editor.inspector_smoke_stage != 3U ||
                     (style != NULL && fabsf(style->stroke.width - 12.5F) < 0.001F)) &&
                    (editor.inspector_smoke_stage != 4U ||
                     (style != NULL && fabsf(style->stroke.width - 13.5F) < 0.001F));
                const bool valid_scrub =
                    editor.inspector_smoke_stage != 6U ||
                    (inspected != NULL &&
                     fabsf(inspected->local_transform.position.x - 285.5F) < 0.001F);
                const bool valid_scale =
                    editor.inspector_smoke_stage != 5U ||
                    (inspected != NULL &&
                     fabsf(inspected->local_transform.scale.x - 1.5F) < 0.001F &&
                     fabsf(inspected->local_transform.scale.y - 1.5F) < 0.001F);
                if (!valid_position || !valid_color || !valid_width || !valid_stroke ||
                    !valid_scrub || !valid_scale) {
                    result = AF_RESULT_INVALID_ARGUMENT;
                    goto cleanup;
                }
            }
            ++editor.inspector_smoke_stage;
        }
        if (editor.inspector_smoke && editor.inspector_smoke_stage == 7U) {
            result = af_editor_color_input_smoke(&editor, width, height);
            if (result != AF_RESULT_SUCCESS)
                goto cleanup;
            ++editor.inspector_smoke_stage;
        }
        if (editor.outline_smoke) {
            const float panel_x = (float)width - editor.right_width + 18.0F;
            SDL_MouseButtonEvent target_click = {
                .button = SDL_BUTTON_LEFT,
                .down = true,
                .x = panel_x + (editor.rendered_frames == 0U ? 110.0F : 36.0F),
                .y = AF_EDITOR_COLOR_TARGET_Y + AF_EDITOR_COLOR_TARGET_HEIGHT * 0.5F};
            af_editor_mouse_down(&editor, width, height, &target_click, SDL_KMOD_NONE);
        }
        if (editor.selected_id && !af_editor_frame_node_active(&editor, editor.selected_id))
            af_editor_select_only(&editor, 0);
        af_editor_update_preview(&editor);
        af_editor_build_hierarchy(&editor);
        if (editor.vector_animation_smoke) {
            editor.selected_property = AF_PROPERTY_PATH_CONTROL_OUT;
            af_editor_status(&editor, "Programmatic animated vector handles capture");
        } else if (editor.outline_smoke) {
            af_editor_status(&editor, editor.color_target_outline
                                          ? "Outline smoke: Outline tab click accepted"
                                          : "Outline smoke: Fill tab click accepted");
        } else if (editor.inspector_smoke) {
            af_editor_status(
                &editor,
                "Inspector smoke: exact values, ratio lock, steppers, and scrubbing accepted");
        } else if (editor.vector_tools_smoke) {
            af_editor_status(&editor, "Vector tools smoke: construction, path surgery, multi-node "
                                      "editing, and animation accepted");
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
            editor.top_menu_item = 1;
            editor.mouse_position =
                (AfVec2){af_editor_top_menu_x(&editor, editor.top_menu) + 22.0F,
                         AF_EDITOR_MENU_HEIGHT + AF_EDITOR_CONTEXT_ITEM_HEIGHT + 12.0F};
        } else if (editor.tooltip_smoke) {
            editor.context_menu = AF_EDITOR_CONTEXT_NONE;
            editor.mouse_position = (AfVec2){editor.left_width + 31.0F, 20.0F};
        }
        if (editor.pan_smoke) {
            const float direction = (editor.rendered_frames / 40U) % 2U == 0U ? 1.0F : -1.0F;
            if (editor.rendered_frames == 0U) {
                result = af_editor_text_cache_smoke(&editor);
                if (result != AF_RESULT_SUCCESS) {
                    goto cleanup;
                }
                const SDL_MouseButtonEvent press = {.button = SDL_BUTTON_MIDDLE,
                                                    .down = true,
                                                    .x = editor.left_width + 160,
                                                    .y = editor.toolbar_height + 90};
                af_editor_mouse_down(&editor, width, height, &press, SDL_KMOD_NONE);
            }
            const SDL_MouseMotionEvent motion = {.x = editor.left_width + 180,
                                                 .y = editor.toolbar_height + 100,
                                                 .xrel = 7.25F * direction,
                                                 .yrel = 2.75F * direction};
            af_editor_mouse_motion(&editor, width, height, &motion);
        }
        af_editor_hierarchy_autoscroll(&editor, height, current_ticks);
        const uint64_t build_start = SDL_GetTicksNS();
        af_editor_build_render_scene(&editor, width, height);
        const double build_ms = (double)(SDL_GetTicksNS() - build_start) / 1000000.0;
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
        if (editor.pan_smoke && editor.rendered_frames >= 10U &&
            editor.rendered_frames + 1U < editor.maximum_frames) {
            const AfVkFrameMetrics frame = af_vk_renderer_frame_metrics(editor.renderer);
            ++editor.pan_sample_count;
            editor.pan_build_ms += build_ms;
            editor.pan_upload_ms += frame.upload_ms;
            editor.pan_draw_batches += frame.draw_batches;
            if (frame.gpu_valid) {
                ++editor.pan_gpu_samples;
                editor.pan_gpu_ms += frame.gpu_ms;
            }
            editor.pan_path_rebuilds += frame.path_rebuilds;
            editor.pan_frame_ms += editor.last_frame_ms;
            editor.pan_max_frame_ms = fmax(editor.pan_max_frame_ms, editor.last_frame_ms);
        }
        ++editor.rendered_frames;
#ifdef __EMSCRIPTEN__
        if (editor.rendered_frames % 120U == 0U) {
            af_editor_save_preferences(&editor);
            af_web_sync_files();
        }
#endif
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
    if (editor.pan_smoke && editor.pan_sample_count != 0U) {
        const double samples = editor.pan_sample_count;
        (void)printf(
            "{\"pan_frames\":%u,\"build_ms\":%.3f,\"upload_ms\":%.3f,\"frame_ms\":%.3f,"
            "\"max_frame_ms\":%.3f,\"path_rebuilds\":%llu,\"gpu_samples\":%u,\"gpu_ms\":%.3f,"
            "\"draw_batches\":%.1f}\n",
            editor.pan_sample_count, editor.pan_build_ms / samples, editor.pan_upload_ms / samples,
            editor.pan_frame_ms / samples, editor.pan_max_frame_ms,
            (unsigned long long)editor.pan_path_rebuilds, editor.pan_gpu_samples,
            editor.pan_gpu_samples == 0U ? -1.0
                                         : editor.pan_gpu_ms / (double)editor.pan_gpu_samples,
            editor.pan_draw_batches / samples);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_log(AF_LOG_ERROR, "Arcflow Editor: %s", af_result_string(result));
    }
    af_editor_workflow_shutdown(&editor);
    af_editor_collaboration_release(&editor);
    af_vk_renderer_destroy(editor.renderer);
    SDL_DestroyCursor(editor.resize_horizontal_cursor);
    SDL_DestroyCursor(editor.resize_vertical_cursor);
    af_platform_window_destroy(&editor.window);
    free(editor.text_preview);
    free(editor.text_preview_styles.styles);
    af_text_engine_destroy(editor.system_font_engine);
    af_file_data_destroy(&editor.allocator, &editor.system_font_file);
    af_editor_fonts_destroy(&editor.font_catalog);
    af_text_engine_destroy(editor.text_engine);
    for (uint32_t i = 0; i < 12; ++i)
        af_file_data_destroy(&editor.allocator, &editor.document_font_files[i]);
    af_file_data_destroy(&editor.allocator, &editor.font_file);
    af_deallocate(&editor.allocator, editor.text_lines);
    af_deallocate(&editor.allocator, editor.text_glyphs);
    af_deallocate(&editor.allocator, editor.path_scratch);
    af_deallocate(&editor.allocator, editor.curve_order);
    af_deallocate(&editor.allocator, editor.preview_styles);
    af_deallocate(&editor.allocator, editor.preview_nodes);
    af_deallocate(&editor.allocator, editor.render_path_commands);
    af_deallocate(&editor.allocator, editor.render_paths);
    af_deallocate(&editor.allocator, editor.render_nodes);
    af_editor_history_destroy(&editor.history);
    af_project_destroy(&editor.project);
    return exit_code;
}
