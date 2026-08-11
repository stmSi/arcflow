static uint32_t af_read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) | ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t af_read_u64(const uint8_t *bytes) {
    return (uint64_t)af_read_u32(bytes) | ((uint64_t)af_read_u32(bytes + 4U) << 32U);
}

static float af_read_f32(const uint8_t *bytes) {
    const uint32_t bits = af_read_u32(bytes);
    float value = 0.0F;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static AfParticleEmitter af_read_particle(const uint8_t *record) {
    return (AfParticleEmitter){
        .id = af_read_u64(record),
        .node_id = af_read_u64(record + 8U),
        .path_id = af_read_u64(record + 16U),
        .type = (AfParticleEmitterType)af_read_u32(record + 24U),
        .output = (AfParticleOutputType)af_read_u32(record + 28U),
        .capacity = af_read_u32(record + 32U),
        .seed = af_read_u32(record + 36U),
        .spawn_rate = af_read_f32(record + 40U),
        .burst_count = af_read_u32(record + 44U),
        .lifetime_min = af_read_f32(record + 48U),
        .lifetime_max = af_read_f32(record + 52U),
        .position_min = {af_read_f32(record + 56U), af_read_f32(record + 60U)},
        .position_max = {af_read_f32(record + 64U), af_read_f32(record + 68U)},
        .velocity_min = {af_read_f32(record + 72U), af_read_f32(record + 76U)},
        .velocity_max = {af_read_f32(record + 80U), af_read_f32(record + 84U)},
        .rotation_min = af_read_f32(record + 88U),
        .rotation_max = af_read_f32(record + 92U),
        .angular_velocity_min = af_read_f32(record + 96U),
        .angular_velocity_max = af_read_f32(record + 100U),
        .size_start_min = af_read_f32(record + 104U),
        .size_start_max = af_read_f32(record + 108U),
        .size_end_min = af_read_f32(record + 112U),
        .size_end_max = af_read_f32(record + 116U),
        .color_start = {af_read_f32(record + 120U), af_read_f32(record + 124U),
                        af_read_f32(record + 128U), af_read_f32(record + 132U)},
        .color_end = {af_read_f32(record + 136U), af_read_f32(record + 140U),
                      af_read_f32(record + 144U), af_read_f32(record + 148U)},
        .gravity = {af_read_f32(record + 152U), af_read_f32(record + 156U)},
        .wind = {af_read_f32(record + 160U), af_read_f32(record + 164U)},
        .drag = af_read_f32(record + 168U),
        .flipbook_columns = af_read_u32(record + 172U),
        .flipbook_rows = af_read_u32(record + 176U),
        .flipbook_frames_per_second = af_read_f32(record + 180U),
        .turbulence_strength = af_read_f32(record + 188U),
        .turbulence_frequency = af_read_f32(record + 192U),
        .force_center = {af_read_f32(record + 196U), af_read_f32(record + 200U)},
        .radial_force = af_read_f32(record + 204U),
        .vortex_force = af_read_f32(record + 208U),
        .attractor_position = {af_read_f32(record + 212U), af_read_f32(record + 216U)},
        .attractor_strength = af_read_f32(record + 220U),
        .attractor_radius = af_read_f32(record + 224U),
        .repulsor_position = {af_read_f32(record + 228U), af_read_f32(record + 232U)},
        .repulsor_strength = af_read_f32(record + 236U),
        .repulsor_radius = af_read_f32(record + 240U),
        .vector_field_direction = {af_read_f32(record + 244U), af_read_f32(record + 248U)},
        .vector_field_strength = af_read_f32(record + 252U),
        .collision_plane_normal = {af_read_f32(record + 256U), af_read_f32(record + 260U)},
        .collision_plane_offset = af_read_f32(record + 264U),
        .collision_restitution = af_read_f32(record + 268U),
        .collision_friction = af_read_f32(record + 272U),
        .depth_min = af_read_f32(record + 276U),
        .depth_max = af_read_f32(record + 280U),
        .depth_velocity_min = af_read_f32(record + 284U),
        .depth_velocity_max = af_read_f32(record + 288U),
        .trail_length = af_read_f32(record + 292U),
        .trail_width = af_read_f32(record + 296U),
        .mesh_scale = af_read_f32(record + 300U),
        .decal_rotation_offset = af_read_f32(record + 304U),
        .distortion_strength = af_read_f32(record + 308U),
        .soft_fade_distance = af_read_f32(record + 312U),
        .light_radius = af_read_f32(record + 316U),
        .collision_plane_enabled = af_read_u32(record + 320U) != 0U,
        .depth_collision_enabled = af_read_u32(record + 324U) != 0U,
        .sub_emitter_id = af_read_u64(record + 328U),
        .sub_emit_birth_count = af_read_u32(record + 336U),
        .sub_emit_death_count = af_read_u32(record + 340U),
        .max_recursion_depth = af_read_u32(record + 344U),
        .event_burst_count = af_read_u32(record + 348U),
        .enabled = af_read_u32(record + 184U) != 0U};
}

static void af_write_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static void af_write_u64(uint8_t *bytes, uint64_t value) {
    af_write_u32(bytes, (uint32_t)value);
    af_write_u32(bytes + 4U, (uint32_t)(value >> 32U));
}

static void af_write_f32(uint8_t *bytes, float value) {
    uint32_t bits = 0U;
    memcpy(&bits, &value, sizeof(bits));
    af_write_u32(bytes, bits);
}

static void af_write_particle(uint8_t *record, const AfParticleEmitter *emitter) {
    af_write_u64(record, emitter->id);
    af_write_u64(record + 8U, emitter->node_id);
    af_write_u64(record + 16U, emitter->path_id);
    af_write_u32(record + 24U, (uint32_t)emitter->type);
    af_write_u32(record + 28U, (uint32_t)emitter->output);
    af_write_u32(record + 32U, emitter->capacity);
    af_write_u32(record + 36U, emitter->seed);
    af_write_f32(record + 40U, emitter->spawn_rate);
    af_write_u32(record + 44U, emitter->burst_count);
    af_write_f32(record + 48U, emitter->lifetime_min);
    af_write_f32(record + 52U, emitter->lifetime_max);
    af_write_f32(record + 56U, emitter->position_min.x);
    af_write_f32(record + 60U, emitter->position_min.y);
    af_write_f32(record + 64U, emitter->position_max.x);
    af_write_f32(record + 68U, emitter->position_max.y);
    af_write_f32(record + 72U, emitter->velocity_min.x);
    af_write_f32(record + 76U, emitter->velocity_min.y);
    af_write_f32(record + 80U, emitter->velocity_max.x);
    af_write_f32(record + 84U, emitter->velocity_max.y);
    af_write_f32(record + 88U, emitter->rotation_min);
    af_write_f32(record + 92U, emitter->rotation_max);
    af_write_f32(record + 96U, emitter->angular_velocity_min);
    af_write_f32(record + 100U, emitter->angular_velocity_max);
    af_write_f32(record + 104U, emitter->size_start_min);
    af_write_f32(record + 108U, emitter->size_start_max);
    af_write_f32(record + 112U, emitter->size_end_min);
    af_write_f32(record + 116U, emitter->size_end_max);
    af_write_f32(record + 120U, emitter->color_start.r);
    af_write_f32(record + 124U, emitter->color_start.g);
    af_write_f32(record + 128U, emitter->color_start.b);
    af_write_f32(record + 132U, emitter->color_start.a);
    af_write_f32(record + 136U, emitter->color_end.r);
    af_write_f32(record + 140U, emitter->color_end.g);
    af_write_f32(record + 144U, emitter->color_end.b);
    af_write_f32(record + 148U, emitter->color_end.a);
    af_write_f32(record + 152U, emitter->gravity.x);
    af_write_f32(record + 156U, emitter->gravity.y);
    af_write_f32(record + 160U, emitter->wind.x);
    af_write_f32(record + 164U, emitter->wind.y);
    af_write_f32(record + 168U, emitter->drag);
    af_write_u32(record + 172U, emitter->flipbook_columns);
    af_write_u32(record + 176U, emitter->flipbook_rows);
    af_write_f32(record + 180U, emitter->flipbook_frames_per_second);
    af_write_u32(record + 184U, emitter->enabled ? 1U : 0U);
    af_write_f32(record + 188U, emitter->turbulence_strength);
    af_write_f32(record + 192U, emitter->turbulence_frequency);
    af_write_f32(record + 196U, emitter->force_center.x);
    af_write_f32(record + 200U, emitter->force_center.y);
    af_write_f32(record + 204U, emitter->radial_force);
    af_write_f32(record + 208U, emitter->vortex_force);
    af_write_f32(record + 212U, emitter->attractor_position.x);
    af_write_f32(record + 216U, emitter->attractor_position.y);
    af_write_f32(record + 220U, emitter->attractor_strength);
    af_write_f32(record + 224U, emitter->attractor_radius);
    af_write_f32(record + 228U, emitter->repulsor_position.x);
    af_write_f32(record + 232U, emitter->repulsor_position.y);
    af_write_f32(record + 236U, emitter->repulsor_strength);
    af_write_f32(record + 240U, emitter->repulsor_radius);
    af_write_f32(record + 244U, emitter->vector_field_direction.x);
    af_write_f32(record + 248U, emitter->vector_field_direction.y);
    af_write_f32(record + 252U, emitter->vector_field_strength);
    af_write_f32(record + 256U, emitter->collision_plane_normal.x);
    af_write_f32(record + 260U, emitter->collision_plane_normal.y);
    af_write_f32(record + 264U, emitter->collision_plane_offset);
    af_write_f32(record + 268U, emitter->collision_restitution);
    af_write_f32(record + 272U, emitter->collision_friction);
    af_write_f32(record + 276U, emitter->depth_min);
    af_write_f32(record + 280U, emitter->depth_max);
    af_write_f32(record + 284U, emitter->depth_velocity_min);
    af_write_f32(record + 288U, emitter->depth_velocity_max);
    af_write_f32(record + 292U, emitter->trail_length);
    af_write_f32(record + 296U, emitter->trail_width);
    af_write_f32(record + 300U, emitter->mesh_scale);
    af_write_f32(record + 304U, emitter->decal_rotation_offset);
    af_write_f32(record + 308U, emitter->distortion_strength);
    af_write_f32(record + 312U, emitter->soft_fade_distance);
    af_write_f32(record + 316U, emitter->light_radius);
    af_write_u32(record + 320U, emitter->collision_plane_enabled ? 1U : 0U);
    af_write_u32(record + 324U, emitter->depth_collision_enabled ? 1U : 0U);
    af_write_u64(record + 328U, emitter->sub_emitter_id);
    af_write_u32(record + 336U, emitter->sub_emit_birth_count);
    af_write_u32(record + 340U, emitter->sub_emit_death_count);
    af_write_u32(record + 344U, emitter->max_recursion_depth);
    af_write_u32(record + 348U, emitter->event_burst_count);
}

static void af_write_component_value(uint8_t *record, uint64_t owner_id, uint64_t property_id,
                                     AfComponentPropertyType type, const AfComponentValue *value) {
    af_write_u64(record, owner_id);
    af_write_u64(record + 8U, property_id);
    af_write_u32(record + 16U, (uint32_t)type);
    if (type == AF_COMPONENT_PROPERTY_NUMBER) {
        af_write_f32(record + 24U, value->number);
    } else if (type == AF_COMPONENT_PROPERTY_BOOLEAN) {
        af_write_u32(record + 24U, value->boolean ? 1U : 0U);
    } else if (type == AF_COMPONENT_PROPERTY_COLOR) {
        af_write_f32(record + 24U, value->color.r);
        af_write_f32(record + 28U, value->color.g);
        af_write_f32(record + 32U, value->color.b);
        af_write_f32(record + 36U, value->color.a);
    } else if (type == AF_COMPONENT_PROPERTY_IMAGE) {
        af_write_u64(record + 24U, value->image_id);
    } else if (type == AF_COMPONENT_PROPERTY_TEXT) {
        memcpy(record + 24U, value->text, AF_COMPONENT_TEXT_CAPACITY);
    }
}

static AfComponentValue af_read_component_value(const uint8_t *record,
                                                AfComponentPropertyType type) {
    AfComponentValue value = {0};
    if (type == AF_COMPONENT_PROPERTY_NUMBER) {
        value.number = af_read_f32(record + 24U);
    } else if (type == AF_COMPONENT_PROPERTY_BOOLEAN) {
        value.boolean = af_read_u32(record + 24U) != 0U;
    } else if (type == AF_COMPONENT_PROPERTY_COLOR) {
        value.color = (AfColor){af_read_f32(record + 24U), af_read_f32(record + 28U),
                                af_read_f32(record + 32U), af_read_f32(record + 36U)};
    } else if (type == AF_COMPONENT_PROPERTY_IMAGE) {
        value.image_id = af_read_u64(record + 24U);
    } else if (type == AF_COMPONENT_PROPERTY_TEXT) {
        memcpy(value.text, record + 24U, AF_COMPONENT_TEXT_CAPACITY);
    }
    return value;
}

static uint64_t af_component_section_size(const AfComponentData *data) {
    return AF_COMPONENT_HEADER_SIZE +
           (uint64_t)data->definition_count * AF_COMPONENT_DEFINITION_RECORD_SIZE +
           (uint64_t)data->reference_count * AF_COMPONENT_REFERENCE_RECORD_SIZE +
           (uint64_t)data->property_count * AF_COMPONENT_VALUE_RECORD_SIZE +
           (uint64_t)data->instance_count * AF_COMPONENT_INSTANCE_RECORD_SIZE +
           (uint64_t)data->override_count * AF_COMPONENT_VALUE_RECORD_SIZE;
}

static uint64_t af_state_machine_section_size(const AfStateMachineData *data) {
    return AF_STATE_MACHINE_HEADER_SIZE +
           (uint64_t)data->machine_count * AF_STATE_MACHINE_RECORD_SIZE +
           (uint64_t)data->state_count * AF_STATE_RECORD_SIZE +
           (uint64_t)data->transition_count * AF_STATE_TRANSITION_RECORD_SIZE +
           (uint64_t)data->input_count * AF_STATE_INPUT_RECORD_SIZE +
           (uint64_t)data->condition_count * AF_STATE_CONDITION_RECORD_SIZE;
}

static void af_write_state_value(uint8_t *bytes, AfStateInputType type, AfStateInputValue value) {
    if (type == AF_STATE_INPUT_BOOLEAN || type == AF_STATE_INPUT_TRIGGER) {
        af_write_u32(bytes, value.boolean ? 1U : 0U);
    } else if (type == AF_STATE_INPUT_INTEGER) {
        af_write_u32(bytes, (uint32_t)value.integer);
    } else if (type == AF_STATE_INPUT_FLOAT) {
        af_write_f32(bytes, value.real);
    }
}

static AfStateInputValue af_read_state_value(const uint8_t *bytes, AfStateInputType type) {
    AfStateInputValue value = {0};
    if (type == AF_STATE_INPUT_BOOLEAN || type == AF_STATE_INPUT_TRIGGER) {
        value.boolean = af_read_u32(bytes) != 0U;
    } else if (type == AF_STATE_INPUT_INTEGER) {
        value.integer = (int32_t)af_read_u32(bytes);
    } else if (type == AF_STATE_INPUT_FLOAT) {
        value.real = af_read_f32(bytes);
    }
    return value;
}

static void af_write_paint(uint8_t *record, const AfPaint *paint) {
    af_write_u32(record, (uint32_t)paint->type);
    af_write_u32(record + 4U, paint->stop_count);
    af_write_f32(record + 8U, paint->solid.r);
    af_write_f32(record + 12U, paint->solid.g);
    af_write_f32(record + 16U, paint->solid.b);
    af_write_f32(record + 20U, paint->solid.a);
    af_write_f32(record + 24U, paint->start.x);
    af_write_f32(record + 28U, paint->start.y);
    af_write_f32(record + 32U, paint->end.x);
    af_write_f32(record + 36U, paint->end.y);
    af_write_f32(record + 40U, paint->start_radius);
    af_write_f32(record + 44U, paint->end_radius);
    af_write_f32(record + 48U, paint->angle);
    af_write_f32(record + 56U, paint->transform.m00);
    af_write_f32(record + 60U, paint->transform.m01);
    af_write_f32(record + 64U, paint->transform.m10);
    af_write_f32(record + 68U, paint->transform.m11);
    af_write_f32(record + 72U, paint->transform.tx);
    af_write_f32(record + 76U, paint->transform.ty);
    for (uint32_t index = 0U; index < AF_MAX_GRADIENT_STOPS; ++index) {
        uint8_t *stop = record + 80U + (uint64_t)index * 20U;
        af_write_f32(stop, paint->stops[index].offset);
        af_write_f32(stop + 4U, paint->stops[index].color.r);
        af_write_f32(stop + 8U, paint->stops[index].color.g);
        af_write_f32(stop + 12U, paint->stops[index].color.b);
        af_write_f32(stop + 16U, paint->stops[index].color.a);
    }
}

static AfPaint af_read_paint(const uint8_t *record) {
    AfPaint paint = {.type = (AfPaintType)af_read_u32(record),
                     .stop_count = af_read_u32(record + 4U),
                     .solid = {af_read_f32(record + 8U), af_read_f32(record + 12U),
                               af_read_f32(record + 16U), af_read_f32(record + 20U)},
                     .start = {af_read_f32(record + 24U), af_read_f32(record + 28U)},
                     .end = {af_read_f32(record + 32U), af_read_f32(record + 36U)},
                     .start_radius = af_read_f32(record + 40U),
                     .end_radius = af_read_f32(record + 44U),
                     .angle = af_read_f32(record + 48U),
                     .transform = {.m00 = af_read_f32(record + 56U),
                                   .m01 = af_read_f32(record + 60U),
                                   .m10 = af_read_f32(record + 64U),
                                   .m11 = af_read_f32(record + 68U),
                                   .tx = af_read_f32(record + 72U),
                                   .ty = af_read_f32(record + 76U)}};
    for (uint32_t index = 0U; index < AF_MAX_GRADIENT_STOPS; ++index) {
        const uint8_t *stop = record + 80U + (uint64_t)index * 20U;
        paint.stops[index] =
            (AfGradientStop){.offset = af_read_f32(stop),
                             .color = {af_read_f32(stop + 4U), af_read_f32(stop + 8U),
                                       af_read_f32(stop + 12U), af_read_f32(stop + 16U)}};
    }
    return paint;
}

static void af_write_style(uint8_t *record, const AfStyle *style) {
    af_write_u64(record, style->id);
    af_write_u64(record + 8U, style->node_id);
    af_write_u64(record + 16U, style->parent_style_id);
    af_write_u32(record + 24U, style->inherit_flags);
    af_write_f32(record + 28U, style->fill_opacity);
    af_write_f32(record + 32U, style->stroke_opacity);
    af_write_u32(record + 36U, style->stroke.enabled ? 1U : 0U);
    af_write_f32(record + 40U, style->stroke.width);
    af_write_f32(record + 44U, style->stroke.end_width);
    af_write_u32(record + 48U, (uint32_t)style->stroke.cap);
    af_write_u32(record + 52U, (uint32_t)style->stroke.join);
    af_write_f32(record + 56U, style->stroke.miter_limit);
    af_write_u32(record + 60U, style->stroke.dash_count);
    af_write_f32(record + 64U, style->stroke.dash_offset);
    for (uint32_t index = 0U; index < AF_MAX_STROKE_DASHES; ++index) {
        af_write_f32(record + 68U + (uint64_t)index * 4U, style->stroke.dashes[index]);
    }
    af_write_u64(record + 136U, style->paint_generation);
    af_write_u64(record + 144U, style->stroke_geometry_generation);
    af_write_paint(record + 152U, &style->fill);
    af_write_paint(record + 152U + AF_PAINT_RECORD_SIZE, &style->stroke_paint);
}

static AfStyle af_read_style(const uint8_t *record) {
    AfStyle style = {.id = af_read_u64(record),
                     .node_id = af_read_u64(record + 8U),
                     .parent_style_id = af_read_u64(record + 16U),
                     .inherit_flags = af_read_u32(record + 24U),
                     .fill_opacity = af_read_f32(record + 28U),
                     .stroke_opacity = af_read_f32(record + 32U),
                     .stroke = {.enabled = af_read_u32(record + 36U) != 0U,
                                .width = af_read_f32(record + 40U),
                                .end_width = af_read_f32(record + 44U),
                                .cap = (AfStrokeCap)af_read_u32(record + 48U),
                                .join = (AfStrokeJoin)af_read_u32(record + 52U),
                                .miter_limit = af_read_f32(record + 56U),
                                .dash_count = af_read_u32(record + 60U),
                                .dash_offset = af_read_f32(record + 64U)},
                     .paint_generation = af_read_u64(record + 136U),
                     .stroke_geometry_generation = af_read_u64(record + 144U),
                     .fill = af_read_paint(record + 152U),
                     .stroke_paint = af_read_paint(record + 152U + AF_PAINT_RECORD_SIZE)};
    for (uint32_t index = 0U; index < AF_MAX_STROKE_DASHES; ++index) {
        style.stroke.dashes[index] = af_read_f32(record + 68U + (uint64_t)index * 4U);
    }
    return style;
}

static void af_write_layout(uint8_t *record, const AfLayoutNode *layout) {
    const float values[] = {layout->anchor_min.x, layout->anchor_min.y, layout->anchor_max.x,
                            layout->anchor_max.y, layout->pivot.x,      layout->pivot.y,
                            layout->offset.x,     layout->offset.y};
    af_write_u64(record, layout->node_id);
    for (uint32_t index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        af_write_f32(record + 8U + (uint64_t)index * 4U, values[index]);
    }
    af_write_u32(record + 40U, (uint32_t)layout->width.mode);
    af_write_f32(record + 44U, layout->width.value);
    af_write_u32(record + 48U, (uint32_t)layout->height.mode);
    af_write_f32(record + 52U, layout->height.value);
    af_write_f32(record + 56U, layout->minimum_size.x);
    af_write_f32(record + 60U, layout->minimum_size.y);
    af_write_f32(record + 64U, layout->maximum_size.x);
    af_write_f32(record + 68U, layout->maximum_size.y);
    af_write_f32(record + 72U, layout->aspect_ratio);
    af_write_u32(record + 76U, (uint32_t)layout->aspect);
    af_write_u32(record + 80U, (uint32_t)layout->direction);
    af_write_u32(record + 84U, (uint32_t)layout->child_alignment);
    af_write_f32(record + 88U, layout->spacing);
    af_write_f32(record + 92U, layout->padding.left);
    af_write_f32(record + 96U, layout->padding.top);
    af_write_f32(record + 100U, layout->padding.right);
    af_write_f32(record + 104U, layout->padding.bottom);
}

static AfLayoutNode af_read_layout(const uint8_t *record) {
    return (AfLayoutNode){.node_id = af_read_u64(record),
                          .anchor_min = {af_read_f32(record + 8U), af_read_f32(record + 12U)},
                          .anchor_max = {af_read_f32(record + 16U), af_read_f32(record + 20U)},
                          .pivot = {af_read_f32(record + 24U), af_read_f32(record + 28U)},
                          .offset = {af_read_f32(record + 32U), af_read_f32(record + 36U)},
                          .width = {.mode = (AfLayoutValueMode)af_read_u32(record + 40U),
                                    .value = af_read_f32(record + 44U)},
                          .height = {.mode = (AfLayoutValueMode)af_read_u32(record + 48U),
                                     .value = af_read_f32(record + 52U)},
                          .minimum_size = {af_read_f32(record + 56U), af_read_f32(record + 60U)},
                          .maximum_size = {af_read_f32(record + 64U), af_read_f32(record + 68U)},
                          .aspect_ratio = af_read_f32(record + 72U),
                          .aspect = (AfAspectConstraint)af_read_u32(record + 76U),
                          .direction = (AfLayoutDirection)af_read_u32(record + 80U),
                          .child_alignment = (AfLayoutAlignment)af_read_u32(record + 84U),
                          .spacing = af_read_f32(record + 88U),
                          .padding = {.left = af_read_f32(record + 92U),
                                      .top = af_read_f32(record + 96U),
                                      .right = af_read_f32(record + 100U),
                                      .bottom = af_read_f32(record + 104U)}};
}

static uint32_t af_checksum(const uint8_t *bytes, size_t size) {
    uint32_t hash = 2166136261U;
    size_t index = 0U;
    for (index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static uint64_t af_align_u64(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1U) & ~(alignment - 1U);
}

static const AfSectionView *af_find_section(const AfSectionView *sections, uint32_t count,
                                            uint32_t type) {
    uint32_t index = 0U;
    for (index = 0U; index < count; ++index) {
        if (sections[index].type == type) {
            return &sections[index];
        }
    }
    return NULL;
}
