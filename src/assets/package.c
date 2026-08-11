#include "assets/package.h"

#include "core/safe_math.h"

#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

const uint8_t AF_PACKAGE_MAGIC[8] = {'A', 'R', 'C', 'F', 'L', 'O', 'W', '\0'};

enum {
    AF_SECTION_NODES = 1U,
    AF_SECTION_CLIPS = 2U,
    AF_SECTION_TRACKS = 3U,
    AF_SECTION_KEYFRAMES = 4U,
    AF_SECTION_STRINGS = 5U,
    AF_SECTION_PATHS = 6U,
    AF_SECTION_PATH_COMMANDS = 7U,
    AF_SECTION_STYLES = 8U,
    AF_SECTION_LAYOUTS = 9U,
    AF_SECTION_COMPONENTS = 10U,
    AF_SECTION_STATE_MACHINES = 11U,
    AF_SECTION_PARTICLES = 12U,
    AF_SECTION_COUNT = 12U,
    AF_NODE_RECORD_SIZE = 80U,
    AF_CLIP_RECORD_SIZE = 24U,
    AF_TRACK_RECORD_SIZE = 32U,
    AF_KEYFRAME_RECORD_SIZE = 40U,
    AF_PATH_RECORD_SIZE = 40U,
    AF_PATH_COMMAND_RECORD_SIZE = 32U,
    AF_PAINT_RECORD_SIZE = 400U,
    AF_STYLE_RECORD_SIZE = 952U,
    AF_LAYOUT_HEADER_SIZE = 48U,
    AF_LAYOUT_RECORD_SIZE = 128U,
    AF_COMPONENT_HEADER_SIZE = 32U,
    AF_COMPONENT_DEFINITION_RECORD_SIZE = 40U,
    AF_COMPONENT_REFERENCE_RECORD_SIZE = 16U,
    AF_COMPONENT_VALUE_RECORD_SIZE = 88U,
    AF_COMPONENT_INSTANCE_RECORD_SIZE = 48U,
    AF_STATE_MACHINE_HEADER_SIZE = 32U,
    AF_STATE_MACHINE_RECORD_SIZE = 40U,
    AF_STATE_RECORD_SIZE = 48U,
    AF_STATE_TRANSITION_RECORD_SIZE = 64U,
    AF_STATE_INPUT_RECORD_SIZE = 32U,
    AF_STATE_CONDITION_RECORD_SIZE = 32U,
    AF_PARTICLE_HEADER_SIZE = 64U,
    AF_PARTICLE_RECORD_SIZE = 512U
};

typedef struct AfSectionView {
    uint32_t type;
    uint64_t offset;
    uint64_t size;
    uint32_t alignment;
    uint32_t count;
} AfSectionView;

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
            sections[index].type > AF_SECTION_PARTICLES ||
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
    uint32_t index = 0U;
    if (section_count != AF_SECTION_COUNT || nodes == NULL || clips == NULL || tracks == NULL ||
        keys == NULL || strings == NULL || paths == NULL || path_commands == NULL ||
        styles == NULL || layouts == NULL || components == NULL || state_machines == NULL ||
        particles == NULL || nodes->count > AF_MAX_NODES || clips->count > AF_MAX_CLIPS ||
        tracks->count > AF_MAX_TRACKS || keys->count > AF_MAX_KEYFRAMES ||
        paths->count > AF_MAX_PATHS || path_commands->count > AF_MAX_PATH_COMMANDS ||
        styles->count > AF_MAX_STYLES || layouts->count > AF_MAX_LAYOUT_NODES ||
        particles->count > AF_MAX_PARTICLE_EMITTERS ||
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
        if (id == 0U || id == AF_INVALID_NODE_ID || type > AF_NODE_PATH ||
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
    return AF_RESULT_SUCCESS;
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

AfResult af_package_write(const AfPackageSource *source, AfAllocator *allocator,
                          AfPackageBlob *out_blob) {
    uint64_t offsets[AF_SECTION_COUNT] = {0};
    uint64_t sizes[AF_SECTION_COUNT] = {0};
    uint64_t total_size = AF_PACKAGE_HEADER_SIZE + AF_SECTION_COUNT * AF_PACKAGE_SECTION_SIZE;
    uint8_t *bytes = NULL;
    uint32_t index = 0U;
    const AfPathData empty_paths = {0};
    const AfStyleData empty_styles = {0};
    const AfComponentData empty_components = {0};
    const AfStateMachineData empty_state_machines = {0};
    AfParticleData empty_particles = {0};
    const AfLayoutData default_layouts = {.reference_size = {1280.0F, 720.0F},
                                          .target_size = {1280.0F, 720.0F},
                                          .scale_mode = AF_CANVAS_SCALE_FIT};
    const AfPathData *path_data = NULL;
    const AfStyleData *style_data = NULL;
    const AfLayoutData *layout_data = NULL;
    const AfComponentData *component_data = NULL;
    const AfStateMachineData *state_machine_data = NULL;
    const AfParticleData *particle_data = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (source == NULL || source->scene == NULL || source->animation == NULL || allocator == NULL ||
        out_blob == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    empty_particles.lod = af_vfx_lod_default();
    *out_blob = (AfPackageBlob){0};
    path_data = source->scene->paths == NULL ? &empty_paths : source->scene->paths;
    style_data = source->scene->styles == NULL ? &empty_styles : source->scene->styles;
    layout_data = source->scene->layouts == NULL ? &default_layouts : source->scene->layouts;
    component_data = source->components == NULL ? &empty_components : source->components;
    state_machine_data =
        source->state_machines == NULL ? &empty_state_machines : source->state_machines;
    particle_data = source->scene->particles == NULL ? &empty_particles : source->scene->particles;
    result = af_scene_validate(source->scene);
    if (result != AF_RESULT_SUCCESS ||
        af_animation_validate(source->animation, source->scene) != AF_RESULT_SUCCESS ||
        af_path_data_validate(path_data) != AF_RESULT_SUCCESS ||
        af_style_data_validate(style_data) != AF_RESULT_SUCCESS ||
        af_layout_validate(layout_data, source->scene) != AF_RESULT_SUCCESS ||
        af_component_validate(component_data, source->scene, source->animation) !=
            AF_RESULT_SUCCESS ||
        af_state_machine_validate(state_machine_data, source->animation, component_data) !=
            AF_RESULT_SUCCESS ||
        af_particle_data_validate(particle_data, source->scene) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    sizes[0] = (uint64_t)source->scene->count * AF_NODE_RECORD_SIZE;
    sizes[1] = (uint64_t)source->animation->clip_count * AF_CLIP_RECORD_SIZE;
    sizes[2] = (uint64_t)source->animation->track_count * AF_TRACK_RECORD_SIZE;
    sizes[3] = (uint64_t)source->animation->keyframe_count * AF_KEYFRAME_RECORD_SIZE;
    sizes[4] = 1U;
    sizes[5] = (uint64_t)path_data->path_count * AF_PATH_RECORD_SIZE;
    sizes[6] = (uint64_t)path_data->command_count * AF_PATH_COMMAND_RECORD_SIZE;
    sizes[7] = (uint64_t)style_data->count * AF_STYLE_RECORD_SIZE;
    sizes[8] = AF_LAYOUT_HEADER_SIZE + (uint64_t)layout_data->count * AF_LAYOUT_RECORD_SIZE;
    sizes[9] = af_component_section_size(component_data);
    sizes[10] = af_state_machine_section_size(state_machine_data);
    sizes[11] = AF_PARTICLE_HEADER_SIZE + (uint64_t)particle_data->count * AF_PARTICLE_RECORD_SIZE;
    for (index = 0U; index < AF_SECTION_COUNT; ++index) {
        offsets[index] = af_align_u64(total_size, 8U);
        if (af_u64_add_overflow(offsets[index], sizes[index], &total_size) ||
            total_size > AF_PACKAGE_MAX_SIZE) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    }
    bytes = af_allocate(allocator, (size_t)total_size, 8U);
    if (bytes == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(bytes, 0, (size_t)total_size);
    memcpy(bytes, AF_PACKAGE_MAGIC, 8U);
    af_write_u32(bytes + 8U, AF_PACKAGE_FORMAT_VERSION);
    af_write_u32(bytes + 12U, AF_RUNTIME_VERSION);
    af_write_u32(bytes + 16U, AF_PACKAGE_ENDIAN_MARKER);
    af_write_u32(bytes + 20U, AF_PACKAGE_HEADER_SIZE);
    af_write_u64(bytes + 24U, total_size);
    af_write_u64(bytes + 32U, AF_PACKAGE_HEADER_SIZE);
    af_write_u32(bytes + 40U, AF_SECTION_COUNT);
    af_write_u32(bytes + 44U, 0U);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE, AF_SECTION_NODES, offsets[0], sizes[0], 8U,
                     source->scene->count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + AF_PACKAGE_SECTION_SIZE, AF_SECTION_CLIPS,
                     offsets[1], sizes[1], 8U, source->animation->clip_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 2U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_TRACKS, offsets[2], sizes[2], 8U, source->animation->track_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 3U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_KEYFRAMES, offsets[3], sizes[3], 8U,
                     source->animation->keyframe_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 4U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_STRINGS, offsets[4], sizes[4], 1U, 1U);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 5U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_PATHS, offsets[5], sizes[5], 8U, path_data->path_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 6U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_PATH_COMMANDS, offsets[6], sizes[6], 8U, path_data->command_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 7U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_STYLES, offsets[7], sizes[7], 8U, style_data->count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 8U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_LAYOUTS, offsets[8], sizes[8], 8U, layout_data->count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 9U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_COMPONENTS, offsets[9], sizes[9], 8U,
                     component_data->definition_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 10U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_STATE_MACHINES, offsets[10], sizes[10], 8U,
                     state_machine_data->machine_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 11U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_PARTICLES, offsets[11], sizes[11], 8U, particle_data->count);
    for (index = 0U; index < source->scene->count; ++index) {
        const AfSceneNode *node = &source->scene->nodes[index];
        uint8_t *record = bytes + offsets[0] + (uint64_t)index * AF_NODE_RECORD_SIZE;
        af_write_u64(record, node->id);
        af_write_u64(record + 8U, node->parent_id);
        af_write_u32(record + 16U, (uint32_t)node->type);
        af_write_u32(record + 20U, node->visible ? 1U : 0U);
        af_write_f32(record + 24U, node->local_transform.position.x);
        af_write_f32(record + 28U, node->local_transform.position.y);
        af_write_f32(record + 32U, node->local_transform.rotation);
        af_write_f32(record + 36U, node->local_transform.scale.x);
        af_write_f32(record + 40U, node->local_transform.scale.y);
        af_write_f32(record + 44U, node->bounds.x);
        af_write_f32(record + 48U, node->bounds.y);
        af_write_f32(record + 52U, node->fill.r);
        af_write_f32(record + 56U, node->fill.g);
        af_write_f32(record + 60U, node->fill.b);
        af_write_f32(record + 64U, node->fill.a);
        af_write_f32(record + 68U, node->opacity);
        af_write_f32(record + 72U, node->corner_radius);
        af_write_u32(record + 76U, (uint32_t)node->layer_order);
    }
    for (index = 0U; index < source->animation->clip_count; ++index) {
        const AfAnimationClip *clip = &source->animation->clips[index];
        uint8_t *record = bytes + offsets[1] + (uint64_t)index * AF_CLIP_RECORD_SIZE;
        af_write_u64(record, clip->id);
        af_write_f32(record + 8U, clip->duration);
        af_write_u32(record + 12U, clip->track_start);
        af_write_u32(record + 16U, clip->track_count);
        af_write_u32(record + 20U, clip->loop ? 1U : 0U);
    }
    for (index = 0U; index < source->animation->track_count; ++index) {
        const AfAnimationTrack *track = &source->animation->tracks[index];
        uint8_t *record = bytes + offsets[2] + (uint64_t)index * AF_TRACK_RECORD_SIZE;
        af_write_u64(record, track->id);
        af_write_u64(record + 8U, track->node_id);
        af_write_u32(record + 16U, (uint32_t)track->property);
        af_write_u32(record + 20U, track->keyframe_start);
        af_write_u32(record + 24U, track->keyframe_count);
        af_write_u32(record + 28U, track->element_index);
    }
    for (index = 0U; index < source->animation->keyframe_count; ++index) {
        const AfKeyframe *key = &source->animation->keyframes[index];
        uint8_t *record = bytes + offsets[3] + (uint64_t)index * AF_KEYFRAME_RECORD_SIZE;
        uint32_t component = 0U;
        af_write_f32(record, key->time);
        for (component = 0U; component < 4U; ++component) {
            af_write_f32(record + 4U + component * 4U, key->value[component]);
            af_write_f32(record + 24U + component * 4U, key->easing[component]);
        }
        af_write_u32(record + 20U, (uint32_t)key->interpolation);
    }
    bytes[offsets[4]] = 0U;
    for (index = 0U; index < path_data->path_count; ++index) {
        const AfPath *path = &path_data->paths[index];
        uint8_t *record = bytes + offsets[5] + (uint64_t)index * AF_PATH_RECORD_SIZE;
        af_write_u64(record, path->id);
        af_write_u64(record + 8U, path->node_id);
        af_write_u32(record + 16U, path->command_start);
        af_write_u32(record + 20U, path->command_count);
        af_write_u32(record + 24U, (uint32_t)path->fill_rule);
        af_write_u64(record + 32U, path->geometry_generation);
    }
    for (index = 0U; index < path_data->command_count; ++index) {
        const AfPathCommand *command = &path_data->commands[index];
        uint8_t *record = bytes + offsets[6] + (uint64_t)index * AF_PATH_COMMAND_RECORD_SIZE;
        af_write_u32(record, (uint32_t)command->type);
        af_write_f32(record + 8U, command->point.x);
        af_write_f32(record + 12U, command->point.y);
        af_write_f32(record + 16U, command->control1.x);
        af_write_f32(record + 20U, command->control1.y);
        af_write_f32(record + 24U, command->control2.x);
        af_write_f32(record + 28U, command->control2.y);
    }
    for (index = 0U; index < style_data->count; ++index) {
        af_write_style(bytes + offsets[7] + (uint64_t)index * AF_STYLE_RECORD_SIZE,
                       &style_data->styles[index]);
    }
    af_write_f32(bytes + offsets[8], layout_data->reference_size.x);
    af_write_f32(bytes + offsets[8] + 4U, layout_data->reference_size.y);
    af_write_f32(bytes + offsets[8] + 8U, layout_data->target_size.x);
    af_write_f32(bytes + offsets[8] + 12U, layout_data->target_size.y);
    af_write_u32(bytes + offsets[8] + 16U, (uint32_t)layout_data->scale_mode);
    af_write_f32(bytes + offsets[8] + 20U, layout_data->safe_area.left);
    af_write_f32(bytes + offsets[8] + 24U, layout_data->safe_area.top);
    af_write_f32(bytes + offsets[8] + 28U, layout_data->safe_area.right);
    af_write_f32(bytes + offsets[8] + 32U, layout_data->safe_area.bottom);
    for (index = 0U; index < layout_data->count; ++index) {
        af_write_layout(bytes + offsets[8] + AF_LAYOUT_HEADER_SIZE +
                            (uint64_t)index * AF_LAYOUT_RECORD_SIZE,
                        &layout_data->nodes[index]);
    }
    af_write_components(bytes + offsets[9], component_data);
    af_write_state_machines(bytes + offsets[10], state_machine_data);
    {
        const AfVfxLodSettings lod =
            particle_data->lod.maximum_particles == 0U ? af_vfx_lod_default() : particle_data->lod;
        af_write_u32(bytes + offsets[11], lod.maximum_particles);
        af_write_f32(bytes + offsets[11] + 4U, lod.spawn_rate_scale);
        af_write_f32(bytes + offsets[11] + 8U, lod.simulation_frequency_scale);
        af_write_f32(bytes + offsets[11] + 12U, lod.distance_start);
        af_write_f32(bytes + offsets[11] + 16U, lod.distance_end);
        af_write_u32(bytes + offsets[11] + 20U, lod.renderer_enabled ? 1U : 0U);
    }
    for (index = 0U; index < particle_data->count; ++index) {
        af_write_particle(bytes + offsets[11] + AF_PARTICLE_HEADER_SIZE +
                              (uint64_t)index * AF_PARTICLE_RECORD_SIZE,
                          &particle_data->emitters[index]);
    }
    af_write_u32(bytes + 48U, af_checksum(bytes + AF_PACKAGE_HEADER_SIZE,
                                          (size_t)total_size - AF_PACKAGE_HEADER_SIZE));
    if (af_package_validate(bytes, (size_t)total_size) != AF_RESULT_SUCCESS) {
        af_deallocate(allocator, bytes);
        return AF_RESULT_INVALID_ARGUMENT;
    }
    out_blob->data = bytes;
    out_blob->size = (size_t)total_size;
    return AF_RESULT_SUCCESS;
}

void af_package_blob_destroy(AfAllocator *allocator, AfPackageBlob *blob) {
    if (blob != NULL) {
        af_deallocate(allocator, blob->data);
        *blob = (AfPackageBlob){0};
    }
}

AfResult af_package_decode(const void *data, size_t size, AfAllocator *allocator,
                           AfLoadedPackage *out_package) {
    AfSectionView sections[AF_PACKAGE_MAX_SECTIONS] = {0};
    uint32_t section_count = 0U;
    const uint8_t *bytes = data;
    const AfSectionView *nodes = NULL;
    const AfSectionView *clips = NULL;
    const AfSectionView *tracks = NULL;
    const AfSectionView *keys = NULL;
    const AfSectionView *paths = NULL;
    const AfSectionView *path_commands = NULL;
    const AfSectionView *styles = NULL;
    const AfSectionView *layouts = NULL;
    const AfSectionView *components = NULL;
    const AfSectionView *state_machines = NULL;
    const AfSectionView *particles = NULL;
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (allocator == NULL || out_package == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_package = (AfLoadedPackage){0};
    result = af_package_validate(data, size);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    result = af_read_sections(bytes, size, sections, &section_count);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    nodes = af_find_section(sections, section_count, AF_SECTION_NODES);
    clips = af_find_section(sections, section_count, AF_SECTION_CLIPS);
    tracks = af_find_section(sections, section_count, AF_SECTION_TRACKS);
    keys = af_find_section(sections, section_count, AF_SECTION_KEYFRAMES);
    paths = af_find_section(sections, section_count, AF_SECTION_PATHS);
    path_commands = af_find_section(sections, section_count, AF_SECTION_PATH_COMMANDS);
    styles = af_find_section(sections, section_count, AF_SECTION_STYLES);
    layouts = af_find_section(sections, section_count, AF_SECTION_LAYOUTS);
    components = af_find_section(sections, section_count, AF_SECTION_COMPONENTS);
    state_machines = af_find_section(sections, section_count, AF_SECTION_STATE_MACHINES);
    particles = af_find_section(sections, section_count, AF_SECTION_PARTICLES);
    out_package->scene.nodes =
        af_allocate_array(allocator, nodes->count, sizeof(AfSceneNode), alignof(AfSceneNode));
    out_package->base_nodes =
        af_allocate_array(allocator, nodes->count, sizeof(AfSceneNode), alignof(AfSceneNode));
    out_package->animation.clips = af_allocate_array(
        allocator, clips->count, sizeof(AfAnimationClip), alignof(AfAnimationClip));
    out_package->animation.tracks = af_allocate_array(
        allocator, tracks->count, sizeof(AfAnimationTrack), alignof(AfAnimationTrack));
    out_package->animation.keyframes =
        af_allocate_array(allocator, keys->count, sizeof(AfKeyframe), alignof(AfKeyframe));
    out_package->path_data.paths =
        af_allocate_array(allocator, paths->count, sizeof(AfPath), alignof(AfPath));
    out_package->path_data.commands = af_allocate_array(
        allocator, path_commands->count, sizeof(AfPathCommand), alignof(AfPathCommand));
    out_package->style_data.styles =
        af_allocate_array(allocator, styles->count, sizeof(AfStyle), alignof(AfStyle));
    out_package->layout_data.nodes =
        af_allocate_array(allocator, layouts->count, sizeof(AfLayoutNode), alignof(AfLayoutNode));
    out_package->particle_data.emitters = af_allocate_array(
        allocator, particles->count, sizeof(AfParticleEmitter), alignof(AfParticleEmitter));
    if ((nodes->count != 0U &&
         (out_package->scene.nodes == NULL || out_package->base_nodes == NULL)) ||
        (clips->count != 0U && out_package->animation.clips == NULL) ||
        (tracks->count != 0U && out_package->animation.tracks == NULL) ||
        (keys->count != 0U && out_package->animation.keyframes == NULL) ||
        (paths->count != 0U && out_package->path_data.paths == NULL) ||
        (path_commands->count != 0U && out_package->path_data.commands == NULL) ||
        (styles->count != 0U && out_package->style_data.styles == NULL) ||
        (layouts->count != 0U && out_package->layout_data.nodes == NULL) ||
        (particles->count != 0U && out_package->particle_data.emitters == NULL)) {
        af_loaded_package_destroy(allocator, out_package);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    result =
        af_decode_components(bytes + components->offset, allocator, &out_package->component_data);
    if (result != AF_RESULT_SUCCESS) {
        af_loaded_package_destroy(allocator, out_package);
        return result;
    }
    result = af_decode_state_machines(bytes + state_machines->offset, allocator,
                                      &out_package->state_machine_data);
    if (result != AF_RESULT_SUCCESS) {
        af_loaded_package_destroy(allocator, out_package);
        return result;
    }
    out_package->scene.count = nodes->count;
    out_package->scene.capacity = nodes->count;
    out_package->animation.clip_count = clips->count;
    out_package->animation.track_count = tracks->count;
    out_package->animation.keyframe_count = keys->count;
    out_package->path_data.path_count = paths->count;
    out_package->path_data.path_capacity = paths->count;
    out_package->path_data.command_count = path_commands->count;
    out_package->path_data.command_capacity = path_commands->count;
    out_package->style_data.count = styles->count;
    out_package->style_data.capacity = styles->count;
    out_package->layout_data.count = layouts->count;
    out_package->layout_data.capacity = layouts->count;
    out_package->particle_data.count = particles->count;
    out_package->particle_data.capacity = particles->count;
    out_package->particle_data.lod = (AfVfxLodSettings){
        .maximum_particles = af_read_u32(bytes + particles->offset),
        .spawn_rate_scale = af_read_f32(bytes + particles->offset + 4U),
        .simulation_frequency_scale = af_read_f32(bytes + particles->offset + 8U),
        .distance_start = af_read_f32(bytes + particles->offset + 12U),
        .distance_end = af_read_f32(bytes + particles->offset + 16U),
        .host_quality = 1.0F,
        .renderer_enabled = af_read_u32(bytes + particles->offset + 20U) != 0U};
    out_package->layout_data.reference_size =
        (AfVec2){af_read_f32(bytes + layouts->offset), af_read_f32(bytes + layouts->offset + 4U)};
    out_package->layout_data.target_size = (AfVec2){af_read_f32(bytes + layouts->offset + 8U),
                                                    af_read_f32(bytes + layouts->offset + 12U)};
    out_package->layout_data.scale_mode =
        (AfCanvasScaleMode)af_read_u32(bytes + layouts->offset + 16U);
    out_package->layout_data.safe_area =
        (AfInsets){.left = af_read_f32(bytes + layouts->offset + 20U),
                   .top = af_read_f32(bytes + layouts->offset + 24U),
                   .right = af_read_f32(bytes + layouts->offset + 28U),
                   .bottom = af_read_f32(bytes + layouts->offset + 32U)};
    out_package->scene.paths = &out_package->path_data;
    out_package->scene.styles = &out_package->style_data;
    out_package->scene.layouts = &out_package->layout_data;
    out_package->scene.particles = &out_package->particle_data;
    for (index = 0U; index < nodes->count; ++index) {
        const uint8_t *record = bytes + nodes->offset + (uint64_t)index * AF_NODE_RECORD_SIZE;
        AfSceneNode *node = &out_package->scene.nodes[index];
        *node = (AfSceneNode){
            .id = af_read_u64(record),
            .parent_id = af_read_u64(record + 8U),
            .type = (AfNodeType)af_read_u32(record + 16U),
            .local_transform = {.position = {af_read_f32(record + 24U), af_read_f32(record + 28U)},
                                .rotation = af_read_f32(record + 32U),
                                .scale = {af_read_f32(record + 36U), af_read_f32(record + 40U)}},
            .bounds = {af_read_f32(record + 44U), af_read_f32(record + 48U)},
            .fill = {af_read_f32(record + 52U), af_read_f32(record + 56U),
                     af_read_f32(record + 60U), af_read_f32(record + 64U)},
            .opacity = af_read_f32(record + 68U),
            .corner_radius = af_read_f32(record + 72U),
            .layer_order = (int32_t)af_read_u32(record + 76U),
            .visible = af_read_u32(record + 20U) != 0U};
    }
    if (nodes->count != 0U) {
        memcpy(out_package->base_nodes, out_package->scene.nodes,
               (size_t)nodes->count * sizeof(AfSceneNode));
    }
    for (index = 0U; index < clips->count; ++index) {
        const uint8_t *record = bytes + clips->offset + (uint64_t)index * AF_CLIP_RECORD_SIZE;
        out_package->animation.clips[index] =
            (AfAnimationClip){.id = af_read_u64(record),
                              .duration = af_read_f32(record + 8U),
                              .track_start = af_read_u32(record + 12U),
                              .track_count = af_read_u32(record + 16U),
                              .loop = af_read_u32(record + 20U) != 0U};
    }
    for (index = 0U; index < tracks->count; ++index) {
        const uint8_t *record = bytes + tracks->offset + (uint64_t)index * AF_TRACK_RECORD_SIZE;
        out_package->animation.tracks[index] =
            (AfAnimationTrack){.id = af_read_u64(record),
                               .node_id = af_read_u64(record + 8U),
                               .property = (AfPropertyType)af_read_u32(record + 16U),
                               .keyframe_start = af_read_u32(record + 20U),
                               .keyframe_count = af_read_u32(record + 24U),
                               .element_index = af_read_u32(record + 28U)};
    }
    for (index = 0U; index < keys->count; ++index) {
        const uint8_t *record = bytes + keys->offset + (uint64_t)index * AF_KEYFRAME_RECORD_SIZE;
        uint32_t component = 0U;
        AfKeyframe *key = &out_package->animation.keyframes[index];
        key->time = af_read_f32(record);
        key->interpolation = (AfInterpolation)af_read_u32(record + 20U);
        for (component = 0U; component < 4U; ++component) {
            key->value[component] = af_read_f32(record + 4U + component * 4U);
            key->easing[component] = af_read_f32(record + 24U + component * 4U);
        }
    }
    for (index = 0U; index < paths->count; ++index) {
        const uint8_t *record = bytes + paths->offset + (uint64_t)index * AF_PATH_RECORD_SIZE;
        out_package->path_data.paths[index] =
            (AfPath){.id = af_read_u64(record),
                     .node_id = af_read_u64(record + 8U),
                     .command_start = af_read_u32(record + 16U),
                     .command_count = af_read_u32(record + 20U),
                     .fill_rule = (AfPathFillRule)af_read_u32(record + 24U),
                     .geometry_generation = af_read_u64(record + 32U)};
    }
    for (index = 0U; index < path_commands->count; ++index) {
        const uint8_t *record =
            bytes + path_commands->offset + (uint64_t)index * AF_PATH_COMMAND_RECORD_SIZE;
        out_package->path_data.commands[index] =
            (AfPathCommand){.type = (AfPathCommandType)af_read_u32(record),
                            .point = {af_read_f32(record + 8U), af_read_f32(record + 12U)},
                            .control1 = {af_read_f32(record + 16U), af_read_f32(record + 20U)},
                            .control2 = {af_read_f32(record + 24U), af_read_f32(record + 28U)}};
    }
    for (index = 0U; index < styles->count; ++index) {
        out_package->style_data.styles[index] =
            af_read_style(bytes + styles->offset + (uint64_t)index * AF_STYLE_RECORD_SIZE);
    }
    for (index = 0U; index < layouts->count; ++index) {
        out_package->layout_data.nodes[index] =
            af_read_layout(bytes + layouts->offset + AF_LAYOUT_HEADER_SIZE +
                           (uint64_t)index * AF_LAYOUT_RECORD_SIZE);
    }
    for (index = 0U; index < particles->count; ++index) {
        out_package->particle_data.emitters[index] =
            af_read_particle(bytes + particles->offset + AF_PARTICLE_HEADER_SIZE +
                             (uint64_t)index * AF_PARTICLE_RECORD_SIZE);
    }
    return af_scene_evaluate_world(&out_package->scene);
}

void af_loaded_package_destroy(AfAllocator *allocator, AfLoadedPackage *package) {
    if (package == NULL) {
        return;
    }
    af_deallocate(allocator, package->scene.nodes);
    af_deallocate(allocator, package->base_nodes);
    af_deallocate(allocator, package->animation.clips);
    af_deallocate(allocator, package->animation.tracks);
    af_deallocate(allocator, package->animation.keyframes);
    af_deallocate(allocator, package->path_data.paths);
    af_deallocate(allocator, package->path_data.commands);
    af_deallocate(allocator, package->style_data.styles);
    af_deallocate(allocator, package->layout_data.nodes);
    af_deallocate(allocator, package->component_data.definitions);
    af_deallocate(allocator, package->component_data.references);
    af_deallocate(allocator, package->component_data.properties);
    af_deallocate(allocator, package->component_data.instances);
    af_deallocate(allocator, package->component_data.overrides);
    af_deallocate(allocator, package->state_machine_data.machines);
    af_deallocate(allocator, package->state_machine_data.states);
    af_deallocate(allocator, package->state_machine_data.transitions);
    af_deallocate(allocator, package->state_machine_data.inputs);
    af_deallocate(allocator, package->state_machine_data.conditions);
    af_deallocate(allocator, package->particle_data.emitters);
    *package = (AfLoadedPackage){0};
}
