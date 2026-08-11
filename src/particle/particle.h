#ifndef AF_PARTICLE_H
#define AF_PARTICLE_H

#include "arcflow/arcflow_result.h"
#include "core/math.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_PARTICLE_EMITTERS 64U
#define AF_MAX_PARTICLES 65536U
#define AF_MAX_PARTICLES_PER_EMITTER 65536U
#define AF_MAX_PARTICLE_PATH_POINTS 4096U
#define AF_PARTICLE_FIXED_STEP (1.0F / 60.0F)
#define AF_PARTICLE_MAX_STEPS_PER_FRAME 6U

typedef enum AfParticleEmitterType {
    AF_PARTICLE_EMITTER_POINT = 0,
    AF_PARTICLE_EMITTER_LINE = 1,
    AF_PARTICLE_EMITTER_RECTANGLE = 2,
    AF_PARTICLE_EMITTER_CIRCLE = 3,
    AF_PARTICLE_EMITTER_VECTOR_PATH = 4
} AfParticleEmitterType;

typedef enum AfParticleOutputType {
    AF_PARTICLE_OUTPUT_BILLBOARD = 0,
    AF_PARTICLE_OUTPUT_UI_BILLBOARD = 1,
    AF_PARTICLE_OUTPUT_FLIPBOOK = 2
} AfParticleOutputType;

typedef struct AfParticleEmitter {
    uint64_t id;
    uint64_t node_id;
    uint64_t path_id;
    AfParticleEmitterType type;
    AfParticleOutputType output;
    uint32_t capacity;
    uint32_t seed;
    float spawn_rate;
    uint32_t burst_count;
    float lifetime_min;
    float lifetime_max;
    AfVec2 position_min;
    AfVec2 position_max;
    AfVec2 velocity_min;
    AfVec2 velocity_max;
    float rotation_min;
    float rotation_max;
    float angular_velocity_min;
    float angular_velocity_max;
    float size_start_min;
    float size_start_max;
    float size_end_min;
    float size_end_max;
    AfColor color_start;
    AfColor color_end;
    AfVec2 gravity;
    AfVec2 wind;
    float drag;
    uint32_t flipbook_columns;
    uint32_t flipbook_rows;
    float flipbook_frames_per_second;
    bool enabled;
} AfParticleEmitter;

typedef struct AfParticleData {
    AfParticleEmitter *emitters;
    uint32_t count;
    uint32_t capacity;
} AfParticleData;

typedef struct AfParticleSpawnSample {
    AfVec2 position;
    AfVec2 velocity;
    float rotation;
    float angular_velocity;
    float size_start;
    float size_end;
    float lifetime;
} AfParticleSpawnSample;

struct AfScene;

AfParticleEmitter af_particle_emitter_default(uint64_t id, uint64_t node_id);
AfResult af_particle_data_validate(const AfParticleData *data, const struct AfScene *scene);
uint32_t af_particle_total_capacity(const AfParticleData *data);
uint32_t af_particle_random_u32(uint32_t seed, uint64_t emitter_id, uint32_t serial, uint32_t lane);
float af_particle_random_unit(uint32_t seed, uint64_t emitter_id, uint32_t serial, uint32_t lane);
AfResult af_particle_spawn_sample(const AfParticleEmitter *emitter, uint32_t serial,
                                  AfParticleSpawnSample *out_sample);

#endif
