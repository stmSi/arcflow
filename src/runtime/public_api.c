#include "arcflow/arcflow_runtime.h"

#include "animation/animation.h"
#include "assets/package.h"
#include "binding/binding.h"
#include "component/component.h"
#include "core/allocator.h"
#include "core/handles.h"
#include "interaction/interaction.h"
#include "runtime/runtime_internal.h"
#include "state_machine/state_machine.h"

#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static AfResult af_runtime_package(AfRuntime *runtime, AfPackageHandle handle,
                                   AfLoadedPackage **out_package, uint32_t *out_index) {
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (runtime == NULL || out_package == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_handle_validate(&runtime->package_pool, handle, &index);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    *out_package = &runtime->packages[index].package;
    if (out_index != NULL) {
        *out_index = index;
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_runtime_instance(AfRuntime *runtime, AfInstanceHandle handle,
                                    AfRuntimeInstanceSlot **out_instance, uint32_t *out_index) {
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (runtime == NULL || out_instance == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_handle_validate(&runtime->instance_pool, handle, &index);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    *out_instance = &runtime->instances[index];
    if (out_index != NULL) {
        *out_index = index;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_runtime_create(const AfRuntimeCreateInfo *create_info, AfRuntime **out_runtime) {
    AfAllocator allocator = {0};
    AfRuntime *runtime = NULL;
    const bool any_vulkan =
        create_info != NULL &&
        (create_info->instance != VK_NULL_HANDLE ||
         create_info->physical_device != VK_NULL_HANDLE || create_info->device != VK_NULL_HANDLE ||
         create_info->graphics_queue != VK_NULL_HANDLE);
    const bool complete_vulkan = create_info != NULL && create_info->instance != VK_NULL_HANDLE &&
                                 create_info->physical_device != VK_NULL_HANDLE &&
                                 create_info->device != VK_NULL_HANDLE &&
                                 create_info->graphics_queue != VK_NULL_HANDLE;
    if (create_info == NULL || out_runtime == NULL || create_info->frames_in_flight == 0U ||
        create_info->frames_in_flight > AF_MAX_FRAMES_IN_FLIGHT ||
        (any_vulkan && !complete_vulkan)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_runtime = NULL;
    af_allocator_init_default(&allocator);
    runtime = af_allocate(&allocator, sizeof(*runtime), alignof(AfRuntime));
    if (runtime == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->allocator = allocator;
    runtime->create_info = *create_info;
    af_handle_pool_init(&runtime->package_pool, runtime->package_handles, AF_RUNTIME_MAX_PACKAGES);
    af_handle_pool_init(&runtime->instance_pool, runtime->instance_handles,
                        AF_RUNTIME_MAX_INSTANCES);
    *out_runtime = runtime;
    return AF_RESULT_SUCCESS;
}

void af_runtime_destroy(AfRuntime *runtime) {
    uint32_t index = 0U;
    AfAllocator allocator = {0};
    if (runtime == NULL) {
        return;
    }
    for (index = 0U; index < AF_RUNTIME_MAX_INSTANCES; ++index) {
        if (runtime->instance_handles[index].occupied != 0U) {
            af_deallocate(&runtime->allocator, runtime->instances[index].scene.nodes);
            af_deallocate(&runtime->allocator, runtime->instances[index].paths.paths);
            af_deallocate(&runtime->allocator, runtime->instances[index].paths.commands);
            af_deallocate(&runtime->allocator, runtime->instances[index].image_bindings);
            af_deallocate(&runtime->allocator, runtime->instances[index].binding_values);
        }
    }
    for (index = 0U; index < AF_RUNTIME_MAX_PACKAGES; ++index) {
        if (runtime->package_handles[index].occupied != 0U) {
            af_loaded_package_destroy(&runtime->allocator, &runtime->packages[index].package);
        }
    }
    allocator = runtime->allocator;
    af_deallocate(&allocator, runtime);
}

AfResult af_package_load(AfRuntime *runtime, const void *data, size_t data_size,
                         AfPackageHandle *out_package) {
    AfPackageHandle handle = 0U;
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (runtime == NULL || out_package == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_package = 0U;
    result = af_handle_allocate(&runtime->package_pool, &handle);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    index = (uint32_t)handle - 1U;
    result =
        af_package_decode(data, data_size, &runtime->allocator, &runtime->packages[index].package);
    if (result != AF_RESULT_SUCCESS) {
        (void)af_handle_release(&runtime->package_pool, handle);
        return result;
    }
    *out_package = handle;
    return AF_RESULT_SUCCESS;
}

AfResult af_package_destroy(AfRuntime *runtime, AfPackageHandle package) {
    AfLoadedPackage *loaded = NULL;
    uint32_t package_index = 0U;
    uint32_t index = 0U;
    AfResult result = af_runtime_package(runtime, package, &loaded, &package_index);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    for (index = 0U; index < AF_RUNTIME_MAX_INSTANCES; ++index) {
        if (runtime->instance_handles[index].occupied != 0U &&
            runtime->instances[index].package_handle == package) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    af_loaded_package_destroy(&runtime->allocator, loaded);
    runtime->packages[package_index] = (AfRuntimePackageSlot){0};
    return af_handle_release(&runtime->package_pool, package);
}

AfResult af_instance_create(AfRuntime *runtime, AfPackageHandle package,
                            AfInstanceHandle *out_instance) {
    AfLoadedPackage *loaded = NULL;
    AfInstanceHandle handle = 0U;
    AfRuntimeInstanceSlot *slot = NULL;
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (runtime == NULL || out_instance == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_instance = 0U;
    result = af_runtime_package(runtime, package, &loaded, NULL);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    result = af_handle_allocate(&runtime->instance_pool, &handle);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    index = (uint32_t)handle - 1U;
    slot = &runtime->instances[index];
    *slot = (AfRuntimeInstanceSlot){.package_handle = package,
                                    .playback = {.speed = 1.0F, .direction = 1},
                                    .canvas = {.width = 1280.0F, .height = 720.0F}};
    slot->scene.nodes = af_allocate_array(&runtime->allocator, loaded->scene.count,
                                          sizeof(AfSceneNode), alignof(AfSceneNode));
    slot->paths.paths = af_allocate_array(&runtime->allocator, loaded->path_data.path_count,
                                          sizeof(AfPath), alignof(AfPath));
    slot->paths.commands = af_allocate_array(&runtime->allocator, loaded->path_data.command_count,
                                             sizeof(AfPathCommand), alignof(AfPathCommand));
    slot->image_bindings = af_allocate_array(&runtime->allocator, loaded->image_data.binding_count,
                                             sizeof(AfImageBinding), alignof(AfImageBinding));
    slot->binding_values =
        af_allocate_array(&runtime->allocator, loaded->binding_data.property_count,
                          sizeof(AfBindingValue), alignof(AfBindingValue));
    if ((loaded->scene.count != 0U && slot->scene.nodes == NULL) ||
        (loaded->path_data.path_count != 0U && slot->paths.paths == NULL) ||
        (loaded->path_data.command_count != 0U && slot->paths.commands == NULL) ||
        (loaded->image_data.binding_count != 0U && slot->image_bindings == NULL) ||
        (loaded->binding_data.property_count != 0U && slot->binding_values == NULL)) {
        af_deallocate(&runtime->allocator, slot->binding_values);
        af_deallocate(&runtime->allocator, slot->image_bindings);
        af_deallocate(&runtime->allocator, slot->paths.commands);
        af_deallocate(&runtime->allocator, slot->paths.paths);
        af_deallocate(&runtime->allocator, slot->scene.nodes);
        (void)af_handle_release(&runtime->instance_pool, handle);
        *slot = (AfRuntimeInstanceSlot){0};
        return AF_RESULT_OUT_OF_MEMORY;
    }
    slot->scene.count = loaded->scene.count;
    slot->scene.capacity = loaded->scene.count;
    slot->paths.path_count = loaded->path_data.path_count;
    slot->paths.path_capacity = loaded->path_data.path_count;
    slot->paths.command_count = loaded->path_data.command_count;
    slot->paths.command_capacity = loaded->path_data.command_count;
    if (slot->paths.path_count != 0U) {
        memcpy(slot->paths.paths, loaded->path_data.paths,
               (size_t)slot->paths.path_count * sizeof(AfPath));
    }
    if (slot->paths.command_count != 0U) {
        memcpy(slot->paths.commands, loaded->path_data.commands,
               (size_t)slot->paths.command_count * sizeof(AfPathCommand));
    }
    slot->scene.paths = &slot->paths;
    slot->scene.styles = &loaded->style_data;
    slot->particles = loaded->particle_data;
    slot->particles.emitters = slot->particle_emitters;
    slot->particles.capacity = AF_MAX_PARTICLE_EMITTERS;
    if (loaded->particle_data.count != 0U) {
        memcpy(slot->particle_emitters, loaded->particle_data.emitters,
               (size_t)loaded->particle_data.count * sizeof(AfParticleEmitter));
    }
    slot->scene.particles = &slot->particles;
    slot->images = loaded->image_data;
    slot->images.bindings = slot->image_bindings;
    slot->images.binding_capacity = loaded->image_data.binding_count;
    slot->images.allocator = NULL;
    if (loaded->image_data.binding_count != 0U) {
        memcpy(slot->image_bindings, loaded->image_data.bindings,
               (size_t)loaded->image_data.binding_count * sizeof(AfImageBinding));
    }
    slot->scene.images = &slot->images;
    for (uint32_t property_index = 0U; property_index < loaded->binding_data.property_count;
         ++property_index) {
        slot->binding_values[property_index] =
            loaded->binding_data.properties[property_index].default_value;
    }
    slot->layout = loaded->layout_data;
    slot->scene.layouts = &slot->layout;
    if (loaded->scene.count != 0U) {
        memcpy(slot->scene.nodes, loaded->base_nodes,
               (size_t)loaded->scene.count * sizeof(AfSceneNode));
    }
    result = af_scene_evaluate_world(&slot->scene);
    if (result == AF_RESULT_SUCCESS) {
        result = af_component_runtime_init(&slot->components, &loaded->component_data,
                                           &loaded->animation, slot->component_states,
                                           AF_MAX_COMPONENT_INSTANCES);
    }
    if (result == AF_RESULT_SUCCESS) {
        (void)af_accessibility_tree_init(&slot->accessibility, slot->accessibility_nodes,
                                         AF_MAX_INTERACTION_NODES);
        af_accessibility_settings_default(&slot->accessibility_settings);
        for (uint32_t node_index = 0U; node_index < slot->scene.count; ++node_index) {
            const bool interactive = slot->scene.nodes[node_index].type != AF_NODE_GROUP;
            slot->interaction_nodes[node_index] =
                (AfInteractionNode){.node_id = slot->scene.nodes[node_index].id,
                                    .enabled = interactive,
                                    .focusable = interactive,
                                    .opacity_threshold = 0.001F};
            if (interactive) {
                const AfAccessibilityNode accessibility = {
                    .node_id = slot->scene.nodes[node_index].id,
                    .focus_order = node_index,
                    .role = slot->scene.nodes[node_index].type == AF_NODE_IMAGE
                                ? AF_ACCESSIBILITY_ROLE_IMAGE
                                : AF_ACCESSIBILITY_ROLE_BUTTON,
                    .focusable = true};
                (void)af_accessibility_set_node(&slot->accessibility, &accessibility);
            }
        }
        af_interaction_init(&slot->interaction, slot->interaction_nodes, slot->scene.count);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_state_machine_runtime_init(&slot->state_machines, &loaded->state_machine_data,
                                               &loaded->animation, slot->state_machine_statuses,
                                               AF_MAX_STATE_MACHINES, slot->state_machine_inputs,
                                               AF_MAX_STATE_INPUTS);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_deallocate(&runtime->allocator, slot->paths.commands);
        af_deallocate(&runtime->allocator, slot->paths.paths);
        af_deallocate(&runtime->allocator, slot->scene.nodes);
        af_deallocate(&runtime->allocator, slot->image_bindings);
        af_deallocate(&runtime->allocator, slot->binding_values);
        (void)af_handle_release(&runtime->instance_pool, handle);
        *slot = (AfRuntimeInstanceSlot){0};
        return result;
    }
    *out_instance = handle;
    return AF_RESULT_SUCCESS;
}

AfResult af_instance_destroy(AfRuntime *runtime, AfInstanceHandle instance) {
    AfRuntimeInstanceSlot *slot = NULL;
    uint32_t index = 0U;
    AfResult result = af_runtime_instance(runtime, instance, &slot, &index);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    af_deallocate(&runtime->allocator, slot->scene.nodes);
    af_deallocate(&runtime->allocator, slot->paths.paths);
    af_deallocate(&runtime->allocator, slot->paths.commands);
    af_deallocate(&runtime->allocator, slot->image_bindings);
    af_deallocate(&runtime->allocator, slot->binding_values);
    runtime->instances[index] = (AfRuntimeInstanceSlot){0};
    return af_handle_release(&runtime->instance_pool, instance);
}

AfResult af_instance_update(AfRuntime *runtime, AfInstanceHandle instance, float delta_time) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfLoadedPackage *loaded = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result != AF_RESULT_SUCCESS || !isfinite(delta_time)) {
        return result != AF_RESULT_SUCCESS ? result : AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_runtime_package(runtime, slot->package_handle, &loaded, NULL);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    af_allocator_begin_frame(&runtime->allocator);
    if (slot->accessibility_settings.reduced_motion == AF_REDUCED_MOTION_DISABLE) {
        delta_time = 0.0F;
    } else if (slot->accessibility_settings.reduced_motion == AF_REDUCED_MOTION_SIMPLIFY) {
        delta_time = fminf(delta_time, 1.0F / 30.0F);
    }
    if (loaded->scene.count != 0U) {
        memcpy(slot->scene.nodes, loaded->base_nodes,
               (size_t)loaded->scene.count * sizeof(AfSceneNode));
    }
    if (loaded->path_data.path_count != 0U) {
        memcpy(slot->paths.paths, loaded->path_data.paths,
               (size_t)loaded->path_data.path_count * sizeof(AfPath));
    }
    if (loaded->path_data.command_count != 0U) {
        memcpy(slot->paths.commands, loaded->path_data.commands,
               (size_t)loaded->path_data.command_count * sizeof(AfPathCommand));
    }
    result = af_runtime_binding_apply_all(slot, loaded, true);
    if (result == AF_RESULT_SUCCESS && loaded->animation.clip_count != 0U) {
        const AfAnimationClip *clip = &loaded->animation.clips[slot->active_clip_index];
        af_playback_update(&slot->playback, clip, delta_time);
        result = af_animation_apply(&loaded->animation, clip, slot->playback.time, &slot->scene);
    } else if (result == AF_RESULT_SUCCESS) {
        result = af_scene_evaluate_world(&slot->scene);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_runtime_binding_apply_all(slot, loaded, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_scene_evaluate_world(&slot->scene);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_component_runtime_update(&slot->components, delta_time);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_state_machine_update(&slot->state_machines, delta_time);
    }
    runtime->last_frame_allocations = af_allocator_end_frame(&runtime->allocator);
    return result;
}

AfResult af_instance_play(AfRuntime *runtime, AfInstanceHandle instance) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS) {
        af_playback_play(&slot->playback);
    }
    return result;
}

AfResult af_instance_pause(AfRuntime *runtime, AfInstanceHandle instance) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS) {
        af_playback_pause(&slot->playback);
    }
    return result;
}

AfResult af_instance_stop(AfRuntime *runtime, AfInstanceHandle instance) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS) {
        af_playback_stop(&slot->playback);
    }
    return result;
}

AfResult af_instance_seek(AfRuntime *runtime, AfInstanceHandle instance, float time_seconds) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfLoadedPackage *loaded = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    result = af_runtime_package(runtime, slot->package_handle, &loaded, NULL);
    if (result != AF_RESULT_SUCCESS || loaded->animation.clip_count == 0U) {
        return result != AF_RESULT_SUCCESS ? result : AF_RESULT_NOT_FOUND;
    }
    af_playback_seek(&slot->playback, &loaded->animation.clips[slot->active_clip_index],
                     time_seconds);
    return af_instance_update(runtime, instance, 0.0F);
}

AfResult af_instance_set_playback_speed(AfRuntime *runtime, AfInstanceHandle instance,
                                        float speed) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS && isfinite(speed)) {
        slot->playback.speed = speed;
    } else if (result == AF_RESULT_SUCCESS) {
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_instance_set_direction(AfRuntime *runtime, AfInstanceHandle instance,
                                   AfPlaybackDirection direction) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS &&
        (direction == AF_PLAYBACK_FORWARD || direction == AF_PLAYBACK_REVERSE)) {
        slot->playback.direction = (int)direction;
    } else if (result == AF_RESULT_SUCCESS) {
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_instance_resize(AfRuntime *runtime, AfInstanceHandle instance,
                            const AfCanvasInfo *canvas) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS && canvas != NULL && canvas->width > 0.0F &&
        canvas->height > 0.0F) {
        slot->canvas = *canvas;
        slot->layout.target_size = (AfVec2){canvas->width, canvas->height};
        result = af_scene_evaluate_world(&slot->scene);
    } else if (result == AF_RESULT_SUCCESS) {
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_instance_set_vfx_quality(AfRuntime *runtime, AfInstanceHandle instance,
                                     AfVfxQuality quality, float view_distance) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result != AF_RESULT_SUCCESS || quality < AF_VFX_QUALITY_DISABLED ||
        quality > AF_VFX_QUALITY_HIGH || !isfinite(view_distance) || view_distance < 0.0F) {
        return result != AF_RESULT_SUCCESS ? result : AF_RESULT_INVALID_ARGUMENT;
    }
    slot->particles.lod.host_quality =
        quality == AF_VFX_QUALITY_DISABLED
            ? 0.0F
            : (quality == AF_VFX_QUALITY_LOW ? 0.35F
                                             : (quality == AF_VFX_QUALITY_MEDIUM ? 0.65F : 1.0F));
    slot->particles.lod.view_distance = view_distance;
    return AF_RESULT_SUCCESS;
}

AfResult af_instance_trigger_vfx(AfRuntime *runtime, AfInstanceHandle instance, uint64_t emitter_id,
                                 uint32_t burst_count) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result != AF_RESULT_SUCCESS || emitter_id == 0U || burst_count == 0U) {
        return result != AF_RESULT_SUCCESS ? result : AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < slot->particles.count; ++index) {
        AfParticleEmitter *emitter = &slot->particles.emitters[index];
        if (emitter->id == emitter_id) {
            if (burst_count > emitter->capacity) {
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            emitter->event_burst_count = burst_count;
            emitter->event_sequence =
                emitter->event_sequence == UINT32_MAX ? 1U : emitter->event_sequence + 1U;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

uint64_t af_runtime_frame_allocation_count(const AfRuntime *runtime) {
    return runtime != NULL ? runtime->last_frame_allocations : 0U;
}

AfResult af_instance_pointer_move(AfRuntime *runtime, AfInstanceHandle instance, float x, float y,
                                  double timestamp) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    return result == AF_RESULT_SUCCESS
               ? af_interaction_pointer_move(&slot->interaction, &slot->scene, x, y, timestamp)
               : result;
}

AfResult af_instance_pointer_down(AfRuntime *runtime, AfInstanceHandle instance, float x, float y,
                                  double timestamp) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    return result == AF_RESULT_SUCCESS
               ? af_interaction_pointer_down(&slot->interaction, &slot->scene, x, y, timestamp)
               : result;
}

AfResult af_instance_pointer_up(AfRuntime *runtime, AfInstanceHandle instance, float x, float y,
                                double timestamp) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    return result == AF_RESULT_SUCCESS
               ? af_interaction_pointer_up(&slot->interaction, &slot->scene, x, y, timestamp)
               : result;
}

AfResult af_instance_keyboard_activate(AfRuntime *runtime, AfInstanceHandle instance,
                                       double timestamp) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    return result == AF_RESULT_SUCCESS
               ? af_interaction_keyboard_activate(&slot->interaction, timestamp)
               : result;
}

AfResult af_instance_controller_navigate(AfRuntime *runtime, AfInstanceHandle instance,
                                         int32_t direction, double timestamp) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    return result == AF_RESULT_SUCCESS ? af_interaction_controller_navigate(
                                             &slot->interaction, &slot->scene, direction, timestamp)
                                       : result;
}

AfResult af_instance_queue_animation_event(AfRuntime *runtime, AfInstanceHandle instance,
                                           uint64_t name_id, uint64_t node_id, double timestamp) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_interaction_queue_animation_event(&slot->interaction, name_id, node_id, timestamp);
    }
    if (result == AF_RESULT_SUCCESS && slot->state_machines.status_count != 0U) {
        result = af_state_machine_notify_event(&slot->state_machines, name_id);
    }
    return result;
}

AfResult af_instance_poll_event(AfRuntime *runtime, AfInstanceHandle instance,
                                AfRuntimeEvent *out_event) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    return result == AF_RESULT_SUCCESS ? af_interaction_poll(&slot->interaction, out_event)
                                       : result;
}

AfResult af_instance_set_boolean_input(AfRuntime *runtime, AfInstanceHandle instance,
                                       uint64_t input_id, int value) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result != AF_RESULT_SUCCESS || (value != 0 && value != 1)) {
        return result != AF_RESULT_SUCCESS ? result : AF_RESULT_INVALID_ARGUMENT;
    }
    return af_state_machine_set_boolean(&slot->state_machines, input_id, value != 0);
}

AfResult af_instance_set_integer_input(AfRuntime *runtime, AfInstanceHandle instance,
                                       uint64_t input_id, int32_t value) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    return result == AF_RESULT_SUCCESS
               ? af_state_machine_set_integer(&slot->state_machines, input_id, value)
               : result;
}

AfResult af_instance_set_float_input(AfRuntime *runtime, AfInstanceHandle instance,
                                     uint64_t input_id, float value) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    return result == AF_RESULT_SUCCESS
               ? af_state_machine_set_float(&slot->state_machines, input_id, value)
               : result;
}

AfResult af_instance_fire_trigger(AfRuntime *runtime, AfInstanceHandle instance,
                                  uint64_t input_id) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    return result == AF_RESULT_SUCCESS
               ? af_state_machine_fire_trigger(&slot->state_machines, input_id)
               : result;
}

AfPropertyId af_property_id(const char *name) { return af_binding_name_id(name); }

AfResult af_instance_property_type(AfRuntime *runtime, AfInstanceHandle instance,
                                   AfPropertyId property_id, AfRuntimePropertyType *out_type) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfLoadedPackage *loaded = NULL;
    const AfExposedProperty *property = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (out_type == NULL || property_id == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS) {
        result = af_runtime_package(runtime, slot->package_handle, &loaded, NULL);
    }
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    property = af_binding_find_const(&loaded->binding_data, property_id);
    if (property == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_type = (AfRuntimePropertyType)property->type;
    return AF_RESULT_SUCCESS;
}

AfResult af_instance_set_property(AfRuntime *runtime, AfInstanceHandle instance,
                                  AfPropertyId property_id, AfRuntimePropertyType type,
                                  const AfRuntimePropertyValue *value) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfLoadedPackage *loaded = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS) {
        result = af_runtime_package(runtime, slot->package_handle, &loaded, NULL);
    }
    return result == AF_RESULT_SUCCESS
               ? af_runtime_binding_set(slot, loaded, property_id, type, value)
               : result;
}

AfResult af_instance_get_property(AfRuntime *runtime, AfInstanceHandle instance,
                                  AfPropertyId property_id, AfRuntimePropertyType type,
                                  AfRuntimePropertyValue *out_value) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfLoadedPackage *loaded = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS) {
        result = af_runtime_package(runtime, slot->package_handle, &loaded, NULL);
    }
    return result == AF_RESULT_SUCCESS
               ? af_runtime_binding_get(slot, loaded, property_id, type, out_value)
               : result;
}

AfResult af_instance_format_property(AfRuntime *runtime, AfInstanceHandle instance,
                                     AfPropertyId property_id, char *output,
                                     size_t output_capacity) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfLoadedPackage *loaded = NULL;
    const AfExposedProperty *property = NULL;
    uint32_t index = 0U;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS) {
        result = af_runtime_package(runtime, slot->package_handle, &loaded, NULL);
    }
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    property = af_binding_find_const(&loaded->binding_data, property_id);
    if (property == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    index = (uint32_t)(property - loaded->binding_data.properties);
    return af_binding_format_value(property->type, &slot->binding_values[index], &property->format,
                                   output, output_capacity);
}

AfResult af_instance_set_locale(AfRuntime *runtime, AfInstanceHandle instance, uint64_t locale_id) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result != AF_RESULT_SUCCESS || locale_id == 0U) {
        return result != AF_RESULT_SUCCESS ? result : AF_RESULT_INVALID_ARGUMENT;
    }
    slot->locale_id = locale_id;
    return AF_RESULT_SUCCESS;
}

AfResult af_instance_set_accessibility(AfRuntime *runtime, AfInstanceHandle instance,
                                       const AfRuntimeAccessibilitySettings *settings) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfAccessibilitySettings converted = {0};
    AfResult result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result != AF_RESULT_SUCCESS || settings == NULL ||
        (settings->high_contrast != 0 && settings->high_contrast != 1)) {
        return result != AF_RESULT_SUCCESS ? result : AF_RESULT_INVALID_ARGUMENT;
    }
    converted = (AfAccessibilitySettings){
        .text_scale = settings->text_scale,
        .reduced_motion = (AfReducedMotionMode)settings->reduced_motion,
        .color_blind_preview = (AfColorBlindPreview)settings->color_blind_preview,
        .high_contrast = settings->high_contrast != 0};
    result = af_accessibility_settings_validate(&converted);
    if (result == AF_RESULT_SUCCESS) {
        slot->accessibility_settings = converted;
        if (converted.reduced_motion == AF_REDUCED_MOTION_DISABLE) {
            af_playback_pause(&slot->playback);
        }
    }
    return result;
}

AfResult af_instance_accessibility_count(AfRuntime *runtime, AfInstanceHandle instance,
                                         uint32_t *out_count) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (out_count == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS) {
        *out_count = slot->accessibility.count;
    }
    return result;
}

AfResult af_instance_accessibility_info(AfRuntime *runtime, AfInstanceHandle instance,
                                        uint32_t index, AfRuntimeAccessibilityInfo *out_info) {
    AfRuntimeInstanceSlot *slot = NULL;
    const AfAccessibilityNode *node = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (out_info == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    if (index >= slot->accessibility.count) {
        return AF_RESULT_NOT_FOUND;
    }
    node = &slot->accessibility.nodes[index];
    *out_info = (AfRuntimeAccessibilityInfo){.node_id = node->node_id,
                                             .states = node->states,
                                             .focus_order = node->focus_order,
                                             .role = (AfRuntimeAccessibilityRole)node->role,
                                             .focusable = node->focusable ? 1 : 0};
    memcpy(out_info->label, node->label, sizeof(out_info->label));
    memcpy(out_info->description, node->description, sizeof(out_info->description));
    return AF_RESULT_SUCCESS;
}

AfResult af_instance_get_scene(AfRuntime *runtime, AfInstanceHandle instance,
                               const AfScene **out_scene) {
    AfRuntimeInstanceSlot *slot = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (out_scene == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_scene = NULL;
    result = af_runtime_instance(runtime, instance, &slot, NULL);
    if (result == AF_RESULT_SUCCESS) {
        *out_scene = &slot->scene;
    }
    return result;
}
