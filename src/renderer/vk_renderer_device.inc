static VKAPI_ATTR VkBool32 VKAPI_CALL af_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data) {
    AfVkRenderer *renderer = user_data;
    (void)types;
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U && renderer != NULL) {
        renderer->validation_error = true;
    }
    af_log((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U
               ? AF_LOG_ERROR
               : ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U
                      ? AF_LOG_WARNING
                      : AF_LOG_DEBUG),
           "Vulkan validation: %s", callback_data != NULL ? callback_data->pMessage : "unknown");
    return VK_FALSE;
}

#if defined(AF_ENABLE_VALIDATION)
static bool af_vk_layer_available(const char *name) {
    VkLayerProperties layers[64];
    uint32_t count = 64U;
    uint32_t index = 0U;
    if (vkEnumerateInstanceLayerProperties(&count, layers) != VK_SUCCESS) {
        return false;
    }
    for (index = 0U; index < count; ++index) {
        if (strcmp(layers[index].layerName, name) == 0) {
            return true;
        }
    }
    return false;
}
#endif

static void af_vk_name(AfVkRenderer *renderer, VkObjectType type, uint64_t handle,
                       const char *name) {
    VkDebugUtilsObjectNameInfoEXT info = {.sType =
                                              VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                                          .objectType = type,
                                          .objectHandle = handle,
                                          .pObjectName = name};
    if (renderer->validation_enabled && vkSetDebugUtilsObjectNameEXT != NULL && handle != 0U) {
        (void)vkSetDebugUtilsObjectNameEXT(renderer->device, &info);
    }
}

static AfResult af_vk_create_instance(AfVkRenderer *renderer) {
    const char *extensions[32];
    const char *layers[1] = {"VK_LAYER_KHRONOS_validation"};
    const char *const *sdl_extensions = NULL;
    Uint32 sdl_extension_count = 0U;
    uint32_t extension_count = 0U;
    uint32_t index = 0U;
    VkApplicationInfo application = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                     .pApplicationName = "Arcflow Player",
                                     .applicationVersion = VK_MAKE_API_VERSION(
                                         0, AF_VERSION_MAJOR, AF_VERSION_MINOR, AF_VERSION_PATCH),
                                     .pEngineName = "Arcflow Runtime",
                                     .engineVersion = VK_MAKE_API_VERSION(
                                         0, AF_VERSION_MAJOR, AF_VERSION_MINOR, AF_VERSION_PATCH),
                                     .apiVersion = VK_API_VERSION_1_3};
    VkDebugUtilsMessengerCreateInfoEXT debug_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = af_vk_debug_callback,
        .pUserData = renderer};
    VkInstanceCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                        .pApplicationInfo = &application};
    VkResult vk_result = VK_SUCCESS;
    sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
    if (sdl_extensions == NULL || sdl_extension_count > 31U) {
        af_log(AF_LOG_ERROR, "SDL Vulkan extensions unavailable: %s", SDL_GetError());
        return AF_RESULT_VULKAN_ERROR;
    }
    for (index = 0U; index < sdl_extension_count; ++index) {
        extensions[extension_count++] = sdl_extensions[index];
    }
#if defined(AF_ENABLE_VALIDATION)
    renderer->validation_enabled = af_vk_layer_available(layers[0]);
#endif
    if (renderer->validation_enabled) {
        extensions[extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        create_info.enabledLayerCount = 1U;
        create_info.ppEnabledLayerNames = layers;
        create_info.pNext = &debug_info;
    }
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extensions;
    vk_result = vkCreateInstance(&create_info, NULL, &renderer->instance);
    if (vk_result != VK_SUCCESS) {
        af_log(AF_LOG_ERROR, "vkCreateInstance failed: %d", (int)vk_result);
        return AF_RESULT_VULKAN_ERROR;
    }
    volkLoadInstance(renderer->instance);
    if (renderer->validation_enabled &&
        vkCreateDebugUtilsMessengerEXT(renderer->instance, &debug_info, NULL,
                                       &renderer->debug_messenger) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    return AF_RESULT_SUCCESS;
}

static bool af_vk_device_extension_available(VkPhysicalDevice device, const char *name) {
    VkExtensionProperties extensions[512];
    uint32_t count = 512U;
    uint32_t index = 0U;
    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, extensions) != VK_SUCCESS) {
        return false;
    }
    for (index = 0U; index < count; ++index) {
        if (strcmp(extensions[index].extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool af_vk_find_queues(AfVkRenderer *renderer, VkPhysicalDevice device,
                              uint32_t *out_graphics, uint32_t *out_present) {
    VkQueueFamilyProperties queues[AF_MAX_QUEUE_FAMILIES];
    uint32_t count = AF_MAX_QUEUE_FAMILIES;
    uint32_t index = 0U;
    uint32_t graphics = UINT32_MAX;
    uint32_t present = UINT32_MAX;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queues);
    for (index = 0U; index < count; ++index) {
        VkBool32 present_support = VK_FALSE;
        const bool supports_present =
            vkGetPhysicalDeviceSurfaceSupportKHR(device, index, renderer->surface,
                                                 &present_support) == VK_SUCCESS &&
            present_support == VK_TRUE;
        if ((queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            if (graphics == UINT32_MAX) {
                graphics = index;
            }
            if (supports_present) {
                graphics = index;
                present = index;
                break;
            }
        }
        if (supports_present && present == UINT32_MAX) {
            present = index;
        }
    }
    if (graphics == UINT32_MAX || present == UINT32_MAX) {
        return false;
    }
    *out_graphics = graphics;
    *out_present = present;
    return true;
}

static AfResult af_vk_create_device(AfVkRenderer *renderer) {
    VkPhysicalDevice devices[32];
    uint32_t device_count = 32U;
    uint32_t device_index = 0U;
    float priority = 1.0F;
    VkResult vk_result = vkEnumeratePhysicalDevices(renderer->instance, &device_count, devices);
    if (vk_result != VK_SUCCESS || device_count == 0U) {
        return AF_RESULT_VULKAN_ERROR;
    }
    for (device_index = 0U; device_index < device_count; ++device_index) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceVulkan13Features features13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan12Features features12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &features13};
        VkPhysicalDeviceFeatures2 features = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                              .pNext = &features12};
        uint32_t graphics_family = UINT32_MAX;
        uint32_t present_family = UINT32_MAX;
        bool has_swapchain = false;
        bool has_queues = false;
        vkGetPhysicalDeviceProperties(devices[device_index], &properties);
        vkGetPhysicalDeviceFeatures2(devices[device_index], &features);
        has_swapchain = af_vk_device_extension_available(devices[device_index],
                                                         VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        has_queues =
            af_vk_find_queues(renderer, devices[device_index], &graphics_family, &present_family);
        if (properties.apiVersion < VK_API_VERSION_1_3 || !features13.dynamicRendering ||
            !features13.synchronization2 || !features12.timelineSemaphore || !has_swapchain ||
            !has_queues) {
            af_log(AF_LOG_DEBUG,
                   "Rejected device %s (api=%u dynamic=%u sync2=%u timeline=%u swapchain=%u "
                   "queues=%u)",
                   properties.deviceName, properties.apiVersion, features13.dynamicRendering,
                   features13.synchronization2, features12.timelineSemaphore,
                   has_swapchain ? 1U : 0U, has_queues ? 1U : 0U);
            continue;
        }
        renderer->physical_device = devices[device_index];
        renderer->graphics_family = graphics_family;
        renderer->present_family = present_family;
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            break;
        }
    }
    if (renderer->physical_device == VK_NULL_HANDLE) {
        af_log(AF_LOG_ERROR, "No Vulkan 1.3 device supports dynamic rendering, synchronization2, "
                             "timeline semaphores, and presentation");
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    {
        VkPhysicalDeviceProperties properties;
        VkSampleCountFlags counts = 0U;
        vkGetPhysicalDeviceProperties(renderer->physical_device, &properties);
        VkQueueFamilyProperties queues[AF_MAX_QUEUE_FAMILIES];
        uint32_t queue_count = AF_MAX_QUEUE_FAMILIES;
        vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device, &queue_count, queues);
        if (renderer->graphics_family < queue_count) {
            renderer->timestamp_valid_bits = queues[renderer->graphics_family].timestampValidBits;
            renderer->timestamp_period = properties.limits.timestampPeriod;
        }
        counts = properties.limits.framebufferColorSampleCounts;
        renderer->sample_count =
            (counts & VK_SAMPLE_COUNT_4_BIT) != 0U
                ? VK_SAMPLE_COUNT_4_BIT
                : ((counts & VK_SAMPLE_COUNT_2_BIT) != 0U ? VK_SAMPLE_COUNT_2_BIT
                                                          : VK_SAMPLE_COUNT_1_BIT);
    }
    {
        const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceQueueCreateInfo queue_infos[2] = {
            {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
             .queueFamilyIndex = renderer->graphics_family,
             .queueCount = 1U,
             .pQueuePriorities = &priority}};
        uint32_t queue_count = 1U;
        VkPhysicalDeviceVulkan13Features features13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = VK_TRUE,
            .dynamicRendering = VK_TRUE};
        VkPhysicalDeviceVulkan12Features features12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &features13,
            .timelineSemaphore = VK_TRUE};
        VkDeviceCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                          .pNext = &features12,
                                          .queueCreateInfoCount = queue_count,
                                          .pQueueCreateInfos = queue_infos,
                                          .enabledExtensionCount = 1U,
                                          .ppEnabledExtensionNames = extensions};
        if (renderer->present_family != renderer->graphics_family) {
            queue_infos[1] =
                (VkDeviceQueueCreateInfo){.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                          .queueFamilyIndex = renderer->present_family,
                                          .queueCount = 1U,
                                          .pQueuePriorities = &priority};
            create_info.queueCreateInfoCount = 2U;
        }
        vk_result =
            vkCreateDevice(renderer->physical_device, &create_info, NULL, &renderer->device);
    }
    if (vk_result != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    volkLoadDevice(renderer->device);
    vkGetDeviceQueue(renderer->device, renderer->graphics_family, 0U, &renderer->graphics_queue);
    vkGetDeviceQueue(renderer->device, renderer->present_family, 0U, &renderer->present_queue);
    af_vk_name(renderer, VK_OBJECT_TYPE_DEVICE, (uint64_t)(uintptr_t)renderer->device,
               "Arcflow device");
    return AF_RESULT_SUCCESS;
}

static VkSurfaceFormatKHR af_vk_choose_surface_format(const VkSurfaceFormatKHR *formats,
                                                      uint32_t count) {
    uint32_t index = 0U;
    for (index = 0U; index < count; ++index) {
        if ((formats[index].format == VK_FORMAT_B8G8R8A8_SRGB ||
             formats[index].format == VK_FORMAT_R8G8B8A8_SRGB) &&
            formats[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[index];
        }
    }
    return formats[0];
}

static VkPresentModeKHR af_vk_choose_present_mode(const VkPresentModeKHR *modes, uint32_t count) {
    uint32_t index = 0U;
    for (index = 0U; index < count; ++index) {
        if (modes[index] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return modes[index];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static uint32_t af_vk_find_memory_type(AfVkRenderer *renderer, uint32_t type_bits,
                                       VkMemoryPropertyFlags required) {
    VkPhysicalDeviceMemoryProperties properties;
    uint32_t index = 0U;
    vkGetPhysicalDeviceMemoryProperties(renderer->physical_device, &properties);
    for (index = 0U; index < properties.memoryTypeCount; ++index) {
        if ((type_bits & (1U << index)) != 0U &&
            (properties.memoryTypes[index].propertyFlags & required) == required) {
            return index;
        }
    }
    return UINT32_MAX;
}

static void af_vk_destroy_capture_targets(AfVkRenderer *renderer) {
    for (uint32_t index = 0U; index < AF_FRAME_COUNT; ++index) {
        AfVkFrame *frame = &renderer->frames[index];
        if (frame->capture_mapping != NULL) {
            vkUnmapMemory(renderer->device, frame->capture_memory);
            frame->capture_mapping = NULL;
        }
        if (frame->capture_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(renderer->device, frame->capture_buffer, NULL);
            frame->capture_buffer = VK_NULL_HANDLE;
        }
        if (frame->capture_memory != VK_NULL_HANDLE) {
            vkFreeMemory(renderer->device, frame->capture_memory, NULL);
            frame->capture_memory = VK_NULL_HANDLE;
        }
        frame->capture_size = 0U;
    }
}

static AfResult af_vk_create_capture_targets(AfVkRenderer *renderer) {
    const VkDeviceSize capture_size =
        (VkDeviceSize)renderer->extent.width * renderer->extent.height * 4U;
    if (!renderer->capture_supported) {
        return AF_RESULT_SUCCESS;
    }
    for (uint32_t index = 0U; index < AF_FRAME_COUNT; ++index) {
        AfVkFrame *frame = &renderer->frames[index];
        const VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                .size = capture_size,
                                                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
        VkMemoryRequirements requirements;
        VkMemoryAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        uint32_t memory_type = UINT32_MAX;
        if (vkCreateBuffer(renderer->device, &buffer_info, NULL, &frame->capture_buffer) !=
            VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        vkGetBufferMemoryRequirements(renderer->device, frame->capture_buffer, &requirements);
        memory_type = af_vk_find_memory_type(renderer, requirements.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (memory_type == UINT32_MAX) {
            af_vk_destroy_capture_targets(renderer);
            renderer->capture_supported = false;
            return AF_RESULT_SUCCESS;
        }
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memory_type;
        if (vkAllocateMemory(renderer->device, &allocation, NULL, &frame->capture_memory) !=
                VK_SUCCESS ||
            vkBindBufferMemory(renderer->device, frame->capture_buffer, frame->capture_memory,
                               0U) != VK_SUCCESS ||
            vkMapMemory(renderer->device, frame->capture_memory, 0U, capture_size, 0U,
                        (void **)&frame->capture_mapping) != VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        frame->capture_size = capture_size;
        af_vk_name(renderer, VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)frame->capture_buffer,
                   "Arcflow frame capture readback");
    }
    return AF_RESULT_SUCCESS;
}

static void af_vk_destroy_multisample_targets(AfVkRenderer *renderer) {
    for (uint32_t index = 0U; index < AF_FRAME_COUNT; ++index) {
        AfVkFrame *frame = &renderer->frames[index];
        if (frame->color_view != VK_NULL_HANDLE) {
            vkDestroyImageView(renderer->device, frame->color_view, NULL);
            frame->color_view = VK_NULL_HANDLE;
        }
        if (frame->color_image != VK_NULL_HANDLE) {
            vkDestroyImage(renderer->device, frame->color_image, NULL);
            frame->color_image = VK_NULL_HANDLE;
        }
        if (frame->color_memory != VK_NULL_HANDLE) {
            vkFreeMemory(renderer->device, frame->color_memory, NULL);
            frame->color_memory = VK_NULL_HANDLE;
        }
        frame->color_initialized = false;
    }
}

static AfResult af_vk_create_multisample_targets(AfVkRenderer *renderer) {
    if (renderer->sample_count == VK_SAMPLE_COUNT_1_BIT) {
        return AF_RESULT_SUCCESS;
    }
    for (uint32_t index = 0U; index < AF_FRAME_COUNT; ++index) {
        AfVkFrame *frame = &renderer->frames[index];
        const VkImageCreateInfo image_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                              .imageType = VK_IMAGE_TYPE_2D,
                                              .format = renderer->swapchain_format,
                                              .extent = {.width = renderer->extent.width,
                                                         .height = renderer->extent.height,
                                                         .depth = 1U},
                                              .mipLevels = 1U,
                                              .arrayLayers = 1U,
                                              .samples = renderer->sample_count,
                                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                                              .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
        VkMemoryRequirements requirements;
        VkMemoryAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = renderer->swapchain_format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0U,
                                 .levelCount = 1U,
                                 .baseArrayLayer = 0U,
                                 .layerCount = 1U}};
        uint32_t memory_type = UINT32_MAX;
        if (vkCreateImage(renderer->device, &image_info, NULL, &frame->color_image) != VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        vkGetImageMemoryRequirements(renderer->device, frame->color_image, &requirements);
        memory_type = af_vk_find_memory_type(renderer, requirements.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == UINT32_MAX) {
            return AF_RESULT_UNSUPPORTED_FEATURE;
        }
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memory_type;
        if (vkAllocateMemory(renderer->device, &allocation, NULL, &frame->color_memory) !=
                VK_SUCCESS ||
            vkBindImageMemory(renderer->device, frame->color_image, frame->color_memory, 0U) !=
                VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        view_info.image = frame->color_image;
        if (vkCreateImageView(renderer->device, &view_info, NULL, &frame->color_view) !=
            VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        af_vk_name(renderer, VK_OBJECT_TYPE_IMAGE, (uint64_t)(uintptr_t)frame->color_image,
                   "Arcflow multisample color target");
        af_vk_name(renderer, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)(uintptr_t)frame->color_view,
                   "Arcflow multisample color view");
    }
    return AF_RESULT_SUCCESS;
}

static void af_vk_destroy_pipeline(AfVkRenderer *renderer) {
    af_vk_destroy_image_pipeline(renderer);
    if (renderer->path_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(renderer->device, renderer->path_pipeline, NULL);
        renderer->path_pipeline = VK_NULL_HANDLE;
    }
    if (renderer->path_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(renderer->device, renderer->path_pipeline_layout, NULL);
        renderer->path_pipeline_layout = VK_NULL_HANDLE;
    }
    if (renderer->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(renderer->device, renderer->pipeline, NULL);
        renderer->pipeline = VK_NULL_HANDLE;
    }
    if (renderer->pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(renderer->device, renderer->pipeline_layout, NULL);
        renderer->pipeline_layout = VK_NULL_HANDLE;
    }
}

static void af_vk_destroy_swapchain(AfVkRenderer *renderer) {
    uint32_t index = 0U;
    af_vk_destroy_capture_targets(renderer);
    af_vk_destroy_multisample_targets(renderer);
    for (index = 0U; index < renderer->swapchain_image_count; ++index) {
        if (renderer->present_semaphores[index] != VK_NULL_HANDLE) {
            vkDestroySemaphore(renderer->device, renderer->present_semaphores[index], NULL);
            renderer->present_semaphores[index] = VK_NULL_HANDLE;
        }
        if (renderer->swapchain_views[index] != VK_NULL_HANDLE) {
            vkDestroyImageView(renderer->device, renderer->swapchain_views[index], NULL);
            renderer->swapchain_views[index] = VK_NULL_HANDLE;
        }
    }
    renderer->swapchain_image_count = 0U;
    if (renderer->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
        renderer->swapchain = VK_NULL_HANDLE;
    }
}
