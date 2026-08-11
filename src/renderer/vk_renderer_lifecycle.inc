AfResult af_vk_renderer_create(AfPlatformWindow *window, AfVkRenderer **out_renderer) {
    AfVkRenderer *renderer = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (window == NULL || window->window == NULL || out_renderer == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_renderer = NULL;
    renderer = calloc(1U, sizeof(*renderer));
    if (renderer == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    renderer->path_scratch = calloc(1U, sizeof(*renderer->path_scratch));
    renderer->path_staging_vertices =
        calloc(AF_MAX_GPU_PATH_VERTICES, sizeof(*renderer->path_staging_vertices));
    if (renderer->path_scratch == NULL || renderer->path_staging_vertices == NULL) {
        af_vk_renderer_destroy(renderer);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    renderer->platform = window;
    if (volkInitialize() != VK_SUCCESS) {
        result = AF_RESULT_VULKAN_ERROR;
    } else if ((result = af_vk_create_instance(renderer)) != AF_RESULT_SUCCESS) {
    } else if (!SDL_Vulkan_CreateSurface(window->window, renderer->instance, NULL,
                                         &renderer->surface)) {
        af_log(AF_LOG_ERROR, "Vulkan surface creation failed: %s", SDL_GetError());
        result = AF_RESULT_VULKAN_ERROR;
    } else if ((result = af_vk_create_device(renderer)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_create_swapchain(renderer)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_create_pipeline(renderer)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_create_path_pipeline(renderer)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_create_image_pipeline(renderer)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_particles_create(renderer->physical_device, renderer->device,
                                                renderer->swapchain_format, renderer->sample_count,
                                                renderer->graphics_family, &renderer->particles)) !=
               AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_create_upload_ring(renderer)) != AF_RESULT_SUCCESS) {
    } else {
        result = af_vk_create_frames(renderer);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_vk_renderer_destroy(renderer);
        return result;
    }
    *out_renderer = renderer;
    return AF_RESULT_SUCCESS;
}

void af_vk_renderer_destroy(AfVkRenderer *renderer) {
    uint32_t index = 0U;
    if (renderer == NULL) {
        return;
    }
    if (renderer->device != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(renderer->device);
        for (index = 0U; index < AF_FRAME_COUNT; ++index) {
            AfVkFrame *frame = &renderer->frames[index];
            if (frame->fence != VK_NULL_HANDLE) {
                vkDestroyFence(renderer->device, frame->fence, NULL);
            }
            if (frame->image_available != VK_NULL_HANDLE) {
                vkDestroySemaphore(renderer->device, frame->image_available, NULL);
            }
            if (frame->command_pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(renderer->device, frame->command_pool, NULL);
            }
        }
        if (renderer->upload_mapping != NULL) {
            vkUnmapMemory(renderer->device, renderer->upload_memory);
        }
        if (renderer->upload_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(renderer->device, renderer->upload_buffer, NULL);
        }
        if (renderer->upload_memory != VK_NULL_HANDLE) {
            vkFreeMemory(renderer->device, renderer->upload_memory, NULL);
        }
        af_vk_particles_destroy(renderer->particles);
        renderer->particles = NULL;
        af_vk_destroy_pipeline(renderer);
        af_vk_destroy_images(renderer);
        af_vk_destroy_swapchain(renderer);
        vkDestroyDevice(renderer->device, NULL);
    }
    if (renderer->surface != VK_NULL_HANDLE && renderer->instance != VK_NULL_HANDLE) {
        SDL_Vulkan_DestroySurface(renderer->instance, renderer->surface, NULL);
    }
    if (renderer->debug_messenger != VK_NULL_HANDLE && renderer->instance != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(renderer->instance, renderer->debug_messenger, NULL);
    }
    if (renderer->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(renderer->instance, NULL);
    }
    for (index = 0U; index < AF_MAX_PATHS; ++index) {
        free(renderer->path_caches[index].vertices);
        free(renderer->stroke_caches[index].vertices);
    }
    free(renderer->path_staging_vertices);
    free(renderer->path_scratch);
    free(renderer);
}

AfRuntimeCreateInfo af_vk_renderer_runtime_create_info(const AfVkRenderer *renderer) {
    if (renderer == NULL) {
        return (AfRuntimeCreateInfo){0};
    }
    return (AfRuntimeCreateInfo){.instance = renderer->instance,
                                 .physical_device = renderer->physical_device,
                                 .device = renderer->device,
                                 .graphics_queue = renderer->graphics_queue,
                                 .graphics_queue_family = renderer->graphics_family,
                                 .frames_in_flight = AF_FRAME_COUNT};
}
