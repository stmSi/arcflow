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
    AF_EDITOR_CONTEXT_GROUP,
    AF_EDITOR_CONTEXT_PARTICLE
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
    uint32_t maximum_frames;
    uint32_t rendered_frames;
    bool resize_smoke;
    bool context_smoke;
    bool tooltip_smoke;
    bool selection_smoke;
    bool vfx_smoke;
    bool particle_view_smoke;
    bool vector_animation_smoke;
    bool vector_animation_smoke_prepared;
    bool running;
    bool dirty;
} AfEditorState;

static void af_editor_prepare_render_paths(AfEditorState *editor);

// clang-format off
#include "editor/editor_app_core.inc"
#include "editor/editor_app_draw.inc"
#include "editor/editor_app_timeline.inc"
#include "editor/editor_app_preview.inc"
#include "editor/editor_app_panels.inc"
#include "editor/editor_app_tooltips.inc"
#include "editor/editor_app_canvas.inc"
#include "editor/editor_app_animation.inc"
#include "editor/editor_app_paths.inc"
#include "editor/editor_app_properties.inc"
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
        if (editor.vector_animation_smoke) {
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
            const uint32_t context = (editor.rendered_frames / 2U) % 11U;
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
