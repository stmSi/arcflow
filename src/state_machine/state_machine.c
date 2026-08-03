#include "state_machine/state_machine.h"

#include <math.h>
#include <string.h>

static bool af_range_valid(uint32_t start, uint32_t count, uint32_t total) {
    return start <= total && count <= total - start;
}

static const AfStateMachineDefinition *af_machine(const AfStateMachineData *data, uint64_t id) {
    if (data == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->machine_count; ++index) {
        if (data->machines[index].id == id) {
            return &data->machines[index];
        }
    }
    return NULL;
}

static const AfStateDefinition *af_state(const AfStateMachineData *data, uint64_t id) {
    if (data == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->state_count; ++index) {
        if (data->states[index].id == id) {
            return &data->states[index];
        }
    }
    return NULL;
}

static const AfAnimationClip *af_state_clip(const AfAnimationData *animation, uint64_t id) {
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

static bool af_component_exists(const AfComponentData *components, uint64_t id) {
    if (id == 0U) {
        return true;
    }
    for (uint32_t index = 0U; components != NULL && index < components->definition_count; ++index) {
        if (components->definitions[index].id == id) {
            return true;
        }
    }
    return false;
}

static bool af_state_input_value_valid(AfStateInputType type, AfStateInputValue value) {
    return type <= AF_STATE_INPUT_TRIGGER && (type != AF_STATE_INPUT_FLOAT || isfinite(value.real));
}

static AfResult af_validate_nested(const AfStateMachineData *data, uint64_t machine_id,
                                   uint64_t *stack, uint32_t depth) {
    const AfStateMachineDefinition *machine = af_machine(data, machine_id);
    if (machine == NULL || depth >= AF_MAX_NESTED_STATE_MACHINES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < depth; ++index) {
        if (stack[index] == machine_id) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    stack[depth] = machine_id;
    for (uint32_t index = 0U; index < machine->state_count; ++index) {
        const AfStateDefinition *state = &data->states[machine->state_start + index];
        if (state->type == AF_STATE_NESTED_MACHINE &&
            af_validate_nested(data, state->nested_machine_id, stack, depth + 1U) !=
                AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_state_machine_validate(const AfStateMachineData *data, const AfAnimationData *animation,
                                   const AfComponentData *components) {
    uint64_t stack[AF_MAX_NESTED_STATE_MACHINES] = {0};
    if (data == NULL || animation == NULL || data->machine_count > AF_MAX_STATE_MACHINES ||
        data->state_count > AF_MAX_STATES || data->transition_count > AF_MAX_TRANSITIONS ||
        data->input_count > AF_MAX_STATE_INPUTS ||
        data->condition_count > AF_MAX_TRANSITION_CONDITIONS ||
        data->machine_count > data->machine_capacity || data->state_count > data->state_capacity ||
        data->transition_count > data->transition_capacity ||
        data->input_count > data->input_capacity ||
        data->condition_count > data->condition_capacity ||
        (data->machine_count != 0U && data->machines == NULL) ||
        (data->state_count != 0U && data->states == NULL) ||
        (data->transition_count != 0U && data->transitions == NULL) ||
        (data->input_count != 0U && data->inputs == NULL) ||
        (data->condition_count != 0U && data->conditions == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->machine_count; ++index) {
        const AfStateMachineDefinition *machine = &data->machines[index];
        const AfStateDefinition *entry = af_state(data, machine->entry_state_id);
        if (machine->id == 0U || entry == NULL || entry->machine_id != machine->id ||
            !af_component_exists(components, machine->owner_component_id) ||
            !af_range_valid(machine->state_start, machine->state_count, data->state_count) ||
            !af_range_valid(machine->transition_start, machine->transition_count,
                            data->transition_count)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < data->machine_count; ++other) {
            if (machine->id == data->machines[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (uint32_t state_index = 0U; state_index < machine->state_count; ++state_index) {
            const AfStateDefinition *state = &data->states[machine->state_start + state_index];
            if (state->id == 0U || state->machine_id != machine->id ||
                state->type > AF_STATE_NESTED_MACHINE ||
                (state->type == AF_STATE_ANIMATION &&
                 af_state_clip(animation, state->clip_id) == NULL) ||
                (state->type == AF_STATE_NESTED_MACHINE &&
                 af_machine(data, state->nested_machine_id) == NULL)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            for (uint32_t other = 0U; other < data->state_count; ++other) {
                if (machine->state_start + state_index != other &&
                    state->id == data->states[other].id) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
            }
        }
        for (uint32_t transition_index = 0U; transition_index < machine->transition_count;
             ++transition_index) {
            const AfStateTransition *transition =
                &data->transitions[machine->transition_start + transition_index];
            const AfStateDefinition *source = af_state(data, transition->source_state_id);
            const AfStateDefinition *target = af_state(data, transition->target_state_id);
            if ((transition->source_state_id != 0U &&
                 (source == NULL || source->machine_id != machine->id)) ||
                target == NULL || target->machine_id != machine->id ||
                !af_range_valid(transition->condition_start, transition->condition_count,
                                data->condition_count) ||
                transition->flags > 7U || !isfinite(transition->duration) ||
                transition->duration < 0.0F || !isfinite(transition->exit_time) ||
                transition->exit_time < 0.0F ||
                ((transition->flags & AF_TRANSITION_REQUIRE_EVENT) != 0U &&
                 transition->event_id == 0U)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            for (uint32_t easing = 0U; easing < 4U; ++easing) {
                if (!isfinite(transition->easing[easing])) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
            }
        }
        if (af_validate_nested(data, machine->id, stack, 0U) != AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < data->input_count; ++index) {
        const AfStateInputDefinition *input = &data->inputs[index];
        if (input->id == 0U || af_machine(data, input->machine_id) == NULL ||
            !af_state_input_value_valid(input->type, input->default_value)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < data->input_count; ++other) {
            if (input->id == data->inputs[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (uint32_t index = 0U; index < data->condition_count; ++index) {
        const AfTransitionCondition *condition = &data->conditions[index];
        const AfStateInputDefinition *input = NULL;
        for (uint32_t input_index = 0U; input_index < data->input_count; ++input_index) {
            if (data->inputs[input_index].id == condition->input_id) {
                input = &data->inputs[input_index];
                break;
            }
        }
        if (input == NULL || input->type != condition->type ||
            condition->comparison > AF_STATE_COMPARE_GREATER_EQUAL ||
            !af_state_input_value_valid(condition->type, condition->value) ||
            ((condition->type == AF_STATE_INPUT_BOOLEAN ||
              condition->type == AF_STATE_INPUT_TRIGGER) &&
             condition->comparison > AF_STATE_COMPARE_NOT_EQUAL)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_state_machine_runtime_init(AfStateMachineRuntime *runtime,
                                       const AfStateMachineData *data,
                                       const AfAnimationData *animation,
                                       AfStateMachineStatus *statuses, uint32_t status_capacity,
                                       AfStateInputState *inputs, uint32_t input_capacity) {
    if (runtime == NULL || data == NULL || animation == NULL ||
        status_capacity < data->machine_count || input_capacity < data->input_count ||
        (status_capacity != 0U && statuses == NULL) || (input_capacity != 0U && inputs == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *runtime = (AfStateMachineRuntime){.data = data,
                                       .animation = animation,
                                       .statuses = statuses,
                                       .status_count = data->machine_count,
                                       .status_capacity = status_capacity,
                                       .inputs = inputs,
                                       .input_count = data->input_count,
                                       .input_capacity = input_capacity};
    for (uint32_t index = 0U; index < data->machine_count; ++index) {
        statuses[index] =
            (AfStateMachineStatus){.machine_id = data->machines[index].id,
                                   .current_state_id = data->machines[index].entry_state_id};
    }
    for (uint32_t index = 0U; index < data->input_count; ++index) {
        inputs[index] = (AfStateInputState){.id = data->inputs[index].id,
                                            .type = data->inputs[index].type,
                                            .value = data->inputs[index].default_value};
    }
    return AF_RESULT_SUCCESS;
}

static AfStateInputState *af_runtime_input(AfStateMachineRuntime *runtime, uint64_t id,
                                           AfStateInputType type) {
    if (runtime == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < runtime->input_count; ++index) {
        if (runtime->inputs[index].id == id && runtime->inputs[index].type == type) {
            return &runtime->inputs[index];
        }
    }
    return NULL;
}

static bool af_compare(const AfStateInputState *input, const AfTransitionCondition *condition) {
    double left = 0.0;
    double right = 0.0;
    if (input->type == AF_STATE_INPUT_TRIGGER) {
        left = input->triggered ? 1.0 : 0.0;
        right = condition->value.boolean ? 1.0 : 0.0;
    } else if (input->type == AF_STATE_INPUT_BOOLEAN) {
        left = input->value.boolean ? 1.0 : 0.0;
        right = condition->value.boolean ? 1.0 : 0.0;
    } else if (input->type == AF_STATE_INPUT_INTEGER) {
        left = (double)input->value.integer;
        right = (double)condition->value.integer;
    } else {
        left = (double)input->value.real;
        right = (double)condition->value.real;
    }
    switch (condition->comparison) {
    case AF_STATE_COMPARE_EQUAL:
        return left == right;
    case AF_STATE_COMPARE_NOT_EQUAL:
        return left != right;
    case AF_STATE_COMPARE_LESS:
        return left < right;
    case AF_STATE_COMPARE_LESS_EQUAL:
        return left <= right;
    case AF_STATE_COMPARE_GREATER:
        return left > right;
    case AF_STATE_COMPARE_GREATER_EQUAL:
        return left >= right;
    }
    return false;
}

static bool af_transition_ready(AfStateMachineRuntime *runtime, const AfStateMachineStatus *status,
                                const AfStateTransition *transition) {
    const AfStateDefinition *state = af_state(runtime->data, status->current_state_id);
    const AfAnimationClip *clip =
        state == NULL ? NULL : af_state_clip(runtime->animation, state->clip_id);
    if ((transition->flags & AF_TRANSITION_REQUIRE_EXIT_TIME) != 0U &&
        status->state_time < transition->exit_time) {
        return false;
    }
    if ((transition->flags & AF_TRANSITION_REQUIRE_COMPLETION) != 0U &&
        (clip == NULL || status->state_time < clip->duration)) {
        return false;
    }
    if ((transition->flags & AF_TRANSITION_REQUIRE_EVENT) != 0U &&
        runtime->event_id != transition->event_id) {
        return false;
    }
    for (uint32_t index = 0U; index < transition->condition_count; ++index) {
        const AfTransitionCondition *condition =
            &runtime->data->conditions[transition->condition_start + index];
        const AfStateInputState *input =
            af_runtime_input(runtime, condition->input_id, condition->type);
        if (input == NULL || !af_compare(input, condition)) {
            return false;
        }
    }
    return true;
}

static void af_transition_consume_triggers(AfStateMachineRuntime *runtime,
                                           const AfStateTransition *transition) {
    for (uint32_t index = 0U; index < transition->condition_count; ++index) {
        const AfTransitionCondition *condition =
            &runtime->data->conditions[transition->condition_start + index];
        if (condition->type == AF_STATE_INPUT_TRIGGER) {
            AfStateInputState *input =
                af_runtime_input(runtime, condition->input_id, AF_STATE_INPUT_TRIGGER);
            if (input != NULL) {
                input->triggered = false;
            }
        }
    }
}

AfResult af_state_machine_update(AfStateMachineRuntime *runtime, float delta_time) {
    if (runtime == NULL || runtime->data == NULL || !isfinite(delta_time) || delta_time < 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t machine_index = 0U; machine_index < runtime->status_count; ++machine_index) {
        AfStateMachineStatus *status = &runtime->statuses[machine_index];
        const AfStateMachineDefinition *machine = af_machine(runtime->data, status->machine_id);
        status->transitions_last_update = 0U;
        if (status->exited) {
            continue;
        }
        status->state_time += delta_time;
        if (status->pending_state_id != 0U) {
            status->transition_time += delta_time;
            if (status->transition_time < status->transition_duration) {
                continue;
            }
            status->current_state_id = status->pending_state_id;
            status->pending_state_id = 0U;
            status->state_time = 0.0F;
            status->transition_time = 0.0F;
            status->transition_duration = 0.0F;
            {
                const AfStateDefinition *target = af_state(runtime->data, status->current_state_id);
                if (target != NULL && target->type == AF_STATE_EXIT) {
                    status->exited = true;
                    continue;
                }
            }
        }
        while (status->transitions_last_update < AF_MAX_TRANSITIONS_PER_UPDATE) {
            const AfStateTransition *best = NULL;
            for (uint32_t index = 0U; index < machine->transition_count; ++index) {
                const AfStateTransition *candidate =
                    &runtime->data->transitions[machine->transition_start + index];
                if ((candidate->source_state_id == 0U ||
                     candidate->source_state_id == status->current_state_id) &&
                    candidate->target_state_id != status->current_state_id &&
                    af_transition_ready(runtime, status, candidate) &&
                    (best == NULL || candidate->priority > best->priority)) {
                    best = candidate;
                }
            }
            if (best == NULL) {
                break;
            }
            ++status->transitions_last_update;
            af_transition_consume_triggers(runtime, best);
            if (best->duration > 0.0F) {
                status->pending_state_id = best->target_state_id;
                status->transition_duration = best->duration;
                status->transition_time = 0.0F;
                break;
            }
            status->current_state_id = best->target_state_id;
            status->state_time = 0.0F;
            const AfStateDefinition *target = af_state(runtime->data, best->target_state_id);
            if (target != NULL && target->type == AF_STATE_EXIT) {
                status->exited = true;
                break;
            }
        }
        if (status->transitions_last_update == AF_MAX_TRANSITIONS_PER_UPDATE && !status->exited &&
            status->pending_state_id == 0U) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    }
    for (uint32_t index = 0U; index < runtime->input_count; ++index) {
        runtime->inputs[index].triggered = false;
    }
    runtime->event_id = 0U;
    return AF_RESULT_SUCCESS;
}

AfResult af_state_machine_set_boolean(AfStateMachineRuntime *runtime, uint64_t input_id,
                                      bool value) {
    AfStateInputState *input = af_runtime_input(runtime, input_id, AF_STATE_INPUT_BOOLEAN);
    if (input == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    input->value.boolean = value;
    return AF_RESULT_SUCCESS;
}

AfResult af_state_machine_set_integer(AfStateMachineRuntime *runtime, uint64_t input_id,
                                      int32_t value) {
    AfStateInputState *input = af_runtime_input(runtime, input_id, AF_STATE_INPUT_INTEGER);
    if (input == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    input->value.integer = value;
    return AF_RESULT_SUCCESS;
}

AfResult af_state_machine_set_float(AfStateMachineRuntime *runtime, uint64_t input_id,
                                    float value) {
    AfStateInputState *input = af_runtime_input(runtime, input_id, AF_STATE_INPUT_FLOAT);
    if (input == NULL || !isfinite(value)) {
        return input == NULL ? AF_RESULT_NOT_FOUND : AF_RESULT_INVALID_ARGUMENT;
    }
    input->value.real = value;
    return AF_RESULT_SUCCESS;
}

AfResult af_state_machine_fire_trigger(AfStateMachineRuntime *runtime, uint64_t input_id) {
    AfStateInputState *input = af_runtime_input(runtime, input_id, AF_STATE_INPUT_TRIGGER);
    if (input == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    input->triggered = true;
    return AF_RESULT_SUCCESS;
}

AfResult af_state_machine_notify_event(AfStateMachineRuntime *runtime, uint64_t event_id) {
    if (runtime == NULL || event_id == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    runtime->event_id = event_id;
    return AF_RESULT_SUCCESS;
}

AfResult af_state_machine_debug(const AfStateMachineRuntime *runtime, uint64_t machine_id,
                                AfStateMachineStatus *out_status) {
    if (runtime == NULL || out_status == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < runtime->status_count; ++index) {
        if (runtime->statuses[index].machine_id == machine_id) {
            *out_status = runtime->statuses[index];
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}
