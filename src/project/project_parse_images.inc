static const char *af_project_image_skip_space(const char *cursor) {
    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    return cursor;
}

static bool af_project_parse_image_asset_line(char *line, AfProject *project) {
    AfImageAsset asset = {0};
    unsigned long long id = 0ULL;
    unsigned long long content_hash = 0ULL;
    unsigned int format = 0U;
    unsigned int color_space = 0U;
    unsigned int compression = 0U;
    unsigned int left = 0U;
    unsigned int top = 0U;
    unsigned int right = 0U;
    unsigned int bottom = 0U;
    unsigned int trim = 0U;
    unsigned int mips = 0U;
    char encoded_path[AF_IMAGE_PATH_CAPACITY * 3U];
    char encoded_name[AF_IMAGE_NAME_CAPACITY * 3U];
    int consumed = 0;
    size_t pixel_count = 0U;
    size_t pixel_size = 0U;
    if (project->image_data.asset_count >= project->image_data.asset_capacity ||
        sscanf(line,
               "image_asset %llu %llu %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %1151s "
               "%191s %n",
               &id, &content_hash, &format, &color_space, &compression, &asset.width, &asset.height,
               &asset.trim_x, &asset.trim_y, &asset.trim_width, &asset.trim_height, &left, &top,
               &right, &bottom, &trim, &mips, encoded_path, encoded_name, &consumed) != 19 ||
        line[consumed] != '\0' || id == 0ULL || id == ULLONG_MAX || format > AF_IMAGE_SOURCE_SVG ||
        color_space > AF_IMAGE_COLOR_LINEAR || compression > AF_IMAGE_COMPRESSION_LOSSLESS ||
        left > UINT16_MAX || top > UINT16_MAX || right > UINT16_MAX || bottom > UINT16_MAX ||
        trim > 1U || mips > 1U ||
        !af_project_image_decode_token(encoded_path, asset.source_path,
                                       sizeof(asset.source_path)) ||
        !af_project_image_decode_token(encoded_name, asset.name, sizeof(asset.name)) ||
        !af_image_path_is_project_relative(asset.source_path) || asset.width == 0U ||
        asset.height == 0U || asset.width > AF_MAX_IMAGE_DIMENSION ||
        asset.height > AF_MAX_IMAGE_DIMENSION ||
        af_size_mul_overflow(asset.width, asset.height, &pixel_count) ||
        pixel_count > AF_MAX_IMAGE_PIXELS || af_size_mul_overflow(pixel_count, 4U, &pixel_size) ||
        pixel_size > UINT32_MAX ||
        af_image_find_const(&project->image_data, (uint64_t)id) != NULL ||
        af_image_find_sequence_const(&project->image_data, (uint64_t)id) != NULL) {
        return false;
    }
    asset.id = (uint64_t)id;
    asset.content_hash = (uint64_t)content_hash;
    asset.source_format = (AfImageSourceFormat)format;
    asset.color_space = (AfImageColorSpace)color_space;
    asset.compression = (AfImageCompression)compression;
    asset.sprite_borders =
        (AfImageBorders){(uint16_t)left, (uint16_t)top, (uint16_t)right, (uint16_t)bottom};
    asset.trim_enabled = trim != 0U;
    asset.mipmaps_enabled = mips != 0U;
    asset.missing = true;
    asset.pixel_size = (uint32_t)pixel_size;
    project->image_data.assets[project->image_data.asset_count++] = asset;
    return true;
}

static bool af_project_parse_image_sequence_line(char *line, AfProject *project) {
    AfImageSequence sequence = {0};
    unsigned long long id = 0ULL;
    unsigned int loop = 0U;
    unsigned int frame_count = 0U;
    char encoded_name[AF_IMAGE_NAME_CAPACITY * 3U];
    int consumed = 0;
    const char *cursor = NULL;
    uint64_t frames[AF_MAX_IMAGE_SEQUENCE_FRAMES];
    if (sscanf(line, "image_sequence %llu %f %u %191s %u %n", &id, &sequence.frames_per_second,
               &loop, encoded_name, &frame_count, &consumed) != 5 ||
        id == 0ULL || id == ULLONG_MAX || loop > 1U || frame_count == 0U ||
        frame_count > AF_MAX_IMAGE_SEQUENCE_FRAMES || !isfinite(sequence.frames_per_second) ||
        sequence.frames_per_second <= 0.0F || sequence.frames_per_second > 1000.0F ||
        !af_project_image_decode_token(encoded_name, sequence.name, sizeof(sequence.name))) {
        return false;
    }
    cursor = line + consumed;
    for (uint32_t frame = 0U; frame < frame_count; ++frame) {
        unsigned long long frame_id = 0ULL;
        int frame_consumed = 0;
        if (sscanf(cursor, " %llu%n", &frame_id, &frame_consumed) != 1 || frame_id == 0ULL ||
            frame_consumed <= 0) {
            return false;
        }
        frames[frame] = (uint64_t)frame_id;
        cursor += frame_consumed;
    }
    if (*af_project_image_skip_space(cursor) != '\0') {
        return false;
    }
    sequence.id = (uint64_t)id;
    sequence.frame_count = frame_count;
    sequence.loop = loop != 0U;
    return af_image_add_sequence(&project->image_data, &sequence, frames) == AF_RESULT_SUCCESS;
}

static bool af_project_parse_image_binding_line(char *line, AfProject *project) {
    AfImageBinding binding = {0};
    unsigned long long node_id = 0ULL;
    unsigned long long asset_id = 0ULL;
    unsigned long long sequence_id = 0ULL;
    unsigned int display_mode = 0U;
    unsigned int left = 0U;
    unsigned int top = 0U;
    unsigned int right = 0U;
    unsigned int bottom = 0U;
    unsigned int loop = 0U;
    int consumed = 0;
    if (sscanf(line, "image_binding %llu %llu %llu %u %u %u %u %u %f %u %n", &node_id, &asset_id,
               &sequence_id, &display_mode, &left, &top, &right, &bottom,
               &binding.frames_per_second, &loop, &consumed) != 10 ||
        line[consumed] != '\0' || node_id == 0ULL || display_mode > AF_IMAGE_DISPLAY_NINE_SLICE ||
        left > UINT16_MAX || top > UINT16_MAX || right > UINT16_MAX || bottom > UINT16_MAX ||
        loop > 1U) {
        return false;
    }
    binding.node_id = (uint64_t)node_id;
    binding.asset_id = (uint64_t)asset_id;
    binding.sequence_id = (uint64_t)sequence_id;
    binding.display_mode = (AfImageDisplayMode)display_mode;
    binding.borders =
        (AfImageBorders){(uint16_t)left, (uint16_t)top, (uint16_t)right, (uint16_t)bottom};
    binding.loop = loop != 0U;
    return af_image_set_binding(&project->image_data, &binding) == AF_RESULT_SUCCESS;
}
