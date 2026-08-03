#include "component/component.h"

#include <math.h>
#include <string.h>

static const AfComponentDefinition *af_component_definition(const AfComponentData *data,
                                                            uint64_t id) {
    if (data == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->definition_count; ++index) {
        if (data->definitions[index].id == id) {
            return &data->definitions[index];
        }
    }
    return NULL;
}

static const AfComponentInstance *af_component_instance(const AfComponentData *data, uint64_t id) {
    if (data == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->instance_count; ++index) {
        if (data->instances[index].id == id) {
            return &data->instances[index];
        }
    }
    return NULL;
}

static const AfAnimationClip *af_component_clip(const AfAnimationData *animation, uint64_t id) {
    if (animation == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < animation->clip_count; ++index) {
        if (animation->clips[index].id == id) {
            return &animation->clips[index];
        }
    }
    return NULL;
}

static bool af_component_value_valid(AfComponentPropertyType type, const AfComponentValue *value) {
    if (type > AF_COMPONENT_PROPERTY_TEXT || value == NULL) {
        return false;
    }
    if (type == AF_COMPONENT_PROPERTY_NUMBER) {
        return isfinite(value->number);
    }
    if (type == AF_COMPONENT_PROPERTY_COLOR) {
        return isfinite(value->color.r) && isfinite(value->color.g) && isfinite(value->color.b) &&
               isfinite(value->color.a) && value->color.r >= 0.0F && value->color.r <= 1.0F &&
               value->color.g >= 0.0F && value->color.g <= 1.0F && value->color.b >= 0.0F &&
               value->color.b <= 1.0F && value->color.a >= 0.0F && value->color.a <= 1.0F;
    }
    if (type == AF_COMPONENT_PROPERTY_IMAGE) {
        return value->image_id != 0U;
    }
    if (type == AF_COMPONENT_PROPERTY_TEXT) {
        return memchr(value->text, '\0', AF_COMPONENT_TEXT_CAPACITY) != NULL;
    }
    return value->boolean == false || value->boolean == true;
}

static bool af_component_range_valid(uint32_t start, uint32_t count, uint32_t total) {
    return start <= total && count <= total - start;
}

static AfResult af_component_visit(const AfComponentData *data, uint32_t definition_index,
                                   uint8_t *marks, uint32_t depth) {
    const AfComponentDefinition *definition = &data->definitions[definition_index];
    if (depth > AF_MAX_COMPONENT_NESTING || marks[definition_index] == 1U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (marks[definition_index] == 2U) {
        return AF_RESULT_SUCCESS;
    }
    marks[definition_index] = 1U;
    for (uint32_t index = 0U; index < definition->reference_count; ++index) {
        const AfComponentReference *reference =
            &data->references[definition->reference_start + index];
        uint32_t child = 0U;
        for (; child < data->definition_count; ++child) {
            if (data->definitions[child].id == reference->component_id) {
                break;
            }
        }
        if (child == data->definition_count ||
            af_component_visit(data, child, marks, depth + 1U) != AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    marks[definition_index] = 2U;
    return AF_RESULT_SUCCESS;
}

AfResult af_component_validate(const AfComponentData *data, const AfScene *scene,
                               const AfAnimationData *animation) {
    uint8_t marks[AF_MAX_COMPONENT_DEFINITIONS] = {0};
    if (data == NULL || scene == NULL || animation == NULL ||
        data->definition_count > AF_MAX_COMPONENT_DEFINITIONS ||
        data->reference_count > AF_MAX_COMPONENT_REFERENCES ||
        data->property_count > AF_MAX_COMPONENT_PROPERTIES ||
        data->instance_count > AF_MAX_COMPONENT_INSTANCES ||
        data->override_count > AF_MAX_COMPONENT_OVERRIDES ||
        data->definition_count > data->definition_capacity ||
        data->reference_count > data->reference_capacity ||
        data->property_count > data->property_capacity ||
        data->instance_count > data->instance_capacity ||
        data->override_count > data->override_capacity ||
        (data->definition_count != 0U && data->definitions == NULL) ||
        (data->reference_count != 0U && data->references == NULL) ||
        (data->property_count != 0U && data->properties == NULL) ||
        (data->instance_count != 0U && data->instances == NULL) ||
        (data->override_count != 0U && data->overrides == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->definition_count; ++index) {
        const AfComponentDefinition *definition = &data->definitions[index];
        if (definition->id == 0U || af_scene_find_const(scene, definition->root_node_id) == NULL ||
            (definition->clip_id != 0U &&
             af_component_clip(animation, definition->clip_id) == NULL) ||
            !af_component_range_valid(definition->reference_start, definition->reference_count,
                                      data->reference_count) ||
            !af_component_range_valid(definition->property_start, definition->property_count,
                                      data->property_count)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < data->definition_count; ++other) {
            if (definition->id == data->definitions[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (uint32_t child = 0U; child < definition->reference_count; ++child) {
            const AfComponentReference *reference =
                &data->references[definition->reference_start + child];
            if (reference->owner_definition_id != definition->id ||
                af_component_definition(data, reference->component_id) == NULL) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (uint32_t property_index = 0U; property_index < definition->property_count;
             ++property_index) {
            const AfComponentProperty *property =
                &data->properties[definition->property_start + property_index];
            if (property->definition_id != definition->id || property->id == 0U ||
                !af_component_value_valid(property->type, &property->default_value)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            for (uint32_t other = property_index + 1U; other < definition->property_count;
                 ++other) {
                if (property->id == data->properties[definition->property_start + other].id) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
            }
        }
    }
    for (uint32_t index = 0U; index < data->definition_count; ++index) {
        if (af_component_visit(data, index, marks, 1U) != AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < data->instance_count; ++index) {
        const AfComponentInstance *instance = &data->instances[index];
        const AfComponentDefinition *definition =
            af_component_definition(data, instance->definition_id);
        if (instance->id == 0U || definition == NULL ||
            af_scene_find_const(scene, instance->node_id) == NULL ||
            !isfinite(instance->playback_time) || !isfinite(instance->playback_speed) ||
            !af_component_range_valid(instance->override_start, instance->override_count,
                                      data->override_count)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < data->instance_count; ++other) {
            if (instance->id == data->instances[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (uint32_t override_index = 0U; override_index < instance->override_count;
             ++override_index) {
            const AfComponentOverride *override =
                &data->overrides[instance->override_start + override_index];
            bool matched = false;
            if (override->instance_id != instance->id ||
                !af_component_value_valid(override->type, &override->value)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            for (uint32_t property_index = 0U; property_index < definition->property_count;
                 ++property_index) {
                const AfComponentProperty *property =
                    &data->properties[definition->property_start + property_index];
                if (property->id == override->property_id && property->type == override->type) {
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            for (uint32_t other = override_index + 1U; other < instance->override_count; ++other) {
                if (override->property_id ==
                    data->overrides[instance->override_start + other].property_id) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_component_runtime_init(AfComponentRuntime *runtime, const AfComponentData *shared,
                                   const AfAnimationData *animation,
                                   AfComponentInstanceState *states, uint32_t state_capacity) {
    if (runtime == NULL || shared == NULL || animation == NULL ||
        (state_capacity != 0U && states == NULL) || state_capacity < shared->instance_count) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *runtime = (AfComponentRuntime){.shared = shared,
                                    .animation = animation,
                                    .states = states,
                                    .state_count = shared->instance_count,
                                    .state_capacity = state_capacity};
    for (uint32_t index = 0U; index < shared->instance_count; ++index) {
        states[index] =
            (AfComponentInstanceState){.instance_id = shared->instances[index].id,
                                       .playback_time = shared->instances[index].playback_time,
                                       .playing = true};
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_component_runtime_update(AfComponentRuntime *runtime, float delta_time) {
    if (runtime == NULL || runtime->shared == NULL || runtime->animation == NULL ||
        runtime->state_count > runtime->state_capacity || !isfinite(delta_time)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < runtime->state_count; ++index) {
        const AfComponentInstance *instance = &runtime->shared->instances[index];
        const AfComponentDefinition *definition =
            af_component_definition(runtime->shared, instance->definition_id);
        const AfAnimationClip *clip =
            definition == NULL ? NULL : af_component_clip(runtime->animation, definition->clip_id);
        AfComponentInstanceState *state = &runtime->states[index];
        if (state->playing && instance->independent_playback && clip != NULL &&
            clip->duration > 0.0F) {
            state->playback_time += delta_time * instance->playback_speed;
            if (clip->loop) {
                state->playback_time = fmodf(state->playback_time, clip->duration);
                if (state->playback_time < 0.0F) {
                    state->playback_time += clip->duration;
                }
            } else {
                state->playback_time = fminf(fmaxf(state->playback_time, 0.0F), clip->duration);
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_component_runtime_set_playing(AfComponentRuntime *runtime, uint64_t instance_id,
                                          bool playing) {
    if (runtime == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < runtime->state_count; ++index) {
        if (runtime->states[index].instance_id == instance_id) {
            runtime->states[index].playing = playing;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

AfResult af_component_resolve_property(const AfComponentData *data, uint64_t instance_id,
                                       uint64_t property_id, AfComponentPropertyType type,
                                       AfComponentValue *out_value) {
    const AfComponentInstance *instance = af_component_instance(data, instance_id);
    const AfComponentDefinition *definition = NULL;
    if (instance == NULL || out_value == NULL || type > AF_COMPONENT_PROPERTY_TEXT) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    definition = af_component_definition(data, instance->definition_id);
    for (uint32_t index = 0U; index < instance->override_count; ++index) {
        const AfComponentOverride *override = &data->overrides[instance->override_start + index];
        if (override->property_id == property_id && override->type == type) {
            *out_value = override->value;
            return AF_RESULT_SUCCESS;
        }
    }
    for (uint32_t index = 0U; index < definition->property_count; ++index) {
        const AfComponentProperty *property = &data->properties[definition->property_start + index];
        if (property->id == property_id && property->type == type) {
            *out_value = property->default_value;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

AfResult af_component_detach(const AfComponentData *data, uint64_t instance_id,
                             AfDetachedComponent *out_detached) {
    const AfComponentInstance *instance = af_component_instance(data, instance_id);
    const AfComponentDefinition *definition = NULL;
    if (instance == NULL || out_detached == NULL ||
        instance->override_count > AF_MAX_COMPONENT_PROPERTIES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    definition = af_component_definition(data, instance->definition_id);
    *out_detached = (AfDetachedComponent){.source_instance_id = instance_id,
                                          .root_node_id = definition->root_node_id,
                                          .clip_id = definition->clip_id,
                                          .override_count = instance->override_count};
    if (instance->override_count != 0U) {
        memcpy(out_detached->overrides, &data->overrides[instance->override_start],
               (size_t)instance->override_count * sizeof(AfComponentOverride));
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_component_duplicate_definition(AfComponentData *data, uint64_t source_id,
                                           uint64_t duplicate_id) {
    const AfComponentDefinition *source = af_component_definition(data, source_id);
    AfComponentDefinition copy = {0};
    if (data == NULL || source == NULL || duplicate_id == 0U ||
        af_component_definition(data, duplicate_id) != NULL ||
        data->definition_count > data->definition_capacity ||
        data->reference_count > data->reference_capacity ||
        data->property_count > data->property_capacity ||
        data->definition_count >= data->definition_capacity ||
        source->reference_count > data->reference_capacity - data->reference_count ||
        source->property_count > data->property_capacity - data->property_count) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    copy = *source;
    copy.id = duplicate_id;
    copy.reference_start = data->reference_count;
    copy.property_start = data->property_count;
    for (uint32_t index = 0U; index < source->reference_count; ++index) {
        data->references[data->reference_count + index] =
            data->references[source->reference_start + index];
        data->references[data->reference_count + index].owner_definition_id = duplicate_id;
    }
    for (uint32_t index = 0U; index < source->property_count; ++index) {
        data->properties[data->property_count + index] =
            data->properties[source->property_start + index];
        data->properties[data->property_count + index].definition_id = duplicate_id;
    }
    data->reference_count += source->reference_count;
    data->property_count += source->property_count;
    data->definitions[data->definition_count++] = copy;
    return AF_RESULT_SUCCESS;
}

AfResult af_component_edit_begin(const AfComponentData *data, uint64_t definition_id,
                                 AfComponentEditSession *session) {
    if (session == NULL || af_component_definition(data, definition_id) == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    *session = (AfComponentEditSession){.definition_id = definition_id, .active = true};
    return AF_RESULT_SUCCESS;
}

void af_component_edit_end(AfComponentEditSession *session) {
    if (session != NULL) {
        *session = (AfComponentEditSession){0};
    }
}
