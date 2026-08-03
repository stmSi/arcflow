#ifndef AF_VK_RENDERER_H
#define AF_VK_RENDERER_H

#include "arcflow/arcflow_runtime.h"
#include "platform/platform_sdl.h"
#include "scene/scene.h"

#include <stdbool.h>

typedef struct AfVkRenderer AfVkRenderer;

AfResult af_vk_renderer_create(AfPlatformWindow *window, AfVkRenderer **out_renderer);
void af_vk_renderer_destroy(AfVkRenderer *renderer);
AfRuntimeCreateInfo af_vk_renderer_runtime_create_info(const AfVkRenderer *renderer);
AfResult af_vk_renderer_draw(AfVkRenderer *renderer, const AfScene *scene);
void af_vk_renderer_request_resize(AfVkRenderer *renderer);
bool af_vk_renderer_has_validation_errors(const AfVkRenderer *renderer);

#endif
