static AfResult af_vk_create_paint_descriptors(AfVkRenderer *renderer) {
    const VkDescriptorSetLayoutBinding binding = {.binding = 0,
                                                  .descriptorType =
                                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                  .descriptorCount = 1,
                                                  .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
    const VkDescriptorSetLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding};
    const VkDescriptorPoolSize size = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       .descriptorCount = AF_FRAME_COUNT};
    const VkDescriptorPoolCreateInfo pool = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                             .maxSets = AF_FRAME_COUNT,
                                             .poolSizeCount = 1,
                                             .pPoolSizes = &size};
    if (vkCreateDescriptorSetLayout(renderer->device, &layout, NULL,
                                    &renderer->paint_descriptor_layout) != VK_SUCCESS ||
        vkCreateDescriptorPool(renderer->device, &pool, NULL, &renderer->paint_descriptor_pool) !=
            VK_SUCCESS)
        return AF_RESULT_VULKAN_ERROR;
    VkDescriptorSetLayout layouts[AF_FRAME_COUNT];
    for (uint32_t i = 0; i < AF_FRAME_COUNT; ++i)
        layouts[i] = renderer->paint_descriptor_layout;
    const VkDescriptorSetAllocateInfo allocate = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = renderer->paint_descriptor_pool,
        .descriptorSetCount = AF_FRAME_COUNT,
        .pSetLayouts = layouts};
    return vkAllocateDescriptorSets(renderer->device, &allocate, renderer->paint_descriptor_sets) ==
                   VK_SUCCESS
               ? AF_RESULT_SUCCESS
               : AF_RESULT_VULKAN_ERROR;
}
static void af_vk_bind_paint_buffers(AfVkRenderer *renderer) {
    for (uint32_t i = 0; i < AF_FRAME_COUNT; ++i) {
        const VkDescriptorBufferInfo buffer = {.buffer = renderer->upload_buffer,
                                               .offset = i * renderer->upload_slice_size +
                                                         renderer->paint_upload_offset,
                                               .range = AF_MAX_GPU_PAINTS * sizeof(AfGpuPaint)};
        const VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                            .dstSet = renderer->paint_descriptor_sets[i],
                                            .dstBinding = 0,
                                            .descriptorCount = 1,
                                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            .pBufferInfo = &buffer};
        vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
    }
}
static AfResult af_vk_upload_paint(AfGpuPaint *paints, uint32_t *count, const AfSceneNode *node,
                                   const AfPaint *paint, float opacity, uint32_t *index) {
    *index = 0;
    if (!paint || paint->type == AF_PAINT_SOLID)
        return AF_RESULT_SUCCESS;
    if (*count + 1 >= AF_MAX_GPU_PAINTS)
        return AF_RESULT_LIMIT_EXCEEDED;
    AfAffine2 inverse;
    if (!af_affine_inverse(node->world_transform, &inverse))
        return AF_RESULT_SUCCESS;
    if (!af_affine_inverse(af_affine_multiply(node->world_transform, paint->transform), &inverse))
        return AF_RESULT_INVALID_ARGUMENT;
    *index = ++*count;
    AfGpuPaint *target = &paints[*index];
    *target = (AfGpuPaint){
        .inverse_x = {inverse.m00, inverse.m01, inverse.tx, opacity},
        .inverse_y = {inverse.m10, inverse.m11, inverse.ty, (float)paint->stop_count},
        .endpoints = {paint->start.x, paint->start.y, paint->end.x, paint->end.y},
        .parameters = {paint->start_radius, paint->end_radius, paint->angle, (float)paint->type}};
    for (uint32_t i = 0; i < paint->stop_count; ++i) {
        const AfColor color = paint->stops[i].color;
        target->colors[i][0] = color.r;
        target->colors[i][1] = color.g;
        target->colors[i][2] = color.b;
        target->colors[i][3] = color.a;
        target->offsets[i] = paint->stops[i].offset;
    }
    return AF_RESULT_SUCCESS;
}
