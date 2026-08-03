#ifndef ARCFLOW_RUNTIME_H
#define ARCFLOW_RUNTIME_H

#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#include "arcflow/arcflow_events.h"
#include "arcflow/arcflow_result.h"
#include "arcflow/arcflow_version.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AF_MAX_FRAMES_IN_FLIGHT 2U

typedef struct AfRuntime AfRuntime;
typedef uint64_t AfPackageHandle;
typedef uint64_t AfInstanceHandle;

typedef struct AfRuntimeCreateInfo {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family;
    uint32_t frames_in_flight;
} AfRuntimeCreateInfo;

typedef struct AfCanvasInfo {
    float width;
    float height;
    float safe_left;
    float safe_top;
    float safe_right;
    float safe_bottom;
} AfCanvasInfo;

typedef enum AfPlaybackDirection {
    AF_PLAYBACK_FORWARD = 1,
    AF_PLAYBACK_REVERSE = -1
} AfPlaybackDirection;

AfResult af_runtime_create(const AfRuntimeCreateInfo *create_info, AfRuntime **out_runtime);
void af_runtime_destroy(AfRuntime *runtime);

AfResult af_package_load(AfRuntime *runtime, const void *data, size_t data_size,
                         AfPackageHandle *out_package);
AfResult af_package_destroy(AfRuntime *runtime, AfPackageHandle package);

AfResult af_instance_create(AfRuntime *runtime, AfPackageHandle package,
                            AfInstanceHandle *out_instance);
AfResult af_instance_destroy(AfRuntime *runtime, AfInstanceHandle instance);
AfResult af_instance_update(AfRuntime *runtime, AfInstanceHandle instance, float delta_time);
AfResult af_instance_play(AfRuntime *runtime, AfInstanceHandle instance);
AfResult af_instance_pause(AfRuntime *runtime, AfInstanceHandle instance);
AfResult af_instance_stop(AfRuntime *runtime, AfInstanceHandle instance);
AfResult af_instance_seek(AfRuntime *runtime, AfInstanceHandle instance, float time_seconds);
AfResult af_instance_set_playback_speed(AfRuntime *runtime, AfInstanceHandle instance, float speed);
AfResult af_instance_set_direction(AfRuntime *runtime, AfInstanceHandle instance,
                                   AfPlaybackDirection direction);
AfResult af_instance_resize(AfRuntime *runtime, AfInstanceHandle instance,
                            const AfCanvasInfo *canvas);
AfResult af_instance_pointer_move(AfRuntime *runtime, AfInstanceHandle instance, float x, float y,
                                  double timestamp);
AfResult af_instance_pointer_down(AfRuntime *runtime, AfInstanceHandle instance, float x, float y,
                                  double timestamp);
AfResult af_instance_pointer_up(AfRuntime *runtime, AfInstanceHandle instance, float x, float y,
                                double timestamp);
AfResult af_instance_keyboard_activate(AfRuntime *runtime, AfInstanceHandle instance,
                                       double timestamp);
AfResult af_instance_controller_navigate(AfRuntime *runtime, AfInstanceHandle instance,
                                         int32_t direction, double timestamp);
AfResult af_instance_queue_animation_event(AfRuntime *runtime, AfInstanceHandle instance,
                                           uint64_t name_id, uint64_t node_id, double timestamp);
AfResult af_instance_poll_event(AfRuntime *runtime, AfInstanceHandle instance,
                                AfRuntimeEvent *out_event);
AfResult af_instance_set_boolean_input(AfRuntime *runtime, AfInstanceHandle instance,
                                       uint64_t input_id, int value);
AfResult af_instance_set_integer_input(AfRuntime *runtime, AfInstanceHandle instance,
                                       uint64_t input_id, int32_t value);
AfResult af_instance_set_float_input(AfRuntime *runtime, AfInstanceHandle instance,
                                     uint64_t input_id, float value);
AfResult af_instance_fire_trigger(AfRuntime *runtime, AfInstanceHandle instance, uint64_t input_id);

uint64_t af_runtime_frame_allocation_count(const AfRuntime *runtime);

#ifdef __cplusplus
}
#endif

#endif
