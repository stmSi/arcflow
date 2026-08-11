#ifndef AF_VK_RENDERER_H
#define AF_VK_RENDERER_H

#include "arcflow/arcflow_runtime.h"
#include "platform/platform_sdl.h"
#include "renderer/vk_particles.h"
#include "scene/scene.h"

#include <stdbool.h>

typedef struct AfVkRenderer AfVkRenderer;

typedef struct AfVkRenderOptions {
    const AfScene *particle_scene;
    AfAffine2 particle_transform;
    int32_t particle_layer_order;
    uint32_t particle_clip_x;
    uint32_t particle_clip_y;
    uint32_t particle_clip_width;
    uint32_t particle_clip_height;
    float image_time;
    bool particle_clip_enabled;
    bool image_time_override;
} AfVkRenderOptions;

AfResult af_vk_renderer_create(AfPlatformWindow *window, AfVkRenderer **out_renderer);
void af_vk_renderer_destroy(AfVkRenderer *renderer);
AfRuntimeCreateInfo af_vk_renderer_runtime_create_info(const AfVkRenderer *renderer);
AfResult af_vk_renderer_draw(AfVkRenderer *renderer, const AfScene *scene);
AfResult af_vk_renderer_draw_timed(AfVkRenderer *renderer, const AfScene *scene, float delta_time);
AfResult af_vk_renderer_draw_timed_with_options(AfVkRenderer *renderer, const AfScene *scene,
                                                float delta_time, const AfVkRenderOptions *options);
AfResult af_vk_renderer_capture_next(AfVkRenderer *renderer, const char *path);
void af_vk_renderer_request_resize(AfVkRenderer *renderer);
bool af_vk_renderer_has_validation_errors(const AfVkRenderer *renderer);
AfVkVfxMetrics af_vk_renderer_vfx_metrics(const AfVkRenderer *renderer);

#endif
