static AfResult af_read_sections(const uint8_t *bytes, size_t size, AfSectionView *sections,
                                 uint32_t *out_count) {
    uint64_t table_offset = 0U;
    uint64_t table_size = 0U;
    uint64_t table_end = 0U;
    uint32_t count = 0U;
    uint32_t index = 0U;
    if (size < AF_PACKAGE_HEADER_SIZE || memcmp(bytes, AF_PACKAGE_MAGIC, 8U) != 0) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    if (af_read_u32(bytes + 8U) != AF_PACKAGE_FORMAT_VERSION ||
        af_read_u32(bytes + 12U) > AF_RUNTIME_VERSION) {
        return AF_RESULT_UNSUPPORTED_VERSION;
    }
    if (af_read_u32(bytes + 16U) != AF_PACKAGE_ENDIAN_MARKER ||
        af_read_u32(bytes + 20U) != AF_PACKAGE_HEADER_SIZE || af_read_u64(bytes + 24U) != size) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    if (af_read_u32(bytes + 44U) != 0U) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    table_offset = af_read_u64(bytes + 32U);
    count = af_read_u32(bytes + 40U);
    if (count == 0U || count > AF_PACKAGE_MAX_SECTIONS ||
        af_u64_mul_overflow(count, AF_PACKAGE_SECTION_SIZE, &table_size) ||
        af_u64_add_overflow(table_offset, table_size, &table_end) ||
        table_offset < AF_PACKAGE_HEADER_SIZE || table_end > size) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    for (index = 0U; index < count; ++index) {
        const uint8_t *entry = bytes + table_offset + (uint64_t)index * AF_PACKAGE_SECTION_SIZE;
        uint64_t end = 0U;
        uint32_t other = 0U;
        sections[index] = (AfSectionView){.type = af_read_u32(entry),
                                          .offset = af_read_u64(entry + 8U),
                                          .size = af_read_u64(entry + 16U),
                                          .alignment = af_read_u32(entry + 24U),
                                          .count = af_read_u32(entry + 28U)};
        if (af_read_u32(entry + 4U) != 0U || sections[index].type < AF_SECTION_NODES ||
            sections[index].type > AF_SECTION_BINDINGS ||
            !af_is_power_of_two(sections[index].alignment) || sections[index].alignment > 4096U ||
            (sections[index].offset & (uint64_t)(sections[index].alignment - 1U)) != 0U ||
            af_u64_add_overflow(sections[index].offset, sections[index].size, &end) || end > size ||
            sections[index].offset < table_end) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (other = 0U; other < index; ++other) {
            uint64_t other_end = 0U;
            if (sections[index].type == sections[other].type ||
                af_u64_add_overflow(sections[other].offset, sections[other].size, &other_end) ||
                (sections[index].size != 0U && sections[other].size != 0U &&
                 sections[index].offset < other_end && sections[other].offset < end)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    *out_count = count;
    return AF_RESULT_SUCCESS;
}

static bool af_node_id_exists(const uint8_t *bytes, const AfSectionView *nodes, uint64_t id) {
    uint32_t index = 0U;
    for (index = 0U; index < nodes->count; ++index) {
        if (af_read_u64(bytes + nodes->offset + (uint64_t)index * AF_NODE_RECORD_SIZE) == id) {
            return true;
        }
    }
    return false;
}

static uint32_t af_node_type_for_id(const uint8_t *bytes, const AfSectionView *nodes, uint64_t id) {
    for (uint32_t index = 0U; index < nodes->count; ++index) {
        const uint8_t *record = bytes + nodes->offset + (uint64_t)index * AF_NODE_RECORD_SIZE;
        if (af_read_u64(record) == id) {
            return af_read_u32(record + 16U);
        }
    }
    return UINT32_MAX;
}

static bool af_path_id_exists(const uint8_t *bytes, const AfSectionView *paths, uint64_t id) {
    for (uint32_t index = 0U; index < paths->count; ++index) {
        if (af_read_u64(bytes + paths->offset + (uint64_t)index * AF_PATH_RECORD_SIZE) == id) {
            return true;
        }
    }
    return false;
}

static bool af_track_target_valid(const uint8_t *bytes, const AfSectionView *nodes,
                                  const AfSectionView *paths, const AfSectionView *path_commands,
                                  uint64_t node_id, AfPropertyType property,
                                  uint32_t element_index) {
    if (!af_node_id_exists(bytes, nodes, node_id)) {
        return false;
    }
    if (property < AF_PROPERTY_PATH_POINT) {
        return element_index == 0U;
    }
    if (af_node_type_for_id(bytes, nodes, node_id) != AF_NODE_PATH) {
        return false;
    }
    for (uint32_t index = 0U; index < paths->count; ++index) {
        const uint8_t *path = bytes + paths->offset + (uint64_t)index * AF_PATH_RECORD_SIZE;
        const uint32_t start = af_read_u32(path + 16U);
        const uint32_t count = af_read_u32(path + 20U);
        if (af_read_u64(path + 8U) != node_id || element_index >= count ||
            start > path_commands->count || element_index >= path_commands->count - start) {
            continue;
        }
        const uint32_t type =
            af_read_u32(bytes + path_commands->offset +
                        (uint64_t)(start + element_index) * AF_PATH_COMMAND_RECORD_SIZE);
        return type != AF_PATH_CLOSE &&
               (property == AF_PROPERTY_PATH_POINT || type == AF_PATH_CUBIC);
    }
    return false;
}

static bool af_particle_record_valid(const uint8_t *record, const uint8_t *bytes,
                                     const AfSectionView *nodes, const AfSectionView *paths) {
    const AfParticleEmitter emitter = af_read_particle(record);
    const float values[] = {
        emitter.spawn_rate,
        emitter.lifetime_min,
        emitter.lifetime_max,
        emitter.position_min.x,
        emitter.position_min.y,
        emitter.position_max.x,
        emitter.position_max.y,
        emitter.velocity_min.x,
        emitter.velocity_min.y,
        emitter.velocity_max.x,
        emitter.velocity_max.y,
        emitter.rotation_min,
        emitter.rotation_max,
        emitter.angular_velocity_min,
        emitter.angular_velocity_max,
        emitter.size_start_min,
        emitter.size_start_max,
        emitter.size_end_min,
        emitter.size_end_max,
        emitter.color_start.r,
        emitter.color_start.g,
        emitter.color_start.b,
        emitter.color_start.a,
        emitter.color_end.r,
        emitter.color_end.g,
        emitter.color_end.b,
        emitter.color_end.a,
        emitter.gravity.x,
        emitter.gravity.y,
        emitter.wind.x,
        emitter.wind.y,
        emitter.drag,
        emitter.flipbook_frames_per_second,
        emitter.turbulence_strength,
        emitter.turbulence_frequency,
        emitter.force_center.x,
        emitter.force_center.y,
        emitter.radial_force,
        emitter.vortex_force,
        emitter.attractor_position.x,
        emitter.attractor_position.y,
        emitter.attractor_strength,
        emitter.attractor_radius,
        emitter.repulsor_position.x,
        emitter.repulsor_position.y,
        emitter.repulsor_strength,
        emitter.repulsor_radius,
        emitter.vector_field_direction.x,
        emitter.vector_field_direction.y,
        emitter.vector_field_strength,
        emitter.collision_plane_normal.x,
        emitter.collision_plane_normal.y,
        emitter.collision_plane_offset,
        emitter.collision_restitution,
        emitter.collision_friction,
        emitter.depth_min,
        emitter.depth_max,
        emitter.depth_velocity_min,
        emitter.depth_velocity_max,
        emitter.trail_length,
        emitter.trail_width,
        emitter.mesh_scale,
        emitter.decal_rotation_offset,
        emitter.distortion_strength,
        emitter.soft_fade_distance,
        emitter.light_radius,
    };
    if (emitter.id == 0U || emitter.id == UINT64_MAX || emitter.node_id == 0U ||
        !af_node_id_exists(bytes, nodes, emitter.node_id) ||
        emitter.type > AF_PARTICLE_EMITTER_VECTOR_PATH ||
        emitter.output > AF_PARTICLE_OUTPUT_LIGHT_PROXY || emitter.capacity == 0U ||
        emitter.capacity > AF_MAX_PARTICLES_PER_EMITTER || emitter.spawn_rate < 0.0F ||
        emitter.burst_count > emitter.capacity || emitter.lifetime_min <= 0.0F ||
        emitter.lifetime_max < emitter.lifetime_min ||
        emitter.position_max.x < emitter.position_min.x ||
        emitter.position_max.y < emitter.position_min.y ||
        emitter.velocity_max.x < emitter.velocity_min.x ||
        emitter.velocity_max.y < emitter.velocity_min.y ||
        emitter.rotation_max < emitter.rotation_min ||
        emitter.angular_velocity_max < emitter.angular_velocity_min ||
        emitter.size_start_min < 0.0F || emitter.size_start_max < emitter.size_start_min ||
        emitter.size_end_min < 0.0F || emitter.size_end_max < emitter.size_end_min ||
        emitter.drag < 0.0F || emitter.flipbook_columns == 0U || emitter.flipbook_columns > 64U ||
        emitter.flipbook_rows == 0U || emitter.flipbook_rows > 64U ||
        emitter.flipbook_frames_per_second < 0.0F || af_read_u32(record + 184U) > 1U ||
        fabsf(emitter.turbulence_strength) > AF_MAX_PARTICLE_FORCE ||
        emitter.turbulence_frequency < 0.0F || emitter.turbulence_frequency > 100.0F ||
        fabsf(emitter.radial_force) > AF_MAX_PARTICLE_FORCE ||
        fabsf(emitter.vortex_force) > AF_MAX_PARTICLE_FORCE ||
        fabsf(emitter.attractor_strength) > AF_MAX_PARTICLE_FORCE ||
        emitter.attractor_radius < 0.0F ||
        (emitter.attractor_strength != 0.0F && emitter.attractor_radius <= 0.0F) ||
        fabsf(emitter.repulsor_strength) > AF_MAX_PARTICLE_FORCE ||
        emitter.repulsor_radius < 0.0F ||
        (emitter.repulsor_strength != 0.0F && emitter.repulsor_radius <= 0.0F) ||
        fabsf(emitter.vector_field_strength) > AF_MAX_PARTICLE_FORCE ||
        (emitter.vector_field_strength != 0.0F &&
         emitter.vector_field_direction.x * emitter.vector_field_direction.x +
                 emitter.vector_field_direction.y * emitter.vector_field_direction.y <
             0.000001F) ||
        emitter.collision_restitution < 0.0F || emitter.collision_restitution > 1.0F ||
        emitter.collision_friction < 0.0F || emitter.collision_friction > 1.0F ||
        af_read_u32(record + 320U) > 1U || af_read_u32(record + 324U) > 1U ||
        (emitter.collision_plane_enabled &&
         emitter.collision_plane_normal.x * emitter.collision_plane_normal.x +
                 emitter.collision_plane_normal.y * emitter.collision_plane_normal.y <
             0.000001F) ||
        emitter.depth_max < emitter.depth_min ||
        emitter.depth_velocity_max < emitter.depth_velocity_min || emitter.trail_length < 0.0F ||
        emitter.trail_length > 100000.0F || emitter.trail_width < 0.0F ||
        emitter.trail_width > 100000.0F || emitter.mesh_scale <= 0.0F ||
        emitter.mesh_scale > 1000.0F || emitter.soft_fade_distance < 0.0F ||
        emitter.light_radius < 0.0F ||
        ((emitter.output == AF_PARTICLE_OUTPUT_TRAIL ||
          emitter.output == AF_PARTICLE_OUTPUT_RIBBON) &&
         (emitter.trail_length <= 0.0F || emitter.trail_width <= 0.0F)) ||
        (emitter.output == AF_PARTICLE_OUTPUT_SOFT && emitter.soft_fade_distance <= 0.0F) ||
        (emitter.output == AF_PARTICLE_OUTPUT_LIGHT_PROXY && emitter.light_radius <= 0.0F) ||
        emitter.sub_emit_birth_count > AF_MAX_PARTICLE_SUB_EMIT_COUNT ||
        emitter.sub_emit_death_count > AF_MAX_PARTICLE_SUB_EMIT_COUNT ||
        emitter.max_recursion_depth > AF_MAX_PARTICLE_RECURSION_DEPTH ||
        emitter.event_burst_count > emitter.capacity ||
        (emitter.sub_emitter_id == 0U &&
         (emitter.sub_emit_birth_count != 0U || emitter.sub_emit_death_count != 0U)) ||
        (emitter.sub_emitter_id != 0U && emitter.max_recursion_depth == 0U) ||
        (emitter.type == AF_PARTICLE_EMITTER_VECTOR_PATH &&
         !af_path_id_exists(bytes, paths, emitter.path_id)) ||
        (emitter.type != AF_PARTICLE_EMITTER_VECTOR_PATH && emitter.path_id != 0U)) {
        return false;
    }
    for (uint32_t index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        if (!isfinite(values[index])) {
            return false;
        }
    }
    for (uint32_t index = 19U; index <= 26U; ++index) {
        if (values[index] < 0.0F || values[index] > 1.0F) {
            return false;
        }
    }
    for (uint32_t reserved = 352U; reserved < AF_PARTICLE_RECORD_SIZE; reserved += 4U) {
        if (af_read_u32(record + reserved) != 0U) {
            return false;
        }
    }
    return true;
}

static bool af_path_node_exists(const uint8_t *bytes, const AfSectionView *paths,
                                uint64_t node_id) {
    for (uint32_t index = 0U; index < paths->count; ++index) {
        if (af_read_u64(bytes + paths->offset + (uint64_t)index * AF_PATH_RECORD_SIZE + 8U) ==
            node_id) {
            return true;
        }
    }
    return false;
}

static bool af_clip_id_exists(const uint8_t *bytes, const AfSectionView *clips, uint64_t id) {
    if (id == 0U) {
        return true;
    }
    for (uint32_t index = 0U; index < clips->count; ++index) {
        if (af_read_u64(bytes + clips->offset + (uint64_t)index * AF_CLIP_RECORD_SIZE) == id) {
            return true;
        }
    }
    return false;
}

static const uint8_t *af_component_definition_record(const uint8_t *bytes,
                                                     const AfSectionView *components, uint64_t id) {
    const uint32_t count = af_read_u32(bytes + components->offset);
    for (uint32_t index = 0U; index < count; ++index) {
        const uint8_t *record = bytes + components->offset + AF_COMPONENT_HEADER_SIZE +
                                (uint64_t)index * AF_COMPONENT_DEFINITION_RECORD_SIZE;
        if (af_read_u64(record) == id) {
            return record;
        }
    }
    return NULL;
}

static bool af_component_value_record_valid(const uint8_t *record) {
    const uint32_t type = af_read_u32(record + 16U);
    if (af_read_u64(record) == 0U || af_read_u64(record + 8U) == 0U ||
        type > AF_COMPONENT_PROPERTY_TEXT || af_read_u32(record + 20U) != 0U) {
        return false;
    }
    if (type == AF_COMPONENT_PROPERTY_NUMBER) {
        return isfinite(af_read_f32(record + 24U));
    }
    if (type == AF_COMPONENT_PROPERTY_BOOLEAN) {
        return af_read_u32(record + 24U) <= 1U;
    }
    if (type == AF_COMPONENT_PROPERTY_COLOR) {
        for (uint32_t offset = 24U; offset <= 36U; offset += 4U) {
            const float value = af_read_f32(record + offset);
            if (!isfinite(value) || value < 0.0F || value > 1.0F) {
                return false;
            }
        }
        return true;
    }
    if (type == AF_COMPONENT_PROPERTY_IMAGE) {
        return af_read_u64(record + 24U) != 0U;
    }
    return memchr(record + 24U, '\0', AF_COMPONENT_TEXT_CAPACITY) != NULL;
}

static bool af_component_branch_valid(const uint8_t *bytes, const AfSectionView *components,
                                      uint64_t definition_id, uint64_t *stack, uint32_t depth) {
    const uint8_t *header = bytes + components->offset;
    const uint32_t definition_count = af_read_u32(header);
    const uint32_t reference_count = af_read_u32(header + 4U);
    const uint8_t *reference_base =
        bytes + components->offset + AF_COMPONENT_HEADER_SIZE +
        (uint64_t)definition_count * AF_COMPONENT_DEFINITION_RECORD_SIZE;
    const uint8_t *definition = af_component_definition_record(bytes, components, definition_id);
    if (definition == NULL || depth >= AF_MAX_COMPONENT_NESTING) {
        return false;
    }
    for (uint32_t index = 0U; index < depth; ++index) {
        if (stack[index] == definition_id) {
            return false;
        }
    }
    stack[depth] = definition_id;
    const uint32_t start = af_read_u32(definition + 24U);
    const uint32_t count = af_read_u32(definition + 28U);
    if (start > reference_count || count > reference_count - start) {
        return false;
    }
    for (uint32_t index = 0U; index < count; ++index) {
        const uint8_t *reference =
            reference_base + (uint64_t)(start + index) * AF_COMPONENT_REFERENCE_RECORD_SIZE;
        if (!af_component_branch_valid(bytes, components, af_read_u64(reference + 8U), stack,
                                       depth + 1U)) {
            return false;
        }
    }
    return true;
}

static AfResult af_validate_components(const uint8_t *bytes, const AfSectionView *components,
                                       const AfSectionView *nodes, const AfSectionView *clips) {
    const uint8_t *header = bytes + components->offset;
    const uint32_t definition_count = af_read_u32(header);
    const uint32_t reference_count = af_read_u32(header + 4U);
    const uint32_t property_count = af_read_u32(header + 8U);
    const uint32_t instance_count = af_read_u32(header + 12U);
    const uint32_t override_count = af_read_u32(header + 16U);
    const uint64_t reference_offset =
        AF_COMPONENT_HEADER_SIZE + (uint64_t)definition_count * AF_COMPONENT_DEFINITION_RECORD_SIZE;
    const uint64_t property_offset =
        reference_offset + (uint64_t)reference_count * AF_COMPONENT_REFERENCE_RECORD_SIZE;
    const uint64_t instance_offset =
        property_offset + (uint64_t)property_count * AF_COMPONENT_VALUE_RECORD_SIZE;
    const uint64_t override_offset =
        instance_offset + (uint64_t)instance_count * AF_COMPONENT_INSTANCE_RECORD_SIZE;
    const uint64_t expected_size =
        override_offset + (uint64_t)override_count * AF_COMPONENT_VALUE_RECORD_SIZE;
    uint64_t stack[AF_MAX_COMPONENT_NESTING] = {0};
    if (definition_count > AF_MAX_COMPONENT_DEFINITIONS ||
        reference_count > AF_MAX_COMPONENT_REFERENCES ||
        property_count > AF_MAX_COMPONENT_PROPERTIES ||
        instance_count > AF_MAX_COMPONENT_INSTANCES ||
        override_count > AF_MAX_COMPONENT_OVERRIDES || components->count != definition_count ||
        components->size != expected_size || af_read_u32(header + 20U) != 0U ||
        af_read_u64(header + 24U) != 0U) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    for (uint32_t index = 0U; index < definition_count; ++index) {
        const uint8_t *record = header + AF_COMPONENT_HEADER_SIZE +
                                (uint64_t)index * AF_COMPONENT_DEFINITION_RECORD_SIZE;
        const uint64_t id = af_read_u64(record);
        const uint32_t reference_start = af_read_u32(record + 24U);
        const uint32_t child_count = af_read_u32(record + 28U);
        const uint32_t property_start = af_read_u32(record + 32U);
        const uint32_t exposed_count = af_read_u32(record + 36U);
        if (id == 0U || !af_node_id_exists(bytes, nodes, af_read_u64(record + 8U)) ||
            !af_clip_id_exists(bytes, clips, af_read_u64(record + 16U)) ||
            reference_start > reference_count || child_count > reference_count - reference_start ||
            property_start > property_count || exposed_count > property_count - property_start ||
            !af_component_branch_valid(bytes, components, id, stack, 0U)) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t other = index + 1U; other < definition_count; ++other) {
            if (id == af_read_u64(header + AF_COMPONENT_HEADER_SIZE +
                                  (uint64_t)other * AF_COMPONENT_DEFINITION_RECORD_SIZE)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        for (uint32_t child = 0U; child < child_count; ++child) {
            const uint8_t *reference =
                header + reference_offset +
                (uint64_t)(reference_start + child) * AF_COMPONENT_REFERENCE_RECORD_SIZE;
            if (af_read_u64(reference) != id ||
                af_component_definition_record(bytes, components, af_read_u64(reference + 8U)) ==
                    NULL) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        for (uint32_t property = 0U; property < exposed_count; ++property) {
            const uint8_t *value =
                header + property_offset +
                (uint64_t)(property_start + property) * AF_COMPONENT_VALUE_RECORD_SIZE;
            if (af_read_u64(value) != id || !af_component_value_record_valid(value)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            for (uint32_t other = property + 1U; other < exposed_count; ++other) {
                const uint8_t *candidate =
                    header + property_offset +
                    (uint64_t)(property_start + other) * AF_COMPONENT_VALUE_RECORD_SIZE;
                if (af_read_u64(value + 8U) == af_read_u64(candidate + 8U)) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
            }
        }
    }
    for (uint32_t index = 0U; index < instance_count; ++index) {
        const uint8_t *record =
            header + instance_offset + (uint64_t)index * AF_COMPONENT_INSTANCE_RECORD_SIZE;
        const uint64_t id = af_read_u64(record);
        const uint8_t *definition =
            af_component_definition_record(bytes, components, af_read_u64(record + 8U));
        const uint32_t start = af_read_u32(record + 24U);
        const uint32_t count = af_read_u32(record + 28U);
        if (id == 0U || definition == NULL ||
            !af_node_id_exists(bytes, nodes, af_read_u64(record + 16U)) || start > override_count ||
            count > override_count - start || af_read_u32(record + 32U) > 1U ||
            !isfinite(af_read_f32(record + 36U)) || !isfinite(af_read_f32(record + 40U)) ||
            af_read_u32(record + 44U) != 0U) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t other = index + 1U; other < instance_count; ++other) {
            if (id == af_read_u64(header + instance_offset +
                                  (uint64_t)other * AF_COMPONENT_INSTANCE_RECORD_SIZE)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        for (uint32_t value_index = 0U; value_index < count; ++value_index) {
            const uint8_t *value = header + override_offset +
                                   (uint64_t)(start + value_index) * AF_COMPONENT_VALUE_RECORD_SIZE;
            const uint32_t property_start = af_read_u32(definition + 32U);
            const uint32_t exposed_count = af_read_u32(definition + 36U);
            bool matched = false;
            if (af_read_u64(value) != id || !af_component_value_record_valid(value)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            for (uint32_t property = 0U; property < exposed_count; ++property) {
                const uint8_t *candidate =
                    header + property_offset +
                    (uint64_t)(property_start + property) * AF_COMPONENT_VALUE_RECORD_SIZE;
                if (af_read_u64(candidate + 8U) == af_read_u64(value + 8U) &&
                    af_read_u32(candidate + 16U) == af_read_u32(value + 16U)) {
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}
