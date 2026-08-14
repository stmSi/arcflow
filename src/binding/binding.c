#include "binding/binding.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool af_binding_string_terminated(const char *value, size_t capacity) {
    return value != NULL && memchr(value, '\0', capacity) != NULL;
}

static bool af_binding_color_valid(AfColor color) {
    return isfinite(color.r) && isfinite(color.g) && isfinite(color.b) && isfinite(color.a) &&
           color.r >= 0.0F && color.r <= 1.0F && color.g >= 0.0F && color.g <= 1.0F &&
           color.b >= 0.0F && color.b <= 1.0F && color.a >= 0.0F && color.a <= 1.0F;
}

static const AfAnimationClip *af_binding_clip(const AfAnimationData *animation, uint64_t id) {
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

static const AfStateInputDefinition *af_binding_state_input(const AfStateMachineData *data,
                                                            uint64_t machine_id, uint64_t id) {
    if (data == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->input_count; ++index) {
        if (data->inputs[index].id == id &&
            (machine_id == 0U || data->inputs[index].machine_id == machine_id)) {
            return &data->inputs[index];
        }
    }
    return NULL;
}

static const AfParticleEmitter *af_binding_emitter(const AfParticleData *data, uint64_t id) {
    if (data == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->count; ++index) {
        if (data->emitters[index].id == id) {
            return &data->emitters[index];
        }
    }
    return NULL;
}

static bool af_binding_component_property_exists(const AfComponentData *data, uint64_t id) {
    if (data == NULL || id == 0U) {
        return false;
    }
    for (uint32_t index = 0U; index < data->property_count; ++index) {
        if (data->properties[index].id == id) {
            return true;
        }
    }
    return false;
}

uint64_t af_binding_name_id(const char *name) {
    const uint8_t *bytes = (const uint8_t *)name;
    uint64_t hash = UINT64_C(14695981039346656037);
    if (name == NULL || name[0] == '\0') {
        return 0U;
    }
    while (*bytes != 0U) {
        hash ^= *bytes++;
        hash *= UINT64_C(1099511628211);
    }
    return hash == 0U ? 1U : hash;
}

AfExposedProperty *af_binding_find(AfBindingData *data, uint64_t property_id) {
    if (data == NULL || property_id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->property_count; ++index) {
        if (data->properties[index].id == property_id) {
            return &data->properties[index];
        }
    }
    return NULL;
}

const AfExposedProperty *af_binding_find_const(const AfBindingData *data, uint64_t property_id) {
    return af_binding_find((AfBindingData *)data, property_id);
}

AfResult af_binding_format_spec_validate(AfBindingValueType type,
                                         const AfBindingFormatSpec *format) {
    if (format == NULL || type > AF_BINDING_VALUE_ANIMATION ||
        format->mode > AF_BINDING_FORMAT_TIME ||
        format->precision > AF_BINDING_MAX_DECIMAL_PRECISION ||
        !af_binding_string_terminated(format->prefix, sizeof(format->prefix)) ||
        !af_binding_string_terminated(format->suffix, sizeof(format->suffix)) ||
        !af_binding_string_terminated(format->singular, sizeof(format->singular)) ||
        !af_binding_string_terminated(format->plural, sizeof(format->plural))) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (format->mode != AF_BINDING_FORMAT_DEFAULT && type != AF_BINDING_VALUE_SCALAR &&
        type != AF_BINDING_VALUE_INTEGER && type != AF_BINDING_VALUE_PROGRESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if ((format->singular[0] == '\0') != (format->plural[0] == '\0')) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_binding_value_validate(AfBindingValueType type, const AfBindingValue *value,
                                   const AfBindingValidationContext *context) {
    if (value == NULL || type > AF_BINDING_VALUE_ANIMATION) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    switch (type) {
    case AF_BINDING_VALUE_SCALAR:
        return isfinite(value->scalar) ? AF_RESULT_SUCCESS : AF_RESULT_INVALID_ARGUMENT;
    case AF_BINDING_VALUE_INTEGER:
    case AF_BINDING_VALUE_BOOLEAN:
    case AF_BINDING_VALUE_VISIBILITY:
        return AF_RESULT_SUCCESS;
    case AF_BINDING_VALUE_COLOR:
        return af_binding_color_valid(value->color) ? AF_RESULT_SUCCESS
                                                    : AF_RESULT_INVALID_ARGUMENT;
    case AF_BINDING_VALUE_TEXT:
        return af_binding_string_terminated(value->text, sizeof(value->text))
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_INVALID_ARGUMENT;
    case AF_BINDING_VALUE_IMAGE:
        if (value->image_id == 0U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        return context == NULL || context->images == NULL ||
                       af_image_find_const(context->images, value->image_id) != NULL ||
                       af_image_find_sequence_const(context->images, value->image_id) != NULL
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_NOT_FOUND;
    case AF_BINDING_VALUE_PROGRESS:
        return isfinite(value->progress) && value->progress >= 0.0F && value->progress <= 1.0F
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_INVALID_ARGUMENT;
    case AF_BINDING_VALUE_ANIMATION:
        if (value->animation_id == 0U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        return context == NULL || context->animation == NULL ||
                       af_binding_clip(context->animation, value->animation_id) != NULL
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_NOT_FOUND;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_binding_format_number(AfBindingValueType type, const AfBindingValue *value,
                                         const AfBindingFormatSpec *format, char *number,
                                         size_t capacity, double *out_quantity) {
    double quantity =
        type == AF_BINDING_VALUE_INTEGER ? (double)value->integer : (double)value->scalar;
    int written = 0;
    if (type == AF_BINDING_VALUE_PROGRESS) {
        quantity = (double)value->progress;
    }
    *out_quantity = quantity;
    switch (format->mode) {
    case AF_BINDING_FORMAT_DEFAULT:
        if (type == AF_BINDING_VALUE_INTEGER) {
            written = snprintf(number, capacity, "%d", value->integer);
        } else {
            written = snprintf(number, capacity, "%.*f", (int)format->precision, quantity);
        }
        break;
    case AF_BINDING_FORMAT_INTEGER:
        written = snprintf(number, capacity, "%.0f", quantity);
        break;
    case AF_BINDING_FORMAT_DECIMAL:
        written = snprintf(number, capacity, "%.*f", (int)format->precision, quantity);
        break;
    case AF_BINDING_FORMAT_PERCENTAGE:
        quantity *= 100.0;
        *out_quantity = quantity;
        written = snprintf(number, capacity, "%.*f%%", (int)format->precision, quantity);
        break;
    case AF_BINDING_FORMAT_TIME: {
        const bool negative = quantity < 0.0;
        const double absolute = fabs(quantity);
        const uint64_t hours = (uint64_t)(absolute / 3600.0);
        const uint32_t minutes = (uint32_t)fmod(absolute / 60.0, 60.0);
        const double seconds = fmod(absolute, 60.0);
        if (hours != 0U) {
            written = snprintf(number, capacity, "%s%llu:%02u:%0*.*f", negative ? "-" : "",
                               (unsigned long long)hours, minutes,
                               format->precision == 0U ? 2 : (int)format->precision + 3,
                               (int)format->precision, seconds);
        } else {
            written = snprintf(number, capacity, "%s%u:%0*.*f", negative ? "-" : "", minutes,
                               format->precision == 0U ? 2 : (int)format->precision + 3,
                               (int)format->precision, seconds);
        }
        break;
    }
    }
    return written >= 0 && (size_t)written < capacity ? AF_RESULT_SUCCESS
                                                      : AF_RESULT_LIMIT_EXCEEDED;
}

AfResult af_binding_format_value(AfBindingValueType type, const AfBindingValue *value,
                                 const AfBindingFormatSpec *format, char *output,
                                 size_t output_capacity) {
    char formatted[128];
    const char *plural = "";
    double quantity = 0.0;
    int written = 0;
    AfResult result = AF_RESULT_SUCCESS;
    if (output == NULL || output_capacity == 0U ||
        output_capacity > AF_BINDING_FORMAT_OUTPUT_CAPACITY ||
        af_binding_value_validate(type, value, NULL) != AF_RESULT_SUCCESS ||
        af_binding_format_spec_validate(type, format) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    output[0] = '\0';
    if (type == AF_BINDING_VALUE_SCALAR || type == AF_BINDING_VALUE_INTEGER ||
        type == AF_BINDING_VALUE_PROGRESS) {
        result =
            af_binding_format_number(type, value, format, formatted, sizeof(formatted), &quantity);
    } else if (type == AF_BINDING_VALUE_BOOLEAN) {
        written = snprintf(formatted, sizeof(formatted), "%s", value->boolean ? "true" : "false");
    } else if (type == AF_BINDING_VALUE_VISIBILITY) {
        written =
            snprintf(formatted, sizeof(formatted), "%s", value->visibility ? "visible" : "hidden");
    } else if (type == AF_BINDING_VALUE_TEXT) {
        written = snprintf(formatted, sizeof(formatted), "%s", value->text);
    } else if (type == AF_BINDING_VALUE_COLOR) {
        written = snprintf(formatted, sizeof(formatted), "rgba(%u,%u,%u,%.3f)",
                           (uint32_t)lroundf(value->color.r * 255.0F),
                           (uint32_t)lroundf(value->color.g * 255.0F),
                           (uint32_t)lroundf(value->color.b * 255.0F), (double)value->color.a);
    } else {
        const uint64_t id = type == AF_BINDING_VALUE_IMAGE ? value->image_id : value->animation_id;
        written = snprintf(formatted, sizeof(formatted), "%llu", (unsigned long long)id);
    }
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    if (type != AF_BINDING_VALUE_SCALAR && type != AF_BINDING_VALUE_INTEGER &&
        type != AF_BINDING_VALUE_PROGRESS &&
        (written < 0 || (size_t)written >= sizeof(formatted))) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (format->singular[0] != '\0') {
        plural = fabs(quantity - 1.0) <= 0.0000001 ? format->singular : format->plural;
    }
    written = snprintf(output, output_capacity, "%s%s%s%s%s", format->prefix, formatted,
                       plural[0] == '\0' ? "" : " ", plural, format->suffix);
    return written >= 0 && (size_t)written < output_capacity ? AF_RESULT_SUCCESS
                                                             : AF_RESULT_LIMIT_EXCEEDED;
}

static AfResult af_binding_target_validate(const AfBindingTarget *target, AfBindingValueType type,
                                           const AfBindingValidationContext *context) {
    const AfSceneNode *node = NULL;
    const AfStateInputDefinition *input = NULL;
    if (target == NULL || context == NULL || target->type > AF_BINDING_TARGET_TEXT_STORAGE ||
        target->property > AF_BINDING_TARGET_TEXT_VALUE) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    switch (target->type) {
    case AF_BINDING_TARGET_NODE:
        node =
            context->scene == NULL ? NULL : af_scene_find_const(context->scene, target->object_id);
        if (node == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        if ((target->property == AF_BINDING_TARGET_NODE_OPACITY &&
             type == AF_BINDING_VALUE_SCALAR) ||
            (target->property == AF_BINDING_TARGET_NODE_FILL_COLOR &&
             type == AF_BINDING_VALUE_COLOR) ||
            (target->property == AF_BINDING_TARGET_NODE_VISIBILITY &&
             type == AF_BINDING_VALUE_VISIBILITY) ||
            (target->property == AF_BINDING_TARGET_NODE_IMAGE && type == AF_BINDING_VALUE_IMAGE &&
             node->type == AF_NODE_IMAGE)) {
            return AF_RESULT_SUCCESS;
        }
        return AF_RESULT_INVALID_ARGUMENT;
    case AF_BINDING_TARGET_ANIMATION:
        if (context->animation != NULL && target->object_id != 0U &&
            af_binding_clip(context->animation, target->object_id) == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        return (target->property == AF_BINDING_TARGET_ANIMATION_SELECTION &&
                type == AF_BINDING_VALUE_ANIMATION) ||
                       (target->property == AF_BINDING_TARGET_ANIMATION_PROGRESS &&
                        type == AF_BINDING_VALUE_PROGRESS)
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_INVALID_ARGUMENT;
    case AF_BINDING_TARGET_PLAYBACK:
        return (target->property == AF_BINDING_TARGET_PLAYBACK_PLAYING &&
                type == AF_BINDING_VALUE_BOOLEAN) ||
                       (target->property == AF_BINDING_TARGET_PLAYBACK_SPEED &&
                        type == AF_BINDING_VALUE_SCALAR) ||
                       (target->property == AF_BINDING_TARGET_PLAYBACK_TIME &&
                        type == AF_BINDING_VALUE_PROGRESS)
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_INVALID_ARGUMENT;
    case AF_BINDING_TARGET_STATE_INPUT:
        if (context->state_machines == NULL) {
            return target->property == AF_BINDING_TARGET_STATE_INPUT_VALUE &&
                           (type == AF_BINDING_VALUE_BOOLEAN || type == AF_BINDING_VALUE_INTEGER ||
                            type == AF_BINDING_VALUE_SCALAR)
                       ? AF_RESULT_SUCCESS
                       : AF_RESULT_INVALID_ARGUMENT;
        }
        input =
            af_binding_state_input(context->state_machines, target->object_id, target->member_id);
        if (input == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        if (target->property != AF_BINDING_TARGET_STATE_INPUT_VALUE) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        return ((input->type == AF_STATE_INPUT_BOOLEAN || input->type == AF_STATE_INPUT_TRIGGER) &&
                type == AF_BINDING_VALUE_BOOLEAN) ||
                       (input->type == AF_STATE_INPUT_INTEGER &&
                        type == AF_BINDING_VALUE_INTEGER) ||
                       (input->type == AF_STATE_INPUT_FLOAT && type == AF_BINDING_VALUE_SCALAR)
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_INVALID_ARGUMENT;
    case AF_BINDING_TARGET_PARTICLE:
        if (af_binding_emitter(context->particles, target->object_id) == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        if (target->property == AF_BINDING_TARGET_PARTICLE_ENABLED) {
            return type == AF_BINDING_VALUE_BOOLEAN ? AF_RESULT_SUCCESS
                                                    : AF_RESULT_INVALID_ARGUMENT;
        }
        if (target->property == AF_BINDING_TARGET_PARTICLE_BURST_COUNT) {
            return type == AF_BINDING_VALUE_INTEGER ? AF_RESULT_SUCCESS
                                                    : AF_RESULT_INVALID_ARGUMENT;
        }
        if (target->property == AF_BINDING_TARGET_PARTICLE_COLOR_START ||
            target->property == AF_BINDING_TARGET_PARTICLE_COLOR_END) {
            return type == AF_BINDING_VALUE_COLOR ? AF_RESULT_SUCCESS : AF_RESULT_INVALID_ARGUMENT;
        }
        return target->property >= AF_BINDING_TARGET_PARTICLE_SPAWN_RATE &&
                       target->property <= AF_BINDING_TARGET_PARTICLE_TURBULENCE &&
                       type == AF_BINDING_VALUE_SCALAR
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_INVALID_ARGUMENT;
    case AF_BINDING_TARGET_TEXT_STORAGE:
        if (target->property != AF_BINDING_TARGET_TEXT_VALUE || type != AF_BINDING_VALUE_TEXT ||
            target->text_capacity == 0U || target->text_capacity > AF_BINDING_TEXT_CAPACITY) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        return context->components == NULL || target->member_id == 0U ||
                       af_binding_component_property_exists(context->components, target->member_id)
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_NOT_FOUND;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

AfResult af_binding_data_validate(const AfBindingData *data,
                                  const AfBindingValidationContext *context) {
    if (data == NULL || context == NULL || data->property_count > data->property_capacity ||
        data->property_capacity > AF_MAX_EXPOSED_PROPERTIES ||
        data->target_count > data->target_capacity ||
        data->target_capacity > AF_MAX_BINDING_TARGETS ||
        (data->property_count != 0U && data->properties == NULL) ||
        (data->target_count != 0U && data->targets == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->property_count; ++index) {
        const AfExposedProperty *property = &data->properties[index];
        if (property->id == 0U || property->type > AF_BINDING_VALUE_ANIMATION ||
            !af_binding_string_terminated(property->debug_name, sizeof(property->debug_name)) ||
            property->target_start > data->target_count ||
            property->target_count > data->target_count - property->target_start ||
            af_binding_format_spec_validate(property->type, &property->format) !=
                AF_RESULT_SUCCESS ||
            af_binding_value_validate(property->type, &property->default_value, context) !=
                AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < data->property_count; ++other) {
            if (property->id == data->properties[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (uint32_t target = 0U; target < property->target_count; ++target) {
            const AfResult result = af_binding_target_validate(
                &data->targets[property->target_start + target], property->type, context);
            if (result != AF_RESULT_SUCCESS) {
                return result;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_binding_validate_update(const AfBindingData *data, uint64_t property_id,
                                    AfBindingValueType type, const AfBindingValue *value,
                                    const AfBindingValidationContext *context) {
    const AfExposedProperty *property = af_binding_find_const(data, property_id);
    if (property == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    if (property->type != type) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return af_binding_value_validate(type, value, context);
}
