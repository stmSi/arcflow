#include "renderer/vk_renderer.h"

AfRuntimeCreateInfo af_vk_renderer_runtime_create_info(const AfVkRenderer *renderer) {
#ifdef __EMSCRIPTEN__
    (void)renderer;
    return (AfRuntimeCreateInfo){.frames_in_flight = 2};
#else
    const AfVkDeviceContext context = af_vk_renderer_device_context(renderer);
    return (AfRuntimeCreateInfo){.instance = context.instance,
                                 .physical_device = context.physical_device,
                                 .device = context.device,
                                 .graphics_queue = context.graphics_queue,
                                 .graphics_queue_family = context.graphics_queue_family,
                                 .frames_in_flight = context.frames_in_flight};
#endif
}
