static void af_vk_renderer_draw_particles(AfVkRenderer *renderer, VkCommandBuffer command_buffer,
                                          uint32_t frame_index, AfAffine2 transform,
                                          const VkRect2D *particle_scissor,
                                          const VkRect2D *full_scissor) {
    if (particle_scissor->extent.width == 0U || particle_scissor->extent.height == 0U) {
        return;
    }
    vkCmdSetScissor(command_buffer, 0U, 1U, particle_scissor);
    af_vk_particles_draw(renderer->particles, command_buffer, frame_index, renderer->extent,
                         transform);
    vkCmdSetScissor(command_buffer, 0U, 1U, full_scissor);
}

AfResult af_vk_renderer_draw_timed_with_options(AfVkRenderer *renderer, const AfScene *scene,
                                                float delta_time,
                                                const AfVkRenderOptions *options) {
    AfVkFrame *frame = NULL;
    uint32_t image_index = 0U;
    uint32_t shape_count = 0U;
    uint32_t path_vertex_count = 0U;
    uint32_t image_vertex_count = 0U;
    uint32_t item_count = 0U;
    VkResult result = VK_SUCCESS;
    AfResult capture_result = AF_RESULT_SUCCESS;
    bool capture_this_frame = false;
    bool present_failed = false;
    bool particles_drawn = false;
    AfVkRenderOptions render_options = {0};
    const AfScene *particle_scene = NULL;
    VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                           .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VkImageMemoryBarrier2 to_render = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                       .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                                       .srcAccessMask = VK_ACCESS_2_NONE,
                                       .dstStageMask =
                                           VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                       .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                       .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                       .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                            .baseMipLevel = 0U,
                                                            .levelCount = 1U,
                                                            .baseArrayLayer = 0U,
                                                            .layerCount = 1U}};
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .imageMemoryBarrierCount = 1U,
                                   .pImageMemoryBarriers = &to_render};
    VkImageMemoryBarrier2 to_multisample = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0U,
                             .levelCount = 1U,
                             .baseArrayLayer = 0U,
                             .layerCount = 1U}};
    VkDependencyInfo multisample_dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                               .imageMemoryBarrierCount = 1U,
                                               .pImageMemoryBarriers = &to_multisample};
    VkRenderingAttachmentInfo color_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = {{0.018F, 0.024F, 0.04F, 1.0F}}}};
    if (af_vk_format_is_srgb(renderer->swapchain_format)) {
        color_attachment.clearValue.color.float32[0] = af_vk_srgb_to_linear_component(0.018F);
        color_attachment.clearValue.color.float32[1] = af_vk_srgb_to_linear_component(0.024F);
        color_attachment.clearValue.color.float32[2] = af_vk_srgb_to_linear_component(0.04F);
    }
    VkRenderingInfo rendering = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                 .layerCount = 1U,
                                 .colorAttachmentCount = 1U,
                                 .pColorAttachments = &color_attachment};
    VkViewport viewport;
    VkRect2D scissor;
    VkRect2D particle_scissor;
    float canvas[2];
    VkDeviceSize upload_offset = 0U;
    VkDeviceSize path_upload_offset = 0U;
    VkDeviceSize image_upload_offset = 0U;
    VkSemaphoreSubmitInfo wait_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                       .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .deviceIndex = 0U,
                                       .value = 0U};
    VkSemaphoreSubmitInfo signal_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                         .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                         .deviceIndex = 0U,
                                         .value = 0U};
    VkCommandBufferSubmitInfo command_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                              .deviceMask = 0U};
    VkSubmitInfo2 submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                            .waitSemaphoreInfoCount = 1U,
                            .pWaitSemaphoreInfos = &wait_info,
                            .commandBufferInfoCount = 1U,
                            .pCommandBufferInfos = &command_info,
                            .signalSemaphoreInfoCount = 1U,
                            .pSignalSemaphoreInfos = &signal_info};
    VkPresentInfoKHR present = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .waitSemaphoreCount = 1U,
                                .pSwapchains = &renderer->swapchain,
                                .swapchainCount = 1U,
                                .pImageIndices = &image_index};
    if (renderer == NULL || scene == NULL || !isfinite(delta_time) || delta_time < 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    render_options.particle_scene = scene;
    render_options.particle_transform = af_affine_identity();
    render_options.particle_layer_order = INT32_MAX;
    if (options != NULL) {
        render_options = *options;
        if (render_options.particle_scene == NULL) {
            render_options.particle_scene = scene;
        }
    }
    particle_scene = render_options.particle_scene;
    if (!isfinite(render_options.particle_transform.m00) ||
        !isfinite(render_options.particle_transform.m01) ||
        !isfinite(render_options.particle_transform.m10) ||
        !isfinite(render_options.particle_transform.m11) ||
        !isfinite(render_options.particle_transform.tx) ||
        !isfinite(render_options.particle_transform.ty)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (renderer->resize_requested) {
        AfResult recreate_result = af_vk_recreate_swapchain(renderer);
        if (recreate_result != AF_RESULT_SUCCESS || renderer->resize_requested) {
            return recreate_result;
        }
    }
    if (renderer->capture_requested && !renderer->capture_supported) {
        renderer->capture_requested = false;
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    capture_this_frame = renderer->capture_requested;
    frame = &renderer->frames[renderer->frame_index];
    if (vkWaitForFences(renderer->device, 1U, &frame->fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    renderer->frame_metrics.gpu_valid = false;
    if (frame->timestamps_pending && renderer->timestamp_pool != VK_NULL_HANDLE) {
        uint64_t timestamps[2];
        if (vkGetQueryPoolResults(renderer->device, renderer->timestamp_pool, renderer->frame_index * 2U,
            2U, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
            const uint64_t mask = renderer->timestamp_valid_bits >= 64U ? UINT64_MAX
                : (UINT64_C(1) << renderer->timestamp_valid_bits) - 1U;
            renderer->frame_metrics.gpu_ms = (double)((timestamps[1] - timestamps[0]) & mask) *
                                             (double)renderer->timestamp_period / 1000000.0;
            renderer->frame_metrics.gpu_valid = true;
        }
        frame->timestamps_pending = false;
    }
    renderer->image_time = render_options.image_time_override ? render_options.image_time
                                                              : renderer->image_time + delta_time;
    if (!isfinite(renderer->image_time) || renderer->image_time < 0.0F) {
        renderer->image_time = 0.0F;
    }
    {
        const AfResult image_result = af_vk_update_image_atlases(renderer, scene->images);
        if (image_result != AF_RESULT_SUCCESS) {
            return image_result;
        }
    }
    {
        const uint64_t upload_start = SDL_GetTicksNS();
        renderer->frame_metrics.path_rebuilds = 0U;
        const AfResult upload_result = af_vk_upload_scene(
            renderer, scene, &shape_count, &path_vertex_count, &image_vertex_count, &item_count);
        renderer->frame_metrics.upload_ms = (double)(SDL_GetTicksNS() - upload_start) / 1000000.0;
        if (upload_result != AF_RESULT_SUCCESS) {
            return upload_result;
        }
    }
    {
        const AfResult particle_result =
            af_vk_particles_update(renderer->particles, particle_scene, renderer->frame_index);
        if (particle_result != AF_RESULT_SUCCESS) {
            return particle_result;
        }
    }
    result = vkAcquireNextImageKHR(renderer->device, renderer->swapchain, UINT64_MAX,
                                   frame->image_available, VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        renderer->resize_requested = true;
        return AF_RESULT_SUCCESS;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return AF_RESULT_VULKAN_ERROR;
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        renderer->resize_requested = true;
    }
    if (vkResetFences(renderer->device, 1U, &frame->fence) != VK_SUCCESS ||
        vkResetCommandPool(renderer->device, frame->command_pool, 0U) != VK_SUCCESS ||
        vkBeginCommandBuffer(frame->command_buffer, &begin_info) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    if (renderer->timestamp_pool != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(frame->command_buffer, renderer->timestamp_pool, renderer->frame_index * 2U, 2U);
        vkCmdWriteTimestamp2(frame->command_buffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                             renderer->timestamp_pool, renderer->frame_index * 2U);
    }
    renderer->frame_metrics.draw_batches = item_count;
    to_render.image = renderer->swapchain_images[image_index];
    if (renderer->sample_count == VK_SAMPLE_COUNT_1_BIT) {
        color_attachment.imageView = renderer->swapchain_views[image_index];
        color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
        color_attachment.resolveImageView = VK_NULL_HANDLE;
        color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    } else {
        color_attachment.imageView = frame->color_view;
        color_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        color_attachment.resolveImageView = renderer->swapchain_views[image_index];
        color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        to_multisample.image = frame->color_image;
    }
    rendering.renderArea.extent = renderer->extent;
    viewport = (VkViewport){.x = 0.0F,
                            .y = 0.0F,
                            .width = (float)renderer->extent.width,
                            .height = (float)renderer->extent.height,
                            .minDepth = 0.0F,
                            .maxDepth = 1.0F};
    scissor = (VkRect2D){.extent = renderer->extent};
    particle_scissor = scissor;
    if (render_options.particle_clip_enabled) {
        const uint32_t clip_x = render_options.particle_clip_x < renderer->extent.width
                                    ? render_options.particle_clip_x
                                    : renderer->extent.width;
        const uint32_t clip_y = render_options.particle_clip_y < renderer->extent.height
                                    ? render_options.particle_clip_y
                                    : renderer->extent.height;
        particle_scissor.offset.x = (int32_t)clip_x;
        particle_scissor.offset.y = (int32_t)clip_y;
        particle_scissor.extent.width =
            render_options.particle_clip_width < renderer->extent.width - clip_x
                ? render_options.particle_clip_width
                : renderer->extent.width - clip_x;
        particle_scissor.extent.height =
            render_options.particle_clip_height < renderer->extent.height - clip_y
                ? render_options.particle_clip_height
                : renderer->extent.height - clip_y;
    }
    canvas[0] = (float)renderer->extent.width;
    canvas[1] = (float)renderer->extent.height;
    upload_offset = renderer->frame_index * renderer->upload_slice_size;
    path_upload_offset = upload_offset + AF_MAX_GPU_SHAPES * sizeof(AfGpuShape);
    image_upload_offset =
        path_upload_offset + (VkDeviceSize)AF_MAX_GPU_PATH_VERTICES * sizeof(AfGpuPathVertex);
    af_vk_particles_record(renderer->particles, frame->command_buffer, renderer->frame_index,
                           delta_time);
    vkCmdPipelineBarrier2(frame->command_buffer, &dependency);
    if (renderer->sample_count != VK_SAMPLE_COUNT_1_BIT && !frame->color_initialized) {
        vkCmdPipelineBarrier2(frame->command_buffer, &multisample_dependency);
    }
    vkCmdBeginRendering(frame->command_buffer, &rendering);
    vkCmdSetViewport(frame->command_buffer, 0U, 1U, &viewport);
    vkCmdSetScissor(frame->command_buffer, 0U, 1U, &scissor);
    {
        AfVkRenderItemType bound_type = (AfVkRenderItemType)UINT32_MAX;
        uint32_t bound_atlas = UINT32_MAX;
        for (uint32_t item_index = 0U; item_index < item_count; ++item_index) {
            const AfVkRenderItem *item = &renderer->render_items[item_index];
            if (!particles_drawn && item->layer_order > render_options.particle_layer_order) {
                af_vk_renderer_draw_particles(
                    renderer, frame->command_buffer, renderer->frame_index,
                    render_options.particle_transform, &particle_scissor, &scissor);
                particles_drawn = true;
                bound_type = (AfVkRenderItemType)UINT32_MAX;
                bound_atlas = UINT32_MAX;
            }
            if (item->type != bound_type) {
                const VkDeviceSize *offset = item->type == AF_VK_RENDER_PATH ? &path_upload_offset
                                             : item->type == AF_VK_RENDER_IMAGE
                                                 ? &image_upload_offset
                                                 : &upload_offset;
                const VkPipeline pipeline =
                    item->type == AF_VK_RENDER_PATH    ? renderer->path_pipeline
                    : item->type == AF_VK_RENDER_IMAGE ? renderer->image_pipeline
                                                       : renderer->pipeline;
                const VkPipelineLayout layout =
                    item->type == AF_VK_RENDER_PATH    ? renderer->path_pipeline_layout
                    : item->type == AF_VK_RENDER_IMAGE ? renderer->image_pipeline_layout
                                                       : renderer->pipeline_layout;
                vkCmdBindPipeline(frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                vkCmdBindVertexBuffers(frame->command_buffer, 0U, 1U, &renderer->upload_buffer,
                                       offset);
                vkCmdPushConstants(frame->command_buffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0U,
                                   sizeof(canvas), canvas);
                if (item->type != AF_VK_RENDER_IMAGE)
                    vkCmdBindDescriptorSets(
                        frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0U, 1U,
                        &renderer->paint_descriptor_sets[renderer->frame_index], 0U, NULL);
                bound_type = item->type;
            }
            if (item->type == AF_VK_RENDER_IMAGE && item->atlas_index != bound_atlas) {
                if (item->atlas_index >= AF_MAX_IMAGE_ATLASES ||
                    renderer->image_atlases[item->atlas_index].view == VK_NULL_HANDLE) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
                vkCmdBindDescriptorSets(frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        renderer->image_pipeline_layout, 0U, 1U,
                                        &renderer->image_descriptor_sets[item->atlas_index], 0U,
                                        NULL);
                bound_atlas = item->atlas_index;
            }
            if (item->type == AF_VK_RENDER_PATH || item->type == AF_VK_RENDER_IMAGE) {
                vkCmdDraw(frame->command_buffer, item->count, 1U, item->first, 0U);
            } else {
                vkCmdDraw(frame->command_buffer, 6U, item->count, 0U, item->first);
            }
        }
        if (!particles_drawn) {
            af_vk_renderer_draw_particles(renderer, frame->command_buffer, renderer->frame_index,
                                          render_options.particle_transform, &particle_scissor,
                                          &scissor);
            particles_drawn = true;
        }
    }
    (void)shape_count;
    (void)path_vertex_count;
    (void)image_vertex_count;
    vkCmdEndRendering(frame->command_buffer);
    if (renderer->timestamp_pool != VK_NULL_HANDLE)
        vkCmdWriteTimestamp2(frame->command_buffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                             renderer->timestamp_pool, renderer->frame_index * 2U + 1U);
    to_render.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_render.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    to_render.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (capture_this_frame) {
        const VkBufferImageCopy copy = {
            .bufferOffset = 0U,
            .bufferRowLength = 0U,
            .bufferImageHeight = 0U,
            .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .mipLevel = 0U,
                                 .baseArrayLayer = 0U,
                                 .layerCount = 1U},
            .imageOffset = {0, 0, 0},
            .imageExtent = {
                .width = renderer->extent.width, .height = renderer->extent.height, .depth = 1U}};
        const VkBufferMemoryBarrier2 to_host = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                                                .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
                                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                .buffer = frame->capture_buffer,
                                                .offset = 0U,
                                                .size = frame->capture_size};
        const VkDependencyInfo host_dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                                  .bufferMemoryBarrierCount = 1U,
                                                  .pBufferMemoryBarriers = &to_host};
        to_render.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        to_render.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        to_render.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        vkCmdPipelineBarrier2(frame->command_buffer, &dependency);
        vkCmdCopyImageToBuffer(frame->command_buffer, renderer->swapchain_images[image_index],
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, frame->capture_buffer, 1U,
                               &copy);
        vkCmdPipelineBarrier2(frame->command_buffer, &host_dependency);
        to_render.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        to_render.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        to_render.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        to_render.dstAccessMask = VK_ACCESS_2_NONE;
        to_render.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_render.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier2(frame->command_buffer, &dependency);
    } else {
        to_render.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        to_render.dstAccessMask = VK_ACCESS_2_NONE;
        to_render.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier2(frame->command_buffer, &dependency);
    }
    if (vkEndCommandBuffer(frame->command_buffer) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    wait_info.semaphore = frame->image_available;
    signal_info.semaphore = renderer->present_semaphores[image_index];
    command_info.commandBuffer = frame->command_buffer;
    present.pWaitSemaphores = &renderer->present_semaphores[image_index];
    if (vkQueueSubmit2(renderer->graphics_queue, 1U, &submit, frame->fence) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    frame->timestamps_pending = renderer->timestamp_pool != VK_NULL_HANDLE;
    frame->color_initialized = true;
    result = vkQueuePresentKHR(renderer->present_queue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        renderer->resize_requested = true;
    } else if (result != VK_SUCCESS) {
        present_failed = true;
    }
    if (capture_this_frame) {
        const AfPixelOrder order = renderer->swapchain_format == VK_FORMAT_B8G8R8A8_SRGB ||
                                           renderer->swapchain_format == VK_FORMAT_B8G8R8A8_UNORM
                                       ? AF_PIXEL_ORDER_BGRA
                                       : AF_PIXEL_ORDER_RGBA;
        if (vkWaitForFences(renderer->device, 1U, &frame->fence, VK_TRUE, UINT64_MAX) !=
            VK_SUCCESS) {
            capture_result = AF_RESULT_VULKAN_ERROR;
        } else {
            capture_result = af_png_write_rgba8(renderer->capture_path, frame->capture_mapping,
                                                renderer->extent.width, renderer->extent.height,
                                                renderer->extent.width * 4U, order);
        }
        renderer->capture_requested = false;
    }
    renderer->frame_index = (renderer->frame_index + 1U) % AF_FRAME_COUNT;
    if (present_failed) {
        return AF_RESULT_VULKAN_ERROR;
    }
    return capture_result;
}

AfResult af_vk_renderer_draw_timed(AfVkRenderer *renderer, const AfScene *scene, float delta_time) {
    return af_vk_renderer_draw_timed_with_options(renderer, scene, delta_time, NULL);
}

AfResult af_vk_renderer_draw(AfVkRenderer *renderer, const AfScene *scene) {
    return af_vk_renderer_draw_timed(renderer, scene, AF_PARTICLE_FIXED_STEP);
}

AfResult af_vk_renderer_capture_next(AfVkRenderer *renderer, const char *path) {
    size_t path_length = 0U;
    if (renderer == NULL || path == NULL || path[0] == '\0') {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!renderer->capture_supported) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    path_length = strlen(path);
    if (path_length >= sizeof(renderer->capture_path)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(renderer->capture_path, path, path_length + 1U);
    renderer->capture_requested = true;
    return AF_RESULT_SUCCESS;
}

void af_vk_renderer_request_resize(AfVkRenderer *renderer) {
    if (renderer != NULL) {
        renderer->resize_requested = true;
    }
}

bool af_vk_renderer_has_validation_errors(const AfVkRenderer *renderer) {
    return renderer != NULL && renderer->validation_error;
}

AfVkVfxMetrics af_vk_renderer_vfx_metrics(const AfVkRenderer *renderer) {
    return renderer == NULL ? (AfVkVfxMetrics){0} : af_vk_particles_metrics(renderer->particles);
}
