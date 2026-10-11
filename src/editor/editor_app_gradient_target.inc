static const AfPaint *af_editor_target_paint(const AfEditorState *editor, const AfStyle *style) {
    return style == NULL                  ? NULL
           : editor->color_target_outline ? &style->stroke_paint
                                          : &style->fill;
}

static uint32_t af_editor_property_element(const AfEditorState *editor, AfPropertyType property) {
    if (af_property_is_path(property))
        return editor->selected_path_command;
    if (af_property_is_gradient(property))
        return (editor->color_target_outline ? AF_ANIMATION_OUTLINE_ELEMENT : 0U) +
               (property >= AF_PROPERTY_GRADIENT_STOP_COLOR
                    ? af_editor_gradient_stop(
                          editor, af_editor_target_paint(
                                      editor, af_style_find_node_const(&editor->project.style_data,
                                                                       editor->selected_id)))
                    : 0U);
    return 0U;
}

static void af_editor_gradient_value(const AfPaint *paint, AfPropertyType property, uint32_t stop,
                                     float value[4]) {
    memset(value, 0, 4 * sizeof(float));
    switch (property) {
    case AF_PROPERTY_GRADIENT_START:
        value[0] = paint->start.x;
        value[1] = paint->start.y;
        break;
    case AF_PROPERTY_GRADIENT_END:
        value[0] = paint->end.x;
        value[1] = paint->end.y;
        break;
    case AF_PROPERTY_GRADIENT_RADII:
        value[0] = paint->start_radius;
        value[1] = paint->end_radius;
        break;
    case AF_PROPERTY_GRADIENT_ANGLE:
        value[0] = paint->angle;
        break;
    case AF_PROPERTY_GRADIENT_STOP_COLOR:
        memcpy(value, &paint->stops[stop].color, sizeof(AfColor));
        break;
    case AF_PROPERTY_GRADIENT_STOP_OFFSET:
        value[0] = paint->stops[stop].offset;
        break;
    default:
        break;
    }
}

static AfPropertyType af_editor_color_property(const AfEditorState *editor) {
    const AfPaint *paint = af_editor_target_paint(
        editor, af_style_find_node_const(&editor->project.style_data, editor->selected_id));
    return paint && paint->type != AF_PAINT_SOLID ? AF_PROPERTY_GRADIENT_STOP_COLOR
           : editor->color_target_outline         ? AF_PROPERTY_OUTLINE_COLOR
                                                  : AF_PROPERTY_FILL_COLOR;
}

static bool af_editor_gradient_has_tracks(const AfEditorState *editor, bool stops_only) {
    for (uint32_t i = 0; i < editor->project.track_count; ++i) {
        const AfProjectTrack *track = &editor->project.tracks[i];
        if (track->node_id == editor->selected_id && af_property_is_gradient(track->property) &&
            (!stops_only || track->property >= AF_PROPERTY_GRADIENT_STOP_COLOR) &&
            (track->element_index >= AF_ANIMATION_OUTLINE_ELEMENT) == editor->color_target_outline)
            return true;
    }
    return false;
}
