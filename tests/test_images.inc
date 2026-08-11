static bool af_test_bytes_contain(const uint8_t *bytes, size_t size, const char *needle) {
    const size_t length = strlen(needle);
    if (length == 0U || length > size) {
        return false;
    }
    for (size_t index = 0U; index <= size - length; ++index) {
        if (memcmp(bytes + index, needle, length) == 0) {
            return true;
        }
    }
    return false;
}

static void af_test_image_write_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static void af_test_image_write_u64(uint8_t *bytes, uint64_t value) {
    af_test_image_write_u32(bytes, (uint32_t)value);
    af_test_image_write_u32(bytes + 4U, (uint32_t)(value >> 32U));
}

static void af_test_image_write_dfd(uint8_t *bytes, bool srgb) {
    af_test_image_write_u32(bytes, 92U);
    af_test_image_write_u32(bytes + 8U, 2U | (88U << 16U));
    af_test_image_write_u32(bytes + 12U, 1U | (1U << 8U) | ((srgb ? 2U : 1U) << 16U));
    af_test_image_write_u32(bytes + 20U, 4U);
    for (uint32_t channel = 0U; channel < 4U; ++channel) {
        uint32_t channel_id = channel == 3U ? 15U : channel;
        if (srgb && channel == 3U) {
            channel_id |= 16U;
        }
        af_test_image_write_u32(bytes + 28U + channel * 16U,
                                channel * 8U | (7U << 16U) | (channel_id << 24U));
        af_test_image_write_u32(bytes + 40U + channel * 16U, 255U);
    }
}

static bool af_test_images(void) {
    static const uint8_t pixels[4U * 4U * 4U] = {
        0,  0,   0,  0,   0,  0,   0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,   0,   0,   0,   255, 32,
        16, 255, 32, 255, 16, 255, 0, 0, 0, 0, 0, 0, 0, 0, 16, 32, 255, 255, 255, 255, 32,  255,
        0,  0,   0,  0,   0,  0,   0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,   0,   0,   0};
    static const char svg[] = "<svg width=\"8\" height=\"6\" viewBox=\"0 0 8 6\">"
                              "<rect x=\"1\" y=\"1\" width=\"6\" height=\"4\" fill=\"#31a8ff\"/>"
                              "<circle cx=\"4\" cy=\"3\" r=\"2\" fill=\"#ff8040cc\"/>"
                              "</svg>";
    static const char unsafe_svg[] = "<svg width=\"8\" height=\"8\"><SCRIPT>bad()</SCRIPT></svg>";
    static const uint8_t jpeg[] = {
        0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00,
        0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x06, 0x04, 0x05, 0x06, 0x05,
        0x04, 0x06, 0x06, 0x05, 0x06, 0x07, 0x07, 0x06, 0x08, 0x0a, 0x10, 0x0a, 0x0a, 0x09, 0x09,
        0x0a, 0x14, 0x0e, 0x0f, 0x0c, 0x10, 0x17, 0x14, 0x18, 0x18, 0x17, 0x14, 0x16, 0x16, 0x1a,
        0x1d, 0x25, 0x1f, 0x1a, 0x1b, 0x23, 0x1c, 0x16, 0x16, 0x20, 0x2c, 0x20, 0x23, 0x26, 0x27,
        0x29, 0x2a, 0x29, 0x19, 0x1f, 0x2d, 0x30, 0x2d, 0x28, 0x30, 0x25, 0x28, 0x29, 0x28, 0xff,
        0xdb, 0x00, 0x43, 0x01, 0x07, 0x07, 0x07, 0x0a, 0x08, 0x0a, 0x13, 0x0a, 0x0a, 0x13, 0x28,
        0x1a, 0x16, 0x1a, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
        0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
        0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
        0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x02,
        0x00, 0x02, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
        0x15, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x05, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00,
        0x14, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x07, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xda, 0x00, 0x0c,
        0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00, 0xa6, 0x01, 0x24, 0x64, 0xff,
        0xd9};
    static const uint8_t oversized_png[33] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00,
                                              0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
                                              0x10, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00,
                                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const char *source_path = "arcflow_test_image_source.png";
    const char *project_path = "arcflow_test_image_project.afproj";
    AfAllocator allocator;
    AfDecodedImage svg_image = {0};
    AfProject project = {0};
    AfProject loaded = {0};
    AfPackageBlob first_package = {0};
    AfPackageBlob second_package = {0};
    AfPackageBlob ktx2_package = {0};
    AfLoadedPackage runtime = {0};
    uint64_t first_asset = 0U;
    uint64_t second_asset = 0U;
    uint64_t first_node = 0U;
    uint64_t second_node = 0U;
    char copied_path[AF_IMAGE_PATH_CAPACITY];
    af_allocator_init_default(&allocator);
    CHECK(af_image_decode_memory(svg, sizeof(svg) - 1U, AF_IMAGE_SOURCE_SVG, &allocator,
                                 &svg_image) == AF_RESULT_SUCCESS);
    CHECK(svg_image.width == 8U && svg_image.height == 6U);
    CHECK(svg_image.pixels[(3U * 8U + 4U) * 4U + 3U] != 0U);
    af_decoded_image_destroy(&svg_image);
    CHECK(af_image_decode_memory(unsafe_svg, sizeof(unsafe_svg) - 1U, AF_IMAGE_SOURCE_SVG,
                                 &allocator, &svg_image) != AF_RESULT_SUCCESS);
    CHECK(af_image_decode_memory(jpeg, sizeof(jpeg), AF_IMAGE_SOURCE_JPEG, &allocator,
                                 &svg_image) == AF_RESULT_SUCCESS);
    CHECK(svg_image.width == 2U && svg_image.height == 2U);
    af_decoded_image_destroy(&svg_image);
    CHECK(af_image_decode_memory(oversized_png, sizeof(oversized_png), AF_IMAGE_SOURCE_PNG,
                                 &allocator, &svg_image) != AF_RESULT_SUCCESS);
    {
        static const uint8_t identifier[12] = {0xABU, 0x4BU, 0x54U, 0x58U, 0x20U, 0x32U,
                                               0x30U, 0xBBU, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
        uint8_t ktx2[260] = {0};
        memcpy(ktx2, identifier, sizeof(identifier));
        af_test_image_write_u32(ktx2 + 12U, 37U);
        af_test_image_write_u32(ktx2 + 16U, 1U);
        af_test_image_write_u32(ktx2 + 20U, 4U);
        af_test_image_write_u32(ktx2 + 24U, 4U);
        af_test_image_write_u32(ktx2 + 36U, 1U);
        af_test_image_write_u32(ktx2 + 40U, 1U);
        af_test_image_write_u32(ktx2 + 48U, 104U);
        af_test_image_write_u32(ktx2 + 52U, 92U);
        af_test_image_write_u64(ktx2 + 80U, 196U);
        af_test_image_write_u64(ktx2 + 88U, 64U);
        af_test_image_write_u64(ktx2 + 96U, 64U);
        af_test_image_write_dfd(ktx2 + 104U, false);
        memset(ktx2 + 196U, 255, 64U);
        CHECK(af_image_decode_memory(ktx2, sizeof(ktx2), AF_IMAGE_SOURCE_KTX2, &allocator,
                                     &svg_image) == AF_RESULT_SUCCESS);
        CHECK(svg_image.width == 4U && svg_image.height == 4U &&
              svg_image.color_space == AF_IMAGE_COLOR_LINEAR);
        af_decoded_image_destroy(&svg_image);
        af_test_image_write_u32(ktx2 + 52U, 0U);
        CHECK(af_image_decode_memory(ktx2, sizeof(ktx2), AF_IMAGE_SOURCE_KTX2, &allocator,
                                     &svg_image) != AF_RESULT_SUCCESS);
        af_test_image_write_u32(ktx2 + 52U, 92U);
        af_test_image_write_dfd(ktx2 + 104U, true);
        CHECK(af_image_decode_memory(ktx2, sizeof(ktx2), AF_IMAGE_SOURCE_KTX2, &allocator,
                                     &svg_image) != AF_RESULT_SUCCESS);
        af_test_image_write_dfd(ktx2 + 104U, false);
        af_test_image_write_u64(ktx2 + 80U, 80U);
        CHECK(af_image_decode_memory(ktx2, sizeof(ktx2), AF_IMAGE_SOURCE_KTX2, &allocator,
                                     &svg_image) != AF_RESULT_SUCCESS);
    }
    CHECK(af_png_write_rgba8(source_path, pixels, 4U, 4U, 16U, AF_PIXEL_ORDER_RGBA) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_create(&project, &allocator, 8U) == AF_RESULT_SUCCESS);
    CHECK(af_project_save_file(&project, project_path, &allocator) == AF_RESULT_SUCCESS);
    CHECK(af_project_import_image(&project, project_path, source_path, &first_asset, &first_node) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_import_image(&project, project_path, source_path, &second_asset,
                                  &second_node) == AF_RESULT_SUCCESS);
    CHECK(first_asset == second_asset);
    CHECK(first_node != second_node);
    CHECK(project.image_data.asset_count == 1U && project.image_data.binding_count == 2U);
    {
        AfProjectNodeSnapshot snapshot = {0};
        CHECK(af_project_snapshot(&project, first_node, &snapshot) == AF_RESULT_SUCCESS);
        CHECK(snapshot.has_image_binding && snapshot.image_binding.asset_id == first_asset);
        CHECK(af_project_remove_node_exact(&project, first_node) == AF_RESULT_SUCCESS);
        CHECK(af_image_find_binding_const(&project.image_data, first_node) == NULL);
        CHECK(af_project_insert_snapshot(&project, &snapshot) == AF_RESULT_SUCCESS);
        CHECK(af_image_find_binding_const(&project.image_data, first_node) != NULL);
    }
    (void)snprintf(project.image_data.assets[0].name, sizeof(project.image_data.assets[0].name),
                   "Pulse Frame");
    CHECK(project.image_data.assets[0].trim_x == 0U);
    CHECK(af_image_trim(&project.image_data.assets[0]) == AF_RESULT_SUCCESS);
    CHECK(project.image_data.assets[0].trim_x == 1U);
    CHECK(project.image_data.assets[0].trim_y == 1U);
    CHECK(project.image_data.assets[0].trim_width == 2U);
    CHECK(project.image_data.assets[0].trim_height == 2U);
    project.image_data.assets[0].sprite_borders = (AfImageBorders){1U, 1U, 1U, 1U};
    project.image_data.bindings[0].display_mode = AF_IMAGE_DISPLAY_NINE_SLICE;
    project.image_data.bindings[0].borders = (AfImageBorders){1U, 1U, 1U, 1U};
    {
        const uint64_t frames[] = {first_asset, first_asset};
        const AfImageSequence sequence = {.id = project.next_asset_id++,
                                          .frame_count = 2U,
                                          .frames_per_second = 12.0F,
                                          .loop = true,
                                          .name = "Pulse Sequence"};
        CHECK(af_image_add_sequence(&project.image_data, &sequence, frames) == AF_RESULT_SUCCESS);
        project.image_data.bindings[1].asset_id = 0U;
        project.image_data.bindings[1].sequence_id = sequence.id;
        project.image_data.bindings[1].frames_per_second = 12.0F;
    }
    CHECK(af_image_build_atlases(&project.image_data) == AF_RESULT_SUCCESS);
    {
        AfImageAsset *asset = &project.image_data.assets[0];
        const AfImageAtlas *atlas = &project.image_data.atlases[asset->atlas_index];
        CHECK(asset->trim_x == 0U && asset->trim_y == 0U && asset->trim_width == asset->width &&
              asset->trim_height == asset->height);
        const uint8_t *interior =
            atlas->pixels + ((size_t)asset->atlas_y * atlas->width + asset->atlas_x) * 4U;
        const uint8_t *left = interior - 4U;
        const uint8_t *top = interior - (size_t)atlas->width * 4U;
        CHECK(memcmp(left, interior, 4U) == 0);
        CHECK(memcmp(top, interior, 4U) == 0);
        {
            const uint32_t atlas_x = asset->atlas_x;
            asset->atlas_x = atlas->width;
            CHECK(af_image_data_validate(&project.image_data, true) != AF_RESULT_SUCCESS);
            asset->atlas_x = atlas_x;
        }
    }
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
    CHECK(af_project_save_file(&project, project_path, &allocator) == AF_RESULT_SUCCESS);
    CHECK(af_project_load_file(project_path, &allocator, &loaded) == AF_RESULT_SUCCESS);
    CHECK(loaded.image_data.asset_count == 1U && loaded.image_data.binding_count == 2U);
    CHECK(strcmp(loaded.image_data.assets[0].name, "Pulse Frame") == 0);
    CHECK(strcmp(loaded.image_data.sequences[0].name, "Pulse Sequence") == 0);
    CHECK(!loaded.image_data.assets[0].missing && loaded.image_data.assets[0].pixels != NULL);
    CHECK(loaded.scene.images == &loaded.image_data);
    CHECK(af_project_compile(&loaded, &allocator, &first_package) == AF_RESULT_SUCCESS);
    CHECK(af_project_compile(&loaded, &allocator, &second_package) == AF_RESULT_SUCCESS);
    CHECK(first_package.size == second_package.size);
    CHECK(memcmp(first_package.data, second_package.data, first_package.size) == 0);
    CHECK(!af_test_bytes_contain(first_package.data, first_package.size, "assets/"));
    CHECK(!af_test_bytes_contain(first_package.data, first_package.size, project_path));
    {
        uint8_t *image_entry =
            first_package.data + AF_PACKAGE_HEADER_SIZE + 12U * AF_PACKAGE_SECTION_SIZE;
        uint8_t *image_section = first_package.data + af_test_read_u64(image_entry + 8U);
        const uint32_t asset_count = af_test_read_u32(image_section);
        const uint32_t binding_count = af_test_read_u32(image_section + 4U);
        const uint32_t sequence_count = af_test_read_u32(image_section + 8U);
        const uint32_t frame_count = af_test_read_u32(image_section + 12U);
        uint8_t *atlas_record = image_section + 64U + (uint64_t)asset_count * 96U +
                                (uint64_t)binding_count * 64U + (uint64_t)sequence_count * 96U +
                                (uint64_t)frame_count * sizeof(uint64_t);
        uint8_t *payload = image_section + af_test_read_u64(atlas_record + 16U);
        const uint8_t saved_identifier = payload[1U];
        CHECK(af_test_read_u32(atlas_record + 84U) == AF_IMAGE_COMPRESSION_LOSSLESS);
        CHECK(af_test_read_u64(atlas_record + 24U) > 1U && payload[0] < 128U);
        payload[1U] = 0U;
        af_test_rechecksum(&first_package);
        CHECK(af_package_validate(first_package.data, first_package.size) ==
              AF_RESULT_INVALID_PACKAGE);
        payload[1U] = saved_identifier;
        af_test_rechecksum(&first_package);
        CHECK(af_package_validate(first_package.data, first_package.size) == AF_RESULT_SUCCESS);
    }
    for (uint32_t index = 0U; index < loaded.image_data.asset_count; ++index) {
        loaded.image_data.assets[index].compression = AF_IMAGE_COMPRESSION_NONE;
    }
    CHECK(af_project_compile(&loaded, &allocator, &ktx2_package) == AF_RESULT_SUCCESS);
    {
        static const uint8_t identifier[12] = {0xABU, 0x4BU, 0x54U, 0x58U, 0x20U, 0x32U,
                                               0x30U, 0xBBU, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
        const uint8_t *image_entry =
            ktx2_package.data + AF_PACKAGE_HEADER_SIZE + 12U * AF_PACKAGE_SECTION_SIZE;
        uint8_t *image_section = ktx2_package.data + af_test_read_u64(image_entry + 8U);
        const uint32_t asset_count = af_test_read_u32(image_section);
        const uint32_t binding_count = af_test_read_u32(image_section + 4U);
        const uint32_t sequence_count = af_test_read_u32(image_section + 8U);
        const uint32_t frame_count = af_test_read_u32(image_section + 12U);
        uint8_t *atlas_record = image_section + 64U + (uint64_t)asset_count * 96U +
                                (uint64_t)binding_count * 64U + (uint64_t)sequence_count * 96U +
                                (uint64_t)frame_count * sizeof(uint64_t);
        uint8_t *ktx2 = image_section + af_test_read_u64(atlas_record + 16U);
        CHECK(af_test_read_u32(atlas_record + 84U) == AF_IMAGE_COMPRESSION_NONE);
        CHECK(memcmp(ktx2, identifier, sizeof(identifier)) == 0);
        CHECK(af_test_read_u32(ktx2 + 12U) == 43U);
        ktx2[0] = 0U;
        af_test_rechecksum(&ktx2_package);
        CHECK(af_package_validate(ktx2_package.data, ktx2_package.size) ==
              AF_RESULT_INVALID_PACKAGE);
    }
    CHECK(af_package_decode(first_package.data, first_package.size, &allocator, &runtime) ==
          AF_RESULT_SUCCESS);
    CHECK(runtime.image_data.asset_count == 1U);
    CHECK(runtime.image_data.binding_count == 2U);
    CHECK(runtime.image_data.sequence_count == 1U);
    CHECK(runtime.image_data.atlas_count == 2U);
    CHECK(runtime.image_data.atlases[0].pixels != NULL);
    CHECK(runtime.image_data.atlases[0].compression == AF_IMAGE_COMPRESSION_LOSSLESS);
    CHECK(runtime.scene.images == &runtime.image_data);
    {
        const uint64_t sequence_id = loaded.image_data.sequences[0].id;
        AfImageBinding binding = loaded.image_data.bindings[1];
        CHECK(af_image_remove_sequence(&loaded.image_data, sequence_id) ==
              AF_RESULT_INVALID_ARGUMENT);
        binding.asset_id = loaded.image_data.assets[0].id;
        binding.sequence_id = 0U;
        binding.frames_per_second = 0.0F;
        CHECK(af_image_set_binding(&loaded.image_data, &binding) == AF_RESULT_SUCCESS);
        CHECK(af_image_remove_sequence(&loaded.image_data, sequence_id) == AF_RESULT_SUCCESS);
        CHECK(loaded.image_data.sequence_count == 0U &&
              loaded.image_data.sequence_frame_count == 0U);
    }
    (void)snprintf(copied_path, sizeof(copied_path), "%s", loaded.image_data.assets[0].source_path);
    af_loaded_package_destroy(&allocator, &runtime);
    af_package_blob_destroy(&allocator, &ktx2_package);
    af_package_blob_destroy(&allocator, &second_package);
    af_package_blob_destroy(&allocator, &first_package);
    af_project_destroy(&loaded);
    af_project_destroy(&project);
    CHECK(remove(source_path) == 0);
    CHECK(remove(project_path) == 0);
    CHECK(remove(copied_path) == 0);
    return true;
}
