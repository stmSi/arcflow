static uint64_t af_binding_section_size(const AfBindingData *data) {
    return AF_BINDING_HEADER_SIZE +
           (uint64_t)data->property_count * AF_BINDING_PROPERTY_RECORD_SIZE +
           (uint64_t)data->target_count * AF_BINDING_TARGET_RECORD_SIZE;
}

static void af_write_binding_value(uint8_t *bytes, AfBindingValueType type,
                                   const AfBindingValue *value) {
    if (type == AF_BINDING_VALUE_SCALAR) {
        af_write_f32(bytes, value->scalar);
    } else if (type == AF_BINDING_VALUE_INTEGER) {
        af_write_u32(bytes, (uint32_t)value->integer);
    } else if (type == AF_BINDING_VALUE_BOOLEAN) {
        af_write_u32(bytes, value->boolean ? 1U : 0U);
    } else if (type == AF_BINDING_VALUE_COLOR) {
        af_write_f32(bytes, value->color.r);
        af_write_f32(bytes + 4U, value->color.g);
        af_write_f32(bytes + 8U, value->color.b);
        af_write_f32(bytes + 12U, value->color.a);
    } else if (type == AF_BINDING_VALUE_TEXT) {
        memcpy(bytes, value->text, AF_BINDING_TEXT_CAPACITY);
    } else if (type == AF_BINDING_VALUE_IMAGE) {
        af_write_u64(bytes, value->image_id);
    } else if (type == AF_BINDING_VALUE_VISIBILITY) {
        af_write_u32(bytes, value->visibility ? 1U : 0U);
    } else if (type == AF_BINDING_VALUE_PROGRESS) {
        af_write_f32(bytes, value->progress);
    } else if (type == AF_BINDING_VALUE_ANIMATION) {
        af_write_u64(bytes, value->animation_id);
    }
}

static AfBindingValue af_read_binding_value(const uint8_t *bytes, AfBindingValueType type) {
    AfBindingValue value = {0};
    if (type == AF_BINDING_VALUE_SCALAR) {
        value.scalar = af_read_f32(bytes);
    } else if (type == AF_BINDING_VALUE_INTEGER) {
        value.integer = (int32_t)af_read_u32(bytes);
    } else if (type == AF_BINDING_VALUE_BOOLEAN) {
        value.boolean = af_read_u32(bytes) != 0U;
    } else if (type == AF_BINDING_VALUE_COLOR) {
        value.color = (AfColor){af_read_f32(bytes), af_read_f32(bytes + 4U),
                                af_read_f32(bytes + 8U), af_read_f32(bytes + 12U)};
    } else if (type == AF_BINDING_VALUE_TEXT) {
        memcpy(value.text, bytes, AF_BINDING_TEXT_CAPACITY);
    } else if (type == AF_BINDING_VALUE_IMAGE) {
        value.image_id = af_read_u64(bytes);
    } else if (type == AF_BINDING_VALUE_VISIBILITY) {
        value.visibility = af_read_u32(bytes) != 0U;
    } else if (type == AF_BINDING_VALUE_PROGRESS) {
        value.progress = af_read_f32(bytes);
    } else if (type == AF_BINDING_VALUE_ANIMATION) {
        value.animation_id = af_read_u64(bytes);
    }
    return value;
}

static void af_write_bindings(uint8_t *bytes, const AfBindingData *data, bool strip_names) {
    af_write_u32(bytes, data->property_count);
    af_write_u32(bytes + 4U, data->target_count);
    af_write_u32(bytes + 8U, strip_names ? 1U : 0U);
    for (uint32_t index = 0U; index < data->property_count; ++index) {
        const AfExposedProperty *property = &data->properties[index];
        uint8_t *record =
            bytes + AF_BINDING_HEADER_SIZE + (uint64_t)index * AF_BINDING_PROPERTY_RECORD_SIZE;
        af_write_u64(record, property->id);
        af_write_u32(record + 8U, (uint32_t)property->type);
        af_write_u32(record + 12U, (uint32_t)property->format.mode);
        af_write_u32(record + 16U, property->format.precision);
        af_write_u32(record + 20U, property->target_start);
        af_write_u32(record + 24U, property->target_count);
        af_write_binding_value(record + 32U, property->type, &property->default_value);
        memcpy(record + 288U, property->format.prefix, AF_BINDING_AFFIX_CAPACITY);
        memcpy(record + 320U, property->format.suffix, AF_BINDING_AFFIX_CAPACITY);
        memcpy(record + 352U, property->format.singular, AF_BINDING_PLURAL_CAPACITY);
        memcpy(record + 384U, property->format.plural, AF_BINDING_PLURAL_CAPACITY);
        if (!strip_names) {
            memcpy(record + 416U, property->debug_name, AF_BINDING_NAME_CAPACITY);
        }
    }
    for (uint32_t index = 0U; index < data->target_count; ++index) {
        const AfBindingTarget *target = &data->targets[index];
        uint8_t *record = bytes + AF_BINDING_HEADER_SIZE +
                          (uint64_t)data->property_count * AF_BINDING_PROPERTY_RECORD_SIZE +
                          (uint64_t)index * AF_BINDING_TARGET_RECORD_SIZE;
        af_write_u32(record, (uint32_t)target->type);
        af_write_u32(record + 4U, (uint32_t)target->property);
        af_write_u64(record + 8U, target->object_id);
        af_write_u64(record + 16U, target->member_id);
        af_write_u32(record + 24U, target->text_capacity);
    }
}

static bool af_binding_record_string_valid(const uint8_t *record, uint32_t offset,
                                           uint32_t capacity) {
    return memchr(record + offset, '\0', capacity) != NULL;
}

static AfResult af_validate_bindings(const uint8_t *bytes, const AfSectionView *section) {
    const uint32_t property_count = af_read_u32(bytes);
    const uint32_t target_count = af_read_u32(bytes + 4U);
    uint64_t expected_size = 0U;
    if (section == NULL || property_count != section->count ||
        property_count > AF_MAX_EXPOSED_PROPERTIES || target_count > AF_MAX_BINDING_TARGETS ||
        af_read_u32(bytes + 8U) > 1U) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    for (uint32_t reserved = 12U; reserved < AF_BINDING_HEADER_SIZE; reserved += 4U) {
        if (af_read_u32(bytes + reserved) != 0U) {
            return AF_RESULT_INVALID_PACKAGE;
        }
    }
    expected_size = AF_BINDING_HEADER_SIZE +
                    (uint64_t)property_count * AF_BINDING_PROPERTY_RECORD_SIZE +
                    (uint64_t)target_count * AF_BINDING_TARGET_RECORD_SIZE;
    if (section->size != expected_size) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    for (uint32_t index = 0U; index < property_count; ++index) {
        const uint8_t *record =
            bytes + AF_BINDING_HEADER_SIZE + (uint64_t)index * AF_BINDING_PROPERTY_RECORD_SIZE;
        const uint32_t type = af_read_u32(record + 8U);
        const uint32_t target_start = af_read_u32(record + 20U);
        const uint32_t count = af_read_u32(record + 24U);
        if (af_read_u64(record) == 0U || type > AF_BINDING_VALUE_ANIMATION ||
            af_read_u32(record + 12U) > AF_BINDING_FORMAT_TIME ||
            af_read_u32(record + 16U) > AF_BINDING_MAX_DECIMAL_PRECISION || count == 0U ||
            target_start > target_count || count > target_count - target_start ||
            af_read_u32(record + 28U) != 0U ||
            !af_binding_record_string_valid(record, 288U, AF_BINDING_AFFIX_CAPACITY) ||
            !af_binding_record_string_valid(record, 320U, AF_BINDING_AFFIX_CAPACITY) ||
            !af_binding_record_string_valid(record, 352U, AF_BINDING_PLURAL_CAPACITY) ||
            !af_binding_record_string_valid(record, 384U, AF_BINDING_PLURAL_CAPACITY) ||
            !af_binding_record_string_valid(record, 416U, AF_BINDING_NAME_CAPACITY)) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t other = index + 1U; other < property_count; ++other) {
            const uint8_t *candidate =
                bytes + AF_BINDING_HEADER_SIZE + (uint64_t)other * AF_BINDING_PROPERTY_RECORD_SIZE;
            if (af_read_u64(record) == af_read_u64(candidate)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        if (type == AF_BINDING_VALUE_TEXT &&
            memchr(record + 32U, '\0', AF_BINDING_TEXT_CAPACITY) == NULL) {
            return AF_RESULT_INVALID_PACKAGE;
        }
    }
    for (uint32_t index = 0U; index < target_count; ++index) {
        const uint8_t *record = bytes + AF_BINDING_HEADER_SIZE +
                                (uint64_t)property_count * AF_BINDING_PROPERTY_RECORD_SIZE +
                                (uint64_t)index * AF_BINDING_TARGET_RECORD_SIZE;
        if (af_read_u32(record) > AF_BINDING_TARGET_TEXT_STORAGE ||
            af_read_u32(record + 4U) > AF_BINDING_TARGET_TEXT_VALUE ||
            af_read_u32(record + 28U) != 0U) {
            return AF_RESULT_INVALID_PACKAGE;
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_decode_bindings(const uint8_t *bytes, AfAllocator *allocator,
                                   AfBindingData *out_data) {
    const uint32_t property_count = af_read_u32(bytes);
    const uint32_t target_count = af_read_u32(bytes + 4U);
    AfExposedProperty *properties = NULL;
    AfBindingTarget *targets = NULL;
    properties = af_allocate_array(allocator, property_count, sizeof(*properties),
                                   alignof(AfExposedProperty));
    targets =
        af_allocate_array(allocator, target_count, sizeof(*targets), alignof(AfBindingTarget));
    if ((property_count != 0U && properties == NULL) || (target_count != 0U && targets == NULL)) {
        af_deallocate(allocator, targets);
        af_deallocate(allocator, properties);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    *out_data = (AfBindingData){.properties = properties,
                                .property_count = property_count,
                                .property_capacity = property_count,
                                .targets = targets,
                                .target_count = target_count,
                                .target_capacity = target_count};
    for (uint32_t index = 0U; index < property_count; ++index) {
        const uint8_t *record =
            bytes + AF_BINDING_HEADER_SIZE + (uint64_t)index * AF_BINDING_PROPERTY_RECORD_SIZE;
        AfExposedProperty *property = &properties[index];
        *property = (AfExposedProperty){.id = af_read_u64(record),
                                        .type = (AfBindingValueType)af_read_u32(record + 8U),
                                        .target_start = af_read_u32(record + 20U),
                                        .target_count = af_read_u32(record + 24U)};
        property->format.mode = (AfBindingFormatMode)af_read_u32(record + 12U);
        property->format.precision = af_read_u32(record + 16U);
        property->default_value = af_read_binding_value(record + 32U, property->type);
        memcpy(property->format.prefix, record + 288U, AF_BINDING_AFFIX_CAPACITY);
        memcpy(property->format.suffix, record + 320U, AF_BINDING_AFFIX_CAPACITY);
        memcpy(property->format.singular, record + 352U, AF_BINDING_PLURAL_CAPACITY);
        memcpy(property->format.plural, record + 384U, AF_BINDING_PLURAL_CAPACITY);
        memcpy(property->debug_name, record + 416U, AF_BINDING_NAME_CAPACITY);
    }
    for (uint32_t index = 0U; index < target_count; ++index) {
        const uint8_t *record = bytes + AF_BINDING_HEADER_SIZE +
                                (uint64_t)property_count * AF_BINDING_PROPERTY_RECORD_SIZE +
                                (uint64_t)index * AF_BINDING_TARGET_RECORD_SIZE;
        targets[index] =
            (AfBindingTarget){.type = (AfBindingTargetType)af_read_u32(record),
                              .property = (AfBindingTargetProperty)af_read_u32(record + 4U),
                              .object_id = af_read_u64(record + 8U),
                              .member_id = af_read_u64(record + 16U),
                              .text_capacity = af_read_u32(record + 24U)};
    }
    return AF_RESULT_SUCCESS;
}
