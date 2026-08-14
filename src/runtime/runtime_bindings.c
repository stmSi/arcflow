#include "runtime/runtime_internal.h"

#include <math.h>
#include <string.h>

_Static_assert(AF_RUNTIME_PROPERTY_SCALAR == (int)AF_BINDING_VALUE_SCALAR,
               "runtime and package property types must match");
_Static_assert(AF_RUNTIME_PROPERTY_ANIMATION == (int)AF_BINDING_VALUE_ANIMATION,
               "runtime and package property types must match");
_Static_assert(AF_RUNTIME_PROPERTY_TEXT_CAPACITY == AF_BINDING_TEXT_CAPACITY,
               "runtime text capacity must match package text capacity");

static uint32_t af_runtime_binding_index(const AfBindingData *data, AfPropertyId id) {
    if (data == NULL || id == 0U) {
        return UINT32_MAX;
    }
    for (uint32_t index = 0U; index < data->property_count; ++index) {
        if (data->properties[index].id == id) {
            return index;
        }
    }
    return UINT32_MAX;
}

static const AfAnimationClip *af_runtime_active_clip(const AfRuntimeInstanceSlot *slot,
                                                     const AfLoadedPackage *loaded) {
    if (slot == NULL || loaded == NULL || slot->active_clip_index >= loaded->animation.clip_count) {
        return NULL;
    }
    return &loaded->animation.clips[slot->active_clip_index];
}

static AfBindingValue af_runtime_binding_value(AfRuntimePropertyType type,
                                               const AfRuntimePropertyValue *source) {
    AfBindingValue value = {0};
    if (type == AF_RUNTIME_PROPERTY_SCALAR) {
        value.scalar = source->scalar;
    } else if (type == AF_RUNTIME_PROPERTY_INTEGER) {
        value.integer = source->integer;
    } else if (type == AF_RUNTIME_PROPERTY_BOOLEAN) {
        value.boolean = source->boolean != 0;
    } else if (type == AF_RUNTIME_PROPERTY_COLOR) {
        value.color = (AfColor){source->color.r, source->color.g, source->color.b, source->color.a};
    } else if (type == AF_RUNTIME_PROPERTY_TEXT) {
        memcpy(value.text, source->text, sizeof(value.text));
    } else if (type == AF_RUNTIME_PROPERTY_IMAGE) {
        value.image_id = source->image_id;
    } else if (type == AF_RUNTIME_PROPERTY_VISIBILITY) {
        value.visibility = source->visibility != 0;
    } else if (type == AF_RUNTIME_PROPERTY_PROGRESS) {
        value.progress = source->progress;
    } else if (type == AF_RUNTIME_PROPERTY_ANIMATION) {
        value.animation_id = source->animation_id;
    }
    return value;
}

static bool af_runtime_binding_control(const AfBindingTarget *target) {
    return target->type == AF_BINDING_TARGET_ANIMATION ||
           target->type == AF_BINDING_TARGET_PLAYBACK ||
           target->type == AF_BINDING_TARGET_STATE_INPUT;
}

static AfParticleEmitter *af_runtime_binding_emitter(AfRuntimeInstanceSlot *slot, uint64_t id) {
    for (uint32_t index = 0U; index < slot->particles.count; ++index) {
        if (slot->particles.emitters[index].id == id) {
            return &slot->particles.emitters[index];
        }
    }
    return NULL;
}

static bool af_runtime_binding_target_value_valid(AfRuntimeInstanceSlot *slot,
                                                  const AfBindingTarget *target,
                                                  AfBindingValueType type,
                                                  const AfBindingValue *value) {
    AfParticleEmitter *emitter = NULL;
    (void)type;
    if (target->property == AF_BINDING_TARGET_NODE_OPACITY) {
        return value->scalar >= 0.0F && value->scalar <= 1.0F;
    }
    if (target->property == AF_BINDING_TARGET_PLAYBACK_SPEED) {
        return fabsf(value->scalar) <= 1000.0F;
    }
    if (target->type != AF_BINDING_TARGET_PARTICLE) {
        return true;
    }
    emitter = af_runtime_binding_emitter(slot, target->object_id);
    if (emitter == NULL) {
        return false;
    }
    if (target->property == AF_BINDING_TARGET_PARTICLE_BURST_COUNT) {
        return value->integer >= 0 && (uint32_t)value->integer <= emitter->capacity;
    }
    if (target->property == AF_BINDING_TARGET_PARTICLE_SPAWN_RATE) {
        return value->scalar >= 0.0F && value->scalar <= 1000000.0F;
    }
    if (target->property == AF_BINDING_TARGET_PARTICLE_LIFETIME_MIN ||
        target->property == AF_BINDING_TARGET_PARTICLE_LIFETIME_MAX) {
        return value->scalar > 0.0F && value->scalar <= 86400.0F;
    }
    if (target->property == AF_BINDING_TARGET_PARTICLE_SIZE_START ||
        target->property == AF_BINDING_TARGET_PARTICLE_SIZE_END) {
        return value->scalar >= 0.0F && value->scalar <= 1000000.0F;
    }
    if (target->property == AF_BINDING_TARGET_PARTICLE_TURBULENCE) {
        return fabsf(value->scalar) <= AF_MAX_PARTICLE_FORCE;
    }
    return true;
}

static AfResult af_runtime_binding_apply_target(AfRuntimeInstanceSlot *slot,
                                                AfLoadedPackage *loaded,
                                                const AfBindingTarget *target,
                                                AfBindingValueType type,
                                                const AfBindingValue *value) {
    AfSceneNode *node = NULL;
    AfParticleEmitter *emitter = NULL;
    const AfAnimationClip *clip = NULL;
    (void)type;
    if (target->type == AF_BINDING_TARGET_NODE) {
        node = af_scene_find(&slot->scene, target->object_id);
        if (node == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        if (target->property == AF_BINDING_TARGET_NODE_OPACITY) {
            node->opacity = value->scalar;
        } else if (target->property == AF_BINDING_TARGET_NODE_FILL_COLOR) {
            node->fill = value->color;
        } else if (target->property == AF_BINDING_TARGET_NODE_VISIBILITY) {
            node->visible = value->visibility;
        } else if (target->property == AF_BINDING_TARGET_NODE_IMAGE) {
            AfImageBinding *binding = af_image_find_binding(&slot->images, target->object_id);
            if (binding == NULL) {
                return AF_RESULT_NOT_FOUND;
            }
            if (af_image_find_const(&loaded->image_data, value->image_id) != NULL) {
                binding->asset_id = value->image_id;
                binding->sequence_id = 0U;
            } else if (af_image_find_sequence_const(&loaded->image_data, value->image_id) != NULL) {
                binding->asset_id = 0U;
                binding->sequence_id = value->image_id;
            } else {
                return AF_RESULT_NOT_FOUND;
            }
        }
        return AF_RESULT_SUCCESS;
    }
    if (target->type == AF_BINDING_TARGET_ANIMATION) {
        if (target->property == AF_BINDING_TARGET_ANIMATION_SELECTION) {
            for (uint32_t index = 0U; index < loaded->animation.clip_count; ++index) {
                if (loaded->animation.clips[index].id == value->animation_id) {
                    slot->active_clip_index = index;
                    clip = &loaded->animation.clips[index];
                    if (slot->playback.time > clip->duration) {
                        slot->playback.time = clip->duration;
                    }
                    return AF_RESULT_SUCCESS;
                }
            }
            return AF_RESULT_NOT_FOUND;
        }
        clip = af_runtime_active_clip(slot, loaded);
        if (clip == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        slot->playback.time = value->progress * clip->duration;
        return AF_RESULT_SUCCESS;
    }
    if (target->type == AF_BINDING_TARGET_PLAYBACK) {
        if (target->property == AF_BINDING_TARGET_PLAYBACK_PLAYING) {
            if (value->boolean) {
                af_playback_play(&slot->playback);
            } else {
                af_playback_pause(&slot->playback);
            }
        } else if (target->property == AF_BINDING_TARGET_PLAYBACK_SPEED) {
            slot->playback.speed = value->scalar;
        } else {
            clip = af_runtime_active_clip(slot, loaded);
            if (clip == NULL) {
                return AF_RESULT_NOT_FOUND;
            }
            slot->playback.time = value->progress * clip->duration;
        }
        return AF_RESULT_SUCCESS;
    }
    if (target->type == AF_BINDING_TARGET_STATE_INPUT) {
        if (type == AF_BINDING_VALUE_BOOLEAN) {
            return af_state_machine_set_boolean(&slot->state_machines, target->member_id,
                                                value->boolean);
        }
        if (type == AF_BINDING_VALUE_INTEGER) {
            return af_state_machine_set_integer(&slot->state_machines, target->member_id,
                                                value->integer);
        }
        return af_state_machine_set_float(&slot->state_machines, target->member_id, value->scalar);
    }
    if (target->type == AF_BINDING_TARGET_PARTICLE) {
        emitter = af_runtime_binding_emitter(slot, target->object_id);
        if (emitter == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        switch (target->property) {
        case AF_BINDING_TARGET_PARTICLE_ENABLED:
            emitter->enabled = value->boolean;
            break;
        case AF_BINDING_TARGET_PARTICLE_SPAWN_RATE:
            emitter->spawn_rate = value->scalar;
            break;
        case AF_BINDING_TARGET_PARTICLE_BURST_COUNT:
            emitter->burst_count = (uint32_t)value->integer;
            break;
        case AF_BINDING_TARGET_PARTICLE_LIFETIME_MIN:
            emitter->lifetime_min = value->scalar;
            break;
        case AF_BINDING_TARGET_PARTICLE_LIFETIME_MAX:
            emitter->lifetime_max = value->scalar;
            break;
        case AF_BINDING_TARGET_PARTICLE_SIZE_START:
            emitter->size_start_min = value->scalar;
            emitter->size_start_max = value->scalar;
            break;
        case AF_BINDING_TARGET_PARTICLE_SIZE_END:
            emitter->size_end_min = value->scalar;
            emitter->size_end_max = value->scalar;
            break;
        case AF_BINDING_TARGET_PARTICLE_COLOR_START:
            emitter->color_start = value->color;
            break;
        case AF_BINDING_TARGET_PARTICLE_COLOR_END:
            emitter->color_end = value->color;
            break;
        case AF_BINDING_TARGET_PARTICLE_TURBULENCE:
            emitter->turbulence_strength = value->scalar;
            break;
        default:
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_runtime_binding_apply_all(AfRuntimeInstanceSlot *slot, AfLoadedPackage *loaded,
                                      bool controls) {
    if (slot == NULL || loaded == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t property_index = 0U; property_index < loaded->binding_data.property_count;
         ++property_index) {
        const AfExposedProperty *property = &loaded->binding_data.properties[property_index];
        for (uint32_t target_index = 0U; target_index < property->target_count; ++target_index) {
            const AfBindingTarget *target =
                &loaded->binding_data.targets[property->target_start + target_index];
            AfResult result = AF_RESULT_SUCCESS;
            if (af_runtime_binding_control(target) != controls) {
                continue;
            }
            result = af_runtime_binding_apply_target(slot, loaded, target, property->type,
                                                     &slot->binding_values[property_index]);
            if (result != AF_RESULT_SUCCESS) {
                return result;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_runtime_binding_set(AfRuntimeInstanceSlot *slot, AfLoadedPackage *loaded,
                                AfPropertyId property_id, AfRuntimePropertyType type,
                                const AfRuntimePropertyValue *source) {
    uint32_t index = UINT32_MAX;
    AfBindingValue value = {0};
    AfBindingValidationContext context = {0};
    const AfExposedProperty *property = NULL;
    if (slot == NULL || loaded == NULL || source == NULL || type > AF_RUNTIME_PROPERTY_ANIMATION) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    index = af_runtime_binding_index(&loaded->binding_data, property_id);
    if (index == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    if ((type == AF_RUNTIME_PROPERTY_BOOLEAN && source->boolean != 0 && source->boolean != 1) ||
        (type == AF_RUNTIME_PROPERTY_VISIBILITY && source->visibility != 0 &&
         source->visibility != 1)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    value = af_runtime_binding_value(type, source);
    context = (AfBindingValidationContext){.scene = &slot->scene,
                                           .animation = &loaded->animation,
                                           .state_machines = &loaded->state_machine_data,
                                           .components = &loaded->component_data,
                                           .images = &loaded->image_data,
                                           .particles = &slot->particles};
    {
        const AfResult validation = af_binding_validate_update(
            &loaded->binding_data, property_id, (AfBindingValueType)type, &value, &context);
        if (validation != AF_RESULT_SUCCESS) {
            return validation;
        }
    }
    property = &loaded->binding_data.properties[index];
    for (uint32_t target_index = 0U; target_index < property->target_count; ++target_index) {
        const AfBindingTarget *target =
            &loaded->binding_data.targets[property->target_start + target_index];
        if (!af_runtime_binding_target_value_valid(slot, target, property->type, &value)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    slot->binding_values[index] = value;
    for (uint32_t target_index = 0U; target_index < property->target_count; ++target_index) {
        const AfBindingTarget *target =
            &loaded->binding_data.targets[property->target_start + target_index];
        const AfResult result =
            af_runtime_binding_apply_target(slot, loaded, target, property->type, &value);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    return af_scene_evaluate_world(&slot->scene);
}

AfResult af_runtime_binding_get(const AfRuntimeInstanceSlot *slot, const AfLoadedPackage *loaded,
                                AfPropertyId property_id, AfRuntimePropertyType type,
                                AfRuntimePropertyValue *out_value) {
    uint32_t index = UINT32_MAX;
    const AfBindingValue *value = NULL;
    if (slot == NULL || loaded == NULL || out_value == NULL ||
        type > AF_RUNTIME_PROPERTY_ANIMATION) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    index = af_runtime_binding_index(&loaded->binding_data, property_id);
    if (index == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    if (loaded->binding_data.properties[index].type != (AfBindingValueType)type) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    value = &slot->binding_values[index];
    *out_value = (AfRuntimePropertyValue){0};
    if (type == AF_RUNTIME_PROPERTY_SCALAR) {
        out_value->scalar = value->scalar;
    } else if (type == AF_RUNTIME_PROPERTY_INTEGER) {
        out_value->integer = value->integer;
    } else if (type == AF_RUNTIME_PROPERTY_BOOLEAN) {
        out_value->boolean = value->boolean ? 1 : 0;
    } else if (type == AF_RUNTIME_PROPERTY_COLOR) {
        out_value->color =
            (AfRuntimeColor){value->color.r, value->color.g, value->color.b, value->color.a};
    } else if (type == AF_RUNTIME_PROPERTY_TEXT) {
        memcpy(out_value->text, value->text, sizeof(out_value->text));
    } else if (type == AF_RUNTIME_PROPERTY_IMAGE) {
        out_value->image_id = value->image_id;
    } else if (type == AF_RUNTIME_PROPERTY_VISIBILITY) {
        out_value->visibility = value->visibility ? 1 : 0;
    } else if (type == AF_RUNTIME_PROPERTY_PROGRESS) {
        out_value->progress = value->progress;
    } else {
        out_value->animation_id = value->animation_id;
    }
    return AF_RESULT_SUCCESS;
}
