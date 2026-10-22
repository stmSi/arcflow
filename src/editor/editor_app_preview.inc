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

static AfVec2 af_editor_node_pivot(const AfEditorState *editor, const AfSceneNode *node) {
    const AfLayoutNode *layout =
        node == NULL ? NULL : af_layout_find_const(&editor->project.layout_data, node->id);
    return layout == NULL ? (AfVec2){0}
                          : (AfVec2){node->resolved_bounds.x * layout->pivot.x,
                                     node->resolved_bounds.y * layout->pivot.y};
}

static void af_editor_update_preview(AfEditorState *editor) {
    const AfPath *selected_path =
        af_path_find_node_const(&editor->project.path_data, editor->selected_id);
    if (selected_path == NULL && af_property_is_path(editor->selected_property)) {
        editor->selected_property = AF_PROPERTY_POSITION;
    } else if (selected_path != NULL &&
               editor->selected_path_command >= selected_path->command_count) {
        editor->selected_path_command = 0U;
        af_editor_path_selection_only(editor, editor->selected_path_command);
    }
    editor->preview_scene.count = editor->project.scene.count;
    af_editor_prepare_render_paths(editor);
    editor->preview_scene.paths = &editor->render_path_data;
    editor->preview_style_data = editor->project.style_data;
    editor->preview_style_data.styles = editor->preview_styles;
    editor->preview_style_data.capacity = AF_MAX_STYLES;
    if (editor->project.style_data.count)
        memcpy(editor->preview_styles, editor->project.style_data.styles,
               (size_t)editor->project.style_data.count * sizeof(AfStyle));
    editor->preview_scene.styles = &editor->preview_style_data;
    editor->preview_scene.layouts = &editor->project.layout_data;
    editor->preview_scene.particles = &editor->project.particle_data;
    editor->preview_scene.images = &editor->project.image_data;
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

static float af_editor_point_segment_distance_squared(AfVec2 point, AfVec2 first, AfVec2 second) {
    const AfVec2 delta = {second.x - first.x, second.y - first.y};
    const float length_squared = delta.x * delta.x + delta.y * delta.y;
    float amount = 0.0F;
    if (length_squared > 0.000001F) {
        amount = af_editor_clamp(((point.x - first.x) * delta.x + (point.y - first.y) * delta.y) /
                                     length_squared,
                                 0.0F, 1.0F);
    }
    {
        const float x = point.x - (first.x + delta.x * amount);
        const float y = point.y - (first.y + delta.y * amount);
        return x * x + y * y;
    }
}

static bool af_editor_point_on_path_outline(const AfPathData *paths, const AfPath *path,
                                            AfVec2 point, float radius,
                                            AfPathTessellationScratch *scratch) {
    AfFlattenedPath flattened = {.points = scratch->points,
                                 .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                 .contours = scratch->contours,
                                 .contour_capacity = AF_MAX_PATH_CONTOURS};
    if (radius <= 0.0F || af_path_flatten(paths, path, 0.25F, &flattened) != AF_RESULT_SUCCESS) {
        return false;
    }
    for (uint32_t contour_index = 0U; contour_index < flattened.contour_count; ++contour_index) {
        const AfPathContour *contour = &flattened.contours[contour_index];
        if (contour->point_count < 2U) {
            continue;
        }
        const uint32_t segment_count =
            contour->closed ? contour->point_count : contour->point_count - 1U;
        for (uint32_t index = 0U; index < segment_count; ++index) {
            const AfVec2 first = flattened.points[contour->point_start + index];
            const AfVec2 second =
                flattened.points[contour->point_start + ((index + 1U) % contour->point_count)];
            if (af_editor_point_segment_distance_squared(point, first, second) <= radius * radius) {
                return true;
            }
        }
    }
    return false;
}

static bool af_editor_point_in_node(const AfSceneNode *node, AfVec2 project_point,
                                    float outline_width) {
    AfAffine2 inverse = {0};
    AfVec2 point = {0};
    const float margin = fmaxf(outline_width, 0.0F) * 0.5F;
    AfVec2 bounds = {0};
    if (node == NULL || node->type == AF_NODE_GROUP || !node->world_visible ||
        !af_affine_inverse(node->world_transform, &inverse)) {
        return false;
    }
    point = af_affine_transform_point(inverse, project_point);
    bounds = node->resolved_bounds;
    if (point.x < -margin || point.y < -margin || point.x > bounds.x + margin ||
        point.y > bounds.y + margin) {
        return false;
    }
    if (node->type == AF_NODE_ELLIPSE && bounds.x > 0.0F && bounds.y > 0.0F) {
        const float x = (point.x - bounds.x * 0.5F) / (bounds.x * 0.5F + margin);
        const float y = (point.y - bounds.y * 0.5F) / (bounds.y * 0.5F + margin);
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
        if (!af_editor_frame_node_active(editor, node->id))
            continue;
        const AfStyle *source_style =
            af_style_find_node_const(&editor->project.style_data, node->id);
        AfStyle resolved_style = {0};
        const AfStyle *style = NULL;
        float outline_width = 0.0F;
        bool contains = false;
        if (source_style != NULL && af_style_resolve(&editor->project.style_data, source_style,
                                                     &resolved_style) == AF_RESULT_SUCCESS) {
            style = &resolved_style;
            if (node->type >= AF_NODE_RECTANGLE && node->type <= AF_NODE_PATH &&
                style->stroke.enabled && style->stroke_opacity > 0.0F) {
                outline_width = fmaxf(style->stroke.width, style->stroke.end_width);
            }
        }
        if (node->type == AF_NODE_PATH) {
            AfAffine2 inverse = {0};
            const AfPath *path = af_path_find_node_const(&editor->render_path_data, node->id);
            if (path != NULL && editor->path_scratch != NULL &&
                af_affine_inverse(node->world_transform, &inverse)) {
                const AfVec2 local = af_affine_transform_point(inverse, project_point);
                contains = af_path_hit_test(&editor->render_path_data, path, local, 0.25F,
                                            editor->path_scratch) ||
                           (outline_width > 0.0F &&
                            af_editor_point_on_path_outline(&editor->render_path_data, path, local,
                                                            outline_width * 0.5F +
                                                                4.0F / fmaxf(editor->zoom, 0.01F),
                                                            editor->path_scratch));
            }
        } else {
            contains = af_editor_point_in_node(node, project_point, outline_width);
        }
        if (contains && node->layer_order >= hit_layer) {
            hit = node->id;
            hit_layer = node->layer_order;
        }
    }
    return hit;
}

static void af_editor_render_grid(AfEditorState *editor, int width, int height, AfAffine2 camera) {
    float canvas_x, canvas_y, canvas_width, canvas_height;
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    float step = af_editor_grid_interval(editor->grid_step, editor->zoom);
    if (!editor->grid_visible || step <= 0)
        return;
    while (fmaxf(canvas_width, canvas_height) / (step * editor->zoom) > 256)
        step *= 5;
    const float spacing = step * editor->zoom;
    const float alpha = af_editor_clamp((spacing - 8) / 22, .08F, .7F);
    for (uint32_t axis = 0; axis < 2; ++axis) {
        const float low = axis ? canvas_y : canvas_x, extent = axis ? canvas_height : canvas_width;
        const double origin = axis ? camera.ty : camera.tx;
        const double first = ceil(((double)low - origin) / (double)spacing);
        for (uint32_t i = 0; i < 257; ++i) {
            const double ordinal = first + i;
            const float pixel = (float)(origin + ordinal * (double)spacing);
            if (pixel >= low + extent)
                break;
            const bool major = fabs(remainder(ordinal, 5)) < .01;
            af_editor_add_rect(editor, axis ? canvas_x : pixel, axis ? pixel : canvas_y,
                               axis ? canvas_width : 1, axis ? 1 : canvas_height,
                               major ? (AfColor){.25F, .28F, .34F, .35F}
                                     : (AfColor){.24F, .27F, .32F, alpha * .45F},
                               -90000);
        }
    }
}
