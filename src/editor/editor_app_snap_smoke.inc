static AfResult af_editor_snapping_smoke(AfEditorState *editor, int width, int height) {
#define SNAP_CHECK(c)                                                                              \
    do {                                                                                           \
        if (!(c))                                                                                  \
            return af_editor_interaction_smoke_failure(editor, __LINE__);                          \
    } while (0)
    SNAP_CHECK(editor->project.scene.capacity == AF_EDITOR_PROJECT_CAPACITY);
    editor->snapping = true;
    SNAP_CHECK(af_editor_create_primitive_at(editor, AF_NODE_RECTANGLE,
                                             (AfEditorToolRect){{240, 180}, {100, 80}}) ==
               AF_RESULT_SUCCESS);
    const uint64_t target = editor->selected_id;
    SNAP_CHECK(af_editor_create_primitive_at(editor, AF_NODE_RECTANGLE,
                                             (AfEditorToolRect){{60, 80}, {40, 30}}) ==
               AF_RESULT_SUCCESS);
    const uint64_t moving = editor->selected_id;
    const AfTransform2D original =
        af_scene_find_const(&editor->project.scene, moving)->local_transform;
    af_editor_update_preview(editor);
    af_editor_begin_selection_modify(editor, true);
    af_editor_begin_transform_key_drag(editor, AF_PROPERTY_POSITION);
    af_editor_prepare_snapping(editor, true);
    SNAP_CHECK(editor->snap_target_count == 1 && editor->snap_targets[0].id == target);
    const AfEditorSnapBounds destination = editor->snap_targets[0].bounds;
    const AfEditorSnapBounds before = editor->snap_drag_bounds;
    const AfVec2 delta = {
        destination.minimum.x - before.maximum.x - 3,
        (destination.minimum.y + destination.maximum.y - before.minimum.y - before.maximum.y) *
                .5F +
            4};
    editor->drag_mode = AF_EDITOR_DRAG_MOVE;
    editor->drag_start_mouse = (AfVec2){0};
    const AfVec2 screen = af_affine_transform_point(af_editor_camera(editor, width, height), delta);
    af_editor_mouse_motion(editor, width, height,
                           &(SDL_MouseMotionEvent){.x = screen.x, .y = screen.y});
    SNAP_CHECK(editor->snap_result.axes[0].active && editor->snap_result.axes[1].active);
    af_editor_update_preview(editor);
    float x0, y0, x1, y1;
    SNAP_CHECK(af_editor_screen_bounds(af_scene_find_const(&editor->preview_scene, moving),
                                       af_affine_identity(), &x0, &y0, &x1, &y1));
    SNAP_CHECK(fabsf(x1 - destination.minimum.x) < .001F &&
               fabsf((y0 + y1 - destination.minimum.y - destination.maximum.y) * .5F) < .001F);
    af_editor_mouse_up(editor, width, height);
    SNAP_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    SNAP_CHECK(memcmp(&original,
                      &af_scene_find_const(&editor->project.scene, moving)->local_transform,
                      sizeof(original)) == 0);
    /* Fine nudges remain one unit with snapping enabled. */
    af_editor_nudge(editor, 1, 0);
    SNAP_CHECK(
        fabsf(af_scene_find_const(&editor->project.scene, moving)->local_transform.position.x -
              original.position.x - 1) < .001F);
    SNAP_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    /* Multi-selection moves together, without snapping to its own objects. */
    af_editor_selection_add(editor, target);
    af_editor_update_preview(editor);
    af_editor_prepare_snapping(editor, true);
    SNAP_CHECK(editor->snap_target_count == 0);
    const AfVec2 free_delta = af_editor_snap_move(editor, (AfVec2){13, 17}, (AfVec2){0});
    SNAP_CHECK(free_delta.x == 13 && free_delta.y == 17);

    const AfTransform2D target_before =
        af_scene_find_const(&editor->project.scene, target)->local_transform;
    af_editor_begin_selection_modify(editor, true);
    af_editor_begin_transform_key_drag(editor, AF_PROPERTY_POSITION);
    editor->drag_mode = AF_EDITOR_DRAG_MOVE;
    editor->drag_start_mouse = (AfVec2){0};
    const AfVec2 group_screen =
        af_affine_transform_point(af_editor_camera(editor, width, height), (AfVec2){13, 17});
    af_editor_mouse_motion(editor, width, height,
                           &(SDL_MouseMotionEvent){.x = group_screen.x, .y = group_screen.y});
    SNAP_CHECK(
        fabsf(af_scene_find_const(&editor->project.scene, target)->local_transform.position.x -
              target_before.position.x - 13) < .001F);
    SNAP_CHECK(
        fabsf(af_scene_find_const(&editor->project.scene, moving)->local_transform.position.y -
              original.position.y - 17) < .001F);
    af_editor_mouse_up(editor, width, height);
    SNAP_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    /* A rotated, scaled parent must receive the same world-space correction. */
    af_editor_select_only(editor, moving);
    const uint64_t parent_id = editor->project.next_id;
    const AfSceneNode parent = {
        .id = parent_id,
        .parent_id = AF_INVALID_NODE_ID,
        .type = AF_NODE_GROUP,
        .local_transform = {.position = {25, 15}, .rotation = .4F, .scale = {1.5F, .8F}},
        .opacity = 1,
        .visible = true};
    SNAP_CHECK(af_project_add_node(&editor->project, &parent, "Snap parent") == AF_RESULT_SUCCESS);
    af_scene_find(&editor->project.scene, moving)->parent_id = parent_id;
    SNAP_CHECK(af_scene_evaluate_world(&editor->project.scene) == AF_RESULT_SUCCESS);
    af_editor_update_preview(editor);
    af_editor_begin_selection_modify(editor, true);
    af_editor_begin_transform_key_drag(editor, AF_PROPERTY_POSITION);
    af_editor_prepare_snapping(editor, true);
    const AfVec2 nested_delta = {destination.minimum.x - editor->snap_drag_bounds.maximum.x - 3,
                                 (destination.minimum.y + destination.maximum.y -
                                  editor->snap_drag_bounds.minimum.y -
                                  editor->snap_drag_bounds.maximum.y) *
                                         .5F +
                                     4};
    const AfVec2 nested_screen =
        af_affine_transform_point(af_editor_camera(editor, width, height), nested_delta);
    editor->drag_mode = AF_EDITOR_DRAG_MOVE;
    editor->drag_start_mouse = (AfVec2){0};
    af_editor_mouse_motion(editor, width, height,
                           &(SDL_MouseMotionEvent){.x = nested_screen.x, .y = nested_screen.y});
    af_editor_update_preview(editor);
    AfEditorSnapBounds nested;
    SNAP_CHECK(af_editor_scene_node_bounds(&editor->preview_scene,
                                           af_scene_find_const(&editor->preview_scene, moving),
                                           false, &nested));
    SNAP_CHECK(fabsf(nested.maximum.x - destination.minimum.x) < .001F);
    af_editor_mouse_up(editor, width, height);
    SNAP_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    af_scene_find(&editor->project.scene, moving)->parent_id = AF_INVALID_NODE_ID;
    SNAP_CHECK(af_project_remove_node_exact(&editor->project, parent_id) == AF_RESULT_SUCCESS);
    SNAP_CHECK(af_scene_evaluate_world(&editor->project.scene) == AF_RESULT_SUCCESS);
    af_editor_select_only(editor, moving);
    af_editor_update_preview(editor);
    af_editor_prepare_snapping(editor, true);
    editor->snapping = false;
    SNAP_CHECK(af_editor_snap_move(editor, delta, (AfVec2){0}).x == delta.x);
    editor->snapping = true;
    /* Leave a snapped drag visible for the capture; it remains one undo transaction. */
    af_editor_begin_selection_modify(editor, true);
    af_editor_begin_transform_key_drag(editor, AF_PROPERTY_POSITION);
    editor->drag_mode = AF_EDITOR_DRAG_MOVE;
    editor->drag_start_mouse = (AfVec2){0};
    af_editor_mouse_motion(editor, width, height,
                           &(SDL_MouseMotionEvent){.x = screen.x, .y = screen.y});
    af_editor_update_preview(editor);
    af_editor_status(editor,
                     "Object edges / centers, visual grid, precise nudges and undo verified");
#undef SNAP_CHECK
    return AF_RESULT_SUCCESS;
}
