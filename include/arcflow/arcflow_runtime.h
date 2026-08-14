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
typedef uint64_t AfPropertyId;

#define AF_RUNTIME_PROPERTY_TEXT_CAPACITY 256U

typedef enum AfRuntimePropertyType {
    AF_RUNTIME_PROPERTY_SCALAR = 0,
    AF_RUNTIME_PROPERTY_INTEGER,
    AF_RUNTIME_PROPERTY_BOOLEAN,
    AF_RUNTIME_PROPERTY_COLOR,
    AF_RUNTIME_PROPERTY_TEXT,
    AF_RUNTIME_PROPERTY_IMAGE,
    AF_RUNTIME_PROPERTY_VISIBILITY,
    AF_RUNTIME_PROPERTY_PROGRESS,
    AF_RUNTIME_PROPERTY_ANIMATION
} AfRuntimePropertyType;

typedef struct AfRuntimeColor {
    float r;
    float g;
    float b;
    float a;
} AfRuntimeColor;

typedef union AfRuntimePropertyValue {
    float scalar;
    int32_t integer;
    int boolean;
    AfRuntimeColor color;
    char text[AF_RUNTIME_PROPERTY_TEXT_CAPACITY];
    uint64_t image_id;
    int visibility;
    float progress;
    uint64_t animation_id;
} AfRuntimePropertyValue;

typedef struct AfRuntimeCreateInfo {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family;
    uint32_t frames_in_flight;
} AfRuntimeCreateInfo;

typedef enum AfRuntimeAccessibilityRole {
    AF_RUNTIME_ACCESSIBILITY_NONE = 0,
    AF_RUNTIME_ACCESSIBILITY_BUTTON,
    AF_RUNTIME_ACCESSIBILITY_CHECKBOX,
    AF_RUNTIME_ACCESSIBILITY_SLIDER,
    AF_RUNTIME_ACCESSIBILITY_TEXT,
    AF_RUNTIME_ACCESSIBILITY_IMAGE,
    AF_RUNTIME_ACCESSIBILITY_LINK,
    AF_RUNTIME_ACCESSIBILITY_LIST,
    AF_RUNTIME_ACCESSIBILITY_LIST_ITEM,
    AF_RUNTIME_ACCESSIBILITY_TEXT_FIELD,
    AF_RUNTIME_ACCESSIBILITY_MENU_ITEM,
    AF_RUNTIME_ACCESSIBILITY_PROGRESS
} AfRuntimeAccessibilityRole;

typedef struct AfRuntimeAccessibilityInfo {
    uint64_t node_id;
    char label[128];
    char description[256];
    uint32_t states;
    uint32_t focus_order;
    AfRuntimeAccessibilityRole role;
    int focusable;
} AfRuntimeAccessibilityInfo;

typedef struct AfRuntimeAccessibilitySettings {
    float text_scale;
    uint32_t reduced_motion;
    uint32_t color_blind_preview;
    int high_contrast;
} AfRuntimeAccessibilitySettings;

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

typedef enum AfVfxQuality {
    AF_VFX_QUALITY_DISABLED = 0,
    AF_VFX_QUALITY_LOW = 1,
    AF_VFX_QUALITY_MEDIUM = 2,
    AF_VFX_QUALITY_HIGH = 3
} AfVfxQuality;

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
AfResult af_instance_set_vfx_quality(AfRuntime *runtime, AfInstanceHandle instance,
                                     AfVfxQuality quality, float view_distance);
AfResult af_instance_trigger_vfx(AfRuntime *runtime, AfInstanceHandle instance, uint64_t emitter_id,
                                 uint32_t burst_count);
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

AfPropertyId af_property_id(const char *name);
AfResult af_instance_property_type(AfRuntime *runtime, AfInstanceHandle instance,
                                   AfPropertyId property_id, AfRuntimePropertyType *out_type);
AfResult af_instance_set_property(AfRuntime *runtime, AfInstanceHandle instance,
                                  AfPropertyId property_id, AfRuntimePropertyType type,
                                  const AfRuntimePropertyValue *value);
AfResult af_instance_get_property(AfRuntime *runtime, AfInstanceHandle instance,
                                  AfPropertyId property_id, AfRuntimePropertyType type,
                                  AfRuntimePropertyValue *out_value);
AfResult af_instance_format_property(AfRuntime *runtime, AfInstanceHandle instance,
                                     AfPropertyId property_id, char *output,
                                     size_t output_capacity);
AfResult af_instance_set_locale(AfRuntime *runtime, AfInstanceHandle instance, uint64_t locale_id);
AfResult af_instance_set_accessibility(AfRuntime *runtime, AfInstanceHandle instance,
                                       const AfRuntimeAccessibilitySettings *settings);
AfResult af_instance_accessibility_count(AfRuntime *runtime, AfInstanceHandle instance,
                                         uint32_t *out_count);
AfResult af_instance_accessibility_info(AfRuntime *runtime, AfInstanceHandle instance,
                                        uint32_t index, AfRuntimeAccessibilityInfo *out_info);

uint64_t af_runtime_frame_allocation_count(const AfRuntime *runtime);

#ifdef __cplusplus
}
#endif

#endif
