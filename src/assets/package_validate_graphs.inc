static const uint8_t *af_package_machine_record(const uint8_t *header, uint32_t machine_count,
                                                uint64_t id) {
    for (uint32_t index = 0U; index < machine_count; ++index) {
        const uint8_t *record =
            header + AF_STATE_MACHINE_HEADER_SIZE + (uint64_t)index * AF_STATE_MACHINE_RECORD_SIZE;
        if (af_read_u64(record) == id) {
            return record;
        }
    }
    return NULL;
}

static const uint8_t *af_package_state_record(const uint8_t *header, uint32_t machine_count,
                                              uint32_t state_count, uint64_t id) {
    const uint64_t offset =
        AF_STATE_MACHINE_HEADER_SIZE + (uint64_t)machine_count * AF_STATE_MACHINE_RECORD_SIZE;
    for (uint32_t index = 0U; index < state_count; ++index) {
        const uint8_t *record = header + offset + (uint64_t)index * AF_STATE_RECORD_SIZE;
        if (af_read_u64(record) == id) {
            return record;
        }
    }
    return NULL;
}

static bool af_package_state_nested_valid(const uint8_t *header, uint32_t machine_count,
                                          uint32_t state_count, uint64_t machine_id,
                                          uint64_t *stack, uint32_t depth) {
    const uint8_t *machine = af_package_machine_record(header, machine_count, machine_id);
    const uint64_t state_offset =
        AF_STATE_MACHINE_HEADER_SIZE + (uint64_t)machine_count * AF_STATE_MACHINE_RECORD_SIZE;
    if (machine == NULL || depth >= AF_MAX_NESTED_STATE_MACHINES) {
        return false;
    }
    for (uint32_t index = 0U; index < depth; ++index) {
        if (stack[index] == machine_id) {
            return false;
        }
    }
    stack[depth] = machine_id;
    const uint32_t start = af_read_u32(machine + 24U);
    const uint32_t count = af_read_u32(machine + 28U);
    if (start > state_count || count > state_count - start) {
        return false;
    }
    for (uint32_t index = 0U; index < count; ++index) {
        const uint8_t *state =
            header + state_offset + (uint64_t)(start + index) * AF_STATE_RECORD_SIZE;
        if (af_read_u32(state + 40U) == AF_STATE_NESTED_MACHINE &&
            !af_package_state_nested_valid(header, machine_count, state_count,
                                           af_read_u64(state + 24U), stack, depth + 1U)) {
            return false;
        }
    }
    return true;
}

static bool af_package_state_input_value_valid(const uint8_t *record, uint32_t type,
                                               uint32_t value_offset) {
    if (type > AF_STATE_INPUT_TRIGGER) {
        return false;
    }
    if (type == AF_STATE_INPUT_BOOLEAN || type == AF_STATE_INPUT_TRIGGER) {
        return af_read_u32(record + value_offset) <= 1U;
    }
    return type != AF_STATE_INPUT_FLOAT || isfinite(af_read_f32(record + value_offset));
}

static AfResult af_validate_state_machines(const uint8_t *bytes,
                                           const AfSectionView *state_machines,
                                           const AfSectionView *clips,
                                           const AfSectionView *components) {
    const uint8_t *header = bytes + state_machines->offset;
    const uint32_t machine_count = af_read_u32(header);
    const uint32_t state_count = af_read_u32(header + 4U);
    const uint32_t transition_count = af_read_u32(header + 8U);
    const uint32_t input_count = af_read_u32(header + 12U);
    const uint32_t condition_count = af_read_u32(header + 16U);
    const uint64_t state_offset =
        AF_STATE_MACHINE_HEADER_SIZE + (uint64_t)machine_count * AF_STATE_MACHINE_RECORD_SIZE;
    const uint64_t transition_offset = state_offset + (uint64_t)state_count * AF_STATE_RECORD_SIZE;
    const uint64_t input_offset =
        transition_offset + (uint64_t)transition_count * AF_STATE_TRANSITION_RECORD_SIZE;
    const uint64_t condition_offset =
        input_offset + (uint64_t)input_count * AF_STATE_INPUT_RECORD_SIZE;
    const uint64_t expected_size =
        condition_offset + (uint64_t)condition_count * AF_STATE_CONDITION_RECORD_SIZE;
    uint64_t stack[AF_MAX_NESTED_STATE_MACHINES] = {0};
    if (machine_count > AF_MAX_STATE_MACHINES || state_count > AF_MAX_STATES ||
        transition_count > AF_MAX_TRANSITIONS || input_count > AF_MAX_STATE_INPUTS ||
        condition_count > AF_MAX_TRANSITION_CONDITIONS || state_machines->count != machine_count ||
        state_machines->size != expected_size || af_read_u32(header + 20U) != 0U ||
        af_read_u64(header + 24U) != 0U) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    for (uint32_t index = 0U; index < input_count; ++index) {
        const uint8_t *input = header + input_offset + (uint64_t)index * AF_STATE_INPUT_RECORD_SIZE;
        const uint32_t type = af_read_u32(input + 16U);
        if (af_package_machine_record(header, machine_count, af_read_u64(input)) == NULL ||
            af_read_u64(input + 8U) == 0U || af_read_u32(input + 20U) != 0U ||
            !af_package_state_input_value_valid(input, type, 24U) ||
            af_read_u32(input + 28U) != 0U) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t other = index + 1U; other < input_count; ++other) {
            if (af_read_u64(input + 8U) ==
                af_read_u64(header + input_offset + (uint64_t)other * AF_STATE_INPUT_RECORD_SIZE +
                            8U)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    for (uint32_t index = 0U; index < condition_count; ++index) {
        const uint8_t *condition =
            header + condition_offset + (uint64_t)index * AF_STATE_CONDITION_RECORD_SIZE;
        const uint64_t input_id = af_read_u64(condition);
        const uint32_t type = af_read_u32(condition + 8U);
        const uint32_t comparison = af_read_u32(condition + 12U);
        const uint8_t *matched_input = NULL;
        for (uint32_t input_index = 0U; input_index < input_count; ++input_index) {
            const uint8_t *input =
                header + input_offset + (uint64_t)input_index * AF_STATE_INPUT_RECORD_SIZE;
            if (af_read_u64(input + 8U) == input_id) {
                matched_input = input;
                break;
            }
        }
        if (matched_input == NULL || af_read_u32(matched_input + 16U) != type ||
            comparison > AF_STATE_COMPARE_GREATER_EQUAL ||
            ((type == AF_STATE_INPUT_BOOLEAN || type == AF_STATE_INPUT_TRIGGER) &&
             comparison > AF_STATE_COMPARE_NOT_EQUAL) ||
            !af_package_state_input_value_valid(condition, type, 16U) ||
            af_read_u32(condition + 20U) != 0U || af_read_u64(condition + 24U) != 0U) {
            return AF_RESULT_INVALID_PACKAGE;
        }
    }
    for (uint32_t index = 0U; index < machine_count; ++index) {
        const uint8_t *machine =
            header + AF_STATE_MACHINE_HEADER_SIZE + (uint64_t)index * AF_STATE_MACHINE_RECORD_SIZE;
        const uint64_t id = af_read_u64(machine);
        const uint64_t component_id = af_read_u64(machine + 8U);
        const uint8_t *entry =
            af_package_state_record(header, machine_count, state_count, af_read_u64(machine + 16U));
        const uint32_t start = af_read_u32(machine + 24U);
        const uint32_t count = af_read_u32(machine + 28U);
        const uint32_t transition_start = af_read_u32(machine + 32U);
        const uint32_t machine_transition_count = af_read_u32(machine + 36U);
        if (id == 0U || entry == NULL || af_read_u64(entry + 8U) != id || start > state_count ||
            count > state_count - start || transition_start > transition_count ||
            machine_transition_count > transition_count - transition_start ||
            (component_id != 0U &&
             af_component_definition_record(bytes, components, component_id) == NULL) ||
            !af_package_state_nested_valid(header, machine_count, state_count, id, stack, 0U)) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t other = index + 1U; other < machine_count; ++other) {
            if (id == af_read_u64(header + AF_STATE_MACHINE_HEADER_SIZE +
                                  (uint64_t)other * AF_STATE_MACHINE_RECORD_SIZE)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        for (uint32_t state_index = 0U; state_index < count; ++state_index) {
            const uint8_t *state =
                header + state_offset + (uint64_t)(start + state_index) * AF_STATE_RECORD_SIZE;
            const uint32_t type = af_read_u32(state + 40U);
            if (af_read_u64(state) == 0U || af_read_u64(state + 8U) != id ||
                type > AF_STATE_NESTED_MACHINE || af_read_u32(state + 44U) != 0U ||
                (type == AF_STATE_ANIMATION &&
                 (af_read_u64(state + 16U) == 0U ||
                  !af_clip_id_exists(bytes, clips, af_read_u64(state + 16U)))) ||
                (type == AF_STATE_NESTED_MACHINE &&
                 af_package_machine_record(header, machine_count, af_read_u64(state + 24U)) ==
                     NULL)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            for (uint32_t other = 0U; other < state_count; ++other) {
                if (start + state_index != other &&
                    af_read_u64(state) == af_read_u64(header + state_offset +
                                                      (uint64_t)other * AF_STATE_RECORD_SIZE)) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
            }
        }
        for (uint32_t transition_index = 0U; transition_index < machine_transition_count;
             ++transition_index) {
            const uint8_t *transition =
                header + transition_offset +
                (uint64_t)(transition_start + transition_index) * AF_STATE_TRANSITION_RECORD_SIZE;
            const uint64_t source_id = af_read_u64(transition);
            const uint8_t *source =
                source_id == 0U
                    ? NULL
                    : af_package_state_record(header, machine_count, state_count, source_id);
            const uint8_t *target = af_package_state_record(header, machine_count, state_count,
                                                            af_read_u64(transition + 8U));
            const uint32_t condition_start = af_read_u32(transition + 16U);
            const uint32_t transition_condition_count = af_read_u32(transition + 20U);
            const uint32_t flags = af_read_u32(transition + 28U);
            if ((source_id != 0U && (source == NULL || af_read_u64(source + 8U) != id)) ||
                target == NULL || af_read_u64(target + 8U) != id ||
                condition_start > condition_count ||
                transition_condition_count > condition_count - condition_start || flags > 7U ||
                !isfinite(af_read_f32(transition + 32U)) || af_read_f32(transition + 32U) < 0.0F ||
                !isfinite(af_read_f32(transition + 36U)) || af_read_f32(transition + 36U) < 0.0F ||
                ((flags & AF_TRANSITION_REQUIRE_EVENT) != 0U &&
                 af_read_u64(transition + 56U) == 0U)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            for (uint32_t easing = 40U; easing <= 52U; easing += 4U) {
                if (!isfinite(af_read_f32(transition + easing))) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
            }
            for (uint32_t condition_index = 0U; condition_index < transition_condition_count;
                 ++condition_index) {
                const uint8_t *condition =
                    header + condition_offset +
                    (uint64_t)(condition_start + condition_index) * AF_STATE_CONDITION_RECORD_SIZE;
                bool owned = false;
                for (uint32_t input_index = 0U; input_index < input_count; ++input_index) {
                    const uint8_t *input =
                        header + input_offset + (uint64_t)input_index * AF_STATE_INPUT_RECORD_SIZE;
                    if (af_read_u64(input + 8U) == af_read_u64(condition) &&
                        af_read_u64(input) == id) {
                        owned = true;
                        break;
                    }
                }
                if (!owned) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_validate_records(const uint8_t *bytes, const AfSectionView *sections,
                                    uint32_t section_count) {
    const AfSectionView *nodes = af_find_section(sections, section_count, AF_SECTION_NODES);
    const AfSectionView *clips = af_find_section(sections, section_count, AF_SECTION_CLIPS);
    const AfSectionView *tracks = af_find_section(sections, section_count, AF_SECTION_TRACKS);
    const AfSectionView *keys = af_find_section(sections, section_count, AF_SECTION_KEYFRAMES);
    const AfSectionView *strings = af_find_section(sections, section_count, AF_SECTION_STRINGS);
    const AfSectionView *paths = af_find_section(sections, section_count, AF_SECTION_PATHS);
    const AfSectionView *path_commands =
        af_find_section(sections, section_count, AF_SECTION_PATH_COMMANDS);
    const AfSectionView *styles = af_find_section(sections, section_count, AF_SECTION_STYLES);
    const AfSectionView *layouts = af_find_section(sections, section_count, AF_SECTION_LAYOUTS);
    const AfSectionView *components =
        af_find_section(sections, section_count, AF_SECTION_COMPONENTS);
    const AfSectionView *state_machines =
        af_find_section(sections, section_count, AF_SECTION_STATE_MACHINES);
    const AfSectionView *particles = af_find_section(sections, section_count, AF_SECTION_PARTICLES);
    const AfSectionView *images = af_find_section(sections, section_count, AF_SECTION_IMAGES);
    uint32_t index = 0U;
    if (section_count != AF_SECTION_COUNT || nodes == NULL || clips == NULL || tracks == NULL ||
        keys == NULL || strings == NULL || paths == NULL || path_commands == NULL ||
        styles == NULL || layouts == NULL || components == NULL || state_machines == NULL ||
        particles == NULL || images == NULL || nodes->count > AF_MAX_NODES ||
        clips->count > AF_MAX_CLIPS || tracks->count > AF_MAX_TRACKS ||
        keys->count > AF_MAX_KEYFRAMES || paths->count > AF_MAX_PATHS ||
        path_commands->count > AF_MAX_PATH_COMMANDS || styles->count > AF_MAX_STYLES ||
        layouts->count > AF_MAX_LAYOUT_NODES || particles->count > AF_MAX_PARTICLE_EMITTERS ||
        nodes->size != (uint64_t)nodes->count * AF_NODE_RECORD_SIZE ||
        clips->size != (uint64_t)clips->count * AF_CLIP_RECORD_SIZE ||
        tracks->size != (uint64_t)tracks->count * AF_TRACK_RECORD_SIZE ||
        keys->size != (uint64_t)keys->count * AF_KEYFRAME_RECORD_SIZE ||
        paths->size != (uint64_t)paths->count * AF_PATH_RECORD_SIZE ||
        path_commands->size != (uint64_t)path_commands->count * AF_PATH_COMMAND_RECORD_SIZE ||
        styles->size != (uint64_t)styles->count * AF_STYLE_RECORD_SIZE ||
        layouts->size != AF_LAYOUT_HEADER_SIZE + (uint64_t)layouts->count * AF_LAYOUT_RECORD_SIZE ||
        particles->size !=
            AF_PARTICLE_HEADER_SIZE + (uint64_t)particles->count * AF_PARTICLE_RECORD_SIZE ||
        strings->size == 0U || strings->size > 1048576U ||
        strings->count != (uint32_t)strings->size ||
        bytes[strings->offset + strings->size - 1U] != 0U) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    {
        const uint8_t *header = bytes + layouts->offset;
        const float reference_x = af_read_f32(header);
        const float reference_y = af_read_f32(header + 4U);
        const float target_x = af_read_f32(header + 8U);
        const float target_y = af_read_f32(header + 12U);
        const float safe_left = af_read_f32(header + 20U);
        const float safe_top = af_read_f32(header + 24U);
        const float safe_right = af_read_f32(header + 28U);
        const float safe_bottom = af_read_f32(header + 32U);
        if (!isfinite(reference_x) || !isfinite(reference_y) || !isfinite(target_x) ||
            !isfinite(target_y) || !isfinite(safe_left) || !isfinite(safe_top) ||
            !isfinite(safe_right) || !isfinite(safe_bottom) || reference_x <= 0.0F ||
            reference_y <= 0.0F || target_x <= 0.0F || target_y <= 0.0F || safe_left < 0.0F ||
            safe_top < 0.0F || safe_right < 0.0F || safe_bottom < 0.0F ||
            safe_left + safe_right >= target_x || safe_top + safe_bottom >= target_y ||
            af_read_u32(header + 16U) > AF_CANVAS_SCALE_STRETCH ||
            af_read_u32(header + 36U) != 0U || af_read_u64(header + 40U) != 0U) {
            return AF_RESULT_INVALID_PACKAGE;
        }
    }
    if (af_validate_components(bytes, components, nodes, clips) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    if (af_validate_state_machines(bytes, state_machines, clips, components) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    {
        const uint8_t *header = bytes + particles->offset;
        const float spawn_rate_scale = af_read_f32(header + 4U);
        const float simulation_frequency_scale = af_read_f32(header + 8U);
        const float distance_start = af_read_f32(header + 12U);
        const float distance_end = af_read_f32(header + 16U);
        if (af_read_u32(header) == 0U || af_read_u32(header) > AF_MAX_PARTICLES ||
            !isfinite(spawn_rate_scale) || spawn_rate_scale < 0.0F || spawn_rate_scale > 1.0F ||
            !isfinite(simulation_frequency_scale) || simulation_frequency_scale <= 0.0F ||
            simulation_frequency_scale > 1.0F || !isfinite(distance_start) ||
            !isfinite(distance_end) || distance_start < 0.0F || distance_end < distance_start ||
            af_read_u32(header + 20U) > 1U) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t reserved = 24U; reserved < AF_PARTICLE_HEADER_SIZE; reserved += 4U) {
            if (af_read_u32(header + reserved) != 0U) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    {
        uint64_t total_capacity = 0U;
        for (index = 0U; index < particles->count; ++index) {
            const uint8_t *record = bytes + particles->offset + AF_PARTICLE_HEADER_SIZE +
                                    (uint64_t)index * AF_PARTICLE_RECORD_SIZE;
            if (!af_particle_record_valid(record, bytes, nodes, paths)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            total_capacity += af_read_u32(record + 32U);
            if (total_capacity > AF_MAX_PARTICLES) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            for (uint32_t other = index + 1U; other < particles->count; ++other) {
                const uint8_t *candidate = bytes + particles->offset + AF_PARTICLE_HEADER_SIZE +
                                           (uint64_t)other * AF_PARTICLE_RECORD_SIZE;
                if (af_read_u64(record) == af_read_u64(candidate) ||
                    af_read_u64(record + 8U) == af_read_u64(candidate + 8U)) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
            }
            if (af_read_u64(record + 328U) != 0U) {
                bool found = false;
                for (uint32_t target = 0U; target < particles->count; ++target) {
                    const uint8_t *candidate = bytes + particles->offset + AF_PARTICLE_HEADER_SIZE +
                                               (uint64_t)target * AF_PARTICLE_RECORD_SIZE;
                    if (af_read_u64(candidate) == af_read_u64(record + 328U)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
            }
        }
    }
    for (index = 0U; index < layouts->count; ++index) {
        const uint8_t *record = bytes + layouts->offset + AF_LAYOUT_HEADER_SIZE +
                                (uint64_t)index * AF_LAYOUT_RECORD_SIZE;
        const AfLayoutNode layout = af_read_layout(record);
        if (af_layout_node_validate(&layout) != AF_RESULT_SUCCESS ||
            !af_node_id_exists(bytes, nodes, layout.node_id)) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t reserved = 108U; reserved < AF_LAYOUT_RECORD_SIZE; reserved += 4U) {
            if (af_read_u32(record + reserved) != 0U) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        for (uint32_t other = index + 1U; other < layouts->count; ++other) {
            const uint8_t *candidate = bytes + layouts->offset + AF_LAYOUT_HEADER_SIZE +
                                       (uint64_t)other * AF_LAYOUT_RECORD_SIZE;
            if (layout.node_id == af_read_u64(candidate)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    for (index = 0U; index < styles->count; ++index) {
        const uint8_t *record = bytes + styles->offset + (uint64_t)index * AF_STYLE_RECORD_SIZE;
        const AfStyle style = af_read_style(record);
        uint64_t parent = style.parent_style_id;
        uint32_t preset_count = style.node_id == 0U ? 1U : 0U;
        if (af_style_validate(&style) != AF_RESULT_SUCCESS || af_read_u32(record + 36U) > 1U ||
            (style.node_id != 0U && !af_node_id_exists(bytes, nodes, style.node_id))) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t other = index + 1U; other < styles->count; ++other) {
            const uint8_t *candidate =
                bytes + styles->offset + (uint64_t)other * AF_STYLE_RECORD_SIZE;
            const uint64_t candidate_node = af_read_u64(candidate + 8U);
            if (style.id == af_read_u64(candidate) ||
                (style.node_id != 0U && style.node_id == candidate_node)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            if (candidate_node == 0U) {
                ++preset_count;
            }
        }
        if (preset_count > AF_MAX_STYLE_PRESETS) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t depth = 0U; parent != 0U; ++depth) {
            bool found = false;
            if (parent == style.id || depth >= AF_MAX_STYLE_PRESETS) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            for (uint32_t candidate_index = 0U; candidate_index < styles->count;
                 ++candidate_index) {
                const uint8_t *candidate =
                    bytes + styles->offset + (uint64_t)candidate_index * AF_STYLE_RECORD_SIZE;
                if (af_read_u64(candidate) == parent) {
                    parent = af_read_u64(candidate + 16U);
                    found = true;
                    break;
                }
            }
            if (!found) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    for (index = 0U; index < nodes->count; ++index) {
        const uint8_t *record = bytes + nodes->offset + (uint64_t)index * AF_NODE_RECORD_SIZE;
        const uint64_t id = af_read_u64(record);
        const uint64_t parent_id = af_read_u64(record + 8U);
        const uint32_t type = af_read_u32(record + 16U);
        uint32_t other = 0U;
        uint32_t float_offset = 0U;
        uint64_t ancestor = parent_id;
        uint32_t depth = 0U;
        if (id == 0U || id == AF_INVALID_NODE_ID || type > AF_NODE_IMAGE ||
            (parent_id != AF_INVALID_NODE_ID && !af_node_id_exists(bytes, nodes, parent_id)) ||
            af_read_u32(record + 20U) > 1U || af_read_f32(record + 44U) < 0.0F ||
            af_read_f32(record + 48U) < 0.0F || af_read_f32(record + 52U) < 0.0F ||
            af_read_f32(record + 52U) > 1.0F || af_read_f32(record + 56U) < 0.0F ||
            af_read_f32(record + 56U) > 1.0F || af_read_f32(record + 60U) < 0.0F ||
            af_read_f32(record + 60U) > 1.0F || af_read_f32(record + 64U) < 0.0F ||
            af_read_f32(record + 64U) > 1.0F || af_read_f32(record + 68U) < 0.0F ||
            af_read_f32(record + 68U) > 1.0F || af_read_f32(record + 72U) < 0.0F) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        if (type == AF_NODE_PATH && !af_path_node_exists(bytes, paths, id)) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (float_offset = 24U; float_offset <= 72U; float_offset += 4U) {
            if (!isfinite(af_read_f32(record + float_offset))) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        for (other = index + 1U; other < nodes->count; ++other) {
            if (id == af_read_u64(bytes + nodes->offset + (uint64_t)other * AF_NODE_RECORD_SIZE)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        while (ancestor != AF_INVALID_NODE_ID && depth++ <= AF_MAX_HIERARCHY_DEPTH) {
            uint32_t parent_index = 0U;
            bool found = false;
            if (ancestor == id) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            for (parent_index = 0U; parent_index < nodes->count; ++parent_index) {
                const uint8_t *parent =
                    bytes + nodes->offset + (uint64_t)parent_index * AF_NODE_RECORD_SIZE;
                if (af_read_u64(parent) == ancestor) {
                    ancestor = af_read_u64(parent + 8U);
                    found = true;
                    break;
                }
            }
            if (!found) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        if (ancestor != AF_INVALID_NODE_ID) {
            return AF_RESULT_INVALID_PACKAGE;
        }
    }
    for (index = 0U; index < clips->count; ++index) {
        const uint8_t *record = bytes + clips->offset + (uint64_t)index * AF_CLIP_RECORD_SIZE;
        const uint32_t start = af_read_u32(record + 12U);
        const uint32_t count = af_read_u32(record + 16U);
        uint32_t other = 0U;
        if (af_read_u64(record) == 0U || !isfinite(af_read_f32(record + 8U)) ||
            af_read_f32(record + 8U) < 0.0F || start > tracks->count ||
            count > tracks->count - start || af_read_u32(record + 20U) > 1U) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (other = index + 1U; other < clips->count; ++other) {
            if (af_read_u64(record) ==
                af_read_u64(bytes + clips->offset + (uint64_t)other * AF_CLIP_RECORD_SIZE)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    for (index = 0U; index < tracks->count; ++index) {
        const uint8_t *record = bytes + tracks->offset + (uint64_t)index * AF_TRACK_RECORD_SIZE;
        const uint32_t start = af_read_u32(record + 20U);
        const uint32_t count = af_read_u32(record + 24U);
        uint32_t key_index = 0U;
        uint32_t other = 0U;
        float last_time = -INFINITY;
        if (af_read_u64(record) == 0U || af_read_u32(record + 16U) > AF_PROPERTY_PATH_CONTROL_OUT ||
            !af_track_target_valid(bytes, nodes, paths, path_commands, af_read_u64(record + 8U),
                                   (AfPropertyType)af_read_u32(record + 16U),
                                   af_read_u32(record + 28U)) ||
            start > keys->count || count > keys->count - start) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (other = index + 1U; other < tracks->count; ++other) {
            if (af_read_u64(record) ==
                af_read_u64(bytes + tracks->offset + (uint64_t)other * AF_TRACK_RECORD_SIZE)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        for (key_index = 0U; key_index < count; ++key_index) {
            const uint8_t *key =
                bytes + keys->offset + (uint64_t)(start + key_index) * AF_KEYFRAME_RECORD_SIZE;
            const float time = af_read_f32(key);
            const uint32_t interpolation = af_read_u32(key + 20U);
            uint32_t component = 0U;
            if (!isfinite(time) || time < last_time ||
                interpolation > AF_INTERPOLATION_CUBIC_BEZIER) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            for (component = 0U; component < 4U; ++component) {
                if (!isfinite(af_read_f32(key + 4U + component * 4U)) ||
                    !isfinite(af_read_f32(key + 24U + component * 4U))) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
            }
            if (interpolation == AF_INTERPOLATION_CUBIC_BEZIER &&
                (af_read_f32(key + 24U) < 0.0F || af_read_f32(key + 24U) > 1.0F ||
                 af_read_f32(key + 32U) < 0.0F || af_read_f32(key + 32U) > 1.0F)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            last_time = time;
        }
    }
    {
        uint32_t covered_commands = 0U;
        for (index = 0U; index < paths->count; ++index) {
            const uint8_t *record = bytes + paths->offset + (uint64_t)index * AF_PATH_RECORD_SIZE;
            const uint64_t id = af_read_u64(record);
            const uint64_t node_id = af_read_u64(record + 8U);
            const uint32_t start = af_read_u32(record + 16U);
            const uint32_t count = af_read_u32(record + 20U);
            bool contour_open = false;
            bool has_segment = false;
            if (id == 0U || node_id == 0U || af_read_u32(record + 24U) > AF_PATH_FILL_EVEN_ODD ||
                af_read_u32(record + 28U) != 0U || af_read_u64(record + 32U) == 0U || count == 0U ||
                count > AF_MAX_PATH_COMMANDS_PER_PATH || start > path_commands->count ||
                count > path_commands->count - start ||
                af_node_type_for_id(bytes, nodes, node_id) != AF_NODE_PATH) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            for (uint32_t other = index + 1U; other < paths->count; ++other) {
                const uint8_t *candidate =
                    bytes + paths->offset + (uint64_t)other * AF_PATH_RECORD_SIZE;
                const uint32_t candidate_start = af_read_u32(candidate + 16U);
                const uint32_t candidate_count = af_read_u32(candidate + 20U);
                if (id == af_read_u64(candidate) || node_id == af_read_u64(candidate + 8U) ||
                    ((uint64_t)start + count > candidate_start &&
                     (uint64_t)candidate_start + candidate_count > start)) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
            }
            for (uint32_t command_index = 0U; command_index < count; ++command_index) {
                const uint8_t *command =
                    bytes + path_commands->offset +
                    (uint64_t)(start + command_index) * AF_PATH_COMMAND_RECORD_SIZE;
                const uint32_t type = af_read_u32(command);
                if (type > AF_PATH_CLOSE || af_read_u32(command + 4U) != 0U) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
                for (uint32_t float_offset = 8U; float_offset < AF_PATH_COMMAND_RECORD_SIZE;
                     float_offset += 4U) {
                    const float value = af_read_f32(command + float_offset);
                    if (!isfinite(value) || fabsf(value) > 1000000.0F) {
                        return AF_RESULT_INVALID_PACKAGE;
                    }
                }
                if (type == AF_PATH_MOVE) {
                    if (contour_open && !has_segment) {
                        return AF_RESULT_INVALID_PACKAGE;
                    }
                    contour_open = true;
                    has_segment = false;
                } else if (type == AF_PATH_LINE || type == AF_PATH_CUBIC) {
                    if (!contour_open) {
                        return AF_RESULT_INVALID_PACKAGE;
                    }
                    has_segment = true;
                } else {
                    if (!contour_open || !has_segment) {
                        return AF_RESULT_INVALID_PACKAGE;
                    }
                    contour_open = false;
                    has_segment = false;
                }
            }
            if (contour_open && !has_segment) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            covered_commands += count;
        }
        if (covered_commands != path_commands->count) {
            return AF_RESULT_INVALID_PACKAGE;
        }
    }
    return af_validate_images(bytes, images, nodes);
}

AfResult af_package_validate(const void *data, size_t size) {
    AfSectionView sections[AF_PACKAGE_MAX_SECTIONS] = {0};
    uint32_t section_count = 0U;
    const uint8_t *bytes = data;
    AfResult result = AF_RESULT_SUCCESS;
    if (data == NULL || size > AF_PACKAGE_MAX_SIZE || size < AF_PACKAGE_HEADER_SIZE) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    result = af_read_sections(bytes, size, sections, &section_count);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    if (af_read_u32(bytes + 48U) !=
        af_checksum(bytes + AF_PACKAGE_HEADER_SIZE, size - AF_PACKAGE_HEADER_SIZE)) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    return af_validate_records(bytes, sections, section_count);
}
