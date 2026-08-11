static char *af_project_next_line(char **cursor) {
    char *line = NULL;
    char *end = NULL;
    if (cursor == NULL || *cursor == NULL || **cursor == '\0') {
        return NULL;
    }
    line = *cursor;
    end = strchr(line, '\n');
    if (end != NULL) {
        *end = '\0';
        *cursor = end + 1;
    } else {
        *cursor = line + strlen(line);
    }
    end = line + strlen(line);
    if (end > line && end[-1] == '\r') {
        end[-1] = '\0';
    }
    return line;
}

static bool af_project_parse_end(const char *cursor) {
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    return *cursor == '\0';
}

static bool af_project_parse_u64(const char *text, uint64_t *out_value) {
    unsigned long long value = 0ULL;
    char *end = NULL;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || !af_project_parse_end(end)) {
        return false;
    }
    *out_value = (uint64_t)value;
    return true;
}

static bool af_project_next_u64_token(const char **cursor, uint64_t *out_value) {
    unsigned long long value = 0ULL;
    char *end = NULL;
    while (**cursor == ' ' || **cursor == '\t') {
        ++(*cursor);
    }
    errno = 0;
    value = strtoull(*cursor, &end, 10);
    if (errno != 0 || end == *cursor || (*end != '\0' && *end != ' ' && *end != '\t')) {
        return false;
    }
    *cursor = end;
    *out_value = (uint64_t)value;
    return true;
}

static bool af_project_next_u32_token(const char **cursor, uint32_t *out_value) {
    uint64_t value = 0U;
    if (!af_project_next_u64_token(cursor, &value) || value > UINT32_MAX) {
        return false;
    }
    *out_value = (uint32_t)value;
    return true;
}

static bool af_project_next_float_token(const char **cursor, float *out_value) {
    char *end = NULL;
    while (**cursor == ' ' || **cursor == '\t') {
        ++(*cursor);
    }
    errno = 0;
    *out_value = strtof(*cursor, &end);
    if (errno != 0 || end == *cursor || !isfinite(*out_value) ||
        (*end != '\0' && *end != ' ' && *end != '\t')) {
        return false;
    }
    *cursor = end;
    return true;
}

static bool af_project_parse_paint_tokens(const char **cursor, AfPaint *paint) {
    uint32_t type = 0U;
    float *values[] = {&paint->solid.r,       &paint->solid.g,       &paint->solid.b,
                       &paint->solid.a,       &paint->start.x,       &paint->start.y,
                       &paint->end.x,         &paint->end.y,         &paint->start_radius,
                       &paint->end_radius,    &paint->angle,         &paint->transform.m00,
                       &paint->transform.m01, &paint->transform.m10, &paint->transform.m11,
                       &paint->transform.tx,  &paint->transform.ty};
    if (!af_project_next_u32_token(cursor, &type) ||
        !af_project_next_u32_token(cursor, &paint->stop_count) || type > AF_PAINT_CONIC_GRADIENT ||
        paint->stop_count > AF_MAX_GRADIENT_STOPS) {
        return false;
    }
    paint->type = (AfPaintType)type;
    for (uint32_t index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        if (!af_project_next_float_token(cursor, values[index])) {
            return false;
        }
    }
    for (uint32_t index = 0U; index < paint->stop_count; ++index) {
        AfGradientStop *stop = &paint->stops[index];
        if (!af_project_next_float_token(cursor, &stop->offset) ||
            !af_project_next_float_token(cursor, &stop->color.r) ||
            !af_project_next_float_token(cursor, &stop->color.g) ||
            !af_project_next_float_token(cursor, &stop->color.b) ||
            !af_project_next_float_token(cursor, &stop->color.a)) {
            return false;
        }
    }
    return true;
}

static bool af_project_parse_style_line(const char *line, AfStyle *style) {
    const char *cursor = line;
    uint32_t enabled = 0U;
    uint32_t cap = 0U;
    uint32_t join = 0U;
    if (strncmp(line, "style ", 6U) != 0) {
        return false;
    }
    cursor += 6U;
    if (!af_project_next_u64_token(&cursor, &style->id) ||
        !af_project_next_u64_token(&cursor, &style->node_id) ||
        !af_project_next_u64_token(&cursor, &style->parent_style_id) ||
        !af_project_next_u32_token(&cursor, &style->inherit_flags) ||
        !af_project_next_float_token(&cursor, &style->fill_opacity) ||
        !af_project_next_float_token(&cursor, &style->stroke_opacity) ||
        !af_project_next_u32_token(&cursor, &enabled) || enabled > 1U ||
        !af_project_next_float_token(&cursor, &style->stroke.width) ||
        !af_project_next_float_token(&cursor, &style->stroke.end_width) ||
        !af_project_next_u32_token(&cursor, &cap) || cap > AF_STROKE_CAP_SQUARE ||
        !af_project_next_u32_token(&cursor, &join) || join > AF_STROKE_JOIN_BEVEL ||
        !af_project_next_float_token(&cursor, &style->stroke.miter_limit) ||
        !af_project_next_u32_token(&cursor, &style->stroke.dash_count) ||
        style->stroke.dash_count > AF_MAX_STROKE_DASHES ||
        !af_project_next_float_token(&cursor, &style->stroke.dash_offset) ||
        !af_project_next_u64_token(&cursor, &style->paint_generation) ||
        !af_project_next_u64_token(&cursor, &style->stroke_geometry_generation)) {
        return false;
    }
    style->stroke.enabled = enabled != 0U;
    style->stroke.cap = (AfStrokeCap)cap;
    style->stroke.join = (AfStrokeJoin)join;
    for (uint32_t index = 0U; index < style->stroke.dash_count; ++index) {
        if (!af_project_next_float_token(&cursor, &style->stroke.dashes[index])) {
            return false;
        }
    }
    return af_project_parse_paint_tokens(&cursor, &style->fill) &&
           af_project_parse_paint_tokens(&cursor, &style->stroke_paint) &&
           af_project_parse_end(cursor) && af_style_validate(style) == AF_RESULT_SUCCESS;
}

static bool af_project_parse_layout_line(const char *line, AfLayoutNode *layout) {
    const char *cursor = line;
    uint32_t width_mode = 0U;
    uint32_t height_mode = 0U;
    uint32_t aspect = 0U;
    uint32_t direction = 0U;
    uint32_t alignment = 0U;
    float *values[] = {&layout->anchor_min.x, &layout->anchor_min.y, &layout->anchor_max.x,
                       &layout->anchor_max.y, &layout->pivot.x,      &layout->pivot.y,
                       &layout->offset.x,     &layout->offset.y};
    if (strncmp(line, "layout ", 7U) != 0) {
        return false;
    }
    cursor += 7U;
    if (!af_project_next_u64_token(&cursor, &layout->node_id)) {
        return false;
    }
    for (uint32_t index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        if (!af_project_next_float_token(&cursor, values[index])) {
            return false;
        }
    }
    if (!af_project_next_u32_token(&cursor, &width_mode) ||
        !af_project_next_float_token(&cursor, &layout->width.value) ||
        !af_project_next_u32_token(&cursor, &height_mode) ||
        !af_project_next_float_token(&cursor, &layout->height.value) ||
        !af_project_next_float_token(&cursor, &layout->minimum_size.x) ||
        !af_project_next_float_token(&cursor, &layout->minimum_size.y) ||
        !af_project_next_float_token(&cursor, &layout->maximum_size.x) ||
        !af_project_next_float_token(&cursor, &layout->maximum_size.y) ||
        !af_project_next_float_token(&cursor, &layout->aspect_ratio) ||
        !af_project_next_u32_token(&cursor, &aspect) ||
        !af_project_next_u32_token(&cursor, &direction) ||
        !af_project_next_u32_token(&cursor, &alignment) ||
        !af_project_next_float_token(&cursor, &layout->spacing) ||
        !af_project_next_float_token(&cursor, &layout->padding.left) ||
        !af_project_next_float_token(&cursor, &layout->padding.top) ||
        !af_project_next_float_token(&cursor, &layout->padding.right) ||
        !af_project_next_float_token(&cursor, &layout->padding.bottom) ||
        !af_project_parse_end(cursor) || width_mode > AF_LAYOUT_VALUE_PERCENT ||
        height_mode > AF_LAYOUT_VALUE_PERCENT || aspect > AF_ASPECT_HEIGHT_CONTROLS_WIDTH ||
        direction > AF_LAYOUT_DIRECTION_VERTICAL || alignment > AF_LAYOUT_ALIGN_STRETCH) {
        return false;
    }
    layout->width.mode = (AfLayoutValueMode)width_mode;
    layout->height.mode = (AfLayoutValueMode)height_mode;
    layout->aspect = (AfAspectConstraint)aspect;
    layout->direction = (AfLayoutDirection)direction;
    layout->child_alignment = (AfLayoutAlignment)alignment;
    return true;
}

static bool af_project_parse_layout_canvas(const char *line, AfLayoutData *layout) {
    const char *cursor = line;
    uint32_t mode = 0U;
    if (strncmp(line, "layout_canvas ", 14U) != 0) {
        return false;
    }
    cursor += 14U;
    if (!af_project_next_float_token(&cursor, &layout->reference_size.x) ||
        !af_project_next_float_token(&cursor, &layout->reference_size.y) ||
        !af_project_next_float_token(&cursor, &layout->target_size.x) ||
        !af_project_next_float_token(&cursor, &layout->target_size.y) ||
        !af_project_next_u32_token(&cursor, &mode) || mode > AF_CANVAS_SCALE_STRETCH ||
        !af_project_next_float_token(&cursor, &layout->safe_area.left) ||
        !af_project_next_float_token(&cursor, &layout->safe_area.top) ||
        !af_project_next_float_token(&cursor, &layout->safe_area.right) ||
        !af_project_next_float_token(&cursor, &layout->safe_area.bottom) ||
        !af_project_parse_end(cursor)) {
        return false;
    }
    layout->scale_mode = (AfCanvasScaleMode)mode;
    return true;
}

static bool af_project_parse_particle_line(const char *line, AfParticleEmitter *emitter) {
    const char *cursor = line;
    uint32_t type = 0U;
    uint32_t output = 0U;
    uint32_t enabled = 0U;
    float *values[] = {
        &emitter->lifetime_min,
        &emitter->lifetime_max,
        &emitter->position_min.x,
        &emitter->position_min.y,
        &emitter->position_max.x,
        &emitter->position_max.y,
        &emitter->velocity_min.x,
        &emitter->velocity_min.y,
        &emitter->velocity_max.x,
        &emitter->velocity_max.y,
        &emitter->rotation_min,
        &emitter->rotation_max,
        &emitter->angular_velocity_min,
        &emitter->angular_velocity_max,
        &emitter->size_start_min,
        &emitter->size_start_max,
        &emitter->size_end_min,
        &emitter->size_end_max,
        &emitter->color_start.r,
        &emitter->color_start.g,
        &emitter->color_start.b,
        &emitter->color_start.a,
        &emitter->color_end.r,
        &emitter->color_end.g,
        &emitter->color_end.b,
        &emitter->color_end.a,
        &emitter->gravity.x,
        &emitter->gravity.y,
        &emitter->wind.x,
        &emitter->wind.y,
        &emitter->drag,
    };
    if (strncmp(line, "particle ", 9U) != 0) {
        return false;
    }
    cursor += 9U;
    if (!af_project_next_u64_token(&cursor, &emitter->id) ||
        !af_project_next_u64_token(&cursor, &emitter->node_id) ||
        !af_project_next_u64_token(&cursor, &emitter->path_id) ||
        !af_project_next_u32_token(&cursor, &type) || type > AF_PARTICLE_EMITTER_VECTOR_PATH ||
        !af_project_next_u32_token(&cursor, &output) || output > AF_PARTICLE_OUTPUT_LIGHT_PROXY ||
        !af_project_next_u32_token(&cursor, &emitter->capacity) ||
        !af_project_next_u32_token(&cursor, &emitter->seed) ||
        !af_project_next_float_token(&cursor, &emitter->spawn_rate) ||
        !af_project_next_u32_token(&cursor, &emitter->burst_count)) {
        return false;
    }
    for (uint32_t index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        if (!af_project_next_float_token(&cursor, values[index])) {
            return false;
        }
    }
    if (!af_project_next_u32_token(&cursor, &emitter->flipbook_columns) ||
        !af_project_next_u32_token(&cursor, &emitter->flipbook_rows) ||
        !af_project_next_float_token(&cursor, &emitter->flipbook_frames_per_second) ||
        !af_project_next_u32_token(&cursor, &enabled) || enabled > 1U ||
        !af_project_parse_end(cursor)) {
        return false;
    }
    emitter->type = (AfParticleEmitterType)type;
    emitter->output = (AfParticleOutputType)output;
    emitter->enabled = enabled != 0U;
    return true;
}

static bool af_project_parse_vfx_lod_line(const char *line, AfVfxLodSettings *lod) {
    const char *cursor = line;
    uint32_t renderer_enabled = 0U;
    if (strncmp(line, "vfx_lod ", 8U) != 0) {
        return false;
    }
    cursor += 8U;
    if (!af_project_next_u32_token(&cursor, &lod->maximum_particles) ||
        !af_project_next_float_token(&cursor, &lod->spawn_rate_scale) ||
        !af_project_next_float_token(&cursor, &lod->simulation_frequency_scale) ||
        !af_project_next_float_token(&cursor, &lod->distance_start) ||
        !af_project_next_float_token(&cursor, &lod->distance_end) ||
        !af_project_next_u32_token(&cursor, &renderer_enabled) || renderer_enabled > 1U ||
        !af_project_parse_end(cursor)) {
        return false;
    }
    lod->host_quality = 1.0F;
    lod->view_distance = 0.0F;
    lod->renderer_enabled = renderer_enabled != 0U;
    return true;
}

static bool af_project_parse_vfx_emitter_line(const char *line, uint64_t *out_id,
                                              AfParticleEmitter *emitter) {
    const char *cursor = line;
    uint32_t collision_plane_enabled = 0U;
    uint32_t depth_collision_enabled = 0U;
    float *values[] = {
        &emitter->turbulence_strength,
        &emitter->turbulence_frequency,
        &emitter->force_center.x,
        &emitter->force_center.y,
        &emitter->radial_force,
        &emitter->vortex_force,
        &emitter->attractor_position.x,
        &emitter->attractor_position.y,
        &emitter->attractor_strength,
        &emitter->attractor_radius,
        &emitter->repulsor_position.x,
        &emitter->repulsor_position.y,
        &emitter->repulsor_strength,
        &emitter->repulsor_radius,
        &emitter->vector_field_direction.x,
        &emitter->vector_field_direction.y,
        &emitter->vector_field_strength,
        &emitter->collision_plane_normal.x,
        &emitter->collision_plane_normal.y,
        &emitter->collision_plane_offset,
        &emitter->collision_restitution,
        &emitter->collision_friction,
        &emitter->depth_min,
        &emitter->depth_max,
        &emitter->depth_velocity_min,
        &emitter->depth_velocity_max,
        &emitter->trail_length,
        &emitter->trail_width,
        &emitter->mesh_scale,
        &emitter->decal_rotation_offset,
        &emitter->distortion_strength,
        &emitter->soft_fade_distance,
        &emitter->light_radius,
    };
    if (strncmp(line, "vfx_emitter ", 12U) != 0) {
        return false;
    }
    cursor += 12U;
    if (!af_project_next_u64_token(&cursor, out_id)) {
        return false;
    }
    for (uint32_t index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        if (!af_project_next_float_token(&cursor, values[index])) {
            return false;
        }
    }
    if (!af_project_next_u32_token(&cursor, &collision_plane_enabled) ||
        collision_plane_enabled > 1U ||
        !af_project_next_u32_token(&cursor, &depth_collision_enabled) ||
        depth_collision_enabled > 1U ||
        !af_project_next_u64_token(&cursor, &emitter->sub_emitter_id) ||
        !af_project_next_u32_token(&cursor, &emitter->sub_emit_birth_count) ||
        !af_project_next_u32_token(&cursor, &emitter->sub_emit_death_count) ||
        !af_project_next_u32_token(&cursor, &emitter->max_recursion_depth) ||
        !af_project_next_u32_token(&cursor, &emitter->event_burst_count) ||
        !af_project_parse_end(cursor)) {
        return false;
    }
    emitter->collision_plane_enabled = collision_plane_enabled != 0U;
    emitter->depth_collision_enabled = depth_collision_enabled != 0U;
    return true;
}

static bool af_project_parse_i32(const char *text, int32_t *out_value) {
    long value = 0L;
    char *end = NULL;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || value < INT32_MIN || value > INT32_MAX ||
        !af_project_parse_end(end)) {
        return false;
    }
    *out_value = (int32_t)value;
    return true;
}

static bool af_project_parse_floats(const char *text, float *values, uint32_t count) {
    uint32_t index = 0U;
    char *end = NULL;
    for (index = 0U; index < count; ++index) {
        while (*text == ' ' || *text == '\t') {
            ++text;
        }
        errno = 0;
        values[index] = strtof(text, &end);
        if (errno != 0 || end == text || !isfinite(values[index])) {
            return false;
        }
        text = end;
    }
    return af_project_parse_end(text);
}

static const char *af_project_value(const char *line, const char *key) {
    const size_t length = strlen(key);
    if (strncmp(line, key, length) != 0 || line[length] != ' ') {
        return NULL;
    }
    return line + length + 1U;
}

static bool af_project_set_field(uint32_t *fields, uint32_t field) {
    if ((*fields & field) != 0U) {
        return false;
    }
    *fields |= field;
    return true;
}
