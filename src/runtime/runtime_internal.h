#ifndef AF_RUNTIME_INTERNAL_H
#define AF_RUNTIME_INTERNAL_H

#include "arcflow/arcflow_runtime.h"
#include "assets/package.h"
#include "binding/binding.h"
#include "component/component.h"
#include "core/allocator.h"
#include "core/handles.h"
#include "interaction/interaction.h"
#include "localization/localization.h"
#include "scene/scene.h"
#include "state_machine/state_machine.h"

#define AF_RUNTIME_MAX_PACKAGES 64U
#define AF_RUNTIME_MAX_INSTANCES 128U

typedef struct AfRuntimePackageSlot {
    AfLoadedPackage package;
} AfRuntimePackageSlot;

typedef struct AfRuntimeInstanceSlot {
    AfPackageHandle package_handle;
    AfScene scene;
    AfPlayback playback;
    AfCanvasInfo canvas;
    AfLayoutData layout;
    AfPathData paths;
    AfParticleData particles;
    AfParticleEmitter particle_emitters[AF_MAX_PARTICLE_EMITTERS];
    AfImageData images;
    AfImageBinding *image_bindings;
    AfBindingValue *binding_values;
    uint32_t active_clip_index;
    AfComponentRuntime components;
    AfComponentInstanceState component_states[AF_MAX_COMPONENT_INSTANCES];
    AfInteractionSystem interaction;
    AfInteractionNode interaction_nodes[AF_MAX_INTERACTION_NODES];
    AfStateMachineRuntime state_machines;
    AfStateMachineStatus state_machine_statuses[AF_MAX_STATE_MACHINES];
    AfStateInputState state_machine_inputs[AF_MAX_STATE_INPUTS];
    AfAccessibilityTree accessibility;
    AfAccessibilityNode accessibility_nodes[AF_MAX_INTERACTION_NODES];
    AfAccessibilitySettings accessibility_settings;
    uint64_t locale_id;
} AfRuntimeInstanceSlot;

struct AfRuntime {
    AfAllocator allocator;
    AfRuntimeCreateInfo create_info;
    AfHandlePool package_pool;
    AfHandleSlot package_handles[AF_RUNTIME_MAX_PACKAGES];
    AfRuntimePackageSlot packages[AF_RUNTIME_MAX_PACKAGES];
    AfHandlePool instance_pool;
    AfHandleSlot instance_handles[AF_RUNTIME_MAX_INSTANCES];
    AfRuntimeInstanceSlot instances[AF_RUNTIME_MAX_INSTANCES];
    uint64_t last_frame_allocations;
};

AfResult af_instance_get_scene(AfRuntime *runtime, AfInstanceHandle instance,
                               const AfScene **out_scene);
AfResult af_runtime_binding_apply_all(AfRuntimeInstanceSlot *slot, AfLoadedPackage *loaded,
                                      bool controls);
AfResult af_runtime_binding_set(AfRuntimeInstanceSlot *slot, AfLoadedPackage *loaded,
                                AfPropertyId property_id, AfRuntimePropertyType type,
                                const AfRuntimePropertyValue *value);
AfResult af_runtime_binding_get(const AfRuntimeInstanceSlot *slot, const AfLoadedPackage *loaded,
                                AfPropertyId property_id, AfRuntimePropertyType type,
                                AfRuntimePropertyValue *out_value);

#endif
