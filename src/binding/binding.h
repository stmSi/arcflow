#ifndef AF_BINDING_H
#define AF_BINDING_H

#include "animation/animation.h"
#include "arcflow/arcflow_result.h"
#include "assets/image.h"
#include "component/component.h"
#include "particle/particle.h"
#include "scene/scene.h"
#include "state_machine/state_machine.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_MAX_EXPOSED_PROPERTIES 1024U
#define AF_MAX_BINDING_TARGETS 4096U
#define AF_BINDING_NAME_CAPACITY 64U
#define AF_BINDING_TEXT_CAPACITY 256U
#define AF_BINDING_AFFIX_CAPACITY 32U
#define AF_BINDING_PLURAL_CAPACITY 32U
#define AF_BINDING_FORMAT_OUTPUT_CAPACITY 512U
#define AF_BINDING_MAX_DECIMAL_PRECISION 6U

typedef enum AfBindingValueType {
    AF_BINDING_VALUE_SCALAR = 0,
    AF_BINDING_VALUE_INTEGER,
    AF_BINDING_VALUE_BOOLEAN,
    AF_BINDING_VALUE_COLOR,
    AF_BINDING_VALUE_TEXT,
    AF_BINDING_VALUE_IMAGE,
    AF_BINDING_VALUE_VISIBILITY,
    AF_BINDING_VALUE_PROGRESS,
    AF_BINDING_VALUE_ANIMATION
} AfBindingValueType;

typedef union AfBindingValue {
    float scalar;
    int32_t integer;
    bool boolean;
    AfColor color;
    char text[AF_BINDING_TEXT_CAPACITY];
    uint64_t image_id;
    bool visibility;
    float progress;
    uint64_t animation_id;
} AfBindingValue;

typedef enum AfBindingFormatMode {
    AF_BINDING_FORMAT_DEFAULT = 0,
    AF_BINDING_FORMAT_INTEGER,
    AF_BINDING_FORMAT_DECIMAL,
    AF_BINDING_FORMAT_PERCENTAGE,
    AF_BINDING_FORMAT_TIME
} AfBindingFormatMode;

typedef struct AfBindingFormatSpec {
    AfBindingFormatMode mode;
    uint32_t precision;
    char prefix[AF_BINDING_AFFIX_CAPACITY];
    char suffix[AF_BINDING_AFFIX_CAPACITY];
    char singular[AF_BINDING_PLURAL_CAPACITY];
    char plural[AF_BINDING_PLURAL_CAPACITY];
} AfBindingFormatSpec;

typedef enum AfBindingTargetType {
    AF_BINDING_TARGET_NODE = 0,
    AF_BINDING_TARGET_ANIMATION,
    AF_BINDING_TARGET_PLAYBACK,
    AF_BINDING_TARGET_STATE_INPUT,
    AF_BINDING_TARGET_PARTICLE,
    AF_BINDING_TARGET_TEXT_STORAGE
} AfBindingTargetType;

typedef enum AfBindingTargetProperty {
    AF_BINDING_TARGET_NODE_OPACITY = 0,
    AF_BINDING_TARGET_NODE_FILL_COLOR,
    AF_BINDING_TARGET_NODE_VISIBILITY,
    AF_BINDING_TARGET_NODE_IMAGE,
    AF_BINDING_TARGET_ANIMATION_SELECTION,
    AF_BINDING_TARGET_ANIMATION_PROGRESS,
    AF_BINDING_TARGET_PLAYBACK_PLAYING,
    AF_BINDING_TARGET_PLAYBACK_SPEED,
    AF_BINDING_TARGET_PLAYBACK_TIME,
    AF_BINDING_TARGET_STATE_INPUT_VALUE,
    AF_BINDING_TARGET_PARTICLE_ENABLED,
    AF_BINDING_TARGET_PARTICLE_SPAWN_RATE,
    AF_BINDING_TARGET_PARTICLE_BURST_COUNT,
    AF_BINDING_TARGET_PARTICLE_LIFETIME_MIN,
    AF_BINDING_TARGET_PARTICLE_LIFETIME_MAX,
    AF_BINDING_TARGET_PARTICLE_SIZE_START,
    AF_BINDING_TARGET_PARTICLE_SIZE_END,
    AF_BINDING_TARGET_PARTICLE_COLOR_START,
    AF_BINDING_TARGET_PARTICLE_COLOR_END,
    AF_BINDING_TARGET_PARTICLE_TURBULENCE,
    AF_BINDING_TARGET_TEXT_VALUE
} AfBindingTargetProperty;

typedef struct AfBindingTarget {
    AfBindingTargetType type;
    AfBindingTargetProperty property;
    uint64_t object_id;
    uint64_t member_id;
    uint32_t text_capacity;
} AfBindingTarget;

typedef struct AfExposedProperty {
    uint64_t id;
    AfBindingValueType type;
    AfBindingValue default_value;
    AfBindingFormatSpec format;
    uint32_t target_start;
    uint32_t target_count;
    char debug_name[AF_BINDING_NAME_CAPACITY];
} AfExposedProperty;

typedef struct AfBindingData {
    AfExposedProperty *properties;
    uint32_t property_count;
    uint32_t property_capacity;
    AfBindingTarget *targets;
    uint32_t target_count;
    uint32_t target_capacity;
} AfBindingData;

typedef struct AfBindingValidationContext {
    const AfScene *scene;
    const AfAnimationData *animation;
    const AfStateMachineData *state_machines;
    const AfComponentData *components;
    const AfImageData *images;
    const AfParticleData *particles;
} AfBindingValidationContext;

uint64_t af_binding_name_id(const char *name);
AfExposedProperty *af_binding_find(AfBindingData *data, uint64_t property_id);
const AfExposedProperty *af_binding_find_const(const AfBindingData *data, uint64_t property_id);
AfResult af_binding_format_spec_validate(AfBindingValueType type,
                                         const AfBindingFormatSpec *format);
AfResult af_binding_value_validate(AfBindingValueType type, const AfBindingValue *value,
                                   const AfBindingValidationContext *context);
AfResult af_binding_format_value(AfBindingValueType type, const AfBindingValue *value,
                                 const AfBindingFormatSpec *format, char *output,
                                 size_t output_capacity);
AfResult af_binding_data_validate(const AfBindingData *data,
                                  const AfBindingValidationContext *context);
AfResult af_binding_validate_update(const AfBindingData *data, uint64_t property_id,
                                    AfBindingValueType type, const AfBindingValue *value,
                                    const AfBindingValidationContext *context);

#endif
