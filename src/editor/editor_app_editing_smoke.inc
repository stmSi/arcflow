#include "runtime/runtime_internal.h"

static AfResult af_editor_editing_smoke(AfEditorState *editor, int width, int height) {
#define EDIT_CHECK(condition)                                                                      \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            af_log(AF_LOG_ERROR, "Editing smoke failed at line %u", (unsigned)__LINE__);           \
            return AF_RESULT_INVALID_ARGUMENT;                                                     \
        }                                                                                          \
    } while (0)
    EDIT_CHECK(af_editor_new_project(editor) == AF_RESULT_SUCCESS);
    editor->welcome_visible = false;
    EDIT_CHECK(af_editor_create_primitive_at(editor, AF_NODE_RECTANGLE,
                                             (AfEditorToolRect){{300, 200}, {240, 160}}) ==
               AF_RESULT_SUCCESS);
    const uint64_t rectangle = editor->selected_id;
    AfStyle *style = af_project_find_node_style(&editor->project, rectangle);
    const AfPaint fill = style->fill;
    editor->color_target_outline = true;
    af_editor_gradient_set_type(editor, AF_PAINT_LINEAR_GRADIENT);
    EDIT_CHECK(style->stroke_paint.type == AF_PAINT_LINEAR_GRADIENT);
    EDIT_CHECK(memcmp(&style->fill, &fill, sizeof(fill)) == 0);
    af_editor_gradient_select_stop(editor, &style->stroke_paint, 0);
    EDIT_CHECK(af_editor_apply_inspector_color(editor, (AfColor){1, 0, 0, 1}, false));
    EDIT_CHECK(style->stroke_paint.type == AF_PAINT_LINEAR_GRADIENT &&
               style->stroke_paint.stops[0].color.r == 1);
    af_editor_update_preview(editor);
    af_editor_key_gradient(editor);
    EDIT_CHECK(editor->project.track_count == 6);
    const AfPaint baseline = style->stroke_paint;
    editor->playhead = 1;
    af_editor_update_preview(editor);
    EDIT_CHECK(af_editor_apply_inspector_color(editor, (AfColor){0, 0, 1, 1}, false));
    EDIT_CHECK(memcmp(&style->stroke_paint, &baseline, sizeof(baseline)) == 0);
    editor->playhead = .5F;
    af_editor_update_preview(editor);
    const AfStyle *preview = af_style_find_node_const(&editor->preview_style_data, rectangle);
    EDIT_CHECK(fabsf(preview->stroke_paint.stops[0].color.r - .5F) < .001F);
    EDIT_CHECK(fabsf(preview->stroke_paint.stops[0].color.b - .5F) < .001F);
    const AfColor inspector_color =
        af_editor_inspector_color(editor, af_scene_find_const(&editor->preview_scene, rectangle));
    EDIT_CHECK(fabsf(inspector_color.r - .5F) < .001F && fabsf(inspector_color.b - .5F) < .001F);
    editor->playhead = 1;
    af_editor_update_preview(editor);
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_SELECT);
    AfAffine2 frame;
    AfVec2 start, end;
    EDIT_CHECK(af_editor_gradient_geometry(editor, width, height, &frame, &start, &end));
    AfVec2 screen = af_affine_transform_point(frame, end);
    EDIT_CHECK(af_editor_gradient_begin_drag(editor, width, height, AF_EDITOR_GRADIENT_DRAG_END,
                                             screen.x, screen.y));
    af_editor_gradient_motion(editor, width, height, screen.x + 40, screen.y + 20);
    af_editor_gradient_finish_drag(editor, false);
    EDIT_CHECK(memcmp(&style->stroke_paint, &baseline, sizeof(baseline)) == 0);
    preview = af_style_find_node_const(&editor->preview_style_data, rectangle);
    EDIT_CHECK(preview->stroke_paint.end.x > baseline.end.x);
    const uint32_t node_count = editor->project.scene.count;
    editor->color_target_outline = false;
    af_editor_gradient_set_type(editor, AF_PAINT_LINEAR_GRADIENT);
    editor->gradient_stop_focus = true;
    editor->timeline_focus = false;
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_DELETE, .down = true});
    EDIT_CHECK(editor->project.scene.count == node_count - 1U);
    EDIT_CHECK(af_scene_find_const(&editor->project.scene, rectangle) == NULL);
    EDIT_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    EDIT_CHECK(af_scene_find_const(&editor->project.scene, rectangle) != NULL);

    const AfPathCommand curve[] = {
        {.type = AF_PATH_MOVE, .point = {0, 50}},
        {.type = AF_PATH_CUBIC, .point = {100, 50}, .control1 = {30, 50}, .control2 = {70, 50}},
        {.type = AF_PATH_CUBIC, .point = {200, 50}, .control1 = {130, 50}, .control2 = {170, 50}}};
    EDIT_CHECK(af_editor_create_path_commands(editor, curve, 3, false, "Smooth path") ==
               AF_RESULT_SUCCESS);
    editor->color_target_outline = false;
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_NODE);
    af_editor_path_selection_only(editor, 1);
    EDIT_CHECK(af_editor_smooth_path_points(editor, false) == AF_RESULT_SUCCESS);
    AfPath *path = af_editor_selected_path(editor);
    EDIT_CHECK(editor->project.path_data.commands[path->command_start + 1U].handle_mode ==
               AF_PATH_HANDLE_SMOOTH);
    af_editor_update_preview(editor);
    EDIT_CHECK(af_editor_begin_path_drag(editor, path, AF_EDITOR_DRAG_PATH_CONTROL2));
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const AfAffine2 transform =
        af_affine_multiply(af_editor_camera(editor, width, height), node->world_transform);
    const AfVec2 moved = af_affine_transform_point(transform, (AfVec2){70, 75});
    af_editor_drag_path_handle(editor, width, height, moved, SDL_KMOD_NONE);
    af_editor_mouse_up(editor, width, height);
    const AfPathCommand *commands = editor->project.path_data.commands + path->command_start;
    const AfVec2 incoming = {commands[1].control2.x - commands[1].point.x,
                             commands[1].control2.y - commands[1].point.y};
    const AfVec2 outgoing = {commands[2].control1.x - commands[1].point.x,
                             commands[2].control1.y - commands[1].point.y};
    EDIT_CHECK(fabsf(incoming.x * outgoing.y - incoming.y * outgoing.x) < .01F);
    EDIT_CHECK(incoming.x * outgoing.x + incoming.y * outgoing.y < 0);
    af_editor_path_selection_only(editor, 2);
    editor->path_segment_t = .5F;
    editor->path_segment_start = (AfVec2){150, 50};
    EDIT_CHECK(af_editor_begin_path_drag(editor, path, AF_EDITOR_DRAG_PATH_SEGMENT));
    af_editor_drag_path_handle(editor, width, height,
                               af_affine_transform_point(transform, (AfVec2){150, 70}),
                               SDL_KMOD_NONE);
    af_editor_mouse_up(editor, width, height);
    commands = editor->project.path_data.commands + path->command_start;
    const AfVec2 bent_in = {commands[1].control2.x - commands[1].point.x,
                            commands[1].control2.y - commands[1].point.y};
    const AfVec2 bent_out = {commands[2].control1.x - commands[1].point.x,
                             commands[2].control1.y - commands[1].point.y};
    EDIT_CHECK(fabsf(bent_in.x * bent_out.y - bent_in.y * bent_out.x) < .01F);
    af_editor_path_selection_only(editor, 1);
    af_editor_cusp_path_points(editor);
    EDIT_CHECK(editor->project.path_data.commands[path->command_start + 1U].handle_mode ==
               AF_PATH_HANDLE_CUSP);
    EDIT_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    EDIT_CHECK(editor->project.path_data.commands[path->command_start + 1U].handle_mode ==
               AF_PATH_HANDLE_SMOOTH);
    af_editor_path_selection_only(editor, 0);
    (void)af_editor_path_selection_add(editor, 2);
    af_editor_combine_points(editor, false);
    path = af_editor_selected_path(editor);
    EDIT_CHECK(path->command_count == 5 && editor->selected_path_command == 3);
    EDIT_CHECK(af_editor_convert_path_point(editor) == AF_RESULT_SUCCESS);
    EDIT_CHECK(editor->project.path_data.commands[path->command_start + 3].type == AF_PATH_CUBIC);
    EDIT_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    EDIT_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    af_editor_path_selection_only(editor, 1);
    (void)af_editor_path_selection_add(editor, 2);
    af_editor_combine_points(editor, true);
    EDIT_CHECK(af_editor_selected_path(editor)->command_count == 2);
    EDIT_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    AfPackageBlob text = {0}, package = {0};
    AfProject reopened = {0};
    EDIT_CHECK(af_project_serialize(&editor->project, &editor->allocator, &text) ==
               AF_RESULT_SUCCESS);
    EDIT_CHECK(af_project_deserialize(text.data, text.size, &editor->allocator, &reopened) ==
               AF_RESULT_SUCCESS);
    const AfPath *saved = af_path_find_node_const(&reopened.path_data, editor->selected_id);
    EDIT_CHECK(saved && reopened.path_data.commands[saved->command_start + 1].handle_mode ==
                            AF_PATH_HANDLE_SMOOTH);
    EDIT_CHECK(reopened.track_count == 6);
    EDIT_CHECK(af_project_compile(&reopened, &editor->allocator, &package) == AF_RESULT_SUCCESS);
    EDIT_CHECK(af_package_validate(package.data, package.size) == AF_RESULT_SUCCESS);
    AfRuntime *runtime = NULL;
    AfPackageHandle loaded_package = 0;
    AfInstanceHandle first_instance = 0, second_instance = 0;
    const AfRuntimeCreateInfo create_info = {.frames_in_flight = 2};
    EDIT_CHECK(af_runtime_create(&create_info, &runtime) == AF_RESULT_SUCCESS);
    EDIT_CHECK(af_package_load(runtime, package.data, package.size, &loaded_package) ==
               AF_RESULT_SUCCESS);
    EDIT_CHECK(af_instance_create(runtime, loaded_package, &first_instance) == AF_RESULT_SUCCESS);
    EDIT_CHECK(af_instance_create(runtime, loaded_package, &second_instance) == AF_RESULT_SUCCESS);
    EDIT_CHECK(af_instance_seek(runtime, first_instance, 1) == AF_RESULT_SUCCESS);
    EDIT_CHECK(af_instance_seek(runtime, second_instance, 0) == AF_RESULT_SUCCESS);
    const AfScene *first_scene = NULL, *second_scene = NULL;
    EDIT_CHECK(af_instance_get_scene(runtime, first_instance, &first_scene) == AF_RESULT_SUCCESS);
    EDIT_CHECK(af_instance_get_scene(runtime, second_instance, &second_scene) == AF_RESULT_SUCCESS);
    EDIT_CHECK(first_scene->styles != second_scene->styles);
    EDIT_CHECK(
        af_style_find_node_const(first_scene->styles, rectangle)->stroke_paint.stops[0].color.b ==
        1);
    EDIT_CHECK(
        af_style_find_node_const(second_scene->styles, rectangle)->stroke_paint.stops[0].color.r ==
        1);
    af_runtime_destroy(runtime);
    af_package_blob_destroy(&editor->allocator, &package);
    af_project_destroy(&reopened);
    af_package_blob_destroy(&editor->allocator, &text);
    af_editor_status(
        editor,
        "Editing verified: outline gradients, animation, smooth handles, combine and Delete");
#undef EDIT_CHECK
    return AF_RESULT_SUCCESS;
}
