static bool af_project_binding_encode_token(const char *input, char *output, size_t capacity) {
    if (input != NULL && input[0] == '\0') {
        if (capacity < 2U) {
            return false;
        }
        output[0] = '~';
        output[1] = '\0';
        return true;
    }
    return af_project_image_encode_token(input, output, capacity);
}

static bool af_project_binding_decode_token(const char *input, char *output, size_t capacity) {
    if (input != NULL && strcmp(input, "~") == 0) {
        if (capacity == 0U) {
            return false;
        }
        output[0] = '\0';
        return true;
    }
    return af_project_image_decode_token(input, output, capacity);
}

static bool af_project_parse_binding_property_line(const char *line, AfProject *project,
                                                   uint32_t *out_target_count) {
    unsigned long long id = 0U;
    unsigned long long resource = 0U;
    unsigned int type = 0U;
    unsigned int target_count = 0U;
    unsigned int mode = 0U;
    unsigned int precision = 0U;
    unsigned int boolean = 0U;
    float scalar = 0.0F;
    int integer = 0;
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 0.0F;
    char name[AF_BINDING_NAME_CAPACITY * 3U];
    char text[AF_BINDING_TEXT_CAPACITY * 3U];
    char prefix[AF_BINDING_AFFIX_CAPACITY * 3U];
    char suffix[AF_BINDING_AFFIX_CAPACITY * 3U];
    char singular[AF_BINDING_PLURAL_CAPACITY * 3U];
    char plural[AF_BINDING_PLURAL_CAPACITY * 3U];
    char trailing = '\0';
    AfExposedProperty property = {0};
    if (line == NULL || project == NULL || out_target_count == NULL ||
        project->binding_data.property_count >= project->binding_data.property_capacity ||
        sscanf(line,
               "binding_property %llu %u %u %u %u %g %d %u %g %g %g %g %llu %191s %767s "
               "%95s %95s %95s %95s %c",
               &id, &type, &target_count, &mode, &precision, &scalar, &integer, &boolean, &r, &g,
               &b, &a, &resource, name, text, prefix, suffix, singular, plural, &trailing) != 19 ||
        id == 0U || type > AF_BINDING_VALUE_ANIMATION || target_count == 0U ||
        target_count > project->binding_data.target_capacity - project->binding_data.target_count ||
        mode > AF_BINDING_FORMAT_TIME || precision > AF_BINDING_MAX_DECIMAL_PRECISION ||
        boolean > 1U || !isfinite(scalar) || !isfinite(r) || !isfinite(g) || !isfinite(b) ||
        !isfinite(a)) {
        return false;
    }
    property = (AfExposedProperty){.id = (uint64_t)id,
                                   .type = (AfBindingValueType)type,
                                   .target_start = project->binding_data.target_count,
                                   .target_count = target_count};
    property.format.mode = (AfBindingFormatMode)mode;
    property.format.precision = precision;
    if (!af_project_binding_decode_token(name, property.debug_name, sizeof(property.debug_name)) ||
        !af_project_binding_decode_token(text, property.default_value.text,
                                         sizeof(property.default_value.text)) ||
        !af_project_binding_decode_token(prefix, property.format.prefix,
                                         sizeof(property.format.prefix)) ||
        !af_project_binding_decode_token(suffix, property.format.suffix,
                                         sizeof(property.format.suffix)) ||
        !af_project_binding_decode_token(singular, property.format.singular,
                                         sizeof(property.format.singular)) ||
        !af_project_binding_decode_token(plural, property.format.plural,
                                         sizeof(property.format.plural))) {
        return false;
    }
    if (property.type == AF_BINDING_VALUE_SCALAR) {
        property.default_value.scalar = scalar;
    } else if (property.type == AF_BINDING_VALUE_INTEGER) {
        property.default_value.integer = integer;
    } else if (property.type == AF_BINDING_VALUE_BOOLEAN) {
        property.default_value.boolean = boolean != 0U;
    } else if (property.type == AF_BINDING_VALUE_COLOR) {
        property.default_value.color = (AfColor){r, g, b, a};
    } else if (property.type == AF_BINDING_VALUE_IMAGE) {
        property.default_value.image_id = (uint64_t)resource;
    } else if (property.type == AF_BINDING_VALUE_VISIBILITY) {
        property.default_value.visibility = boolean != 0U;
    } else if (property.type == AF_BINDING_VALUE_PROGRESS) {
        property.default_value.progress = scalar;
    } else if (property.type == AF_BINDING_VALUE_ANIMATION) {
        property.default_value.animation_id = (uint64_t)resource;
    }
    if (af_binding_find_const(&project->binding_data, property.id) != NULL) {
        return false;
    }
    project->binding_data.properties[project->binding_data.property_count++] = property;
    *out_target_count = target_count;
    return true;
}

static bool af_project_parse_binding_target_line(const char *line, AfProject *project,
                                                 uint64_t expected_property_id) {
    unsigned long long property_id = 0U;
    unsigned long long object_id = 0U;
    unsigned long long member_id = 0U;
    unsigned int type = 0U;
    unsigned int property = 0U;
    unsigned int text_capacity = 0U;
    char trailing = '\0';
    if (line == NULL || project == NULL || expected_property_id == 0U ||
        project->binding_data.target_count >= project->binding_data.target_capacity ||
        sscanf(line, "binding_target %llu %u %u %llu %llu %u %c", &property_id, &type, &property,
               &object_id, &member_id, &text_capacity, &trailing) != 6 ||
        property_id != expected_property_id || type > AF_BINDING_TARGET_TEXT_STORAGE ||
        property > AF_BINDING_TARGET_TEXT_VALUE) {
        return false;
    }
    project->binding_data.targets[project->binding_data.target_count++] =
        (AfBindingTarget){.type = (AfBindingTargetType)type,
                          .property = (AfBindingTargetProperty)property,
                          .object_id = (uint64_t)object_id,
                          .member_id = (uint64_t)member_id,
                          .text_capacity = text_capacity};
    return true;
}

AfResult af_project_add_exposed_property(AfProject *project, const AfExposedProperty *property,
                                         const AfBindingTarget *targets) {
    AfExposedProperty stored = {0};
    AfBindingValidationContext context = {0};
    if (project == NULL || property == NULL || property->id == 0U || property->target_count == 0U ||
        targets == NULL ||
        project->binding_data.property_count >= project->binding_data.property_capacity ||
        property->target_count >
            project->binding_data.target_capacity - project->binding_data.target_count ||
        af_binding_find_const(&project->binding_data, property->id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    stored = *property;
    stored.target_start = project->binding_data.target_count;
    memcpy(project->binding_data.targets + stored.target_start, targets,
           (size_t)stored.target_count * sizeof(*targets));
    project->binding_data.properties[project->binding_data.property_count++] = stored;
    project->binding_data.target_count += stored.target_count;
    context = (AfBindingValidationContext){.scene = &project->scene,
                                           .animation = NULL,
                                           .state_machines = NULL,
                                           .components = NULL,
                                           .images = &project->image_data,
                                           .particles = &project->particle_data};
    if (af_binding_data_validate(&project->binding_data, &context) != AF_RESULT_SUCCESS) {
        project->binding_data.target_count -= stored.target_count;
        --project->binding_data.property_count;
        memset(project->binding_data.targets + stored.target_start, 0,
               (size_t)stored.target_count * sizeof(*targets));
        project->binding_data.properties[project->binding_data.property_count] =
            (AfExposedProperty){0};
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_remove_exposed_property(AfProject *project, uint64_t property_id) {
    uint32_t property_index = UINT32_MAX;
    uint32_t target_start = 0U;
    uint32_t target_count = 0U;
    if (project == NULL || property_id == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->binding_data.property_count; ++index) {
        if (project->binding_data.properties[index].id == property_id) {
            property_index = index;
            break;
        }
    }
    if (property_index == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    target_start = project->binding_data.properties[property_index].target_start;
    target_count = project->binding_data.properties[property_index].target_count;
    if (target_start + target_count < project->binding_data.target_count) {
        memmove(project->binding_data.targets + target_start,
                project->binding_data.targets + target_start + target_count,
                (size_t)(project->binding_data.target_count - target_start - target_count) *
                    sizeof(*project->binding_data.targets));
    }
    project->binding_data.target_count -= target_count;
    memset(project->binding_data.targets + project->binding_data.target_count, 0,
           (size_t)target_count * sizeof(*project->binding_data.targets));
    if (property_index + 1U < project->binding_data.property_count) {
        memmove(project->binding_data.properties + property_index,
                project->binding_data.properties + property_index + 1U,
                (size_t)(project->binding_data.property_count - property_index - 1U) *
                    sizeof(*project->binding_data.properties));
    }
    --project->binding_data.property_count;
    project->binding_data.properties[project->binding_data.property_count] = (AfExposedProperty){0};
    for (uint32_t index = property_index; index < project->binding_data.property_count; ++index) {
        project->binding_data.properties[index].target_start -= target_count;
    }
    return AF_RESULT_SUCCESS;
}
