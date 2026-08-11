#include "particle/particle.h"

#include "path/path.h"
#include "scene/scene.h"

#include <math.h>
#include <stddef.h>

static float af_particle_clamp(float value, float minimum, float maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

AfVfxLodSettings af_vfx_lod_default(void) {
    return (AfVfxLodSettings){.maximum_particles = AF_MAX_PARTICLES,
                              .spawn_rate_scale = 1.0F,
                              .simulation_frequency_scale = 1.0F,
                              .distance_start = 1000.0F,
                              .distance_end = 5000.0F,
                              .host_quality = 1.0F,
                              .renderer_enabled = true};
}

static bool af_vfx_lod_is_unset(const AfVfxLodSettings *lod) {
    return lod->maximum_particles == 0U && lod->spawn_rate_scale == 0.0F &&
           lod->simulation_frequency_scale == 0.0F && lod->distance_start == 0.0F &&
           lod->distance_end == 0.0F && lod->host_quality == 0.0F && lod->view_distance == 0.0F &&
           !lod->renderer_enabled;
}

float af_vfx_lod_factor(const AfVfxLodSettings *lod) {
    float distance_factor = 1.0F;
    if (lod == NULL || af_vfx_lod_is_unset(lod)) {
        return 1.0F;
    }
    if (!lod->renderer_enabled || lod->host_quality <= 0.0F) {
        return 0.0F;
    }
    if (lod->view_distance > lod->distance_start) {
        distance_factor = lod->distance_end <= lod->distance_start
                              ? 0.0F
                              : 1.0F - (lod->view_distance - lod->distance_start) /
                                           (lod->distance_end - lod->distance_start);
    }
    return af_particle_clamp(lod->host_quality * distance_factor, 0.0F, 1.0F);
}

static bool af_particle_finite_color(AfColor color) {
    return isfinite(color.r) && isfinite(color.g) && isfinite(color.b) && isfinite(color.a) &&
           color.r >= 0.0F && color.r <= 1.0F && color.g >= 0.0F && color.g <= 1.0F &&
           color.b >= 0.0F && color.b <= 1.0F && color.a >= 0.0F && color.a <= 1.0F;
}

AfParticleEmitter af_particle_emitter_default(uint64_t id, uint64_t node_id) {
    return (AfParticleEmitter){.id = id,
                               .node_id = node_id,
                               .type = AF_PARTICLE_EMITTER_POINT,
                               .output = AF_PARTICLE_OUTPUT_BILLBOARD,
                               .capacity = 2048U,
                               .seed = 1U,
                               .spawn_rate = 48.0F,
                               .burst_count = 0U,
                               .lifetime_min = 0.8F,
                               .lifetime_max = 1.6F,
                               .velocity_min = {-35.0F, -110.0F},
                               .velocity_max = {35.0F, -65.0F},
                               .angular_velocity_min = -1.0F,
                               .angular_velocity_max = 1.0F,
                               .size_start_min = 8.0F,
                               .size_start_max = 16.0F,
                               .size_end_min = 1.0F,
                               .size_end_max = 4.0F,
                               .color_start = {0.35F, 0.82F, 1.0F, 1.0F},
                               .color_end = {0.16F, 0.35F, 1.0F, 0.0F},
                               .gravity = {0.0F, 45.0F},
                               .drag = 0.15F,
                               .flipbook_columns = 1U,
                               .flipbook_rows = 1U,
                               .turbulence_frequency = 0.02F,
                               .collision_plane_normal = {0.0F, -1.0F},
                               .collision_restitution = 0.5F,
                               .collision_friction = 0.1F,
                               .depth_min = 0.0F,
                               .depth_max = 1.0F,
                               .trail_length = 36.0F,
                               .trail_width = 6.0F,
                               .mesh_scale = 1.0F,
                               .soft_fade_distance = 20.0F,
                               .light_radius = 2.0F,
                               .max_recursion_depth = 1U,
                               .enabled = true};
}

uint32_t af_particle_total_capacity(const AfParticleData *data) {
    uint64_t total = 0U;
    if (data == NULL || (data->count != 0U && data->emitters == NULL)) {
        return 0U;
    }
    for (uint32_t index = 0U; index < data->count; ++index) {
        total += data->emitters[index].capacity;
        if (total > AF_MAX_PARTICLES) {
            return AF_MAX_PARTICLES + 1U;
        }
    }
    return (uint32_t)total;
}

AfResult af_particle_data_validate(const AfParticleData *data, const AfScene *scene) {
    const AfVfxLodSettings *lod = data == NULL ? NULL : &data->lod;
    if (data == NULL || scene == NULL || data->count > data->capacity ||
        data->capacity > AF_MAX_PARTICLE_EMITTERS ||
        (data->count != 0U && data->emitters == NULL) ||
        af_particle_total_capacity(data) > AF_MAX_PARTICLES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!af_vfx_lod_is_unset(lod) &&
        (lod->maximum_particles == 0U || lod->maximum_particles > AF_MAX_PARTICLES ||
         !isfinite(lod->spawn_rate_scale) || lod->spawn_rate_scale < 0.0F ||
         lod->spawn_rate_scale > 1.0F || !isfinite(lod->simulation_frequency_scale) ||
         lod->simulation_frequency_scale <= 0.0F || lod->simulation_frequency_scale > 1.0F ||
         !isfinite(lod->distance_start) || !isfinite(lod->distance_end) ||
         lod->distance_start < 0.0F || lod->distance_end < lod->distance_start ||
         !isfinite(lod->host_quality) || lod->host_quality < 0.0F || lod->host_quality > 1.0F ||
         !isfinite(lod->view_distance) || lod->view_distance < 0.0F)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->count; ++index) {
        const AfParticleEmitter *emitter = &data->emitters[index];
        const float values[] = {
            emitter->spawn_rate,
            emitter->lifetime_min,
            emitter->lifetime_max,
            emitter->position_min.x,
            emitter->position_min.y,
            emitter->position_max.x,
            emitter->position_max.y,
            emitter->velocity_min.x,
            emitter->velocity_min.y,
            emitter->velocity_max.x,
            emitter->velocity_max.y,
            emitter->rotation_min,
            emitter->rotation_max,
            emitter->angular_velocity_min,
            emitter->angular_velocity_max,
            emitter->size_start_min,
            emitter->size_start_max,
            emitter->size_end_min,
            emitter->size_end_max,
            emitter->gravity.x,
            emitter->gravity.y,
            emitter->wind.x,
            emitter->wind.y,
            emitter->drag,
            emitter->flipbook_frames_per_second,
            emitter->turbulence_strength,
            emitter->turbulence_frequency,
            emitter->force_center.x,
            emitter->force_center.y,
            emitter->radial_force,
            emitter->vortex_force,
            emitter->attractor_position.x,
            emitter->attractor_position.y,
            emitter->attractor_strength,
            emitter->attractor_radius,
            emitter->repulsor_position.x,
            emitter->repulsor_position.y,
            emitter->repulsor_strength,
            emitter->repulsor_radius,
            emitter->vector_field_direction.x,
            emitter->vector_field_direction.y,
            emitter->vector_field_strength,
            emitter->collision_plane_normal.x,
            emitter->collision_plane_normal.y,
            emitter->collision_plane_offset,
            emitter->collision_restitution,
            emitter->collision_friction,
            emitter->depth_min,
            emitter->depth_max,
            emitter->depth_velocity_min,
            emitter->depth_velocity_max,
            emitter->trail_length,
            emitter->trail_width,
            emitter->mesh_scale,
            emitter->decal_rotation_offset,
            emitter->distortion_strength,
            emitter->soft_fade_distance,
            emitter->light_radius,
        };
        if (emitter->id == 0U || emitter->id == UINT64_MAX || emitter->node_id == 0U ||
            af_scene_find_const(scene, emitter->node_id) == NULL ||
            emitter->type > AF_PARTICLE_EMITTER_VECTOR_PATH ||
            emitter->output > AF_PARTICLE_OUTPUT_LIGHT_PROXY || emitter->capacity == 0U ||
            emitter->capacity > AF_MAX_PARTICLES_PER_EMITTER || emitter->spawn_rate < 0.0F ||
            emitter->burst_count > emitter->capacity || emitter->lifetime_min <= 0.0F ||
            emitter->lifetime_max < emitter->lifetime_min ||
            emitter->position_max.x < emitter->position_min.x ||
            emitter->position_max.y < emitter->position_min.y ||
            emitter->velocity_max.x < emitter->velocity_min.x ||
            emitter->velocity_max.y < emitter->velocity_min.y ||
            emitter->rotation_max < emitter->rotation_min ||
            emitter->angular_velocity_max < emitter->angular_velocity_min ||
            emitter->size_start_min < 0.0F || emitter->size_start_max < emitter->size_start_min ||
            emitter->size_end_min < 0.0F || emitter->size_end_max < emitter->size_end_min ||
            emitter->drag < 0.0F || emitter->flipbook_columns == 0U ||
            emitter->flipbook_columns > 64U || emitter->flipbook_rows == 0U ||
            emitter->flipbook_rows > 64U || emitter->flipbook_frames_per_second < 0.0F ||
            fabsf(emitter->turbulence_strength) > AF_MAX_PARTICLE_FORCE ||
            emitter->turbulence_frequency < 0.0F || emitter->turbulence_frequency > 100.0F ||
            fabsf(emitter->radial_force) > AF_MAX_PARTICLE_FORCE ||
            fabsf(emitter->vortex_force) > AF_MAX_PARTICLE_FORCE ||
            fabsf(emitter->attractor_strength) > AF_MAX_PARTICLE_FORCE ||
            emitter->attractor_radius < 0.0F ||
            (emitter->attractor_strength != 0.0F && emitter->attractor_radius <= 0.0F) ||
            fabsf(emitter->repulsor_strength) > AF_MAX_PARTICLE_FORCE ||
            emitter->repulsor_radius < 0.0F ||
            (emitter->repulsor_strength != 0.0F && emitter->repulsor_radius <= 0.0F) ||
            fabsf(emitter->vector_field_strength) > AF_MAX_PARTICLE_FORCE ||
            (emitter->vector_field_strength != 0.0F &&
             emitter->vector_field_direction.x * emitter->vector_field_direction.x +
                     emitter->vector_field_direction.y * emitter->vector_field_direction.y <
                 0.000001F) ||
            emitter->collision_restitution < 0.0F || emitter->collision_restitution > 1.0F ||
            emitter->collision_friction < 0.0F || emitter->collision_friction > 1.0F ||
            (emitter->collision_plane_enabled &&
             emitter->collision_plane_normal.x * emitter->collision_plane_normal.x +
                     emitter->collision_plane_normal.y * emitter->collision_plane_normal.y <
                 0.000001F) ||
            emitter->depth_max < emitter->depth_min ||
            emitter->depth_velocity_max < emitter->depth_velocity_min ||
            emitter->trail_length < 0.0F || emitter->trail_length > 100000.0F ||
            emitter->trail_width < 0.0F || emitter->trail_width > 100000.0F ||
            emitter->mesh_scale <= 0.0F || emitter->mesh_scale > 1000.0F ||
            emitter->soft_fade_distance < 0.0F || emitter->light_radius < 0.0F ||
            ((emitter->output == AF_PARTICLE_OUTPUT_TRAIL ||
              emitter->output == AF_PARTICLE_OUTPUT_RIBBON) &&
             (emitter->trail_length <= 0.0F || emitter->trail_width <= 0.0F)) ||
            (emitter->output == AF_PARTICLE_OUTPUT_SOFT && emitter->soft_fade_distance <= 0.0F) ||
            (emitter->output == AF_PARTICLE_OUTPUT_LIGHT_PROXY && emitter->light_radius <= 0.0F) ||
            emitter->sub_emit_birth_count > AF_MAX_PARTICLE_SUB_EMIT_COUNT ||
            emitter->sub_emit_death_count > AF_MAX_PARTICLE_SUB_EMIT_COUNT ||
            emitter->max_recursion_depth > AF_MAX_PARTICLE_RECURSION_DEPTH ||
            emitter->event_burst_count > emitter->capacity ||
            !af_particle_finite_color(emitter->color_start) ||
            !af_particle_finite_color(emitter->color_end)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t value_index = 0U; value_index < sizeof(values) / sizeof(values[0]);
             ++value_index) {
            if (!isfinite(values[value_index])) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        if (emitter->type == AF_PARTICLE_EMITTER_VECTOR_PATH) {
            const AfPath *path =
                scene->paths == NULL ? NULL : af_path_find_const(scene->paths, emitter->path_id);
            if (path == NULL) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        } else if (emitter->path_id != 0U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (emitter->sub_emitter_id == 0U) {
            if (emitter->sub_emit_birth_count != 0U || emitter->sub_emit_death_count != 0U) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        } else {
            bool found = false;
            for (uint32_t target = 0U; target < data->count; ++target) {
                if (data->emitters[target].id == emitter->sub_emitter_id) {
                    found = true;
                    break;
                }
            }
            if (!found || emitter->max_recursion_depth == 0U) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (uint32_t other = index + 1U; other < data->count; ++other) {
            if (emitter->id == data->emitters[other].id ||
                emitter->node_id == data->emitters[other].node_id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfVec2 af_particle_direction(AfVec2 value) {
    const float length = sqrtf(value.x * value.x + value.y * value.y);
    return length > 0.00001F ? (AfVec2){value.x / length, value.y / length} : (AfVec2){0};
}

AfVec2 af_particle_force_sample(const AfParticleEmitter *emitter, AfVec2 position, float time) {
    AfVec2 force = {0};
    AfVec2 delta;
    AfVec2 direction;
    float distance = 0.0F;
    if (emitter == NULL || !isfinite(position.x) || !isfinite(position.y) || !isfinite(time)) {
        return force;
    }
    force = (AfVec2){emitter->gravity.x + emitter->wind.x, emitter->gravity.y + emitter->wind.y};
    if (emitter->turbulence_strength != 0.0F) {
        const float frequency = emitter->turbulence_frequency;
        force.x += cosf(position.y * frequency + time) * emitter->turbulence_strength;
        force.y += sinf(position.x * frequency - time) * emitter->turbulence_strength;
    }
    delta = (AfVec2){position.x - emitter->force_center.x, position.y - emitter->force_center.y};
    direction = af_particle_direction(delta);
    force.x += direction.x * emitter->radial_force - direction.y * emitter->vortex_force;
    force.y += direction.y * emitter->radial_force + direction.x * emitter->vortex_force;
    delta = (AfVec2){emitter->attractor_position.x - position.x,
                     emitter->attractor_position.y - position.y};
    distance = sqrtf(delta.x * delta.x + delta.y * delta.y);
    if (emitter->attractor_radius > 0.0F && distance < emitter->attractor_radius) {
        direction = af_particle_direction(delta);
        const float amount = 1.0F - distance / emitter->attractor_radius;
        force.x += direction.x * emitter->attractor_strength * amount;
        force.y += direction.y * emitter->attractor_strength * amount;
    }
    delta = (AfVec2){position.x - emitter->repulsor_position.x,
                     position.y - emitter->repulsor_position.y};
    distance = sqrtf(delta.x * delta.x + delta.y * delta.y);
    if (emitter->repulsor_radius > 0.0F && distance < emitter->repulsor_radius) {
        direction = af_particle_direction(delta);
        const float amount = 1.0F - distance / emitter->repulsor_radius;
        force.x += direction.x * emitter->repulsor_strength * amount;
        force.y += direction.y * emitter->repulsor_strength * amount;
    }
    direction = af_particle_direction(emitter->vector_field_direction);
    force.x += direction.x * emitter->vector_field_strength;
    force.y += direction.y * emitter->vector_field_strength;
    distance = sqrtf(force.x * force.x + force.y * force.y);
    if (distance > AF_MAX_PARTICLE_FORCE) {
        force.x *= AF_MAX_PARTICLE_FORCE / distance;
        force.y *= AF_MAX_PARTICLE_FORCE / distance;
    }
    return force;
}

AfResult af_particle_collide_sample(const AfParticleEmitter *emitter, AfVec2 previous,
                                    AfVec2 *position, AfVec2 *velocity, float *depth,
                                    float *depth_velocity) {
    if (emitter == NULL || position == NULL || velocity == NULL || depth == NULL ||
        depth_velocity == NULL || !isfinite(previous.x) || !isfinite(previous.y) ||
        !isfinite(position->x) || !isfinite(position->y) || !isfinite(velocity->x) ||
        !isfinite(velocity->y) || !isfinite(*depth) || !isfinite(*depth_velocity)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (emitter->collision_plane_enabled) {
        const AfVec2 normal = af_particle_direction(emitter->collision_plane_normal);
        const float distance =
            position->x * normal.x + position->y * normal.y - emitter->collision_plane_offset;
        if (distance < 0.0F) {
            const float normal_speed = velocity->x * normal.x + velocity->y * normal.y;
            position->x -= normal.x * distance;
            position->y -= normal.y * distance;
            if (normal_speed < 0.0F) {
                velocity->x -= (1.0F + emitter->collision_restitution) * normal_speed * normal.x;
                velocity->y -= (1.0F + emitter->collision_restitution) * normal_speed * normal.y;
                velocity->x *= 1.0F - emitter->collision_friction;
                velocity->y *= 1.0F - emitter->collision_friction;
            }
        }
    }
    if (emitter->depth_collision_enabled) {
        if (*depth < emitter->depth_min) {
            *depth = emitter->depth_min;
            *depth_velocity = fabsf(*depth_velocity) * emitter->collision_restitution;
        } else if (*depth > emitter->depth_max) {
            *depth = emitter->depth_max;
            *depth_velocity = -fabsf(*depth_velocity) * emitter->collision_restitution;
        }
    }
    (void)previous;
    return AF_RESULT_SUCCESS;
}

uint32_t af_particle_random_u32(uint32_t seed, uint64_t emitter_id, uint32_t serial,
                                uint32_t lane) {
    uint32_t value = seed ^ (uint32_t)emitter_id ^ (uint32_t)(emitter_id >> 32U);
    value ^= serial * 0x9e3779b9U;
    value ^= lane * 0x85ebca6bU;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

float af_particle_random_unit(uint32_t seed, uint64_t emitter_id, uint32_t serial, uint32_t lane) {
    return (float)(af_particle_random_u32(seed, emitter_id, serial, lane) >> 8U) *
           (1.0F / 16777216.0F);
}

static float af_particle_lerp(float start, float end, float amount) {
    return start + (end - start) * amount;
}

AfResult af_particle_spawn_sample(const AfParticleEmitter *emitter, uint32_t serial,
                                  AfParticleSpawnSample *out_sample) {
    float x = 0.0F;
    float y = 0.0F;
    if (emitter == NULL || out_sample == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    x = af_particle_random_unit(emitter->seed, emitter->id, serial, 0U);
    y = af_particle_random_unit(emitter->seed, emitter->id, serial, 1U);
    *out_sample = (AfParticleSpawnSample){
        .position = {af_particle_lerp(emitter->position_min.x, emitter->position_max.x, x),
                     af_particle_lerp(emitter->position_min.y, emitter->position_max.y, y)},
        .velocity =
            {af_particle_lerp(emitter->velocity_min.x, emitter->velocity_max.x,
                              af_particle_random_unit(emitter->seed, emitter->id, serial, 2U)),
             af_particle_lerp(emitter->velocity_min.y, emitter->velocity_max.y,
                              af_particle_random_unit(emitter->seed, emitter->id, serial, 3U))},
        .rotation =
            af_particle_lerp(emitter->rotation_min, emitter->rotation_max,
                             af_particle_random_unit(emitter->seed, emitter->id, serial, 4U)),
        .angular_velocity =
            af_particle_lerp(emitter->angular_velocity_min, emitter->angular_velocity_max,
                             af_particle_random_unit(emitter->seed, emitter->id, serial, 5U)),
        .size_start =
            af_particle_lerp(emitter->size_start_min, emitter->size_start_max,
                             af_particle_random_unit(emitter->seed, emitter->id, serial, 6U)),
        .size_end =
            af_particle_lerp(emitter->size_end_min, emitter->size_end_max,
                             af_particle_random_unit(emitter->seed, emitter->id, serial, 7U)),
        .lifetime =
            af_particle_lerp(emitter->lifetime_min, emitter->lifetime_max,
                             af_particle_random_unit(emitter->seed, emitter->id, serial, 8U))};
    return AF_RESULT_SUCCESS;
}
