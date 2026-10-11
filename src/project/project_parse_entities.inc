static bool af_project_parse_node_field(const char *line, AfProjectNodeSnapshot *snapshot,
                                        uint32_t *fields) {
    const char *value = NULL;
    float values[4] = {0};
    uint64_t integer = 0U;
    if ((value = af_project_value(line, "text_format")) != NULL) {
        float v[6];
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_TEXT_FORMAT) ||
            !af_project_parse_floats(value, v, 6))
            return false;
        for (uint32_t i = 0; i < 4; ++i)
            if (v[i] < 0 || v[i] > (i == 0 || i == 3 ? 2 : 1) || floorf(v[i]) != v[i])
                return false;
        snapshot->text.font_family = (uint32_t)v[0];
        snapshot->text.bold = v[1] != 0;
        snapshot->text.italic = v[2] != 0;
        snapshot->text.alignment = (uint32_t)v[3];
        snapshot->text.letter_spacing = v[4];
        snapshot->text.line_spacing = v[5];
        return true;
    }
    if ((value = af_project_value(line, "text_settings")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_TEXT) ||
            !af_project_parse_floats(value, values, 4U) || values[0] < 1 || values[0] > 3 ||
            floorf(values[0]) != values[0])
            return false;
        snapshot->text.mode = (AfProjectTextMode)(uint32_t)values[0];
        snapshot->text.font_size = values[1];
        snapshot->text.width = values[2];
        snapshot->text.offset = values[3];
        return true;
    }
    const bool font_name = (value = af_project_value(line, "text_font")) != NULL;
    if (font_name || (value = af_project_value(line, "text_utf8")) != NULL) {
        char *destination = font_name ? snapshot->text.font_name : snapshot->text.utf8;
        const size_t capacity =
            font_name ? sizeof(snapshot->text.font_name) : AF_PROJECT_TEXT_BYTES;
        if (!af_project_set_field(fields, font_name ? AF_PROJECT_FIELD_TEXT_FONT
                                                    : AF_PROJECT_FIELD_TEXT_UTF8))
            return false;
        const size_t length = strlen(value);
        if (length == 0 || length % 2 != 0 || length / 2 >= capacity)
            return false;
        for (size_t i = 0; i < length / 2; ++i) {
            unsigned int byte = 0;
            for (size_t j = 0; j < 2; ++j) {
                const char c = value[i * 2 + j];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                    return false;
                byte = byte * 16U + (unsigned int)(c <= '9' ? c - '0' : c - 'a' + 10);
            }
            if (!byte)
                return false;
            destination[i] = (char)byte;
        }
        destination[length / 2] = '\0';
        return true;
    }
    if ((value = af_project_value(line, "text_guide")) != NULL) {
        float v[7];
        if (snapshot->text.guide_count >= AF_PROJECT_TEXT_GUIDE_COMMANDS ||
            !af_project_parse_floats(value, v, 7) || v[0] < 0 || v[0] > (float)AF_PATH_CLOSE ||
            floorf(v[0]) != v[0])
            return false;
        snapshot->text.guide[snapshot->text.guide_count++] =
            (AfPathCommand){.type = (AfPathCommandType)(uint32_t)v[0],
                            .point = {v[1], v[2]},
                            .control1 = {v[3], v[4]},
                            .control2 = {v[5], v[6]}};
        return true;
    }
    if ((value = af_project_value(line, "polygon")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_POLYGON) ||
            !af_project_parse_floats(value, values, 2U) || values[0] < 0.0F || values[0] > 64.0F ||
            floorf(values[0]) != values[0] || values[1] < 0.0F || values[1] > 0.95F)
            return false;
        snapshot->node.polygon_sides = (uint32_t)values[0];
        snapshot->node.polygon_inset = values[1];
        return true;
    }
    if ((value = af_project_value(line, "id")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_FIELD_ID) &&
               af_project_parse_u64(value, &snapshot->node.id);
    }
    if ((value = af_project_value(line, "parent")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_PARENT)) {
            return false;
        }
        if (strcmp(value, "none") == 0) {
            snapshot->node.parent_id = AF_INVALID_NODE_ID;
            return true;
        }
        return af_project_parse_u64(value, &snapshot->node.parent_id);
    }
    if ((value = af_project_value(line, "type")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_TYPE)) {
            return false;
        }
        if (strcmp(value, "group") == 0) {
            snapshot->node.type = AF_NODE_GROUP;
        } else if (strcmp(value, "rectangle") == 0) {
            snapshot->node.type = AF_NODE_RECTANGLE;
        } else if (strcmp(value, "rounded_rectangle") == 0) {
            snapshot->node.type = AF_NODE_ROUNDED_RECTANGLE;
        } else if (strcmp(value, "ellipse") == 0) {
            snapshot->node.type = AF_NODE_ELLIPSE;
        } else if (strcmp(value, "path") == 0) {
            snapshot->node.type = AF_NODE_PATH;
        } else if (strcmp(value, "image") == 0) {
            snapshot->node.type = AF_NODE_IMAGE;
        } else {
            return false;
        }
        return true;
    }
    if ((value = af_project_value(line, "name")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_NAME) || !af_project_name_valid(value)) {
            return false;
        }
        (void)snprintf(snapshot->name, sizeof(snapshot->name), "%s", value);
        return true;
    }
    if ((value = af_project_value(line, "position")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_POSITION) ||
            !af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        snapshot->node.local_transform.position = (AfVec2){values[0], values[1]};
        return true;
    }
    if ((value = af_project_value(line, "rotation")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_FIELD_ROTATION) &&
               af_project_parse_floats(value, &snapshot->node.local_transform.rotation, 1U);
    }
    if ((value = af_project_value(line, "scale")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_SCALE) ||
            !af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        snapshot->node.local_transform.scale = (AfVec2){values[0], values[1]};
        return true;
    }
    if ((value = af_project_value(line, "skew")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_SKEW) ||
            !af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        snapshot->node.local_transform.skew = (AfVec2){values[0], values[1]};
        return true;
    }
    if ((value = af_project_value(line, "bounds")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_BOUNDS) ||
            !af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        snapshot->node.bounds = (AfVec2){values[0], values[1]};
        return true;
    }
    if ((value = af_project_value(line, "fill")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_FILL) ||
            !af_project_parse_floats(value, values, 4U)) {
            return false;
        }
        snapshot->node.fill = (AfColor){values[0], values[1], values[2], values[3]};
        return true;
    }
    if ((value = af_project_value(line, "opacity")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_FIELD_OPACITY) &&
               af_project_parse_floats(value, &snapshot->node.opacity, 1U);
    }
    if ((value = af_project_value(line, "corner_radius")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_FIELD_RADIUS) &&
               af_project_parse_floats(value, &snapshot->node.corner_radius, 1U);
    }
    if ((value = af_project_value(line, "layer")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_FIELD_LAYER) &&
               af_project_parse_i32(value, &snapshot->node.layer_order);
    }
    if ((value = af_project_value(line, "visible")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_VISIBLE) ||
            !af_project_parse_u64(value, &integer) || integer > 1U) {
            return false;
        }
        snapshot->node.visible = integer != 0U;
        return true;
    }
    return true;
}

static bool af_project_parse_clip_field(const char *line, AfProjectClipSnapshot *snapshot,
                                        uint32_t *fields) {
    const char *value = NULL;
    float values[2] = {0};
    uint64_t integer = 0U;
    if ((value = af_project_value(line, "id")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_CLIP_ID) &&
               af_project_parse_u64(value, &snapshot->clip.id);
    }
    if ((value = af_project_value(line, "name")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_CLIP_NAME) || !af_project_name_valid(value)) {
            return false;
        }
        (void)snprintf(snapshot->clip.name, sizeof(snapshot->clip.name), "%s", value);
        return true;
    }
    if ((value = af_project_value(line, "duration")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_CLIP_DURATION) &&
               af_project_parse_floats(value, &snapshot->clip.duration, 1U);
    }
    if ((value = af_project_value(line, "loop")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_CLIP_LOOP) ||
            !af_project_parse_u64(value, &integer) || integer > 1U) {
            return false;
        }
        snapshot->clip.loop = integer != 0U;
        return true;
    }
    if ((value = af_project_value(line, "loop_region")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_CLIP_REGION) ||
            !af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        snapshot->clip.loop_start = values[0];
        snapshot->clip.loop_end = values[1];
        return true;
    }
    return true;
}

static bool af_project_parse_track_field(const char *line, AfProjectTrackSnapshot *snapshot,
                                         uint32_t *fields) {
    const char *value = NULL;
    if ((value = af_project_value(line, "id")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_TRACK_ID) &&
               af_project_parse_u64(value, &snapshot->track.id);
    }
    if ((value = af_project_value(line, "clip")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_TRACK_CLIP) &&
               af_project_parse_u64(value, &snapshot->track.clip_id);
    }
    if ((value = af_project_value(line, "node")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_TRACK_NODE) &&
               af_project_parse_u64(value, &snapshot->track.node_id);
    }
    if ((value = af_project_value(line, "property")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_TRACK_PROPERTY)) {
            return false;
        }
        if (strcmp(value, "position") == 0) {
            snapshot->track.property = AF_PROPERTY_POSITION;
        } else if (strcmp(value, "rotation") == 0) {
            snapshot->track.property = AF_PROPERTY_ROTATION;
        } else if (strcmp(value, "scale") == 0) {
            snapshot->track.property = AF_PROPERTY_SCALE;
        } else if (strcmp(value, "opacity") == 0) {
            snapshot->track.property = AF_PROPERTY_OPACITY;
        } else if (strcmp(value, "fill_color") == 0) {
            snapshot->track.property = AF_PROPERTY_FILL_COLOR;
        } else if (strcmp(value, "corner_radius") == 0) {
            snapshot->track.property = AF_PROPERTY_CORNER_RADIUS;
        } else if (strcmp(value, "skew") == 0) {
            snapshot->track.property = AF_PROPERTY_SKEW;
        } else if (strcmp(value, "path_point") == 0) {
            snapshot->track.property = AF_PROPERTY_PATH_POINT;
        } else if (strcmp(value, "path_control_in") == 0) {
            snapshot->track.property = AF_PROPERTY_PATH_CONTROL_IN;
        } else if (strcmp(value, "path_control_out") == 0) {
            snapshot->track.property = AF_PROPERTY_PATH_CONTROL_OUT;
        } else {
            return false;
        }
        return true;
    }
    if ((value = af_project_value(line, "element")) != NULL) {
        uint64_t element = 0U;
        if (!af_project_parse_u64(value, &element) || element > UINT32_MAX) {
            return false;
        }
        snapshot->track.element_index = (uint32_t)element;
        return true;
    }
    return true;
}

static bool af_project_parse_keyframe_field(const char *line, AfProjectKeyframeSnapshot *snapshot,
                                            uint32_t *fields) {
    const char *value = NULL;
    if ((value = af_project_value(line, "id")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_KEY_ID) &&
               af_project_parse_u64(value, &snapshot->keyframe.id);
    }
    if ((value = af_project_value(line, "track")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_KEY_TRACK) &&
               af_project_parse_u64(value, &snapshot->keyframe.track_id);
    }
    if ((value = af_project_value(line, "time")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_KEY_TIME) &&
               af_project_parse_floats(value, &snapshot->keyframe.keyframe.time, 1U);
    }
    if ((value = af_project_value(line, "value")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_KEY_VALUE) &&
               af_project_parse_floats(value, snapshot->keyframe.keyframe.value, 4U);
    }
    if ((value = af_project_value(line, "interpolation")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_KEY_INTERPOLATION)) {
            return false;
        }
        if (strcmp(value, "step") == 0) {
            snapshot->keyframe.keyframe.interpolation = AF_INTERPOLATION_STEP;
        } else if (strcmp(value, "linear") == 0) {
            snapshot->keyframe.keyframe.interpolation = AF_INTERPOLATION_LINEAR;
        } else if (strcmp(value, "cubic_bezier") == 0) {
            snapshot->keyframe.keyframe.interpolation = AF_INTERPOLATION_CUBIC_BEZIER;
        } else {
            return false;
        }
        return true;
    }
    if ((value = af_project_value(line, "easing")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_KEY_EASING) &&
               af_project_parse_floats(value, snapshot->keyframe.keyframe.easing, 4U);
    }
    return true;
}

static bool af_project_parse_path_line(const char *line, AfProjectPathSnapshot *snapshot,
                                       AfPathCommand *commands, uint32_t *command_count,
                                       uint32_t *fields) {
    const char *value = NULL;
    float values[6] = {0};
    if ((value = af_project_value(line, "id")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_PATH_ID) &&
               af_project_parse_u64(value, &snapshot->path.id);
    }
    if ((value = af_project_value(line, "node")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_PATH_NODE) &&
               af_project_parse_u64(value, &snapshot->path.node_id);
    }
    if ((value = af_project_value(line, "fill_rule")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_PATH_FILL_RULE)) {
            return false;
        }
        if (strcmp(value, "non_zero") == 0) {
            snapshot->path.fill_rule = AF_PATH_FILL_NON_ZERO;
        } else if (strcmp(value, "even_odd") == 0) {
            snapshot->path.fill_rule = AF_PATH_FILL_EVEN_ODD;
        } else {
            return false;
        }
        return true;
    }
    if ((value = af_project_value(line, "generation")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_PATH_GENERATION) &&
               af_project_parse_u64(value, &snapshot->path.geometry_generation);
    }
    if (*command_count >= AF_MAX_PATH_COMMANDS_PER_PATH) {
        return false;
    }
    if ((value = af_project_value(line, "move")) != NULL) {
        if (!af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        commands[(*command_count)++] =
            (AfPathCommand){.type = AF_PATH_MOVE, .point = {values[0], values[1]}};
        return true;
    }
    if ((value = af_project_value(line, "line")) != NULL) {
        if (!af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        commands[(*command_count)++] =
            (AfPathCommand){.type = AF_PATH_LINE, .point = {values[0], values[1]}};
        return true;
    }
    if ((value = af_project_value(line, "cubic")) != NULL) {
        if (!af_project_parse_floats(value, values, 6U)) {
            return false;
        }
        commands[(*command_count)++] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                                       .control1 = {values[0], values[1]},
                                                       .control2 = {values[2], values[3]},
                                                       .point = {values[4], values[5]}};
        return true;
    }
    if (strcmp(line, "close") == 0) {
        commands[(*command_count)++] = (AfPathCommand){.type = AF_PATH_CLOSE};
        return true;
    }
    return true;
}
