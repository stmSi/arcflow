static AfResult af_vk_update_path_cache(AfVkRenderer *renderer, const AfPathData *data,
                                        const AfPath *path, AfVkPathCache **out_cache) {
    AfVkPathCache *cache = NULL;
    AfVkPathCache *empty = NULL;
    uint32_t vertex_count = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    for (uint32_t index = 0U; index < AF_MAX_PATHS; ++index) {
        if (renderer->path_caches[index].path_id == path->id) {
            cache = &renderer->path_caches[index];
            break;
        }
        if (empty == NULL && renderer->path_caches[index].path_id == 0U) {
            empty = &renderer->path_caches[index];
        }
    }
    if (cache == NULL) {
        cache = empty;
    }
    if (cache == NULL) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    cache->seen_serial = renderer->path_cache_serial;
    if (cache->source == data && cache->path_id == path->id &&
        cache->geometry_generation == path->geometry_generation) {
        *out_cache = cache;
        return AF_RESULT_SUCCESS;
    }
    result = af_path_tessellate(data, path, 0.25F, renderer->path_scratch,
                                renderer->path_staging_vertices, AF_MAX_PATH_TESSELLATED_VERTICES,
                                &vertex_count);
    if (result != AF_RESULT_SUCCESS ||
        vertex_count >
            AF_MAX_GPU_PATH_VERTICES - (renderer->cached_path_vertex_count - cache->vertex_count)) {
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
    renderer->cached_path_vertex_count =
        renderer->cached_path_vertex_count - cache->vertex_count + vertex_count;
    cache->source = data;
    cache->path_id = path->id;
    cache->geometry_generation = path->geometry_generation;
    cache->vertex_count = vertex_count;
    ++cache->rebuild_count;
    *out_cache = cache;
    return AF_RESULT_SUCCESS;
}

static AfResult af_vk_update_stroke_cache(AfVkRenderer *renderer, const AfPathData *paths,
                                          const AfPath *path, const AfStyleData *styles,
                                          const AfStyle *style, AfVkStrokeCache **out_cache) {
    AfVkStrokeCache *cache = NULL;
    AfVkStrokeCache *empty = NULL;
    AfFlattenedPath flattened = {.points = renderer->path_scratch->points,
                                 .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                 .contours = renderer->path_scratch->contours,
                                 .contour_capacity = AF_MAX_PATH_CONTOURS};
    uint32_t vertex_count = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    for (uint32_t index = 0U; index < AF_MAX_PATHS; ++index) {
        if (renderer->stroke_caches[index].path_id == path->id &&
            renderer->stroke_caches[index].style_id == style->id) {
            cache = &renderer->stroke_caches[index];
            break;
        }
        if (empty == NULL && renderer->stroke_caches[index].path_id == 0U) {
            empty = &renderer->stroke_caches[index];
        }
    }
    if (cache == NULL) {
        cache = empty;
    }
    if (cache == NULL) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    cache->seen_serial = renderer->path_cache_serial;
    if (cache->path_source == paths && cache->style_source == styles &&
        cache->path_id == path->id && cache->style_id == style->id &&
        cache->path_geometry_generation == path->geometry_generation &&
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
    cache->stroke_geometry_generation = style->stroke_geometry_generation;
    cache->vertex_count = vertex_count;
    ++cache->rebuild_count;
    *out_cache = cache;
    return AF_RESULT_SUCCESS;
}

static void af_vk_insert_render_item(AfVkRenderer *renderer, uint32_t *count, AfVkRenderItem item) {
    uint32_t insert = *count;
    while (insert > 0U && renderer->render_items[insert - 1U].layer_order > item.layer_order) {
        renderer->render_items[insert] = renderer->render_items[insert - 1U];
        --insert;
    }
    renderer->render_items[insert] = item;
    ++(*count);
}

static AfResult af_vk_upload_scene(AfVkRenderer *renderer, const AfScene *scene,
                                   uint32_t *out_shape_count, uint32_t *out_path_vertex_count,
                                   uint32_t *out_item_count) {
    uint8_t *slice = renderer->upload_mapping + renderer->frame_index * renderer->upload_slice_size;
    AfGpuShape *shapes = (AfGpuShape *)(void *)slice;
    AfGpuPathVertex *path_vertices =
        (AfGpuPathVertex *)(void *)(slice + AF_MAX_NODES * sizeof(AfGpuShape));
    uint32_t shape_count = 0U;
    uint32_t path_vertex_count = 0U;
    uint32_t item_count = 0U;
    uint32_t index = 0U;
    if (++renderer->path_cache_serial == 0U) {
        renderer->path_cache_serial = 1U;
    }
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
            if (result != AF_RESULT_SUCCESS ||
                cache->vertex_count > AF_MAX_GPU_PATH_VERTICES - path_vertex_count) {
                return result == AF_RESULT_SUCCESS ? AF_RESULT_LIMIT_EXCEEDED : result;
            }
            for (uint32_t vertex_index = 0U; vertex_index < cache->vertex_count; ++vertex_index) {
                AfColor color = node->fill;
                const AfVec2 world =
                    af_affine_transform_point(node->world_transform, cache->vertices[vertex_index]);
                if (style != NULL && af_paint_sample(&style->fill, cache->vertices[vertex_index],
                                                     style->fill_opacity * node->world_opacity,
                                                     &color) != AF_RESULT_SUCCESS) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
                path_vertices[path_vertex_count++] = (AfGpuPathVertex){
                    .position = {world.x, world.y},
                    .color = {color.r, color.g, color.b,
                              style == NULL ? color.a * node->world_opacity : color.a}};
            }
            if (cache->vertex_count != 0U) {
                af_vk_insert_render_item(renderer, &item_count,
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
                    AfColor color;
                    const AfVec2 local = stroke_cache->vertices[vertex_index];
                    const AfVec2 world = af_affine_transform_point(node->world_transform, local);
                    if (af_paint_sample(&style->stroke_paint, local,
                                        style->stroke_opacity * node->world_opacity,
                                        &color) != AF_RESULT_SUCCESS) {
                        return AF_RESULT_INVALID_ARGUMENT;
                    }
                    path_vertices[path_vertex_count++] =
                        (AfGpuPathVertex){.position = {world.x, world.y},
                                          .color = {color.r, color.g, color.b, color.a}};
                }
                if (stroke_cache->vertex_count != 0U) {
                    af_vk_insert_render_item(renderer, &item_count,
                                             (AfVkRenderItem){.type = AF_VK_RENDER_PATH,
                                                              .layer_order = node->layer_order,
                                                              .first = stroke_first,
                                                              .count = stroke_cache->vertex_count});
                }
            }
        } else {
            if (node->type > AF_NODE_ELLIPSE || shape_count >= AF_MAX_NODES) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            AfColor color = node->fill;
            if (style != NULL &&
                af_paint_sample(
                    &style->fill,
                    (AfVec2){node->resolved_bounds.x * 0.5F, node->resolved_bounds.y * 0.5F},
                    style->fill_opacity, &color) != AF_RESULT_SUCCESS) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            shapes[shape_count] = (AfGpuShape){
                .matrix = {node->world_transform.m00, node->world_transform.m01,
                           node->world_transform.m10, node->world_transform.m11},
                .translation_bounds = {node->world_transform.tx, node->world_transform.ty,
                                       node->resolved_bounds.x, node->resolved_bounds.y},
                .color = {color.r, color.g, color.b, color.a},
                .parameters = {(float)node->type, node->corner_radius, node->world_opacity, 0.0F},
                .layer_order = node->layer_order};
            af_vk_insert_render_item(renderer, &item_count,
                                     (AfVkRenderItem){.type = AF_VK_RENDER_ANALYTIC,
                                                      .layer_order = node->layer_order,
                                                      .first = shape_count,
                                                      .count = 1U});
            ++shape_count;
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
    *out_item_count = item_count;
    return AF_RESULT_SUCCESS;
}
