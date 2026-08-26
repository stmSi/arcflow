#define AF_LOTTIE_VERSION_TEXT "5.12.2"
#define AF_LOTTIE_SPEC_VERSION 10001U
#define AF_LOTTIE_MAX_FRAME_RATE 240.0F
#define AF_LOTTIE_RADIANS_TO_DEGREES 57.29577951308232F

typedef struct AfLottieKeyRef {
    const AfProjectKeyframe *key;
} AfLottieKeyRef;

typedef struct AfLottieValueMap {
    uint32_t dimensions;
    int8_t components[4];
    float scales[4];
    float defaults[4];
} AfLottieValueMap;

static const AfProjectClip *af_lottie_find_clip(const AfProject *project, uint64_t clip_id) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->clip_count; ++index) {
        if (project->clips[index].id == clip_id) {
            return &project->clips[index];
        }
    }
    return NULL;
}

static const AfProjectTrack *af_lottie_find_track(const AfProject *project, uint64_t clip_id,
                                                  uint64_t node_id, AfPropertyType property) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->track_count; ++index) {
        const AfProjectTrack *track = &project->tracks[index];
        if (track->clip_id == clip_id && track->node_id == node_id && track->property == property) {
            return track;
        }
    }
    return NULL;
}

static int af_lottie_compare_keys(const void *left, const void *right) {
    const AfProjectKeyframe *a = ((const AfLottieKeyRef *)left)->key;
    const AfProjectKeyframe *b = ((const AfLottieKeyRef *)right)->key;
    if (a->keyframe.time < b->keyframe.time) {
        return -1;
    }
    if (a->keyframe.time > b->keyframe.time) {
        return 1;
    }
    return a->id < b->id ? -1 : (a->id > b->id ? 1 : 0);
}

static AfResult af_lottie_collect_keys(const AfProject *project, const AfProjectTrack *track,
                                       AfLottieKeyRef **out_keys, uint32_t *out_count) {
    AfLottieKeyRef *keys = NULL;
    uint32_t count = 0U;
    if (project == NULL || track == NULL || out_keys == NULL || out_count == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_keys = NULL;
    *out_count = 0U;
    for (uint32_t index = 0U; index < project->keyframe_count; ++index) {
        count += project->keyframes[index].track_id == track->id ? 1U : 0U;
    }
    if (count == 0U) {
        return AF_RESULT_SUCCESS;
    }
    keys = malloc((size_t)count * sizeof(*keys));
    if (keys == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    count = 0U;
    for (uint32_t index = 0U; index < project->keyframe_count; ++index) {
        if (project->keyframes[index].track_id == track->id) {
            keys[count++].key = &project->keyframes[index];
        }
    }
    qsort(keys, count, sizeof(*keys), af_lottie_compare_keys);
    {
        uint32_t compacted = 0U;
        for (uint32_t index = 0U; index < count; ++index) {
            if (compacted != 0U &&
                keys[index].key->keyframe.time == keys[compacted - 1U].key->keyframe.time) {
                keys[compacted - 1U] = keys[index];
            } else {
                keys[compacted++] = keys[index];
            }
        }
        count = compacted;
    }
    *out_keys = keys;
    *out_count = count;
    return AF_RESULT_SUCCESS;
}

static bool af_lottie_json_string(AfInterchangeWriter *writer, const char *text) {
    static const char hexadecimal[] = "0123456789abcdef";
    if (writer == NULL || text == NULL || !af_interchange_writer_append(writer, "\"")) {
        return false;
    }
    for (const unsigned char *byte = (const unsigned char *)text; *byte != 0U; ++byte) {
        char escaped[7] = {0};
        if (*byte == '\"' || *byte == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char)*byte;
        } else if (*byte == '\b') {
            memcpy(escaped, "\\b", 3U);
        } else if (*byte == '\f') {
            memcpy(escaped, "\\f", 3U);
        } else if (*byte == '\n') {
            memcpy(escaped, "\\n", 3U);
        } else if (*byte == '\r') {
            memcpy(escaped, "\\r", 3U);
        } else if (*byte == '\t') {
            memcpy(escaped, "\\t", 3U);
        } else if (*byte < 32U) {
            memcpy(escaped, "\\u00", 4U);
            escaped[4] = hexadecimal[*byte >> 4U];
            escaped[5] = hexadecimal[*byte & 15U];
        } else {
            escaped[0] = (char)*byte;
        }
        if (!af_interchange_writer_append(writer, "%s", escaped)) {
            return false;
        }
    }
    return af_interchange_writer_append(writer, "\"");
}

static float af_lottie_value_component(const AfLottieValueMap *map, const AfKeyframe *key,
                                       uint32_t dimension) {
    const int8_t component = map->components[dimension];
    const float source =
        component < 0 || key == NULL ? map->defaults[dimension] : key->value[(uint32_t)component];
    return source * map->scales[dimension];
}

static bool af_lottie_write_value(AfInterchangeWriter *writer, const AfLottieValueMap *map,
                                  const AfKeyframe *key, bool force_array) {
    const bool array = force_array || map->dimensions != 1U;
    if (array && !af_interchange_writer_append(writer, "[")) {
        return false;
    }
    for (uint32_t dimension = 0U; dimension < map->dimensions; ++dimension) {
        if (!af_interchange_writer_append(writer, dimension == 0U ? "%.9g" : ",%.9g",
                                          af_lottie_value_component(map, key, dimension))) {
            return false;
        }
    }
    return !array || af_interchange_writer_append(writer, "]");
}

static bool af_lottie_write_property(AfInterchangeWriter *writer, const AfProject *project,
                                     const AfProjectTrack *track, const AfLottieValueMap *map,
                                     float frame_rate, AfResult *out_result) {
    AfLottieKeyRef *keys = NULL;
    uint32_t key_count = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (writer == NULL || project == NULL || map == NULL || out_result == NULL ||
        map->dimensions == 0U || map->dimensions > 4U) {
        if (out_result != NULL) {
            *out_result = AF_RESULT_INVALID_ARGUMENT;
        }
        return false;
    }
    if (track != NULL) {
        result = af_lottie_collect_keys(project, track, &keys, &key_count);
        if (result != AF_RESULT_SUCCESS) {
            *out_result = result;
            return false;
        }
    }
    if (key_count == 0U) {
        const bool written = af_interchange_writer_append(writer, "{\"a\":0,\"k\":") &&
                             af_lottie_write_value(writer, map, NULL, false) &&
                             af_interchange_writer_append(writer, "}");
        free(keys);
        return written;
    }
    if (!af_interchange_writer_append(writer, "{\"a\":1,\"k\":[")) {
        free(keys);
        return false;
    }
    for (uint32_t index = 0U; index < key_count; ++index) {
        const AfKeyframe *key = &keys[index].key->keyframe;
        if (!af_interchange_writer_append(
                writer, index == 0U ? "{\"t\":%.9g,\"s\":" : ",{\"t\":%.9g,\"s\":",
                key->time * frame_rate) ||
            !af_lottie_write_value(writer, map, key, true)) {
            free(keys);
            return false;
        }
        if (index + 1U < key_count) {
            const AfKeyframe *next = &keys[index + 1U].key->keyframe;
            if (!af_interchange_writer_append(writer, ",\"e\":") ||
                !af_lottie_write_value(writer, map, next, true)) {
                free(keys);
                return false;
            }
            if (key->interpolation == AF_INTERPOLATION_STEP) {
                if (!af_interchange_writer_append(writer, ",\"h\":1")) {
                    free(keys);
                    return false;
                }
            } else {
                const float out_x =
                    key->interpolation == AF_INTERPOLATION_CUBIC_BEZIER ? key->easing[0] : 0.0F;
                const float out_y =
                    key->interpolation == AF_INTERPOLATION_CUBIC_BEZIER ? key->easing[1] : 0.0F;
                const float in_x =
                    key->interpolation == AF_INTERPOLATION_CUBIC_BEZIER ? key->easing[2] : 1.0F;
                const float in_y =
                    key->interpolation == AF_INTERPOLATION_CUBIC_BEZIER ? key->easing[3] : 1.0F;
                if (!af_interchange_writer_append(
                        writer, ",\"o\":{\"x\":%.9g,\"y\":%.9g},\"i\":{\"x\":%.9g,\"y\":%.9g}",
                        out_x, out_y, in_x, in_y)) {
                    free(keys);
                    return false;
                }
            }
        }
        if (!af_interchange_writer_append(writer, "}")) {
            free(keys);
            return false;
        }
    }
    free(keys);
    return af_interchange_writer_append(writer, "]}");
}

static bool af_lottie_layer_transform(AfInterchangeWriter *writer, const AfProject *project,
                                      const AfProjectClip *clip, const AfSceneNode *node,
                                      float frame_rate, AfResult *out_result,
                                      AfInterchangeDiagnostics *diagnostics,
                                      uint32_t source_index) {
    const float degrees = AF_LOTTIE_RADIANS_TO_DEGREES;
    AfLottieValueMap position = {
        .dimensions = 2U,
        .components = {0, 1, -1, -1},
        .scales = {1.0F, 1.0F, 1.0F, 1.0F},
        .defaults = {node->local_transform.position.x, node->local_transform.position.y}};
    AfLottieValueMap rotation = {.dimensions = 1U,
                                 .components = {0, -1, -1, -1},
                                 .scales = {degrees, 1.0F, 1.0F, 1.0F},
                                 .defaults = {node->local_transform.rotation}};
    AfLottieValueMap scale = {
        .dimensions = 2U,
        .components = {0, 1, -1, -1},
        .scales = {100.0F, 100.0F, 1.0F, 1.0F},
        .defaults = {node->local_transform.scale.x, node->local_transform.scale.y}};
    AfLottieValueMap opacity = {.dimensions = 1U,
                                .components = {0, -1, -1, -1},
                                .scales = {100.0F, 1.0F, 1.0F, 1.0F},
                                .defaults = {node->opacity}};
    AfLottieValueMap skew = {.dimensions = 1U,
                             .components = {0, -1, -1, -1},
                             .scales = {degrees, 1.0F, 1.0F, 1.0F},
                             .defaults = {node->local_transform.skew.x}};
    const AfProjectTrack *skew_track =
        af_lottie_find_track(project, clip->id, node->id, AF_PROPERTY_SKEW);
    if (!af_interchange_writer_append(writer, "{\"a\":{\"a\":0,\"k\":[0,0]},\"p\":") ||
        !af_lottie_write_property(
            writer, project,
            af_lottie_find_track(project, clip->id, node->id, AF_PROPERTY_POSITION), &position,
            frame_rate, out_result) ||
        !af_interchange_writer_append(writer, ",\"s\":") ||
        !af_lottie_write_property(
            writer, project, af_lottie_find_track(project, clip->id, node->id, AF_PROPERTY_SCALE),
            &scale, frame_rate, out_result) ||
        !af_interchange_writer_append(writer, ",\"r\":") ||
        !af_lottie_write_property(
            writer, project,
            af_lottie_find_track(project, clip->id, node->id, AF_PROPERTY_ROTATION), &rotation,
            frame_rate, out_result) ||
        !af_interchange_writer_append(writer, ",\"o\":") ||
        !af_lottie_write_property(
            writer, project, af_lottie_find_track(project, clip->id, node->id, AF_PROPERTY_OPACITY),
            &opacity, frame_rate, out_result)) {
        return false;
    }
    if (skew_track != NULL || fabsf(node->local_transform.skew.x) > 0.00001F ||
        fabsf(node->local_transform.skew.y) > 0.00001F) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                              AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, source_index, node->id,
                              "Two-axis Arcflow skew is approximated with Lottie single-axis skew");
        if (!af_interchange_writer_append(writer, ",\"sk\":") ||
            !af_lottie_write_property(writer, project, skew_track, &skew, frame_rate, out_result) ||
            !af_interchange_writer_append(writer, ",\"sa\":{\"a\":0,\"k\":0}")) {
            return false;
        }
    }
    return af_interchange_writer_append(writer, "}");
}

static bool af_lottie_path_contour(AfInterchangeWriter *writer, const AfPathData *paths,
                                   uint32_t command_start, uint32_t command_count,
                                   const char *name) {
    AfVec2 *vertices = NULL;
    AfVec2 *in_tangents = NULL;
    AfVec2 *out_tangents = NULL;
    uint32_t vertex_count = 0U;
    bool closed = false;
    bool result = false;
    if (command_count == 0U) {
        return true;
    }
    vertices = calloc(command_count, sizeof(*vertices));
    in_tangents = calloc(command_count, sizeof(*in_tangents));
    out_tangents = calloc(command_count, sizeof(*out_tangents));
    if (vertices == NULL || in_tangents == NULL || out_tangents == NULL) {
        goto cleanup;
    }
    for (uint32_t index = 0U; index < command_count; ++index) {
        const AfPathCommand *command = &paths->commands[command_start + index];
        if (command->type == AF_PATH_CLOSE) {
            closed = true;
            continue;
        }
        if (command->type == AF_PATH_MOVE || command->type == AF_PATH_LINE ||
            command->type == AF_PATH_CUBIC) {
            if (command->type == AF_PATH_CUBIC && vertex_count != 0U) {
                out_tangents[vertex_count - 1U] =
                    (AfVec2){command->control1.x - vertices[vertex_count - 1U].x,
                             command->control1.y - vertices[vertex_count - 1U].y};
                in_tangents[vertex_count] = (AfVec2){command->control2.x - command->point.x,
                                                     command->control2.y - command->point.y};
            }
            vertices[vertex_count++] = command->point;
        }
    }
    if (vertex_count == 0U || !af_interchange_writer_append(writer, "{\"ty\":\"sh\",\"nm\":") ||
        !af_lottie_json_string(writer, name) ||
        !af_interchange_writer_append(writer, ",\"ks\":{\"a\":0,\"k\":{\"c\":%s,\"v\":[",
                                      closed ? "true" : "false")) {
        goto cleanup;
    }
    for (uint32_t index = 0U; index < vertex_count; ++index) {
        if (!af_interchange_writer_append(writer, index == 0U ? "[%.9g,%.9g]" : ",[%.9g,%.9g]",
                                          vertices[index].x, vertices[index].y)) {
            goto cleanup;
        }
    }
    if (!af_interchange_writer_append(writer, "],\"i\":[")) {
        goto cleanup;
    }
    for (uint32_t index = 0U; index < vertex_count; ++index) {
        if (!af_interchange_writer_append(writer, index == 0U ? "[%.9g,%.9g]" : ",[%.9g,%.9g]",
                                          in_tangents[index].x, in_tangents[index].y)) {
            goto cleanup;
        }
    }
    if (!af_interchange_writer_append(writer, "],\"o\":[")) {
        goto cleanup;
    }
    for (uint32_t index = 0U; index < vertex_count; ++index) {
        if (!af_interchange_writer_append(writer, index == 0U ? "[%.9g,%.9g]" : ",[%.9g,%.9g]",
                                          out_tangents[index].x, out_tangents[index].y)) {
            goto cleanup;
        }
    }
    result = af_interchange_writer_append(writer, "]}}}");

cleanup:
    free(out_tangents);
    free(in_tangents);
    free(vertices);
    return result;
}

static bool af_lottie_node_geometry(AfInterchangeWriter *writer, const AfProject *project,
                                    const AfProjectClip *clip, const AfSceneNode *node,
                                    const char *name, float frame_rate, AfResult *out_result,
                                    AfInterchangeDiagnostics *diagnostics, uint32_t source_index) {
    const AfProjectTrack *radius_track =
        af_lottie_find_track(project, clip->id, node->id, AF_PROPERTY_CORNER_RADIUS);
    AfLottieValueMap radius = {.dimensions = 1U,
                               .components = {0, -1, -1, -1},
                               .scales = {1.0F, 1.0F, 1.0F, 1.0F},
                               .defaults = {node->corner_radius}};
    if (node->type == AF_NODE_RECTANGLE || node->type == AF_NODE_ROUNDED_RECTANGLE) {
        if (!af_interchange_writer_append(writer, "{\"ty\":\"rc\",\"nm\":") ||
            !af_lottie_json_string(writer, name) ||
            !af_interchange_writer_append(
                writer,
                ",\"p\":{\"a\":0,\"k\":[%.9g,%.9g]},\"s\":{\"a\":0,\"k\":[%.9g,%.9g]},\"r\":",
                node->bounds.x * 0.5F, node->bounds.y * 0.5F, node->bounds.x, node->bounds.y) ||
            !af_lottie_write_property(writer, project, radius_track, &radius, frame_rate,
                                      out_result) ||
            !af_interchange_writer_append(writer, "}")) {
            return false;
        }
        return true;
    }
    if (node->type == AF_NODE_ELLIPSE) {
        return af_interchange_writer_append(writer, "{\"ty\":\"el\",\"nm\":") &&
               af_lottie_json_string(writer, name) &&
               af_interchange_writer_append(
                   writer, ",\"p\":{\"a\":0,\"k\":[%.9g,%.9g]},\"s\":{\"a\":0,\"k\":[%.9g,%.9g]}}",
                   node->bounds.x * 0.5F, node->bounds.y * 0.5F, node->bounds.x, node->bounds.y);
    }
    if (node->type == AF_NODE_PATH) {
        const AfPath *path = af_path_find_node_const(&project->path_data, node->id);
        uint32_t contour = 0U;
        uint32_t start = 0U;
        if (path == NULL || path->command_start > project->path_data.command_count ||
            path->command_count > project->path_data.command_count - path->command_start) {
            *out_result = AF_RESULT_INVALID_ARGUMENT;
            return false;
        }
        for (uint32_t index = 0U; index <= path->command_count; ++index) {
            const bool boundary =
                index == path->command_count ||
                (index != start &&
                 project->path_data.commands[path->command_start + index].type == AF_PATH_MOVE);
            if (boundary) {
                char contour_name[AF_PROJECT_NAME_CAPACITY + 24U];
                (void)snprintf(contour_name, sizeof(contour_name), "%s contour %u", name,
                               contour + 1U);
                if (contour != 0U && !af_interchange_writer_append(writer, ",")) {
                    return false;
                }
                if (!af_lottie_path_contour(writer, &project->path_data,
                                            path->command_start + start, index - start,
                                            contour_name)) {
                    return false;
                }
                ++contour;
                start = index;
            }
        }
        for (uint32_t index = 0U; index < project->track_count; ++index) {
            const AfProjectTrack *track = &project->tracks[index];
            if (track->clip_id == clip->id && track->node_id == node->id &&
                track->property >= AF_PROPERTY_PATH_POINT &&
                track->property <= AF_PROPERTY_PATH_CONTROL_OUT) {
                af_interchange_report(
                    diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                    AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, source_index, node->id,
                    "Animated path points are exported at their authored shape; use GIF or MP4 "
                    "to preserve this motion");
                break;
            }
        }
        return true;
    }
    return true;
}

static bool af_lottie_node_style(AfInterchangeWriter *writer, const AfProject *project,
                                 const AfProjectClip *clip, const AfSceneNode *node,
                                 float frame_rate, AfResult *out_result,
                                 AfInterchangeDiagnostics *diagnostics, uint32_t source_index) {
    const AfStyle *style = af_style_find_node_const(&project->style_data, node->id);
    AfStyle resolved = {0};
    AfColor fill = node->fill;
    float fill_opacity = 1.0F;
    const AfProjectTrack *fill_track =
        af_lottie_find_track(project, clip->id, node->id, AF_PROPERTY_FILL_COLOR);
    if (style != NULL) {
        if (af_style_resolve(&project->style_data, style, &resolved) != AF_RESULT_SUCCESS) {
            *out_result = AF_RESULT_INVALID_ARGUMENT;
            return false;
        }
        if (resolved.fill.type == AF_PAINT_SOLID) {
            fill = resolved.fill.solid;
        } else {
            af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                                  AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, source_index,
                                  node->id,
                                  "Gradient fill is approximated by the object's solid fallback");
        }
        fill_opacity = resolved.fill_opacity;
    }
    {
        AfLottieValueMap color = {.dimensions = 4U,
                                  .components = {0, 1, 2, -1},
                                  .scales = {1.0F, 1.0F, 1.0F, 1.0F},
                                  .defaults = {fill.r, fill.g, fill.b, 1.0F}};
        AfLottieValueMap alpha = {.dimensions = 1U,
                                  .components = {3, -1, -1, -1},
                                  .scales = {100.0F * fill_opacity, 1.0F, 1.0F, 1.0F},
                                  .defaults = {fill.a}};
        if (!af_interchange_writer_append(writer, ",{\"ty\":\"fl\",\"c\":") ||
            !af_lottie_write_property(writer, project, fill_track, &color, frame_rate,
                                      out_result) ||
            !af_interchange_writer_append(writer, ",\"o\":") ||
            !af_lottie_write_property(writer, project, fill_track, &alpha, frame_rate,
                                      out_result) ||
            !af_interchange_writer_append(
                writer, ",\"r\":%u}",
                node->type == AF_NODE_PATH &&
                        af_path_find_node_const(&project->path_data, node->id) != NULL &&
                        af_path_find_node_const(&project->path_data, node->id)->fill_rule ==
                            AF_PATH_FILL_EVEN_ODD
                    ? 2U
                    : 1U)) {
            return false;
        }
    }
    if (style != NULL && resolved.stroke.enabled) {
        if (resolved.stroke_paint.type != AF_PAINT_SOLID ||
            fabsf(resolved.stroke.width - resolved.stroke.end_width) > 0.00001F ||
            resolved.stroke.dash_count != 0U) {
            af_interchange_report(
                diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, source_index, node->id,
                "Gradient, dashed, or variable-width stroke is approximated by a solid stroke");
        }
        if (!af_interchange_writer_append(
                writer,
                ",{\"ty\":\"st\",\"c\":{\"a\":0,\"k\":[%.9g,%.9g,%.9g,1]},"
                "\"o\":{\"a\":0,\"k\":%.9g},\"w\":{\"a\":0,\"k\":%.9g},"
                "\"lc\":%u,\"lj\":%u,\"ml\":%.9g}",
                resolved.stroke_paint.solid.r, resolved.stroke_paint.solid.g,
                resolved.stroke_paint.solid.b,
                resolved.stroke_paint.solid.a * resolved.stroke_opacity * 100.0F,
                resolved.stroke.width,
                resolved.stroke.cap == AF_STROKE_CAP_ROUND
                    ? 2U
                    : (resolved.stroke.cap == AF_STROKE_CAP_SQUARE ? 3U : 1U),
                resolved.stroke.join == AF_STROKE_JOIN_ROUND
                    ? 2U
                    : (resolved.stroke.join == AF_STROKE_JOIN_BEVEL ? 3U : 1U),
                resolved.stroke.miter_limit)) {
            return false;
        }
    }
    return true;
}

AfResult af_interchange_export_lottie_json(const AfProject *project, uint64_t clip_id,
                                           const AfLottieExportOptions *options, char *output,
                                           size_t output_capacity, size_t *out_size,
                                           AfInterchangeDiagnostics *diagnostics) {
    AfInterchangeWriter writer = {.output = output, .capacity = output_capacity};
    const AfProjectClip *clip = af_lottie_find_clip(project, clip_id);
    const float frame_rate = options == NULL || options->frame_rate == 0.0F
                                 ? AF_INTERCHANGE_DEFAULT_FRAME_RATE
                                 : options->frame_rate;
    uint32_t order[AF_MAX_NODES];
    AfResult result = AF_RESULT_SUCCESS;
    if (output != NULL && output_capacity != 0U) {
        output[0] = '\0';
    }
    if (out_size != NULL) {
        *out_size = 0U;
    }
    if (project == NULL || clip == NULL || output == NULL || output_capacity == 0U ||
        out_size == NULL || !isfinite(frame_rate) || frame_rate <= 0.0F ||
        frame_rate > AF_LOTTIE_MAX_FRAME_RATE || !isfinite(project->canvas_size.x) ||
        !isfinite(project->canvas_size.y) || project->canvas_size.x <= 0.0F ||
        project->canvas_size.y <= 0.0F || !isfinite(clip->duration) || clip->duration < 0.0F ||
        project->scene.count > AF_MAX_NODES || project->scene.nodes == NULL ||
        (project->track_count != 0U && project->tracks == NULL) ||
        (project->keyframe_count != 0U && project->keyframes == NULL) ||
        af_scene_validate(&project->scene) != AF_RESULT_SUCCESS ||
        af_path_data_validate(&project->path_data) != AF_RESULT_SUCCESS ||
        af_style_data_validate(&project->style_data) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->scene.count; ++index) {
        uint32_t position = index;
        while (position > 0U && project->scene.nodes[order[position - 1U]].layer_order <
                                    project->scene.nodes[index].layer_order) {
            order[position] = order[position - 1U];
            --position;
        }
        order[position] = index;
    }
    if (project->particle_data.count != 0U) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                              AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, 0U, 0U,
                              "Particles are not representable in Lottie; use GIF or MP4");
    }
    if (!af_interchange_writer_append(
            &writer,
            "{\"v\":\"%s\",\"ver\":%u,\"fr\":%.9g,\"ip\":0,\"op\":%.9g,\"w\":%u,\"h\":%u,"
            "\"nm\":",
            AF_LOTTIE_VERSION_TEXT, AF_LOTTIE_SPEC_VERSION, frame_rate,
            fmaxf(1.0F, ceilf(clip->duration * frame_rate)),
            (uint32_t)fmaxf(1.0F, roundf(project->canvas_size.x)),
            (uint32_t)fmaxf(1.0F, roundf(project->canvas_size.y))) ||
        !af_lottie_json_string(&writer, clip->name) ||
        !af_interchange_writer_append(&writer, ",\"ddd\":0,\"assets\":[],\"layers\":[")) {
        goto output_limit;
    }
    for (uint32_t output_index = 0U; output_index < project->scene.count; ++output_index) {
        const uint32_t node_index = order[output_index];
        const AfSceneNode *node = &project->scene.nodes[node_index];
        const char *name = project->names == NULL ? "Layer" : project->names[node_index];
        uint32_t parent_index = UINT32_MAX;
        if (node->parent_id != AF_INVALID_NODE_ID) {
            for (uint32_t index = 0U; index < project->scene.count; ++index) {
                if (project->scene.nodes[index].id == node->parent_id) {
                    parent_index = index;
                    break;
                }
            }
        }
        if (!af_interchange_writer_append(&writer,
                                          output_index == 0U
                                              ? "{\"ddd\":0,\"ind\":%u,\"ty\":4,\"nm\":"
                                              : ",{\"ddd\":0,\"ind\":%u,\"ty\":4,\"nm\":",
                                          node_index + 1U) ||
            !af_lottie_json_string(&writer, name) ||
            (parent_index != UINT32_MAX &&
             !af_interchange_writer_append(&writer, ",\"parent\":%u", parent_index + 1U)) ||
            !af_interchange_writer_append(&writer, ",\"sr\":1,\"ks\":") ||
            !af_lottie_layer_transform(&writer, project, clip, node, frame_rate, &result,
                                       diagnostics, node_index) ||
            !af_interchange_writer_append(&writer, ",\"ao\":0,\"shapes\":[")) {
            goto output_limit;
        }
        if (node->type != AF_NODE_GROUP && node->type != AF_NODE_IMAGE) {
            if (!af_lottie_node_geometry(&writer, project, clip, node, name, frame_rate, &result,
                                         diagnostics, node_index) ||
                !af_lottie_node_style(&writer, project, clip, node, frame_rate, &result,
                                      diagnostics, node_index)) {
                goto output_limit;
            }
        } else if (node->type == AF_NODE_IMAGE) {
            af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                                  AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, node_index,
                                  node->id,
                                  "Image layer was omitted from Lottie; use dotLottie asset "
                                  "embedding in a future export version or use GIF/MP4");
        }
        if (!af_interchange_writer_append(&writer,
                                          "],\"ip\":0,\"op\":%.9g,\"st\":0,\"bm\":0,\"hd\":%s}",
                                          fmaxf(1.0F, ceilf(clip->duration * frame_rate)),
                                          node->visible ? "false" : "true")) {
            goto output_limit;
        }
    }
    if (!af_interchange_writer_append(&writer, "]}\n")) {
        goto output_limit;
    }
    *out_size = writer.size;
    return AF_RESULT_SUCCESS;

output_limit:
    output[0] = '\0';
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                          AF_INTERCHANGE_DIAGNOSTIC_OUTPUT_LIMIT, 0U, 0U,
                          "Lottie JSON output capacity is too small");
    return AF_RESULT_LIMIT_EXCEEDED;
}

typedef struct AfDotLottieWriter {
    uint8_t *output;
    size_t capacity;
    size_t size;
} AfDotLottieWriter;

typedef struct AfDotLottieEntry {
    const char *name;
    const uint8_t *data;
    uint32_t size;
    uint32_t compressed_size;
    uint32_t crc;
    uint32_t offset;
} AfDotLottieEntry;

static bool af_dotlottie_write(AfDotLottieWriter *writer, const void *data, size_t size) {
    if (writer == NULL || data == NULL || size > writer->capacity - writer->size) {
        return false;
    }
    memcpy(writer->output + writer->size, data, size);
    writer->size += size;
    return true;
}

static bool af_dotlottie_u16(AfDotLottieWriter *writer, uint16_t value) {
    const uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8U)};
    return af_dotlottie_write(writer, bytes, sizeof(bytes));
}

static bool af_dotlottie_u32(AfDotLottieWriter *writer, uint32_t value) {
    const uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8U), (uint8_t)(value >> 16U),
                              (uint8_t)(value >> 24U)};
    return af_dotlottie_write(writer, bytes, sizeof(bytes));
}

static uint32_t af_dotlottie_crc32(const uint8_t *data, uint32_t size) {
    uint32_t crc = UINT32_MAX;
    for (uint32_t index = 0U; index < size; ++index) {
        crc ^= data[index];
        for (uint32_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ (UINT32_C(0xedb88320) & (uint32_t)-(int32_t)(crc & 1U));
        }
    }
    return crc ^ UINT32_MAX;
}

static uint32_t af_dotlottie_deflate_size(uint32_t size) {
    const uint32_t blocks = size == 0U ? 1U : (size + 65534U) / 65535U;
    return size + blocks * 5U;
}

static bool af_dotlottie_deflate(AfDotLottieWriter *writer, const uint8_t *data, uint32_t size) {
    uint32_t offset = 0U;
    do {
        const uint32_t remaining = size - offset;
        const uint16_t block_size = (uint16_t)(remaining > 65535U ? 65535U : remaining);
        const uint16_t inverse_size = (uint16_t)~block_size;
        const uint8_t header = offset + block_size == size ? 1U : 0U;
        if (!af_dotlottie_write(writer, &header, 1U) || !af_dotlottie_u16(writer, block_size) ||
            !af_dotlottie_u16(writer, inverse_size) ||
            (block_size != 0U && !af_dotlottie_write(writer, data + offset, block_size))) {
            return false;
        }
        offset += block_size;
    } while (offset < size);
    return true;
}

static bool af_dotlottie_local_entry(AfDotLottieWriter *writer, AfDotLottieEntry *entry) {
    const size_t name_size = strlen(entry->name);
    if (name_size > UINT16_MAX || writer->size > UINT32_MAX) {
        return false;
    }
    entry->offset = (uint32_t)writer->size;
    return af_dotlottie_u32(writer, UINT32_C(0x04034b50)) && af_dotlottie_u16(writer, 20U) &&
           af_dotlottie_u16(writer, 0U) && af_dotlottie_u16(writer, 8U) &&
           af_dotlottie_u16(writer, 0U) && af_dotlottie_u16(writer, 0U) &&
           af_dotlottie_u32(writer, entry->crc) &&
           af_dotlottie_u32(writer, entry->compressed_size) &&
           af_dotlottie_u32(writer, entry->size) && af_dotlottie_u16(writer, (uint16_t)name_size) &&
           af_dotlottie_u16(writer, 0U) && af_dotlottie_write(writer, entry->name, name_size) &&
           af_dotlottie_deflate(writer, entry->data, entry->size);
}

static bool af_dotlottie_central_entry(AfDotLottieWriter *writer, const AfDotLottieEntry *entry) {
    const size_t name_size = strlen(entry->name);
    return af_dotlottie_u32(writer, UINT32_C(0x02014b50)) && af_dotlottie_u16(writer, 20U) &&
           af_dotlottie_u16(writer, 20U) && af_dotlottie_u16(writer, 0U) &&
           af_dotlottie_u16(writer, 8U) && af_dotlottie_u16(writer, 0U) &&
           af_dotlottie_u16(writer, 0U) && af_dotlottie_u32(writer, entry->crc) &&
           af_dotlottie_u32(writer, entry->compressed_size) &&
           af_dotlottie_u32(writer, entry->size) && af_dotlottie_u16(writer, (uint16_t)name_size) &&
           af_dotlottie_u16(writer, 0U) && af_dotlottie_u16(writer, 0U) &&
           af_dotlottie_u16(writer, 0U) && af_dotlottie_u16(writer, 0U) &&
           af_dotlottie_u32(writer, 0U) && af_dotlottie_u32(writer, entry->offset) &&
           af_dotlottie_write(writer, entry->name, name_size);
}

AfResult af_interchange_pack_dotlottie(const char *lottie_json, size_t lottie_size, uint8_t *output,
                                       size_t output_capacity, size_t *out_size,
                                       AfInterchangeDiagnostics *diagnostics) {
    static const char manifest[] =
        "{\"version\":\"2\",\"generator\":\"Arcflow 1.0\",\"initial\":{"
        "\"animation\":\"animation\"},\"animations\":[{\"id\":\"animation\"}]}\n";
    AfDotLottieWriter writer = {.output = output, .capacity = output_capacity};
    AfDotLottieEntry entries[2] = {
        {.name = "manifest.json",
         .data = (const uint8_t *)manifest,
         .size = (uint32_t)(sizeof(manifest) - 1U)},
        {.name = "a/animation.json", .data = (const uint8_t *)lottie_json}};
    uint32_t central_offset = 0U;
    uint32_t central_size = 0U;
    if (out_size != NULL) {
        *out_size = 0U;
    }
    if (lottie_json == NULL || lottie_size == 0U || lottie_size > UINT32_MAX || output == NULL ||
        output_capacity == 0U || out_size == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    entries[1].size = (uint32_t)lottie_size;
    for (uint32_t index = 0U; index < 2U; ++index) {
        entries[index].compressed_size = af_dotlottie_deflate_size(entries[index].size);
        entries[index].crc = af_dotlottie_crc32(entries[index].data, entries[index].size);
        if (!af_dotlottie_local_entry(&writer, &entries[index])) {
            goto output_limit;
        }
    }
    if (writer.size > UINT32_MAX) {
        goto output_limit;
    }
    central_offset = (uint32_t)writer.size;
    for (uint32_t index = 0U; index < 2U; ++index) {
        if (!af_dotlottie_central_entry(&writer, &entries[index])) {
            goto output_limit;
        }
    }
    if (writer.size > UINT32_MAX || writer.size < central_offset) {
        goto output_limit;
    }
    central_size = (uint32_t)writer.size - central_offset;
    if (!af_dotlottie_u32(&writer, UINT32_C(0x06054b50)) || !af_dotlottie_u16(&writer, 0U) ||
        !af_dotlottie_u16(&writer, 0U) || !af_dotlottie_u16(&writer, 2U) ||
        !af_dotlottie_u16(&writer, 2U) || !af_dotlottie_u32(&writer, central_size) ||
        !af_dotlottie_u32(&writer, central_offset) || !af_dotlottie_u16(&writer, 0U)) {
        goto output_limit;
    }
    *out_size = writer.size;
    return AF_RESULT_SUCCESS;

output_limit:
    af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                          AF_INTERCHANGE_DIAGNOSTIC_OUTPUT_LIMIT, 0U, 0U,
                          "dotLottie output capacity is too small");
    return AF_RESULT_LIMIT_EXCEEDED;
}
