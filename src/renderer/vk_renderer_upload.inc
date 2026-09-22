static uint64_t af_vk_path_content_hash(const AfPathData *data, const AfPath *path) {
    uint64_t hash = af_hash_bytes(&path->fill_rule, sizeof(path->fill_rule));
    const uint64_t commands_hash = af_hash_bytes(
        &data->commands[path->command_start], (size_t)path->command_count * sizeof(AfPathCommand));
    hash ^= commands_hash + UINT64_C(0x9e3779b97f4a7c15) + (hash << 6U) + (hash >> 2U);
    return hash;
}

static AfResult af_vk_update_path_cache(AfVkRenderer *renderer, const AfPathData *data,
                                        const AfPath *path, AfVkPathCache **out_cache) {
    AfVkPathCache *cache = NULL;
    AfVkPathCache *empty = NULL;
    AfVkPathCache *recyclable = NULL;
    uint32_t vertex_count = 0U;
    const uint64_t content_hash = af_vk_path_content_hash(data, path);
    AfResult result = AF_RESULT_SUCCESS;
    for (uint32_t index = 0U; index < AF_MAX_PATHS; ++index) {
        if (renderer->path_caches[index].path_id == path->id) {
            cache = &renderer->path_caches[index];
            break;
        }
        if (empty == NULL && renderer->path_caches[index].path_id == 0U) {
            empty = &renderer->path_caches[index];
        } else if (recyclable == NULL &&
                   renderer->path_caches[index].seen_serial != renderer->path_cache_serial) {
            recyclable = &renderer->path_caches[index];
        }
    }
    if (cache == NULL) {
        cache = empty != NULL ? empty : recyclable;
    }
    if (cache == NULL) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    cache->seen_serial = renderer->path_cache_serial;
    if (cache->source == data && cache->path_id == path->id &&
        cache->geometry_generation == path->geometry_generation &&
        cache->content_hash == content_hash) {
        *out_cache = cache;
        return AF_RESULT_SUCCESS;
    }
    result = af_path_tessellate(data, path, 0.25F, renderer->path_scratch,
                                renderer->path_staging_vertices, AF_MAX_PATH_TESSELLATED_VERTICES,
                                &vertex_count);
    if (result != AF_RESULT_SUCCESS) {
        af_log(AF_LOG_ERROR, "Path %llu tessellation failed: %s", (unsigned long long)path->id,
               af_result_string(result));
        return result;
    }
    if (vertex_count >
        AF_MAX_GPU_PATH_VERTICES - (renderer->cached_path_vertex_count - cache->vertex_count)) {
        af_log(AF_LOG_ERROR,
               "Path cache vertex limit: path=%llu new=%u cached=%u previous=%u limit=%u",
               (unsigned long long)path->id, vertex_count, renderer->cached_path_vertex_count,
               cache->vertex_count, AF_MAX_GPU_PATH_VERTICES);
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (vertex_count != cache->vertex_count) {
        AfVec2 *vertices = NULL;
        if (vertex_count != 0U) {
            vertices = realloc(cache->vertices, (size_t)vertex_count * sizeof(*vertices));
            if (vertices == NULL) {
                return AF_RESULT_OUT_OF_MEMORY;
            }
        } else {
            free(cache->vertices);
        }
        cache->vertices = vertices;
    }
    if (vertex_count != 0U) {
        memcpy(cache->vertices, renderer->path_staging_vertices,
               (size_t)vertex_count * sizeof(*cache->vertices));
    }
    renderer->cached_path_vertex_count =
        renderer->cached_path_vertex_count - cache->vertex_count + vertex_count;
    cache->source = data;
    cache->path_id = path->id;
    cache->geometry_generation = path->geometry_generation;
    cache->content_hash = content_hash;
    cache->vertex_count = vertex_count;
    ++cache->rebuild_count;
    ++renderer->frame_metrics.path_rebuilds;
    *out_cache = cache;
    return AF_RESULT_SUCCESS;
}

static AfResult af_vk_update_stroke_cache(AfVkRenderer *renderer, const AfPathData *paths,
                                          const AfPath *path, const AfStyleData *styles,
                                          const AfStyle *style, AfVkStrokeCache **out_cache) {
    AfVkStrokeCache *cache = NULL;
    AfVkStrokeCache *empty = NULL;
    AfVkStrokeCache *recyclable = NULL;
    AfFlattenedPath flattened = {.points = renderer->path_scratch->points,
                                 .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                 .contours = renderer->path_scratch->contours,
                                 .contour_capacity = AF_MAX_PATH_CONTOURS};
    uint32_t vertex_count = 0U;
    const uint64_t path_content_hash = af_vk_path_content_hash(paths, path);
    AfResult result = AF_RESULT_SUCCESS;
    for (uint32_t index = 0U; index < AF_MAX_PATHS; ++index) {
        if (renderer->stroke_caches[index].path_id == path->id &&
            renderer->stroke_caches[index].style_id == style->id) {
            cache = &renderer->stroke_caches[index];
            break;
        }
        if (empty == NULL && renderer->stroke_caches[index].path_id == 0U) {
            empty = &renderer->stroke_caches[index];
        } else if (recyclable == NULL &&
                   renderer->stroke_caches[index].seen_serial != renderer->path_cache_serial) {
            recyclable = &renderer->stroke_caches[index];
        }
    }
    if (cache == NULL) {
        cache = empty != NULL ? empty : recyclable;
    }
    if (cache == NULL) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    cache->seen_serial = renderer->path_cache_serial;
    if (cache->path_source == paths && cache->style_source == styles &&
        cache->path_id == path->id && cache->style_id == style->id &&
        cache->path_geometry_generation == path->geometry_generation &&
        cache->path_content_hash == path_content_hash &&
        cache->stroke_geometry_generation == style->stroke_geometry_generation) {
        *out_cache = cache;
        return AF_RESULT_SUCCESS;
    }
    result = af_path_flatten(paths, path, 0.25F, &flattened);
    if (result == AF_RESULT_SUCCESS) {
        result = af_stroke_tessellate(&flattened, &style->stroke, renderer->path_staging_vertices,
                                      AF_MAX_PATH_TESSELLATED_VERTICES, &vertex_count);
    }
    if (result != AF_RESULT_SUCCESS ||
        vertex_count > AF_MAX_PATH_TESSELLATED_VERTICES -
                           (renderer->cached_stroke_vertex_count - cache->vertex_count)) {
        return result == AF_RESULT_SUCCESS ? AF_RESULT_LIMIT_EXCEEDED : result;
    }
    if (vertex_count != cache->vertex_count) {
        AfVec2 *vertices = NULL;
        if (vertex_count != 0U) {
            vertices = realloc(cache->vertices, (size_t)vertex_count * sizeof(*vertices));
            if (vertices == NULL) {
                return AF_RESULT_OUT_OF_MEMORY;
            }
        } else {
            free(cache->vertices);
        }
        cache->vertices = vertices;
    }
    if (vertex_count != 0U) {
        memcpy(cache->vertices, renderer->path_staging_vertices,
               (size_t)vertex_count * sizeof(*cache->vertices));
    }
    renderer->cached_stroke_vertex_count =
        renderer->cached_stroke_vertex_count - cache->vertex_count + vertex_count;
    cache->path_source = paths;
    cache->style_source = styles;
    cache->path_id = path->id;
    cache->style_id = style->id;
    cache->path_geometry_generation = path->geometry_generation;
    cache->path_content_hash = path_content_hash;
    cache->stroke_geometry_generation = style->stroke_geometry_generation;
    cache->vertex_count = vertex_count;
    ++cache->rebuild_count;
    ++renderer->frame_metrics.path_rebuilds;
    *out_cache = cache;
    return AF_RESULT_SUCCESS;
}

static void af_vk_append_render_item(AfVkRenderer *renderer, uint32_t *count, AfVkRenderItem item) {
    renderer->render_items[(*count)++] = item;
}

static void af_vk_prune_path_caches(AfVkRenderer *renderer, const AfPathData *paths) {
    for (uint32_t index = 0U; index < AF_MAX_PATHS; ++index) {
        AfVkPathCache *path_cache = &renderer->path_caches[index];
        AfVkStrokeCache *stroke_cache = &renderer->stroke_caches[index];
        if (path_cache->path_id != 0U &&
            (paths == NULL || af_path_find_const(paths, path_cache->path_id) == NULL)) {
            renderer->cached_path_vertex_count -= path_cache->vertex_count;
            free(path_cache->vertices);
            *path_cache = (AfVkPathCache){0};
        }
        if (stroke_cache->path_id != 0U &&
            (paths == NULL || af_path_find_const(paths, stroke_cache->path_id) == NULL)) {
            renderer->cached_stroke_vertex_count -= stroke_cache->vertex_count;
            free(stroke_cache->vertices);
            *stroke_cache = (AfVkStrokeCache){0};
        }
    }
}

static const AfImageAsset *af_vk_image_asset_at_time(const AfImageData *images,
                                                     const AfImageBinding *binding, float time) {
    uint64_t asset_id = binding->asset_id;
    if (binding->sequence_id != 0U) {
        const AfImageSequence *sequence =
            af_image_find_sequence_const(images, binding->sequence_id);
        if (sequence == NULL || sequence->frame_count == 0U) {
            return NULL;
        }
        {
            const float fps = binding->frames_per_second > 0.0F ? binding->frames_per_second
                                                                : sequence->frames_per_second;
            uint64_t frame = (uint64_t)floorf(fmaxf(time, 0.0F) * fps);
            if (binding->loop) {
                frame %= sequence->frame_count;
            } else if (frame >= sequence->frame_count) {
                frame = sequence->frame_count - 1U;
            }
            asset_id = images->sequence_frames[sequence->frame_start + (uint32_t)frame];
        }
    }
    return af_image_find_const(images, asset_id);
}

static void af_vk_emit_image_quad(AfGpuImageVertex *vertices, uint32_t *count,
                                  const AfSceneNode *node, float x0, float y0, float x1, float y1,
                                  float u0, float v0, float u1, float v1) {
    static const uint32_t corners[6] = {0U, 1U, 2U, 0U, 2U, 3U};
    const AfVec2 local[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    const AfVec2 uv[4] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    for (uint32_t index = 0U; index < 6U; ++index) {
        const uint32_t corner = corners[index];
        const AfVec2 world = af_affine_transform_point(node->world_transform, local[corner]);
        vertices[*count] = (AfGpuImageVertex){.position = {world.x, world.y},
                                              .uv = {uv[corner].x, uv[corner].y},
                                              .color = {node->fill.r, node->fill.g, node->fill.b,
                                                        node->fill.a * node->world_opacity}};
        ++(*count);
    }
}

static AfResult af_vk_emit_image_node(const AfSceneNode *node, const AfImageData *images,
                                      float time, AfGpuImageVertex *vertices, uint32_t *count,
                                      uint32_t *out_atlas_index) {
    const AfImageBinding *binding = af_image_find_binding_const(images, node->id);
    const AfImageAsset *asset =
        binding == NULL ? NULL : af_vk_image_asset_at_time(images, binding, time);
    const AfImageAtlas *atlas = NULL;
    float source_x[4];
    float source_y[4];
    float target_x[4];
    float target_y[4];
    if (asset != NULL && asset->missing) {
        return AF_RESULT_SUCCESS;
    }
    if (asset == NULL || asset->atlas_index >= images->atlas_count ||
        *count > AF_MAX_GPU_IMAGE_VERTICES - 54U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    atlas = &images->atlases[asset->atlas_index];
    if (atlas->pixels == NULL || atlas->width == 0U || atlas->height == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_atlas_index = asset->atlas_index;
    if (binding->display_mode != AF_IMAGE_DISPLAY_NINE_SLICE) {
        const float x0 = (float)asset->trim_x / (float)asset->width * node->resolved_bounds.x;
        const float y0 = (float)asset->trim_y / (float)asset->height * node->resolved_bounds.y;
        const float x1 = (float)(asset->trim_x + asset->trim_width) / (float)asset->width *
                         node->resolved_bounds.x;
        const float y1 = (float)(asset->trim_y + asset->trim_height) / (float)asset->height *
                         node->resolved_bounds.y;
        af_vk_emit_image_quad(vertices, count, node, x0, y0, x1, y1,
                              (float)asset->atlas_x / (float)atlas->width,
                              (float)asset->atlas_y / (float)atlas->height,
                              (float)(asset->atlas_x + asset->trim_width) / (float)atlas->width,
                              (float)(asset->atlas_y + asset->trim_height) / (float)atlas->height);
        return AF_RESULT_SUCCESS;
    }
    {
        float left = (float)binding->borders.left;
        float top = (float)binding->borders.top;
        float right = (float)binding->borders.right;
        float bottom = (float)binding->borders.bottom;
        float horizontal_scale = 1.0F;
        float vertical_scale = 1.0F;
        left = fminf(left, (float)asset->trim_width);
        right = fminf(right, (float)asset->trim_width - left);
        top = fminf(top, (float)asset->trim_height);
        bottom = fminf(bottom, (float)asset->trim_height - top);
        if (left + right > node->resolved_bounds.x && left + right > 0.0F) {
            horizontal_scale = node->resolved_bounds.x / (left + right);
        }
        if (top + bottom > node->resolved_bounds.y && top + bottom > 0.0F) {
            vertical_scale = node->resolved_bounds.y / (top + bottom);
        }
        source_x[0] = (float)asset->atlas_x;
        source_x[1] = source_x[0] + left;
        source_x[2] = (float)(asset->atlas_x + asset->trim_width) - right;
        source_x[3] = (float)(asset->atlas_x + asset->trim_width);
        source_y[0] = (float)asset->atlas_y;
        source_y[1] = source_y[0] + top;
        source_y[2] = (float)(asset->atlas_y + asset->trim_height) - bottom;
        source_y[3] = (float)(asset->atlas_y + asset->trim_height);
        target_x[0] = 0.0F;
        target_x[1] = left * horizontal_scale;
        target_x[2] = node->resolved_bounds.x - right * horizontal_scale;
        target_x[3] = node->resolved_bounds.x;
        target_y[0] = 0.0F;
        target_y[1] = top * vertical_scale;
        target_y[2] = node->resolved_bounds.y - bottom * vertical_scale;
        target_y[3] = node->resolved_bounds.y;
    }
    for (uint32_t y = 0U; y < 3U; ++y) {
        for (uint32_t x = 0U; x < 3U; ++x) {
            if (target_x[x + 1U] <= target_x[x] || target_y[y + 1U] <= target_y[y]) {
                continue;
            }
            af_vk_emit_image_quad(
                vertices, count, node, target_x[x], target_y[y], target_x[x + 1U], target_y[y + 1U],
                source_x[x] / (float)atlas->width, source_y[y] / (float)atlas->height,
                source_x[x + 1U] / (float)atlas->width, source_y[y + 1U] / (float)atlas->height);
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_vk_upload_scene(AfVkRenderer *renderer, const AfScene *scene,
                                   uint32_t *out_shape_count, uint32_t *out_path_vertex_count,
                                   uint32_t *out_image_vertex_count, uint32_t *out_item_count) {
    uint8_t *slice = renderer->upload_mapping + renderer->frame_index * renderer->upload_slice_size;
    AfGpuShape *shapes = (AfGpuShape *)(void *)slice;
    AfGpuPathVertex *path_vertices =
        (AfGpuPathVertex *)(void *)(slice + AF_MAX_GPU_SHAPES * sizeof(AfGpuShape));
    AfGpuImageVertex *image_vertices =
        (AfGpuImageVertex *)(void *)(slice + AF_MAX_GPU_SHAPES * sizeof(AfGpuShape) +
                                     (size_t)AF_MAX_GPU_PATH_VERTICES * sizeof(AfGpuPathVertex));
    AfGpuPaint *paints = (AfGpuPaint *)(void *)(slice + renderer->paint_upload_offset);
    uint32_t paint_count = 0;
    uint32_t shape_count = 0U;
    uint32_t path_vertex_count = 0U;
    uint32_t item_count = 0U;
    uint32_t image_vertex_count = 0U;
    uint32_t index = 0U;
    if (++renderer->path_cache_serial == 0U) {
        renderer->path_cache_serial = 1U;
    }
    af_vk_prune_path_caches(renderer, scene->paths);
    for (index = 0U; index < scene->count && item_count < AF_MAX_RENDER_ITEMS; ++index) {
        const AfSceneNode *node = &scene->nodes[index];
        const AfStyle *source_style =
            scene->styles == NULL ? NULL : af_style_find_node_const(scene->styles, node->id);
        AfStyle resolved_style;
        const AfStyle *style = NULL;
        if (node->type == AF_NODE_GROUP || !node->world_visible || node->world_opacity <= 0.0F) {
            continue;
        }
        if (source_style != NULL) {
            if (af_style_resolve(scene->styles, source_style, &resolved_style) !=
                AF_RESULT_SUCCESS) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            style = &resolved_style;
        }
        uint32_t fill_index = 0, stroke_index = 0;
        AfColor fill = node->fill, stroke = {0};
        if (style) {
            fill.a *= style->fill_opacity;
            AfResult paint_result = af_vk_upload_paint(paints, &paint_count, node, &style->fill,
                                                       style->fill_opacity, &fill_index);
            if (paint_result != AF_RESULT_SUCCESS)
                return paint_result;
            if (style->stroke.enabled) {
                stroke = style->stroke_paint.solid;
                stroke.a *= style->stroke_opacity;
                paint_result = af_vk_upload_paint(paints, &paint_count, node, &style->stroke_paint,
                                                  style->stroke_opacity, &stroke_index);
                if (paint_result != AF_RESULT_SUCCESS)
                    return paint_result;
            }
        }
        if (node->type == AF_NODE_PATH) {
            const AfPath *path =
                scene->paths == NULL ? NULL : af_path_find_node_const(scene->paths, node->id);
            AfVkPathCache *cache = NULL;
            AfResult result = AF_RESULT_SUCCESS;
            const uint32_t first = path_vertex_count;
            if (path == NULL) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            result = af_vk_update_path_cache(renderer, scene->paths, path, &cache);
            if (result != AF_RESULT_SUCCESS) {
                return result;
            }
            if (cache->vertex_count > AF_MAX_GPU_PATH_VERTICES - path_vertex_count) {
                af_log(AF_LOG_ERROR,
                       "Frame path vertex limit: path=%llu path_vertices=%u incoming=%u limit=%u",
                       (unsigned long long)path->id, path_vertex_count, cache->vertex_count,
                       AF_MAX_GPU_PATH_VERTICES);
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            for (uint32_t vertex_index = 0U; vertex_index < cache->vertex_count; ++vertex_index) {
                AfColor color = fill;
                const AfVec2 local = cache->vertices[vertex_index];
                const AfVec2 world = af_affine_transform_point(node->world_transform, local);
                path_vertices[path_vertex_count++] =
                    (AfGpuPathVertex){.position = {world.x, world.y},
                                      .color = {color.r, color.g, color.b,
                                                node->world_opacity * (fill_index ? 1 : color.a)},
                                      .paint_index = fill_index};
            }
            if (cache->vertex_count != 0U) {
                af_vk_append_render_item(renderer, &item_count,
                                         (AfVkRenderItem){.type = AF_VK_RENDER_PATH,
                                                          .layer_order = node->layer_order,
                                                          .first = first,
                                                          .count = cache->vertex_count});
            }
            if (style != NULL && style->stroke.enabled) {
                AfVkStrokeCache *stroke_cache = NULL;
                const uint32_t stroke_first = path_vertex_count;
                result = af_vk_update_stroke_cache(renderer, scene->paths, path, scene->styles,
                                                   style, &stroke_cache);
                if (result != AF_RESULT_SUCCESS ||
                    stroke_cache->vertex_count > AF_MAX_GPU_PATH_VERTICES - path_vertex_count) {
                    return result == AF_RESULT_SUCCESS ? AF_RESULT_LIMIT_EXCEEDED : result;
                }
                for (uint32_t vertex_index = 0U; vertex_index < stroke_cache->vertex_count;
                     ++vertex_index) {
                    AfColor color = stroke;
                    const AfVec2 local = stroke_cache->vertices[vertex_index];
                    const AfVec2 world = af_affine_transform_point(node->world_transform, local);
                    path_vertices[path_vertex_count++] = (AfGpuPathVertex){
                        .position = {world.x, world.y},
                        .color = {color.r, color.g, color.b,
                                  node->world_opacity * (stroke_index ? 1 : color.a)},
                        .paint_index = stroke_index};
                }
                if (stroke_cache->vertex_count != 0U) {
                    af_vk_append_render_item(renderer, &item_count,
                                             (AfVkRenderItem){.type = AF_VK_RENDER_PATH,
                                                              .layer_order = node->layer_order,
                                                              .first = stroke_first,
                                                              .count = stroke_cache->vertex_count});
                }
            }
        } else if (node->type == AF_NODE_IMAGE) {
            uint32_t atlas_index = 0U;
            const uint32_t first = image_vertex_count;
            AfResult result =
                scene->images == NULL
                    ? AF_RESULT_INVALID_ARGUMENT
                    : af_vk_emit_image_node(node, scene->images, renderer->image_time,
                                            image_vertices, &image_vertex_count, &atlas_index);
            if (result != AF_RESULT_SUCCESS) {
                return result;
            }
            if (image_vertex_count != first) {
                af_vk_append_render_item(renderer, &item_count,
                                         (AfVkRenderItem){.type = AF_VK_RENDER_IMAGE,
                                                          .layer_order = node->layer_order,
                                                          .first = first,
                                                          .count = image_vertex_count - first,
                                                          .atlas_index = atlas_index});
            }
        } else {
            const bool stroke_visible = style != NULL && style->stroke.enabled &&
                                        style->stroke.width > 0.0F && style->stroke_opacity > 0.0F;
            if (node->type > AF_NODE_ELLIPSE ||
                shape_count > AF_MAX_GPU_SHAPES - (stroke_visible ? 2U : 1U)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            AfColor color = fill;
            shapes[shape_count] = (AfGpuShape){
                .matrix = {node->world_transform.m00, node->world_transform.m01,
                           node->world_transform.m10, node->world_transform.m11},
                .translation_bounds = {node->world_transform.tx, node->world_transform.ty,
                                       node->resolved_bounds.x, node->resolved_bounds.y},
                .color = {color.r, color.g, color.b, color.a},
                .parameters = {(float)node->type, node->corner_radius, node->world_opacity, 0.0F},
                .layer_order = node->layer_order,
                .paint_index = fill_index};
            af_vk_append_render_item(renderer, &item_count,
                                     (AfVkRenderItem){.type = AF_VK_RENDER_ANALYTIC,
                                                      .layer_order = node->layer_order,
                                                      .first = shape_count,
                                                      .count = 1U});
            ++shape_count;
            if (stroke_visible) {
                AfColor stroke_color = stroke;
                shapes[shape_count] = (AfGpuShape){
                    .matrix = {node->world_transform.m00, node->world_transform.m01,
                               node->world_transform.m10, node->world_transform.m11},
                    .translation_bounds = {node->world_transform.tx, node->world_transform.ty,
                                           node->resolved_bounds.x, node->resolved_bounds.y},
                    .color = {stroke_color.r, stroke_color.g, stroke_color.b, stroke_color.a},
                    .parameters = {(float)node->type, node->corner_radius, node->world_opacity,
                                   style->stroke.width},
                    .layer_order = node->layer_order,
                    .paint_index = stroke_index};
                af_vk_append_render_item(renderer, &item_count,
                                         (AfVkRenderItem){.type = AF_VK_RENDER_ANALYTIC,
                                                          .layer_order = node->layer_order,
                                                          .first = shape_count,
                                                          .count = 1U});
                ++shape_count;
            }
        }
    }
    for (index = 0U; index < AF_MAX_PATHS; ++index) {
        AfVkPathCache *cache = &renderer->path_caches[index];
        if (cache->path_id != 0U && cache->seen_serial != renderer->path_cache_serial) {
            renderer->cached_path_vertex_count -= cache->vertex_count;
            free(cache->vertices);
            *cache = (AfVkPathCache){0};
        }
        {
            AfVkStrokeCache *stroke_cache = &renderer->stroke_caches[index];
            if (stroke_cache->path_id != 0U &&
                stroke_cache->seen_serial != renderer->path_cache_serial) {
                renderer->cached_stroke_vertex_count -= stroke_cache->vertex_count;
                free(stroke_cache->vertices);
                *stroke_cache = (AfVkStrokeCache){0};
            }
        }
    }
    *out_shape_count = shape_count;
    *out_path_vertex_count = path_vertex_count;
    *out_image_vertex_count = image_vertex_count;
    *out_item_count = af_vk_prepare_render_items(renderer->render_items,
                                                 renderer->render_item_scratch, item_count);
    return AF_RESULT_SUCCESS;
}
