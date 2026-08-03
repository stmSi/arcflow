#ifndef AF_COMPONENT_H
#define AF_COMPONENT_H

#include "animation/animation.h"
#include "arcflow/arcflow_result.h"
#include "scene/scene.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_COMPONENT_DEFINITIONS 256U
#define AF_MAX_COMPONENT_REFERENCES 1024U
#define AF_MAX_COMPONENT_PROPERTIES 2048U
#define AF_MAX_COMPONENT_INSTANCES 1024U
#define AF_MAX_COMPONENT_OVERRIDES 4096U
#define AF_MAX_COMPONENT_NESTING 16U
#define AF_COMPONENT_TEXT_CAPACITY 64U

typedef enum AfComponentPropertyType {
    AF_COMPONENT_PROPERTY_NUMBER = 0,
    AF_COMPONENT_PROPERTY_BOOLEAN,
    AF_COMPONENT_PROPERTY_COLOR,
    AF_COMPONENT_PROPERTY_IMAGE,
    AF_COMPONENT_PROPERTY_TEXT
} AfComponentPropertyType;

typedef union AfComponentValue {
    float number;
    bool boolean;
    AfColor color;
    uint64_t image_id;
    char text[AF_COMPONENT_TEXT_CAPACITY];
} AfComponentValue;

typedef struct AfComponentReference {
    uint64_t owner_definition_id;
    uint64_t component_id;
} AfComponentReference;

typedef struct AfComponentProperty {
    uint64_t definition_id;
    uint64_t id;
    AfComponentPropertyType type;
    AfComponentValue default_value;
} AfComponentProperty;

typedef struct AfComponentDefinition {
    uint64_t id;
    uint64_t root_node_id;
    uint64_t clip_id;
    uint32_t reference_start;
    uint32_t reference_count;
    uint32_t property_start;
    uint32_t property_count;
} AfComponentDefinition;

typedef struct AfComponentOverride {
    uint64_t instance_id;
    uint64_t property_id;
    AfComponentPropertyType type;
    AfComponentValue value;
} AfComponentOverride;

typedef struct AfComponentInstance {
    uint64_t id;
    uint64_t definition_id;
    uint64_t node_id;
    uint32_t override_start;
    uint32_t override_count;
    float playback_time;
    float playback_speed;
    bool independent_playback;
} AfComponentInstance;

typedef struct AfComponentData {
    AfComponentDefinition *definitions;
    uint32_t definition_count;
    uint32_t definition_capacity;
    AfComponentReference *references;
    uint32_t reference_count;
    uint32_t reference_capacity;
    AfComponentProperty *properties;
    uint32_t property_count;
    uint32_t property_capacity;
    AfComponentInstance *instances;
    uint32_t instance_count;
    uint32_t instance_capacity;
    AfComponentOverride *overrides;
    uint32_t override_count;
    uint32_t override_capacity;
} AfComponentData;

typedef struct AfComponentInstanceState {
    uint64_t instance_id;
    float playback_time;
    bool playing;
} AfComponentInstanceState;

typedef struct AfComponentRuntime {
    const AfComponentData *shared;
    const AfAnimationData *animation;
    AfComponentInstanceState *states;
    uint32_t state_count;
    uint32_t state_capacity;
} AfComponentRuntime;

typedef struct AfDetachedComponent {
    uint64_t source_instance_id;
    uint64_t root_node_id;
    uint64_t clip_id;
    AfComponentOverride overrides[AF_MAX_COMPONENT_PROPERTIES];
    uint32_t override_count;
} AfDetachedComponent;

typedef struct AfComponentEditSession {
    uint64_t definition_id;
    bool active;
} AfComponentEditSession;

AfResult af_component_validate(const AfComponentData *data, const AfScene *scene,
                               const AfAnimationData *animation);
AfResult af_component_runtime_init(AfComponentRuntime *runtime, const AfComponentData *shared,
                                   const AfAnimationData *animation,
                                   AfComponentInstanceState *states, uint32_t state_capacity);
AfResult af_component_runtime_update(AfComponentRuntime *runtime, float delta_time);
AfResult af_component_runtime_set_playing(AfComponentRuntime *runtime, uint64_t instance_id,
                                          bool playing);
AfResult af_component_resolve_property(const AfComponentData *data, uint64_t instance_id,
                                       uint64_t property_id, AfComponentPropertyType type,
                                       AfComponentValue *out_value);
AfResult af_component_detach(const AfComponentData *data, uint64_t instance_id,
                             AfDetachedComponent *out_detached);
AfResult af_component_duplicate_definition(AfComponentData *data, uint64_t source_id,
                                           uint64_t duplicate_id);
AfResult af_component_edit_begin(const AfComponentData *data, uint64_t definition_id,
                                 AfComponentEditSession *session);
void af_component_edit_end(AfComponentEditSession *session);

#endif
