#ifndef AF_ARCFLOW_RENDERER_ADAPTER_H
#define AF_ARCFLOW_RENDERER_ADAPTER_H

#include "af/vulkan.h"
#include "arcflow/arcflow_runtime.h"

/* Arcflow-specific adaptation stays in the application, outside the framework. */
AfRuntimeCreateInfo af_vk_renderer_runtime_create_info(const AfVkRenderer *renderer);

#endif
