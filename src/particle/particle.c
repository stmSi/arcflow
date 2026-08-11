#include "particle/particle.h"

#include "path/path.h"
#include "scene/scene.h"

#include <math.h>
#include <stddef.h>

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
    if (data == NULL || scene == NULL || data->count > data->capacity ||
        data->capacity > AF_MAX_PARTICLE_EMITTERS ||
        (data->count != 0U && data->emitters == NULL) ||
        af_particle_total_capacity(data) > AF_MAX_PARTICLES) {
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
        };
        if (emitter->id == 0U || emitter->id == UINT64_MAX || emitter->node_id == 0U ||
            af_scene_find_const(scene, emitter->node_id) == NULL ||
            emitter->type > AF_PARTICLE_EMITTER_VECTOR_PATH ||
            emitter->output > AF_PARTICLE_OUTPUT_FLIPBOOK || emitter->capacity == 0U ||
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
        for (uint32_t other = index + 1U; other < data->count; ++other) {
            if (emitter->id == data->emitters[other].id ||
                emitter->node_id == data->emitters[other].node_id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
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
