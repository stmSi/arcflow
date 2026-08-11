static uint32_t af_image_packbits_size(const uint8_t *bytes, uint32_t size) {
    uint32_t read = 0U;
    uint32_t written = 0U;
    while (read < size) {
        uint32_t run = 1U;
        while (run < 130U && run < size - read && bytes[read + run] == bytes[read]) {
            ++run;
        }
        if (run >= 3U) {
            written += 2U;
            read += run;
        } else {
            const uint32_t start = read;
            read += run;
            while (read < size && read - start < 128U) {
                run = 1U;
                while (run < 3U && run < size - read && bytes[read + run] == bytes[read]) {
                    ++run;
                }
                if (run >= 3U) {
                    break;
                }
                read += run;
            }
            written += 1U + read - start;
        }
    }
    return written;
}

static uint32_t af_image_packbits_write(uint8_t *output, const uint8_t *bytes, uint32_t size) {
    uint32_t read = 0U;
    uint32_t written = 0U;
    while (read < size) {
        uint32_t run = 1U;
        while (run < 130U && run < size - read && bytes[read + run] == bytes[read]) {
            ++run;
        }
        if (run >= 3U) {
            output[written++] = (uint8_t)(128U + run - 3U);
            output[written++] = bytes[read];
            read += run;
        } else {
            const uint32_t start = read;
            read += run;
            while (read < size && read - start < 128U) {
                run = 1U;
                while (run < 3U && run < size - read && bytes[read + run] == bytes[read]) {
                    ++run;
                }
                if (run >= 3U) {
                    break;
                }
                read += run;
            }
            output[written++] = (uint8_t)(read - start - 1U);
            memcpy(output + written, bytes + start, read - start);
            written += read - start;
        }
    }
    return written;
}

enum {
    AF_KTX2_HEADER_SIZE = 80U,
    AF_KTX2_LEVEL_RECORD_SIZE = 24U,
    AF_KTX2_DFD_SIZE = 92U,
    AF_KTX2_FORMAT_RGBA8_UNORM = 37U,
    AF_KTX2_FORMAT_RGBA8_SRGB = 43U
};

typedef struct AfImagePackageAtlas {
    uint8_t *ktx2;
    uint32_t ktx2_size;
    uint32_t stored_size;
    AfImageCompression storage;
} AfImagePackageAtlas;

typedef struct AfImagePackagePlan {
    AfImagePackageAtlas atlases[AF_MAX_IMAGE_ATLASES];
    AfAllocator *allocator;
} AfImagePackagePlan;

static uint32_t af_ktx2_data_offset(uint32_t mip_count) {
    const uint32_t dfd_end =
        AF_KTX2_HEADER_SIZE + mip_count * AF_KTX2_LEVEL_RECORD_SIZE + AF_KTX2_DFD_SIZE;
    return (dfd_end + 3U) & ~3U;
}

static void af_ktx2_write_dfd(uint8_t *bytes, AfImageColorSpace color_space) {
    memset(bytes, 0, AF_KTX2_DFD_SIZE);
    af_write_u32(bytes, AF_KTX2_DFD_SIZE);
    af_write_u32(bytes + 8U, 2U | (88U << 16U));
    af_write_u32(bytes + 12U,
                 1U | (1U << 8U) | ((color_space == AF_IMAGE_COLOR_SRGB ? 2U : 1U) << 16U));
    af_write_u32(bytes + 20U, 4U);
    for (uint32_t channel = 0U; channel < 4U; ++channel) {
        uint32_t channel_id = channel == 3U ? 15U : channel;
        if (channel == 3U && color_space == AF_IMAGE_COLOR_SRGB) {
            channel_id |= 16U;
        }
        af_write_u32(bytes + 28U + channel * 16U, channel * 8U | (7U << 16U) | (channel_id << 24U));
        af_write_u32(bytes + 40U + channel * 16U, 255U);
    }
}

static void af_ktx2_write_atlas(uint8_t *bytes, const AfImageAtlas *atlas) {
    static const uint8_t identifier[12] = {0xABU, 0x4BU, 0x54U, 0x58U, 0x20U, 0x32U,
                                           0x30U, 0xBBU, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    const uint32_t dfd_offset = AF_KTX2_HEADER_SIZE + atlas->mip_count * AF_KTX2_LEVEL_RECORD_SIZE;
    const uint32_t data_offset = af_ktx2_data_offset(atlas->mip_count);
    uint32_t mip_width = atlas->width;
    uint32_t mip_height = atlas->height;
    memset(bytes, 0, data_offset);
    memcpy(bytes, identifier, sizeof(identifier));
    af_write_u32(bytes + 12U, atlas->color_space == AF_IMAGE_COLOR_SRGB
                                  ? AF_KTX2_FORMAT_RGBA8_SRGB
                                  : AF_KTX2_FORMAT_RGBA8_UNORM);
    af_write_u32(bytes + 16U, 1U);
    af_write_u32(bytes + 20U, atlas->width);
    af_write_u32(bytes + 24U, atlas->height);
    af_write_u32(bytes + 36U, 1U);
    af_write_u32(bytes + 40U, atlas->mip_count);
    af_write_u32(bytes + 48U, dfd_offset);
    af_write_u32(bytes + 52U, AF_KTX2_DFD_SIZE);
    for (uint32_t mip = 0U; mip < atlas->mip_count; ++mip) {
        uint8_t *record = bytes + AF_KTX2_HEADER_SIZE + mip * AF_KTX2_LEVEL_RECORD_SIZE;
        const uint64_t mip_size = (uint64_t)mip_width * mip_height * 4U;
        af_write_u64(record, data_offset + atlas->mip_offsets[mip]);
        af_write_u64(record + 8U, mip_size);
        af_write_u64(record + 16U, mip_size);
        mip_width = mip_width > 1U ? mip_width / 2U : 1U;
        mip_height = mip_height > 1U ? mip_height / 2U : 1U;
    }
    af_ktx2_write_dfd(bytes + dfd_offset, atlas->color_space);
    memcpy(bytes + data_offset, atlas->pixels, atlas->pixel_size);
}

static void af_image_package_plan_destroy(AfImagePackagePlan *plan) {
    if (plan == NULL) {
        return;
    }
    for (uint32_t index = 0U; index < AF_MAX_IMAGE_ATLASES; ++index) {
        af_deallocate(plan->allocator, plan->atlases[index].ktx2);
    }
    *plan = (AfImagePackagePlan){0};
}

static AfResult af_image_package_plan_create(const AfImageData *images, AfAllocator *allocator,
                                             AfImagePackagePlan *out_plan) {
    AfImagePackagePlan plan = {.allocator = allocator};
    for (uint32_t index = 0U; index < images->atlas_count; ++index) {
        const AfImageAtlas *atlas = &images->atlases[index];
        AfImagePackageAtlas *payload = &plan.atlases[index];
        uint64_t ktx2_size = 0U;
        if (atlas->pixel_size == 0U) {
            continue;
        }
        ktx2_size = (uint64_t)af_ktx2_data_offset(atlas->mip_count) + atlas->pixel_size;
        if (ktx2_size > UINT32_MAX || ktx2_size > AF_PACKAGE_MAX_SIZE) {
            af_image_package_plan_destroy(&plan);
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        payload->ktx2_size = (uint32_t)ktx2_size;
        payload->ktx2 = af_allocate(allocator, payload->ktx2_size, alignof(uint8_t));
        if (payload->ktx2 == NULL) {
            af_image_package_plan_destroy(&plan);
            return AF_RESULT_OUT_OF_MEMORY;
        }
        af_ktx2_write_atlas(payload->ktx2, atlas);
        payload->stored_size = payload->ktx2_size;
        if (atlas->compression == AF_IMAGE_COMPRESSION_LOSSLESS) {
            const uint32_t compressed = af_image_packbits_size(payload->ktx2, payload->ktx2_size);
            if (compressed < payload->ktx2_size) {
                payload->stored_size = compressed;
                payload->storage = AF_IMAGE_COMPRESSION_LOSSLESS;
            }
        }
    }
    *out_plan = plan;
    return AF_RESULT_SUCCESS;
}

static uint64_t af_image_section_size(const AfImageData *images, const AfImagePackagePlan *plan) {
    uint64_t size = AF_IMAGE_SECTION_HEADER_SIZE +
                    (uint64_t)images->asset_count * AF_IMAGE_ASSET_RECORD_SIZE +
                    (uint64_t)images->binding_count * AF_IMAGE_BINDING_RECORD_SIZE +
                    (uint64_t)images->sequence_count * AF_IMAGE_SEQUENCE_RECORD_SIZE +
                    (uint64_t)images->sequence_frame_count * sizeof(uint64_t) +
                    (uint64_t)images->atlas_count * AF_IMAGE_ATLAS_RECORD_SIZE;
    for (uint32_t index = 0U; index < images->atlas_count; ++index) {
        size += plan->atlases[index].stored_size;
    }
    return size;
}

static void af_write_images(uint8_t *bytes, const AfImageData *images,
                            const AfImagePackagePlan *plan) {
    const uint64_t asset_offset = AF_IMAGE_SECTION_HEADER_SIZE;
    const uint64_t binding_offset =
        asset_offset + (uint64_t)images->asset_count * AF_IMAGE_ASSET_RECORD_SIZE;
    const uint64_t sequence_offset =
        binding_offset + (uint64_t)images->binding_count * AF_IMAGE_BINDING_RECORD_SIZE;
    const uint64_t frame_offset =
        sequence_offset + (uint64_t)images->sequence_count * AF_IMAGE_SEQUENCE_RECORD_SIZE;
    const uint64_t atlas_offset =
        frame_offset + (uint64_t)images->sequence_frame_count * sizeof(uint64_t);
    uint64_t pixel_offset =
        atlas_offset + (uint64_t)images->atlas_count * AF_IMAGE_ATLAS_RECORD_SIZE;
    af_write_u32(bytes, images->asset_count);
    af_write_u32(bytes + 4U, images->binding_count);
    af_write_u32(bytes + 8U, images->sequence_count);
    af_write_u32(bytes + 12U, images->sequence_frame_count);
    af_write_u32(bytes + 16U, images->atlas_count);
    for (uint32_t index = 0U; index < images->asset_count; ++index) {
        const AfImageAsset *asset = &images->assets[index];
        uint8_t *record = bytes + asset_offset + (uint64_t)index * AF_IMAGE_ASSET_RECORD_SIZE;
        uint32_t flags = 0U;
        flags |= asset->trim_enabled ? 1U : 0U;
        flags |= asset->mipmaps_enabled ? 2U : 0U;
        af_write_u64(record, asset->id);
        af_write_u64(record + 8U, asset->content_hash);
        af_write_u32(record + 16U, asset->width);
        af_write_u32(record + 20U, asset->height);
        af_write_u32(record + 24U, asset->trim_x);
        af_write_u32(record + 28U, asset->trim_y);
        af_write_u32(record + 32U, asset->trim_width);
        af_write_u32(record + 36U, asset->trim_height);
        af_write_u32(record + 40U, asset->atlas_index);
        af_write_u32(record + 44U, asset->atlas_x);
        af_write_u32(record + 48U, asset->atlas_y);
        af_write_u32(record + 52U, (uint32_t)asset->source_format);
        af_write_u32(record + 56U, (uint32_t)asset->color_space);
        af_write_u32(record + 60U, (uint32_t)asset->compression);
        af_write_u32(record + 64U, asset->sprite_borders.left);
        af_write_u32(record + 68U, asset->sprite_borders.top);
        af_write_u32(record + 72U, asset->sprite_borders.right);
        af_write_u32(record + 76U, asset->sprite_borders.bottom);
        af_write_u32(record + 80U, flags);
    }
    for (uint32_t index = 0U; index < images->binding_count; ++index) {
        const AfImageBinding *binding = &images->bindings[index];
        uint8_t *record = bytes + binding_offset + (uint64_t)index * AF_IMAGE_BINDING_RECORD_SIZE;
        af_write_u64(record, binding->node_id);
        af_write_u64(record + 8U, binding->asset_id);
        af_write_u64(record + 16U, binding->sequence_id);
        af_write_u32(record + 24U, (uint32_t)binding->display_mode);
        af_write_u32(record + 28U, binding->borders.left);
        af_write_u32(record + 32U, binding->borders.top);
        af_write_u32(record + 36U, binding->borders.right);
        af_write_u32(record + 40U, binding->borders.bottom);
        af_write_f32(record + 44U, binding->frames_per_second);
        af_write_u32(record + 48U, binding->loop ? 1U : 0U);
    }
    for (uint32_t index = 0U; index < images->sequence_count; ++index) {
        const AfImageSequence *sequence = &images->sequences[index];
        uint8_t *record = bytes + sequence_offset + (uint64_t)index * AF_IMAGE_SEQUENCE_RECORD_SIZE;
        af_write_u64(record, sequence->id);
        af_write_u32(record + 8U, sequence->frame_start);
        af_write_u32(record + 12U, sequence->frame_count);
        af_write_f32(record + 16U, sequence->frames_per_second);
        af_write_u32(record + 20U, sequence->loop ? 1U : 0U);
        memcpy(record + 24U, sequence->name, AF_IMAGE_NAME_CAPACITY);
    }
    for (uint32_t index = 0U; index < images->sequence_frame_count; ++index) {
        af_write_u64(bytes + frame_offset + (uint64_t)index * sizeof(uint64_t),
                     images->sequence_frames[index]);
    }
    for (uint32_t index = 0U; index < images->atlas_count; ++index) {
        const AfImageAtlas *atlas = &images->atlases[index];
        const AfImagePackageAtlas *payload = &plan->atlases[index];
        uint8_t *record = bytes + atlas_offset + (uint64_t)index * AF_IMAGE_ATLAS_RECORD_SIZE;
        af_write_u32(record, atlas->width);
        af_write_u32(record + 4U, atlas->height);
        af_write_u32(record + 8U, atlas->mip_count);
        af_write_u32(record + 12U, (uint32_t)atlas->color_space);
        af_write_u64(record + 16U, pixel_offset);
        af_write_u64(record + 24U, payload->stored_size);
        for (uint32_t mip = 0U; mip < AF_IMAGE_MAX_MIPS; ++mip) {
            af_write_u32(record + 32U + mip * 4U, atlas->mip_offsets[mip]);
        }
        af_write_u32(record + 84U, (uint32_t)payload->storage);
        af_write_u64(record + 88U, payload->ktx2_size);
        if (payload->stored_size != 0U) {
            if (payload->storage == AF_IMAGE_COMPRESSION_LOSSLESS) {
                (void)af_image_packbits_write(bytes + pixel_offset, payload->ktx2,
                                              payload->ktx2_size);
            } else {
                memcpy(bytes + pixel_offset, payload->ktx2, payload->ktx2_size);
            }
            pixel_offset += payload->stored_size;
        }
    }
}

static bool af_image_section_has_asset(const uint8_t *section, uint64_t asset_offset,
                                       uint32_t asset_count, uint64_t id) {
    for (uint32_t index = 0U; index < asset_count; ++index) {
        if (af_read_u64(section + asset_offset + (uint64_t)index * AF_IMAGE_ASSET_RECORD_SIZE) ==
            id) {
            return true;
        }
    }
    return false;
}

static bool af_image_section_has_sequence(const uint8_t *section, uint64_t sequence_offset,
                                          uint32_t sequence_count, uint64_t id) {
    for (uint32_t index = 0U; index < sequence_count; ++index) {
        if (af_read_u64(section + sequence_offset +
                        (uint64_t)index * AF_IMAGE_SEQUENCE_RECORD_SIZE) == id) {
            return true;
        }
    }
    return false;
}

static uint64_t af_image_expected_mip_size(uint32_t width, uint32_t height, uint32_t mip_count) {
    uint64_t total = 0U;
    for (uint32_t mip = 0U; mip < mip_count; ++mip) {
        total += (uint64_t)width * height * 4U;
        width = width > 1U ? width / 2U : 1U;
        height = height > 1U ? height / 2U : 1U;
    }
    return total;
}

static uint64_t af_ktx2_expected_size(uint32_t width, uint32_t height, uint32_t mip_count) {
    return (uint64_t)af_ktx2_data_offset(mip_count) +
           af_image_expected_mip_size(width, height, mip_count);
}

static bool af_ktx2_validate_atlas(const uint8_t *bytes, uint64_t size, uint32_t width,
                                   uint32_t height, uint32_t mip_count,
                                   AfImageColorSpace color_space) {
    static const uint8_t identifier[12] = {0xABU, 0x4BU, 0x54U, 0x58U, 0x20U, 0x32U,
                                           0x30U, 0xBBU, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    uint8_t expected_dfd[AF_KTX2_DFD_SIZE];
    const uint32_t dfd_offset = AF_KTX2_HEADER_SIZE + mip_count * AF_KTX2_LEVEL_RECORD_SIZE;
    const uint32_t data_offset = af_ktx2_data_offset(mip_count);
    uint64_t raw_offset = 0U;
    uint32_t mip_width = width;
    uint32_t mip_height = height;
    if (size != af_ktx2_expected_size(width, height, mip_count) ||
        memcmp(bytes, identifier, sizeof(identifier)) != 0 ||
        af_read_u32(bytes + 12U) != (color_space == AF_IMAGE_COLOR_SRGB
                                         ? AF_KTX2_FORMAT_RGBA8_SRGB
                                         : AF_KTX2_FORMAT_RGBA8_UNORM) ||
        af_read_u32(bytes + 16U) != 1U || af_read_u32(bytes + 20U) != width ||
        af_read_u32(bytes + 24U) != height || af_read_u32(bytes + 28U) != 0U ||
        af_read_u32(bytes + 32U) != 0U || af_read_u32(bytes + 36U) != 1U ||
        af_read_u32(bytes + 40U) != mip_count || af_read_u32(bytes + 44U) != 0U ||
        af_read_u32(bytes + 48U) != dfd_offset || af_read_u32(bytes + 52U) != AF_KTX2_DFD_SIZE ||
        af_read_u32(bytes + 56U) != 0U || af_read_u32(bytes + 60U) != 0U ||
        af_read_u64(bytes + 64U) != 0U || af_read_u64(bytes + 72U) != 0U) {
        return false;
    }
    af_ktx2_write_dfd(expected_dfd, color_space);
    if (memcmp(bytes + dfd_offset, expected_dfd, sizeof(expected_dfd)) != 0) {
        return false;
    }
    for (uint32_t mip = 0U; mip < mip_count; ++mip) {
        const uint8_t *record = bytes + AF_KTX2_HEADER_SIZE + mip * AF_KTX2_LEVEL_RECORD_SIZE;
        const uint64_t mip_size = (uint64_t)mip_width * mip_height * 4U;
        if (af_read_u64(record) != data_offset + raw_offset ||
            af_read_u64(record + 8U) != mip_size || af_read_u64(record + 16U) != mip_size) {
            return false;
        }
        raw_offset += mip_size;
        mip_width = mip_width > 1U ? mip_width / 2U : 1U;
        mip_height = mip_height > 1U ? mip_height / 2U : 1U;
    }
    return data_offset + raw_offset == size;
}

static void af_ktx2_decode_atlas(const uint8_t *bytes, uint32_t mip_count, uint8_t *pixels) {
    uint32_t output_offset = 0U;
    for (uint32_t mip = 0U; mip < mip_count; ++mip) {
        const uint8_t *record = bytes + AF_KTX2_HEADER_SIZE + mip * AF_KTX2_LEVEL_RECORD_SIZE;
        const uint32_t source_offset = (uint32_t)af_read_u64(record);
        const uint32_t mip_size = (uint32_t)af_read_u64(record + 8U);
        memcpy(pixels + output_offset, bytes + source_offset, mip_size);
        output_offset += mip_size;
    }
}

static uint32_t af_image_max_mips(uint32_t width, uint32_t height) {
    uint32_t edge = width > height ? width : height;
    uint32_t count = edge == 0U ? 0U : 1U;
    while (edge > 1U) {
        edge /= 2U;
        ++count;
    }
    return count;
}

static bool af_image_packbits_validate(const uint8_t *bytes, uint64_t stored_size,
                                       uint64_t decoded_size) {
    uint64_t read = 0U;
    uint64_t written = 0U;
    while (read < stored_size) {
        const uint8_t control = bytes[read++];
        uint64_t count = 0U;
        if (control < 128U) {
            count = (uint64_t)control + 1U;
            if (count > stored_size - read) {
                return false;
            }
            read += count;
        } else {
            count = (uint64_t)control - 128U + 3U;
            if (read >= stored_size) {
                return false;
            }
            ++read;
        }
        if (count > decoded_size - written) {
            return false;
        }
        written += count;
    }
    return read == stored_size && written == decoded_size;
}

static bool af_ktx2_validate_packbits(const uint8_t *bytes, uint32_t stored_size,
                                      uint32_t decoded_size, uint32_t width, uint32_t height,
                                      uint32_t mip_count, AfImageColorSpace color_space) {
    uint8_t prefix[AF_KTX2_HEADER_SIZE + AF_IMAGE_MAX_MIPS * AF_KTX2_LEVEL_RECORD_SIZE +
                   AF_KTX2_DFD_SIZE + 3U];
    const uint32_t prefix_size = af_ktx2_data_offset(mip_count);
    uint32_t read = 0U;
    uint32_t written = 0U;
    if (!af_image_packbits_validate(bytes, stored_size, decoded_size)) {
        return false;
    }
    while (read < stored_size && written < prefix_size) {
        const uint8_t control = bytes[read++];
        const uint32_t count =
            control < 128U ? (uint32_t)control + 1U : (uint32_t)control - 128U + 3U;
        const uint32_t copy_count = count < prefix_size - written ? count : prefix_size - written;
        if (control < 128U) {
            memcpy(prefix + written, bytes + read, copy_count);
            read += count;
        } else {
            memset(prefix + written, bytes[read++], copy_count);
        }
        written += copy_count;
    }
    return written == prefix_size &&
           af_ktx2_validate_atlas(prefix, decoded_size, width, height, mip_count, color_space);
}

static void af_image_packbits_decode(const uint8_t *bytes, uint32_t stored_size, uint8_t *output) {
    uint32_t read = 0U;
    uint32_t written = 0U;
    while (read < stored_size) {
        const uint8_t control = bytes[read++];
        if (control < 128U) {
            const uint32_t count = (uint32_t)control + 1U;
            memcpy(output + written, bytes + read, count);
            read += count;
            written += count;
        } else {
            const uint32_t count = (uint32_t)control - 128U + 3U;
            memset(output + written, bytes[read++], count);
            written += count;
        }
    }
}

static AfResult af_validate_images(const uint8_t *bytes, const AfSectionView *images,
                                   const AfSectionView *nodes) {
    const uint8_t *section = bytes + images->offset;
    uint32_t asset_count = 0U;
    uint32_t binding_count = 0U;
    uint32_t sequence_count = 0U;
    uint32_t frame_count = 0U;
    uint32_t atlas_count = 0U;
    uint64_t asset_offset = AF_IMAGE_SECTION_HEADER_SIZE;
    uint64_t binding_offset = 0U;
    uint64_t sequence_offset = 0U;
    uint64_t frame_offset = 0U;
    uint64_t atlas_offset = 0U;
    uint64_t fixed_size = 0U;
    uint64_t expected_pixel_offset = 0U;
    if (images->size < AF_IMAGE_SECTION_HEADER_SIZE) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    asset_count = af_read_u32(section);
    binding_count = af_read_u32(section + 4U);
    sequence_count = af_read_u32(section + 8U);
    frame_count = af_read_u32(section + 12U);
    atlas_count = af_read_u32(section + 16U);
    if (asset_count != images->count || asset_count > AF_MAX_IMAGE_ASSETS ||
        binding_count > AF_MAX_IMAGE_BINDINGS || sequence_count > AF_MAX_IMAGE_SEQUENCES ||
        frame_count > AF_MAX_IMAGE_SEQUENCE_FRAMES || atlas_count > AF_MAX_IMAGE_ATLASES) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    binding_offset = asset_offset + (uint64_t)asset_count * AF_IMAGE_ASSET_RECORD_SIZE;
    sequence_offset = binding_offset + (uint64_t)binding_count * AF_IMAGE_BINDING_RECORD_SIZE;
    frame_offset = sequence_offset + (uint64_t)sequence_count * AF_IMAGE_SEQUENCE_RECORD_SIZE;
    atlas_offset = frame_offset + (uint64_t)frame_count * sizeof(uint64_t);
    fixed_size = atlas_offset + (uint64_t)atlas_count * AF_IMAGE_ATLAS_RECORD_SIZE;
    if (fixed_size > images->size) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    expected_pixel_offset = fixed_size;
    for (uint32_t index = 0U; index < atlas_count; ++index) {
        const uint8_t *record =
            section + atlas_offset + (uint64_t)index * AF_IMAGE_ATLAS_RECORD_SIZE;
        const uint32_t width = af_read_u32(record);
        const uint32_t height = af_read_u32(record + 4U);
        const uint32_t mip_count = af_read_u32(record + 8U);
        const uint32_t color_space = af_read_u32(record + 12U);
        const uint64_t pixel_offset = af_read_u64(record + 16U);
        const uint64_t pixel_size = af_read_u64(record + 24U);
        const uint32_t storage = af_read_u32(record + 84U);
        const uint64_t decoded_size = af_read_u64(record + 88U);
        const uint64_t expected_ktx2_size = af_ktx2_expected_size(width, height, mip_count);
        if (color_space > AF_IMAGE_COLOR_LINEAR || storage > AF_IMAGE_COMPRESSION_LOSSLESS ||
            pixel_offset != expected_pixel_offset ||
            (pixel_size == 0U && (width != 0U || height != 0U || mip_count != 0U ||
                                  decoded_size != 0U || storage != AF_IMAGE_COMPRESSION_NONE)) ||
            (pixel_size != 0U && (width == 0U || height == 0U)) ||
            (pixel_size != 0U &&
             (!af_is_power_of_two(width) || !af_is_power_of_two(height) ||
              width > AF_MAX_IMAGE_DIMENSION || height > AF_MAX_IMAGE_DIMENSION ||
              mip_count == 0U || mip_count > AF_IMAGE_MAX_MIPS || pixel_offset < fixed_size ||
              mip_count > af_image_max_mips(width, height) || pixel_offset > images->size ||
              pixel_size > images->size - pixel_offset || decoded_size != expected_ktx2_size ||
              (storage == AF_IMAGE_COMPRESSION_NONE &&
               (pixel_size != decoded_size ||
                !af_ktx2_validate_atlas(section + pixel_offset, pixel_size, width, height,
                                        mip_count, (AfImageColorSpace)color_space))) ||
              (storage == AF_IMAGE_COMPRESSION_LOSSLESS &&
               (pixel_size >= decoded_size ||
                !af_ktx2_validate_packbits(section + pixel_offset, (uint32_t)pixel_size,
                                           (uint32_t)decoded_size, width, height, mip_count,
                                           (AfImageColorSpace)color_space)))))) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        expected_pixel_offset += pixel_size;
        {
            uint64_t offset = 0U;
            uint32_t mip_width = width;
            uint32_t mip_height = height;
            for (uint32_t mip = 0U; mip < mip_count; ++mip) {
                if (af_read_u32(record + 32U + mip * 4U) != offset) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
                offset += (uint64_t)mip_width * mip_height * 4U;
                mip_width = mip_width > 1U ? mip_width / 2U : 1U;
                mip_height = mip_height > 1U ? mip_height / 2U : 1U;
            }
            for (uint32_t mip = mip_count; mip < AF_IMAGE_MAX_MIPS; ++mip) {
                if (af_read_u32(record + 32U + mip * 4U) != 0U) {
                    return AF_RESULT_INVALID_PACKAGE;
                }
            }
        }
        for (uint32_t other = 0U; other < index; ++other) {
            const uint8_t *candidate =
                section + atlas_offset + (uint64_t)other * AF_IMAGE_ATLAS_RECORD_SIZE;
            const uint64_t candidate_offset = af_read_u64(candidate + 16U);
            const uint64_t candidate_size = af_read_u64(candidate + 24U);
            if (pixel_size != 0U && candidate_size != 0U &&
                pixel_offset < candidate_offset + candidate_size &&
                candidate_offset < pixel_offset + pixel_size) {
                return AF_RESULT_INVALID_PACKAGE;
            }
            if (af_read_u32(candidate + 12U) == color_space) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    if (expected_pixel_offset != images->size) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    for (uint32_t index = 0U; index < asset_count; ++index) {
        const uint8_t *record =
            section + asset_offset + (uint64_t)index * AF_IMAGE_ASSET_RECORD_SIZE;
        const uint64_t id = af_read_u64(record);
        const uint32_t width = af_read_u32(record + 16U);
        const uint32_t height = af_read_u32(record + 20U);
        const uint32_t trim_x = af_read_u32(record + 24U);
        const uint32_t trim_y = af_read_u32(record + 28U);
        const uint32_t trim_width = af_read_u32(record + 32U);
        const uint32_t trim_height = af_read_u32(record + 36U);
        const uint32_t atlas_index = af_read_u32(record + 40U);
        if (id == 0U || id == UINT64_MAX || width == 0U || height == 0U ||
            width > AF_MAX_IMAGE_DIMENSION || height > AF_MAX_IMAGE_DIMENSION || trim_width == 0U ||
            trim_height == 0U || trim_width > width || trim_height > height ||
            trim_x > width - trim_width || trim_y > height - trim_height ||
            atlas_index >= atlas_count || af_read_u32(record + 52U) > AF_IMAGE_SOURCE_SVG ||
            af_read_u32(record + 56U) > AF_IMAGE_COLOR_LINEAR ||
            af_read_u32(record + 60U) > AF_IMAGE_COMPRESSION_LOSSLESS ||
            af_read_u32(record + 64U) > UINT16_MAX || af_read_u32(record + 68U) > UINT16_MAX ||
            af_read_u32(record + 72U) > UINT16_MAX || af_read_u32(record + 76U) > UINT16_MAX ||
            af_read_u32(record + 80U) > 3U) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        {
            const uint8_t *atlas =
                section + atlas_offset + (uint64_t)atlas_index * AF_IMAGE_ATLAS_RECORD_SIZE;
            if (trim_width > af_read_u32(atlas) || trim_height > af_read_u32(atlas + 4U) ||
                af_read_u32(record + 44U) > af_read_u32(atlas) - trim_width ||
                af_read_u32(record + 48U) > af_read_u32(atlas + 4U) - trim_height ||
                af_read_u32(record + 56U) != af_read_u32(atlas + 12U)) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        for (uint32_t other = index + 1U; other < asset_count; ++other) {
            const uint8_t *candidate =
                section + asset_offset + (uint64_t)other * AF_IMAGE_ASSET_RECORD_SIZE;
            if (af_read_u64(candidate) == id) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    for (uint32_t index = 0U; index < sequence_count; ++index) {
        const uint8_t *record =
            section + sequence_offset + (uint64_t)index * AF_IMAGE_SEQUENCE_RECORD_SIZE;
        const uint64_t id = af_read_u64(record);
        const uint32_t start = af_read_u32(record + 8U);
        const uint32_t count = af_read_u32(record + 12U);
        if (id == 0U || count == 0U || start > frame_count || count > frame_count - start ||
            af_image_section_has_asset(section, asset_offset, asset_count, id) ||
            !isfinite(af_read_f32(record + 16U)) || af_read_f32(record + 16U) <= 0.0F ||
            af_read_u32(record + 20U) > 1U ||
            memchr(record + 24U, '\0', AF_IMAGE_NAME_CAPACITY) == NULL) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t frame = 0U; frame < count; ++frame) {
            if (!af_image_section_has_asset(
                    section, asset_offset, asset_count,
                    af_read_u64(section + frame_offset +
                                (uint64_t)(start + frame) * sizeof(uint64_t)))) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
        for (uint32_t other = index + 1U; other < sequence_count; ++other) {
            if (af_read_u64(section + sequence_offset +
                            (uint64_t)other * AF_IMAGE_SEQUENCE_RECORD_SIZE) == id) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    for (uint32_t index = 0U; index < binding_count; ++index) {
        const uint8_t *record =
            section + binding_offset + (uint64_t)index * AF_IMAGE_BINDING_RECORD_SIZE;
        const uint64_t node_id = af_read_u64(record);
        const uint64_t asset_id = af_read_u64(record + 8U);
        const uint64_t sequence_id = af_read_u64(record + 16U);
        if (node_id == 0U || af_node_type_for_id(bytes, nodes, node_id) != AF_NODE_IMAGE ||
            (asset_id == 0U) == (sequence_id == 0U) ||
            (asset_id != 0U &&
             !af_image_section_has_asset(section, asset_offset, asset_count, asset_id)) ||
            (sequence_id != 0U && !af_image_section_has_sequence(section, sequence_offset,
                                                                 sequence_count, sequence_id)) ||
            af_read_u32(record + 24U) > AF_IMAGE_DISPLAY_NINE_SLICE ||
            af_read_u32(record + 28U) > UINT16_MAX || af_read_u32(record + 32U) > UINT16_MAX ||
            af_read_u32(record + 36U) > UINT16_MAX || af_read_u32(record + 40U) > UINT16_MAX ||
            !isfinite(af_read_f32(record + 44U)) || af_read_f32(record + 44U) < 0.0F ||
            af_read_u32(record + 48U) > 1U) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t other = index + 1U; other < binding_count; ++other) {
            if (af_read_u64(section + binding_offset +
                            (uint64_t)other * AF_IMAGE_BINDING_RECORD_SIZE) == node_id) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    for (uint32_t index = 0U; index < nodes->count; ++index) {
        const uint8_t *node = bytes + nodes->offset + (uint64_t)index * AF_NODE_RECORD_SIZE;
        if (af_read_u32(node + 16U) == AF_NODE_IMAGE) {
            bool found = false;
            for (uint32_t binding = 0U; binding < binding_count; ++binding) {
                found = found || af_read_u64(section + binding_offset +
                                             (uint64_t)binding * AF_IMAGE_BINDING_RECORD_SIZE) ==
                                     af_read_u64(node);
            }
            if (!found) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_decode_images(const uint8_t *section, AfAllocator *allocator,
                                 AfImageData *out_images) {
    const uint32_t asset_count = af_read_u32(section);
    const uint32_t binding_count = af_read_u32(section + 4U);
    const uint32_t sequence_count = af_read_u32(section + 8U);
    const uint32_t frame_count = af_read_u32(section + 12U);
    const uint32_t atlas_count = af_read_u32(section + 16U);
    const uint64_t asset_offset = AF_IMAGE_SECTION_HEADER_SIZE;
    const uint64_t binding_offset =
        asset_offset + (uint64_t)asset_count * AF_IMAGE_ASSET_RECORD_SIZE;
    const uint64_t sequence_offset =
        binding_offset + (uint64_t)binding_count * AF_IMAGE_BINDING_RECORD_SIZE;
    const uint64_t frame_offset =
        sequence_offset + (uint64_t)sequence_count * AF_IMAGE_SEQUENCE_RECORD_SIZE;
    const uint64_t atlas_offset = frame_offset + (uint64_t)frame_count * sizeof(uint64_t);
    AfResult result = af_image_data_create(out_images, allocator);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    out_images->asset_count = asset_count;
    out_images->binding_count = binding_count;
    out_images->sequence_count = sequence_count;
    out_images->sequence_frame_count = frame_count;
    out_images->atlas_count = atlas_count;
    for (uint32_t index = 0U; index < asset_count; ++index) {
        const uint8_t *record =
            section + asset_offset + (uint64_t)index * AF_IMAGE_ASSET_RECORD_SIZE;
        AfImageAsset *asset = &out_images->assets[index];
        *asset = (AfImageAsset){.id = af_read_u64(record),
                                .content_hash = af_read_u64(record + 8U),
                                .width = af_read_u32(record + 16U),
                                .height = af_read_u32(record + 20U),
                                .trim_x = af_read_u32(record + 24U),
                                .trim_y = af_read_u32(record + 28U),
                                .trim_width = af_read_u32(record + 32U),
                                .trim_height = af_read_u32(record + 36U),
                                .atlas_index = af_read_u32(record + 40U),
                                .atlas_x = af_read_u32(record + 44U),
                                .atlas_y = af_read_u32(record + 48U),
                                .source_format = (AfImageSourceFormat)af_read_u32(record + 52U),
                                .color_space = (AfImageColorSpace)af_read_u32(record + 56U),
                                .compression = (AfImageCompression)af_read_u32(record + 60U),
                                .sprite_borders = {(uint16_t)af_read_u32(record + 64U),
                                                   (uint16_t)af_read_u32(record + 68U),
                                                   (uint16_t)af_read_u32(record + 72U),
                                                   (uint16_t)af_read_u32(record + 76U)},
                                .trim_enabled = (af_read_u32(record + 80U) & 1U) != 0U,
                                .mipmaps_enabled = (af_read_u32(record + 80U) & 2U) != 0U};
    }
    for (uint32_t index = 0U; index < binding_count; ++index) {
        const uint8_t *record =
            section + binding_offset + (uint64_t)index * AF_IMAGE_BINDING_RECORD_SIZE;
        out_images->bindings[index] = (AfImageBinding){
            .node_id = af_read_u64(record),
            .asset_id = af_read_u64(record + 8U),
            .sequence_id = af_read_u64(record + 16U),
            .display_mode = (AfImageDisplayMode)af_read_u32(record + 24U),
            .borders = {(uint16_t)af_read_u32(record + 28U), (uint16_t)af_read_u32(record + 32U),
                        (uint16_t)af_read_u32(record + 36U), (uint16_t)af_read_u32(record + 40U)},
            .frames_per_second = af_read_f32(record + 44U),
            .loop = af_read_u32(record + 48U) != 0U};
    }
    for (uint32_t index = 0U; index < sequence_count; ++index) {
        const uint8_t *record =
            section + sequence_offset + (uint64_t)index * AF_IMAGE_SEQUENCE_RECORD_SIZE;
        out_images->sequences[index] =
            (AfImageSequence){.id = af_read_u64(record),
                              .frame_start = af_read_u32(record + 8U),
                              .frame_count = af_read_u32(record + 12U),
                              .frames_per_second = af_read_f32(record + 16U),
                              .loop = af_read_u32(record + 20U) != 0U};
        memcpy(out_images->sequences[index].name, record + 24U, AF_IMAGE_NAME_CAPACITY);
    }
    for (uint32_t index = 0U; index < frame_count; ++index) {
        out_images->sequence_frames[index] =
            af_read_u64(section + frame_offset + (uint64_t)index * sizeof(uint64_t));
    }
    for (uint32_t index = 0U; index < atlas_count; ++index) {
        const uint8_t *record =
            section + atlas_offset + (uint64_t)index * AF_IMAGE_ATLAS_RECORD_SIZE;
        AfImageAtlas *atlas = &out_images->atlases[index];
        const uint64_t pixel_offset = af_read_u64(record + 16U);
        const uint32_t stored_size = (uint32_t)af_read_u64(record + 24U);
        const uint32_t ktx2_size = (uint32_t)af_read_u64(record + 88U);
        const AfImageCompression storage = (AfImageCompression)af_read_u32(record + 84U);
        *atlas = (AfImageAtlas){.width = af_read_u32(record),
                                .height = af_read_u32(record + 4U),
                                .mip_count = af_read_u32(record + 8U),
                                .color_space = (AfImageColorSpace)af_read_u32(record + 12U),
                                .compression = storage};
        atlas->pixel_size =
            (uint32_t)af_image_expected_mip_size(atlas->width, atlas->height, atlas->mip_count);
        for (uint32_t mip = 0U; mip < AF_IMAGE_MAX_MIPS; ++mip) {
            atlas->mip_offsets[mip] = af_read_u32(record + 32U + mip * 4U);
        }
        if (ktx2_size != 0U) {
            const uint8_t *ktx2 = section + pixel_offset;
            uint8_t *decoded_ktx2 = NULL;
            if (storage == AF_IMAGE_COMPRESSION_LOSSLESS) {
                decoded_ktx2 = af_allocate(allocator, ktx2_size, alignof(uint8_t));
                if (decoded_ktx2 == NULL) {
                    af_image_data_destroy(out_images);
                    return AF_RESULT_OUT_OF_MEMORY;
                }
                af_image_packbits_decode(section + pixel_offset, stored_size, decoded_ktx2);
                ktx2 = decoded_ktx2;
            }
            if (!af_ktx2_validate_atlas(ktx2, ktx2_size, atlas->width, atlas->height,
                                        atlas->mip_count, atlas->color_space)) {
                af_deallocate(allocator, decoded_ktx2);
                af_image_data_destroy(out_images);
                return AF_RESULT_INVALID_PACKAGE;
            }
            atlas->pixels = af_allocate(allocator, atlas->pixel_size, alignof(uint8_t));
            if (atlas->pixels == NULL) {
                af_deallocate(allocator, decoded_ktx2);
                af_image_data_destroy(out_images);
                return AF_RESULT_OUT_OF_MEMORY;
            }
            af_ktx2_decode_atlas(ktx2, atlas->mip_count, atlas->pixels);
            af_deallocate(allocator, decoded_ktx2);
        }
    }
    return AF_RESULT_SUCCESS;
}
