#ifndef AF_STATE_MACHINE_H
#define AF_STATE_MACHINE_H

#include "animation/animation.h"
#include "arcflow/arcflow_result.h"
#include "component/component.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_STATE_MACHINES 64U
#define AF_MAX_STATES 1024U
#define AF_MAX_TRANSITIONS 4096U
#define AF_MAX_STATE_INPUTS 512U
#define AF_MAX_TRANSITION_CONDITIONS 8192U
#define AF_MAX_NESTED_STATE_MACHINES 8U
#define AF_MAX_TRANSITIONS_PER_UPDATE 32U

typedef enum AfStateInputType {
    AF_STATE_INPUT_BOOLEAN = 0,
    AF_STATE_INPUT_INTEGER,
    AF_STATE_INPUT_FLOAT,
    AF_STATE_INPUT_TRIGGER
} AfStateInputType;

typedef union AfStateInputValue {
    bool boolean;
    int32_t integer;
    float real;
} AfStateInputValue;

typedef enum AfStateType {
    AF_STATE_ENTRY = 0,
    AF_STATE_EXIT,
    AF_STATE_ANIMATION,
    AF_STATE_NESTED_MACHINE
} AfStateType;

typedef enum AfStateComparison {
    AF_STATE_COMPARE_EQUAL = 0,
    AF_STATE_COMPARE_NOT_EQUAL,
    AF_STATE_COMPARE_LESS,
    AF_STATE_COMPARE_LESS_EQUAL,
    AF_STATE_COMPARE_GREATER,
    AF_STATE_COMPARE_GREATER_EQUAL
} AfStateComparison;

enum {
    AF_TRANSITION_REQUIRE_EXIT_TIME = 1U << 0U,
    AF_TRANSITION_REQUIRE_COMPLETION = 1U << 1U,
    AF_TRANSITION_REQUIRE_EVENT = 1U << 2U
};

typedef struct AfStateMachineDefinition {
    uint64_t id;
    uint64_t owner_component_id;
    uint64_t entry_state_id;
    uint32_t state_start;
    uint32_t state_count;
    uint32_t transition_start;
    uint32_t transition_count;
} AfStateMachineDefinition;

typedef struct AfStateDefinition {
    uint64_t id;
    uint64_t machine_id;
    uint64_t clip_id;
    uint64_t nested_machine_id;
    uint64_t tag_id;
    AfStateType type;
} AfStateDefinition;

typedef struct AfStateInputDefinition {
    uint64_t machine_id;
    uint64_t id;
    AfStateInputType type;
    AfStateInputValue default_value;
} AfStateInputDefinition;

typedef struct AfTransitionCondition {
    uint64_t input_id;
    AfStateInputType type;
    AfStateComparison comparison;
    AfStateInputValue value;
} AfTransitionCondition;

typedef struct AfStateTransition {
    uint64_t source_state_id;
    uint64_t target_state_id;
    uint64_t event_id;
    uint32_t condition_start;
    uint32_t condition_count;
    int32_t priority;
    uint32_t flags;
    float duration;
    float exit_time;
    float easing[4];
} AfStateTransition;

typedef struct AfStateMachineData {
    AfStateMachineDefinition *machines;
    uint32_t machine_count;
    uint32_t machine_capacity;
    AfStateDefinition *states;
    uint32_t state_count;
    uint32_t state_capacity;
    AfStateTransition *transitions;
    uint32_t transition_count;
    uint32_t transition_capacity;
    AfStateInputDefinition *inputs;
    uint32_t input_count;
    uint32_t input_capacity;
    AfTransitionCondition *conditions;
    uint32_t condition_count;
    uint32_t condition_capacity;
} AfStateMachineData;

typedef struct AfStateMachineStatus {
    uint64_t machine_id;
    uint64_t current_state_id;
    uint64_t pending_state_id;
    float state_time;
    float transition_time;
    float transition_duration;
    uint32_t transitions_last_update;
    bool exited;
} AfStateMachineStatus;

typedef struct AfStateInputState {
    uint64_t id;
    AfStateInputType type;
    AfStateInputValue value;
    bool triggered;
} AfStateInputState;

typedef struct AfStateMachineRuntime {
    const AfStateMachineData *data;
    const AfAnimationData *animation;
    AfStateMachineStatus *statuses;
    uint32_t status_count;
    uint32_t status_capacity;
    AfStateInputState *inputs;
    uint32_t input_count;
    uint32_t input_capacity;
    uint64_t event_id;
} AfStateMachineRuntime;

AfResult af_state_machine_validate(const AfStateMachineData *data, const AfAnimationData *animation,
                                   const AfComponentData *components);
AfResult af_state_machine_runtime_init(AfStateMachineRuntime *runtime,
                                       const AfStateMachineData *data,
                                       const AfAnimationData *animation,
                                       AfStateMachineStatus *statuses, uint32_t status_capacity,
                                       AfStateInputState *inputs, uint32_t input_capacity);
AfResult af_state_machine_update(AfStateMachineRuntime *runtime, float delta_time);
AfResult af_state_machine_set_boolean(AfStateMachineRuntime *runtime, uint64_t input_id,
                                      bool value);
AfResult af_state_machine_set_integer(AfStateMachineRuntime *runtime, uint64_t input_id,
                                      int32_t value);
AfResult af_state_machine_set_float(AfStateMachineRuntime *runtime, uint64_t input_id, float value);
AfResult af_state_machine_fire_trigger(AfStateMachineRuntime *runtime, uint64_t input_id);
AfResult af_state_machine_notify_event(AfStateMachineRuntime *runtime, uint64_t event_id);
AfResult af_state_machine_debug(const AfStateMachineRuntime *runtime, uint64_t machine_id,
                                AfStateMachineStatus *out_status);

#endif
