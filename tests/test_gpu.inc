#if defined(AF_BUILD_GPU_TESTS)
static bool af_test_load_shader(const char *path, uint32_t **out_code, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    long length = 0L;
    uint32_t *code = NULL;
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0L ||
        (length & 3L) != 0L || fseek(file, 0L, SEEK_SET) != 0) {
        if (file != NULL) {
            (void)fclose(file);
        }
        return false;
    }
    code = malloc((size_t)length);
    if (code == NULL || fread(code, 1U, (size_t)length, file) != (size_t)length) {
        free(code);
        (void)fclose(file);
        return false;
    }
    if (fclose(file) != 0) {
        free(code);
        return false;
    }
    *out_code = code;
    *out_size = (size_t)length;
    return true;
}
#endif

static bool af_test_gpu_smoke(void) {
#if defined(AF_BUILD_GPU_TESTS)
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    uint32_t *vertex_code = NULL;
    uint32_t *fragment_code = NULL;
    size_t vertex_size = 0U;
    size_t fragment_size = 0U;
    uint32_t graphics_family = UINT32_MAX;
    bool passed = false;
    VkApplicationInfo application = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                     .pApplicationName = "Arcflow Tests",
                                     .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo instance_info = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                          .pApplicationInfo = &application};
    if (volkInitialize() != VK_SUCCESS ||
        vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
        (void)printf("[SKIP] gpu smoke (Vulkan 1.3 unavailable)\n");
        return true;
    }
    volkLoadInstance(instance);
    {
        VkPhysicalDevice devices[16];
        uint32_t device_count = 0U;
        uint32_t device_index = 0U;
        VkResult enumerate_result = vkEnumeratePhysicalDevices(instance, &device_count, NULL);
        if (enumerate_result != VK_SUCCESS || device_count == 0U) {
            (void)printf("[SKIP] gpu smoke (physical-device enumeration unavailable: %d)\n",
                         (int)enumerate_result);
            passed = true;
            goto cleanup;
        }
        device_count = device_count > 16U ? 16U : device_count;
        enumerate_result = vkEnumeratePhysicalDevices(instance, &device_count, devices);
        if (enumerate_result != VK_SUCCESS && enumerate_result != VK_INCOMPLETE) {
            (void)printf("[SKIP] gpu smoke (physical-device query unavailable: %d)\n",
                         (int)enumerate_result);
            passed = true;
            goto cleanup;
        }
        for (device_index = 0U; device_index < device_count; ++device_index) {
            VkPhysicalDeviceProperties properties;
            VkPhysicalDeviceVulkan13Features features13 = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            VkPhysicalDeviceVulkan12Features features12 = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .pNext = &features13};
            VkPhysicalDeviceFeatures2 features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &features12};
            VkQueueFamilyProperties families[64];
            uint32_t family_count = 64U;
            uint32_t family_index = 0U;
            vkGetPhysicalDeviceProperties(devices[device_index], &properties);
            vkGetPhysicalDeviceFeatures2(devices[device_index], &features);
            if (properties.apiVersion < VK_API_VERSION_1_3 || !features13.dynamicRendering ||
                !features13.synchronization2 || !features12.timelineSemaphore) {
                continue;
            }
            vkGetPhysicalDeviceQueueFamilyProperties(devices[device_index], &family_count,
                                                     families);
            for (family_index = 0U; family_index < family_count; ++family_index) {
                if ((families[family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
                    physical_device = devices[device_index];
                    graphics_family = family_index;
                    break;
                }
            }
            if (physical_device != VK_NULL_HANDLE) {
                break;
            }
        }
    }
    if (physical_device == VK_NULL_HANDLE) {
        (void)printf("[SKIP] gpu smoke (required Vulkan features unavailable)\n");
        passed = true;
        goto cleanup;
    }
    {
        float priority = 1.0F;
        VkDeviceQueueCreateInfo queue_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                              .queueFamilyIndex = graphics_family,
                                              .queueCount = 1U,
                                              .pQueuePriorities = &priority};
        VkPhysicalDeviceVulkan13Features features13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = VK_TRUE,
            .dynamicRendering = VK_TRUE};
        VkPhysicalDeviceVulkan12Features features12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &features13,
            .timelineSemaphore = VK_TRUE};
        VkDeviceCreateInfo device_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                          .pNext = &features12,
                                          .queueCreateInfoCount = 1U,
                                          .pQueueCreateInfos = &queue_info};
        const VkResult device_result =
            vkCreateDevice(physical_device, &device_info, NULL, &device);
        if (device_result != VK_SUCCESS) {
            (void)printf("[FAIL] gpu smoke (logical-device creation: %d)\n", (int)device_result);
            goto cleanup;
        }
    }
    volkLoadDevice(device);
    vkGetDeviceQueue(device, graphics_family, 0U, &queue);
    if (queue == VK_NULL_HANDLE ||
        !af_test_load_shader(ARCFLOW_TEST_VERTEX_SHADER, &vertex_code, &vertex_size) ||
        !af_test_load_shader(ARCFLOW_TEST_FRAGMENT_SHADER, &fragment_code, &fragment_size)) {
        goto cleanup;
    }
    {
        VkShaderModuleCreateInfo vertex_info = {.sType =
                                                    VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                .codeSize = vertex_size,
                                                .pCode = vertex_code};
        VkShaderModuleCreateInfo fragment_info = {.sType =
                                                      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                  .codeSize = fragment_size,
                                                  .pCode = fragment_code};
        const VkResult vertex_result =
            vkCreateShaderModule(device, &vertex_info, NULL, &vertex_shader);
        const VkResult fragment_result = vertex_result == VK_SUCCESS
                                             ? vkCreateShaderModule(device, &fragment_info, NULL,
                                                                    &fragment_shader)
                                             : VK_ERROR_INITIALIZATION_FAILED;
        if (vertex_result != VK_SUCCESS || fragment_result != VK_SUCCESS) {
            (void)printf("[FAIL] gpu smoke (shader modules: vertex %d, fragment %d)\n",
                         (int)vertex_result, (int)fragment_result);
            goto cleanup;
        }
    }
    {
        VkPushConstantRange push = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                    .size = 2U * sizeof(float)};
        VkPipelineLayoutCreateInfo layout_info = {.sType =
                                                      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                  .pushConstantRangeCount = 1U,
                                                  .pPushConstantRanges = &push};
        VkPipelineShaderStageCreateInfo stages[2] = {
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
             .stage = VK_SHADER_STAGE_VERTEX_BIT,
             .module = vertex_shader,
             .pName = "main"},
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
             .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
             .module = fragment_shader,
             .pName = "main"}};
        VkVertexInputBindingDescription binding = {
            .binding = 0U, .stride = 68U, .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE};
        VkVertexInputAttributeDescription attributes[4] = {
            {.location = 0U, .format = VK_FORMAT_R32G32B32A32_SFLOAT},
            {.location = 1U, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 16U},
            {.location = 2U, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 32U},
            {.location = 3U, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 48U}};
        VkPipelineVertexInputStateCreateInfo vertex_input = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1U,
            .pVertexBindingDescriptions = &binding,
            .vertexAttributeDescriptionCount = 4U,
            .pVertexAttributeDescriptions = attributes};
        VkPipelineInputAssemblyStateCreateInfo assembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        VkPipelineViewportStateCreateInfo viewport = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1U,
            .scissorCount = 1U};
        VkPipelineRasterizationStateCreateInfo raster = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .lineWidth = 1.0F};
        VkPipelineMultisampleStateCreateInfo multisample = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
        VkPipelineColorBlendAttachmentState attachment = {
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
        VkPipelineColorBlendStateCreateInfo blend = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1U,
            .pAttachments = &attachment};
        VkDynamicState states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2U,
            .pDynamicStates = states};
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        VkPipelineRenderingCreateInfo rendering = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1U,
            .pColorAttachmentFormats = &format};
        VkGraphicsPipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &rendering,
            .stageCount = 2U,
            .pStages = stages,
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &assembly,
            .pViewportState = &viewport,
            .pRasterizationState = &raster,
            .pMultisampleState = &multisample,
            .pColorBlendState = &blend,
            .pDynamicState = &dynamic};
        const VkResult layout_result =
            vkCreatePipelineLayout(device, &layout_info, NULL, &layout);
        if (layout_result != VK_SUCCESS) {
            (void)printf("[FAIL] gpu smoke (pipeline layout: %d)\n", (int)layout_result);
            goto cleanup;
        }
        pipeline_info.layout = layout;
        {
            const VkResult pipeline_result = vkCreateGraphicsPipelines(
                device, VK_NULL_HANDLE, 1U, &pipeline_info, NULL, &pipeline);
            if (pipeline_result != VK_SUCCESS) {
                (void)printf("[FAIL] gpu smoke (graphics pipeline: %d)\n",
                             (int)pipeline_result);
                goto cleanup;
            }
        }
    }
    passed = true;

cleanup:
    free(fragment_code);
    free(vertex_code);
    if (device != VK_NULL_HANDLE) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, NULL);
        }
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, layout, NULL);
        }
        if (fragment_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, fragment_shader, NULL);
        }
        if (vertex_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, vertex_shader, NULL);
        }
        vkDestroyDevice(device, NULL);
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, NULL);
    }
    return passed;
#else
    (void)printf("[SKIP] gpu smoke (disabled)\n");
    return true;
#endif
}

typedef bool (*AfTestFunction)(void);

typedef struct AfTestCase {
    const char *name;
    AfTestFunction function;
} AfTestCase;
