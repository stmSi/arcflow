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
    const AfPath *selected_path =
        af_path_find_node_const(&editor->project.path_data, editor->selected_id);
    if (selected_path == NULL && editor->selected_property >= AF_PROPERTY_PATH_POINT) {
        editor->selected_property = AF_PROPERTY_POSITION;
    } else if (selected_path != NULL &&
               editor->selected_path_command >= selected_path->command_count) {
        editor->selected_path_command = 0U;
    }
    editor->preview_scene.count = editor->project.scene.count;
    af_editor_prepare_render_paths(editor);
    editor->preview_scene.paths = &editor->render_path_data;
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
            const AfPath *path = af_path_find_node_const(&editor->render_path_data, node->id);
            contains = path != NULL && editor->path_scratch != NULL &&
                       af_affine_inverse(node->world_transform, &inverse) &&
                       af_path_hit_test(&editor->render_path_data, path,
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
