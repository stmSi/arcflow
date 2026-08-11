static void af_write_section(uint8_t *entry, uint32_t type, uint64_t offset, uint64_t size,
                             uint32_t alignment, uint32_t count) {
    af_write_u32(entry, type);
    af_write_u32(entry + 4U, 0U);
    af_write_u64(entry + 8U, offset);
    af_write_u64(entry + 16U, size);
    af_write_u32(entry + 24U, alignment);
    af_write_u32(entry + 28U, count);
}

static void af_write_components(uint8_t *section, const AfComponentData *data) {
    const uint64_t reference_offset =
        AF_COMPONENT_HEADER_SIZE +
        (uint64_t)data->definition_count * AF_COMPONENT_DEFINITION_RECORD_SIZE;
    const uint64_t property_offset =
        reference_offset + (uint64_t)data->reference_count * AF_COMPONENT_REFERENCE_RECORD_SIZE;
    const uint64_t instance_offset =
        property_offset + (uint64_t)data->property_count * AF_COMPONENT_VALUE_RECORD_SIZE;
    const uint64_t override_offset =
        instance_offset + (uint64_t)data->instance_count * AF_COMPONENT_INSTANCE_RECORD_SIZE;
    af_write_u32(section, data->definition_count);
    af_write_u32(section + 4U, data->reference_count);
    af_write_u32(section + 8U, data->property_count);
    af_write_u32(section + 12U, data->instance_count);
    af_write_u32(section + 16U, data->override_count);
    for (uint32_t index = 0U; index < data->definition_count; ++index) {
        const AfComponentDefinition *definition = &data->definitions[index];
        uint8_t *record = section + AF_COMPONENT_HEADER_SIZE +
                          (uint64_t)index * AF_COMPONENT_DEFINITION_RECORD_SIZE;
        af_write_u64(record, definition->id);
        af_write_u64(record + 8U, definition->root_node_id);
        af_write_u64(record + 16U, definition->clip_id);
        af_write_u32(record + 24U, definition->reference_start);
        af_write_u32(record + 28U, definition->reference_count);
        af_write_u32(record + 32U, definition->property_start);
        af_write_u32(record + 36U, definition->property_count);
    }
    for (uint32_t index = 0U; index < data->reference_count; ++index) {
        uint8_t *record =
            section + reference_offset + (uint64_t)index * AF_COMPONENT_REFERENCE_RECORD_SIZE;
        af_write_u64(record, data->references[index].owner_definition_id);
        af_write_u64(record + 8U, data->references[index].component_id);
    }
    for (uint32_t index = 0U; index < data->property_count; ++index) {
        const AfComponentProperty *property = &data->properties[index];
        af_write_component_value(
            section + property_offset + (uint64_t)index * AF_COMPONENT_VALUE_RECORD_SIZE,
            property->definition_id, property->id, property->type, &property->default_value);
    }
    for (uint32_t index = 0U; index < data->instance_count; ++index) {
        const AfComponentInstance *instance = &data->instances[index];
        uint8_t *record =
            section + instance_offset + (uint64_t)index * AF_COMPONENT_INSTANCE_RECORD_SIZE;
        af_write_u64(record, instance->id);
        af_write_u64(record + 8U, instance->definition_id);
        af_write_u64(record + 16U, instance->node_id);
        af_write_u32(record + 24U, instance->override_start);
        af_write_u32(record + 28U, instance->override_count);
        af_write_u32(record + 32U, instance->independent_playback ? 1U : 0U);
        af_write_f32(record + 36U, instance->playback_time);
        af_write_f32(record + 40U, instance->playback_speed);
    }
    for (uint32_t index = 0U; index < data->override_count; ++index) {
        const AfComponentOverride *override = &data->overrides[index];
        af_write_component_value(
            section + override_offset + (uint64_t)index * AF_COMPONENT_VALUE_RECORD_SIZE,
            override->instance_id, override->property_id, override->type, &override->value);
    }
}

static void af_write_state_machines(uint8_t *section, const AfStateMachineData *data) {
    const uint64_t state_offset =
        AF_STATE_MACHINE_HEADER_SIZE + (uint64_t)data->machine_count * AF_STATE_MACHINE_RECORD_SIZE;
    const uint64_t transition_offset =
        state_offset + (uint64_t)data->state_count * AF_STATE_RECORD_SIZE;
    const uint64_t input_offset =
        transition_offset + (uint64_t)data->transition_count * AF_STATE_TRANSITION_RECORD_SIZE;
    const uint64_t condition_offset =
        input_offset + (uint64_t)data->input_count * AF_STATE_INPUT_RECORD_SIZE;
    af_write_u32(section, data->machine_count);
    af_write_u32(section + 4U, data->state_count);
    af_write_u32(section + 8U, data->transition_count);
    af_write_u32(section + 12U, data->input_count);
    af_write_u32(section + 16U, data->condition_count);
    for (uint32_t index = 0U; index < data->machine_count; ++index) {
        const AfStateMachineDefinition *machine = &data->machines[index];
        uint8_t *record =
            section + AF_STATE_MACHINE_HEADER_SIZE + (uint64_t)index * AF_STATE_MACHINE_RECORD_SIZE;
        af_write_u64(record, machine->id);
        af_write_u64(record + 8U, machine->owner_component_id);
        af_write_u64(record + 16U, machine->entry_state_id);
        af_write_u32(record + 24U, machine->state_start);
        af_write_u32(record + 28U, machine->state_count);
        af_write_u32(record + 32U, machine->transition_start);
        af_write_u32(record + 36U, machine->transition_count);
    }
    for (uint32_t index = 0U; index < data->state_count; ++index) {
        const AfStateDefinition *state = &data->states[index];
        uint8_t *record = section + state_offset + (uint64_t)index * AF_STATE_RECORD_SIZE;
        af_write_u64(record, state->id);
        af_write_u64(record + 8U, state->machine_id);
        af_write_u64(record + 16U, state->clip_id);
        af_write_u64(record + 24U, state->nested_machine_id);
        af_write_u64(record + 32U, state->tag_id);
        af_write_u32(record + 40U, (uint32_t)state->type);
    }
    for (uint32_t index = 0U; index < data->transition_count; ++index) {
        const AfStateTransition *transition = &data->transitions[index];
        uint8_t *record =
            section + transition_offset + (uint64_t)index * AF_STATE_TRANSITION_RECORD_SIZE;
        af_write_u64(record, transition->source_state_id);
        af_write_u64(record + 8U, transition->target_state_id);
        af_write_u32(record + 16U, transition->condition_start);
        af_write_u32(record + 20U, transition->condition_count);
        af_write_u32(record + 24U, (uint32_t)transition->priority);
        af_write_u32(record + 28U, transition->flags);
        af_write_f32(record + 32U, transition->duration);
        af_write_f32(record + 36U, transition->exit_time);
        for (uint32_t easing = 0U; easing < 4U; ++easing) {
            af_write_f32(record + 40U + (uint64_t)easing * 4U, transition->easing[easing]);
        }
        af_write_u64(record + 56U, transition->event_id);
    }
    for (uint32_t index = 0U; index < data->input_count; ++index) {
        const AfStateInputDefinition *input = &data->inputs[index];
        uint8_t *record = section + input_offset + (uint64_t)index * AF_STATE_INPUT_RECORD_SIZE;
        af_write_u64(record, input->machine_id);
        af_write_u64(record + 8U, input->id);
        af_write_u32(record + 16U, (uint32_t)input->type);
        af_write_state_value(record + 24U, input->type, input->default_value);
    }
    for (uint32_t index = 0U; index < data->condition_count; ++index) {
        const AfTransitionCondition *condition = &data->conditions[index];
        uint8_t *record =
            section + condition_offset + (uint64_t)index * AF_STATE_CONDITION_RECORD_SIZE;
        af_write_u64(record, condition->input_id);
        af_write_u32(record + 8U, (uint32_t)condition->type);
        af_write_u32(record + 12U, (uint32_t)condition->comparison);
        af_write_state_value(record + 16U, condition->type, condition->value);
    }
}

static AfResult af_decode_components(const uint8_t *section, AfAllocator *allocator,
                                     AfComponentData *data) {
    const uint32_t definition_count = af_read_u32(section);
    const uint32_t reference_count = af_read_u32(section + 4U);
    const uint32_t property_count = af_read_u32(section + 8U);
    const uint32_t instance_count = af_read_u32(section + 12U);
    const uint32_t override_count = af_read_u32(section + 16U);
    const uint64_t reference_offset =
        AF_COMPONENT_HEADER_SIZE + (uint64_t)definition_count * AF_COMPONENT_DEFINITION_RECORD_SIZE;
    const uint64_t property_offset =
        reference_offset + (uint64_t)reference_count * AF_COMPONENT_REFERENCE_RECORD_SIZE;
    const uint64_t instance_offset =
        property_offset + (uint64_t)property_count * AF_COMPONENT_VALUE_RECORD_SIZE;
    const uint64_t override_offset =
        instance_offset + (uint64_t)instance_count * AF_COMPONENT_INSTANCE_RECORD_SIZE;
    data->definitions = af_allocate_array(
        allocator, definition_count, sizeof(AfComponentDefinition), alignof(AfComponentDefinition));
    data->references = af_allocate_array(allocator, reference_count, sizeof(AfComponentReference),
                                         alignof(AfComponentReference));
    data->properties = af_allocate_array(allocator, property_count, sizeof(AfComponentProperty),
                                         alignof(AfComponentProperty));
    data->instances = af_allocate_array(allocator, instance_count, sizeof(AfComponentInstance),
                                        alignof(AfComponentInstance));
    data->overrides = af_allocate_array(allocator, override_count, sizeof(AfComponentOverride),
                                        alignof(AfComponentOverride));
    if ((definition_count != 0U && data->definitions == NULL) ||
        (reference_count != 0U && data->references == NULL) ||
        (property_count != 0U && data->properties == NULL) ||
        (instance_count != 0U && data->instances == NULL) ||
        (override_count != 0U && data->overrides == NULL)) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    data->definition_count = definition_count;
    data->definition_capacity = definition_count;
    data->reference_count = reference_count;
    data->reference_capacity = reference_count;
    data->property_count = property_count;
    data->property_capacity = property_count;
    data->instance_count = instance_count;
    data->instance_capacity = instance_count;
    data->override_count = override_count;
    data->override_capacity = override_count;
    for (uint32_t index = 0U; index < definition_count; ++index) {
        const uint8_t *record = section + AF_COMPONENT_HEADER_SIZE +
                                (uint64_t)index * AF_COMPONENT_DEFINITION_RECORD_SIZE;
        data->definitions[index] =
            (AfComponentDefinition){.id = af_read_u64(record),
                                    .root_node_id = af_read_u64(record + 8U),
                                    .clip_id = af_read_u64(record + 16U),
                                    .reference_start = af_read_u32(record + 24U),
                                    .reference_count = af_read_u32(record + 28U),
                                    .property_start = af_read_u32(record + 32U),
                                    .property_count = af_read_u32(record + 36U)};
    }
    for (uint32_t index = 0U; index < reference_count; ++index) {
        const uint8_t *record =
            section + reference_offset + (uint64_t)index * AF_COMPONENT_REFERENCE_RECORD_SIZE;
        data->references[index] = (AfComponentReference){.owner_definition_id = af_read_u64(record),
                                                         .component_id = af_read_u64(record + 8U)};
    }
    for (uint32_t index = 0U; index < property_count; ++index) {
        const uint8_t *record =
            section + property_offset + (uint64_t)index * AF_COMPONENT_VALUE_RECORD_SIZE;
        const AfComponentPropertyType type = (AfComponentPropertyType)af_read_u32(record + 16U);
        data->properties[index] =
            (AfComponentProperty){.definition_id = af_read_u64(record),
                                  .id = af_read_u64(record + 8U),
                                  .type = type,
                                  .default_value = af_read_component_value(record, type)};
    }
    for (uint32_t index = 0U; index < instance_count; ++index) {
        const uint8_t *record =
            section + instance_offset + (uint64_t)index * AF_COMPONENT_INSTANCE_RECORD_SIZE;
        data->instances[index] =
            (AfComponentInstance){.id = af_read_u64(record),
                                  .definition_id = af_read_u64(record + 8U),
                                  .node_id = af_read_u64(record + 16U),
                                  .override_start = af_read_u32(record + 24U),
                                  .override_count = af_read_u32(record + 28U),
                                  .playback_time = af_read_f32(record + 36U),
                                  .playback_speed = af_read_f32(record + 40U),
                                  .independent_playback = af_read_u32(record + 32U) != 0U};
    }
    for (uint32_t index = 0U; index < override_count; ++index) {
        const uint8_t *record =
            section + override_offset + (uint64_t)index * AF_COMPONENT_VALUE_RECORD_SIZE;
        const AfComponentPropertyType type = (AfComponentPropertyType)af_read_u32(record + 16U);
        data->overrides[index] =
            (AfComponentOverride){.instance_id = af_read_u64(record),
                                  .property_id = af_read_u64(record + 8U),
                                  .type = type,
                                  .value = af_read_component_value(record, type)};
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_decode_state_machines(const uint8_t *section, AfAllocator *allocator,
                                         AfStateMachineData *data) {
    const uint32_t machine_count = af_read_u32(section);
    const uint32_t state_count = af_read_u32(section + 4U);
    const uint32_t transition_count = af_read_u32(section + 8U);
    const uint32_t input_count = af_read_u32(section + 12U);
    const uint32_t condition_count = af_read_u32(section + 16U);
    const uint64_t state_offset =
        AF_STATE_MACHINE_HEADER_SIZE + (uint64_t)machine_count * AF_STATE_MACHINE_RECORD_SIZE;
    const uint64_t transition_offset = state_offset + (uint64_t)state_count * AF_STATE_RECORD_SIZE;
    const uint64_t input_offset =
        transition_offset + (uint64_t)transition_count * AF_STATE_TRANSITION_RECORD_SIZE;
    const uint64_t condition_offset =
        input_offset + (uint64_t)input_count * AF_STATE_INPUT_RECORD_SIZE;
    data->machines = af_allocate_array(allocator, machine_count, sizeof(AfStateMachineDefinition),
                                       alignof(AfStateMachineDefinition));
    data->states = af_allocate_array(allocator, state_count, sizeof(AfStateDefinition),
                                     alignof(AfStateDefinition));
    data->transitions = af_allocate_array(allocator, transition_count, sizeof(AfStateTransition),
                                          alignof(AfStateTransition));
    data->inputs = af_allocate_array(allocator, input_count, sizeof(AfStateInputDefinition),
                                     alignof(AfStateInputDefinition));
    data->conditions = af_allocate_array(allocator, condition_count, sizeof(AfTransitionCondition),
                                         alignof(AfTransitionCondition));
    if ((machine_count != 0U && data->machines == NULL) ||
        (state_count != 0U && data->states == NULL) ||
        (transition_count != 0U && data->transitions == NULL) ||
        (input_count != 0U && data->inputs == NULL) ||
        (condition_count != 0U && data->conditions == NULL)) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    data->machine_count = machine_count;
    data->machine_capacity = machine_count;
    data->state_count = state_count;
    data->state_capacity = state_count;
    data->transition_count = transition_count;
    data->transition_capacity = transition_count;
    data->input_count = input_count;
    data->input_capacity = input_count;
    data->condition_count = condition_count;
    data->condition_capacity = condition_count;
    for (uint32_t index = 0U; index < machine_count; ++index) {
        const uint8_t *record =
            section + AF_STATE_MACHINE_HEADER_SIZE + (uint64_t)index * AF_STATE_MACHINE_RECORD_SIZE;
        data->machines[index] =
            (AfStateMachineDefinition){.id = af_read_u64(record),
                                       .owner_component_id = af_read_u64(record + 8U),
                                       .entry_state_id = af_read_u64(record + 16U),
                                       .state_start = af_read_u32(record + 24U),
                                       .state_count = af_read_u32(record + 28U),
                                       .transition_start = af_read_u32(record + 32U),
                                       .transition_count = af_read_u32(record + 36U)};
    }
    for (uint32_t index = 0U; index < state_count; ++index) {
        const uint8_t *record = section + state_offset + (uint64_t)index * AF_STATE_RECORD_SIZE;
        data->states[index] = (AfStateDefinition){.id = af_read_u64(record),
                                                  .machine_id = af_read_u64(record + 8U),
                                                  .clip_id = af_read_u64(record + 16U),
                                                  .nested_machine_id = af_read_u64(record + 24U),
                                                  .tag_id = af_read_u64(record + 32U),
                                                  .type = (AfStateType)af_read_u32(record + 40U)};
    }
    for (uint32_t index = 0U; index < transition_count; ++index) {
        const uint8_t *record =
            section + transition_offset + (uint64_t)index * AF_STATE_TRANSITION_RECORD_SIZE;
        AfStateTransition *transition = &data->transitions[index];
        *transition = (AfStateTransition){.source_state_id = af_read_u64(record),
                                          .target_state_id = af_read_u64(record + 8U),
                                          .condition_start = af_read_u32(record + 16U),
                                          .condition_count = af_read_u32(record + 20U),
                                          .priority = (int32_t)af_read_u32(record + 24U),
                                          .flags = af_read_u32(record + 28U),
                                          .duration = af_read_f32(record + 32U),
                                          .exit_time = af_read_f32(record + 36U),
                                          .event_id = af_read_u64(record + 56U)};
        for (uint32_t easing = 0U; easing < 4U; ++easing) {
            transition->easing[easing] = af_read_f32(record + 40U + (uint64_t)easing * 4U);
        }
    }
    for (uint32_t index = 0U; index < input_count; ++index) {
        const uint8_t *record =
            section + input_offset + (uint64_t)index * AF_STATE_INPUT_RECORD_SIZE;
        const AfStateInputType type = (AfStateInputType)af_read_u32(record + 16U);
        data->inputs[index] =
            (AfStateInputDefinition){.machine_id = af_read_u64(record),
                                     .id = af_read_u64(record + 8U),
                                     .type = type,
                                     .default_value = af_read_state_value(record + 24U, type)};
    }
    for (uint32_t index = 0U; index < condition_count; ++index) {
        const uint8_t *record =
            section + condition_offset + (uint64_t)index * AF_STATE_CONDITION_RECORD_SIZE;
        const AfStateInputType type = (AfStateInputType)af_read_u32(record + 8U);
        data->conditions[index] =
            (AfTransitionCondition){.input_id = af_read_u64(record),
                                    .type = type,
                                    .comparison = (AfStateComparison)af_read_u32(record + 12U),
                                    .value = af_read_state_value(record + 16U, type)};
    }
    return AF_RESULT_SUCCESS;
}
