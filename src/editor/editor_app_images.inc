static bool af_editor_sequence_uses_asset(const AfImageData *images, uint64_t sequence_id,
                                          uint64_t asset_id) {
    const AfImageSequence *sequence = af_image_find_sequence_const(images, sequence_id);
    if (sequence == NULL) {
        return false;
    }
    for (uint32_t frame = 0U; frame < sequence->frame_count; ++frame) {
        if (images->sequence_frames[sequence->frame_start + frame] == asset_id) {
            return true;
        }
    }
    return false;
}

static bool af_editor_binding_uses_asset(const AfImageData *images, const AfImageBinding *binding,
                                         uint64_t asset_id) {
    return binding->asset_id == asset_id ||
           (binding->sequence_id != 0U &&
            af_editor_sequence_uses_asset(images, binding->sequence_id, asset_id));
}

static uint32_t af_editor_asset_use_count(const AfEditorState *editor, uint64_t asset_id) {
    uint32_t count = 0U;
    for (uint32_t index = 0U; index < editor->project.image_data.binding_count; ++index) {
        count += af_editor_binding_uses_asset(&editor->project.image_data,
                                              &editor->project.image_data.bindings[index], asset_id)
                     ? 1U
                     : 0U;
    }
    return count;
}

static void af_editor_select_asset_users(AfEditorState *editor, uint64_t asset_id) {
    af_editor_select_only(editor, 0U);
    for (uint32_t index = 0U; index < editor->project.image_data.binding_count; ++index) {
        const AfImageBinding *binding = &editor->project.image_data.bindings[index];
        if (af_editor_binding_uses_asset(&editor->project.image_data, binding, asset_id)) {
            af_editor_selection_add(editor, binding->node_id);
        }
    }
    af_editor_status(editor, editor->selected_count == 0U ? "Asset has no scene dependencies"
                                                          : "Asset users selected");
}

static AfResult af_editor_reimport_images(AfEditorState *editor) {
    uint32_t missing = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (editor->project_path[0] == '\0') {
        af_editor_status(editor, "Save the project before reimporting images");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_project_reimport_images(&editor->project, editor->project_path, &missing);
    (void)af_image_build_atlases(&editor->project.image_data);
    af_editor_mark_dirty(editor);
    if (result == AF_RESULT_SUCCESS) {
        af_editor_status(editor, "All image assets reimported");
    } else {
        char status[96];
        (void)snprintf(status, sizeof(status), "%u image source%s missing", missing,
                       missing == 1U ? " is" : "s are");
        af_editor_status(editor, status);
    }
    return result;
}

static AfResult af_editor_create_flipbook(AfEditorState *editor) {
    AfImageBinding *binding =
        af_image_find_binding(&editor->project.image_data, editor->selected_id);
    uint64_t frames[AF_MAX_IMAGE_ASSETS];
    AfImageSequence sequence = {0};
    AfImageBinding replacement = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (binding == NULL || editor->project.image_data.asset_count < 2U) {
        af_editor_status(editor, "Select an image and import at least two frames");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (binding->sequence_id != 0U) {
        af_editor_status(editor, "Selected image already uses a flipbook");
        return AF_RESULT_SUCCESS;
    }
    for (uint32_t index = 0U; index < editor->project.image_data.asset_count; ++index) {
        frames[index] = editor->project.image_data.assets[index].id;
    }
    sequence = (AfImageSequence){.id = editor->project.next_asset_id,
                                 .frame_count = editor->project.image_data.asset_count,
                                 .frames_per_second = 12.0F,
                                 .loop = true};
    (void)snprintf(sequence.name, sizeof(sequence.name), "Flipbook %llu",
                   (unsigned long long)sequence.id);
    result = af_image_add_sequence(&editor->project.image_data, &sequence, frames);
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(editor, "Could not create the image sequence");
        return result;
    }
    replacement = *binding;
    replacement.asset_id = 0U;
    replacement.sequence_id = sequence.id;
    replacement.frames_per_second = sequence.frames_per_second;
    replacement.loop = true;
    result = af_image_set_binding(&editor->project.image_data, &replacement);
    if (result != AF_RESULT_SUCCESS) {
        (void)af_image_remove_sequence(&editor->project.image_data, sequence.id);
        return result;
    }
    ++editor->project.next_asset_id;
    af_editor_mark_dirty(editor);
    af_editor_status(editor, "Flipbook created from all imported images");
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_remove_flipbook(AfEditorState *editor) {
    AfImageBinding *binding =
        af_image_find_binding(&editor->project.image_data, editor->selected_id);
    const AfImageSequence *sequence =
        binding == NULL
            ? NULL
            : af_image_find_sequence_const(&editor->project.image_data, binding->sequence_id);
    AfImageBinding replacement = {0};
    uint64_t sequence_id = 0U;
    if (binding == NULL || sequence == NULL || sequence->frame_count == 0U) {
        af_editor_status(editor, "Selected image does not use a flipbook");
        return AF_RESULT_NOT_FOUND;
    }
    sequence_id = sequence->id;
    replacement = *binding;
    replacement.asset_id = editor->project.image_data.sequence_frames[sequence->frame_start];
    replacement.sequence_id = 0U;
    replacement.frames_per_second = 0.0F;
    if (af_image_set_binding(&editor->project.image_data, &replacement) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < editor->project.image_data.binding_count; ++index) {
        if (editor->project.image_data.bindings[index].sequence_id == sequence_id) {
            af_editor_mark_dirty(editor);
            af_editor_status(editor, "Flipbook removed from selected image");
            return AF_RESULT_SUCCESS;
        }
    }
    (void)af_image_remove_sequence(&editor->project.image_data, sequence_id);
    af_editor_mark_dirty(editor);
    af_editor_status(editor, "Flipbook removed");
    return AF_RESULT_SUCCESS;
}

static void af_editor_toggle_image_nine_slice(AfEditorState *editor) {
    AfImageBinding *binding =
        af_image_find_binding(&editor->project.image_data, editor->selected_id);
    if (binding == NULL) {
        return;
    }
    binding->display_mode = binding->display_mode == AF_IMAGE_DISPLAY_STRETCH
                                ? AF_IMAGE_DISPLAY_NINE_SLICE
                                : AF_IMAGE_DISPLAY_STRETCH;
    ++editor->project.image_data.generation;
    af_editor_mark_dirty(editor);
    af_editor_status(editor, binding->display_mode == AF_IMAGE_DISPLAY_NINE_SLICE
                                 ? "Nine-slice enabled"
                                 : "Image stretch enabled");
}

static void af_editor_smoke_pixels(uint8_t *pixels, uint32_t width, uint32_t height,
                                   bool alternate) {
    for (uint32_t y = 0U; y < height; ++y) {
        for (uint32_t x = 0U; x < width; ++x) {
            const size_t offset = ((size_t)y * width + x) * 4U;
            const bool border = x < 7U || y < 7U || x + 7U >= width || y + 7U >= height;
            const bool check = ((x / 8U) + (y / 8U)) % 2U != 0U;
            pixels[offset] = border ? 245U : (alternate ? (check ? 245U : 66U) : 48U);
            pixels[offset + 1U] =
                border ? (alternate ? 110U : 196U) : (alternate ? 76U : (check ? 202U : 116U));
            pixels[offset + 2U] =
                border ? (alternate ? 72U : 255U) : (alternate ? 156U : (check ? 255U : 220U));
            pixels[offset + 3U] = 255U;
        }
    }
}

static AfResult af_editor_prepare_image_smoke(AfEditorState *editor) {
    uint8_t first_pixels[64U * 64U * 4U];
    uint8_t second_pixels[64U * 64U * 4U];
    uint64_t asset_ids[2] = {0U, 0U};
    uint64_t node_ids[2] = {0U, 0U};
    bool deduplicated = false;
    AfResult result = AF_RESULT_SUCCESS;
    af_editor_smoke_pixels(first_pixels, 64U, 64U, false);
    af_editor_smoke_pixels(second_pixels, 64U, 64U, true);
    for (uint32_t index = 0U; index < 2U; ++index) {
        AfImageAsset asset = {.id = editor->project.next_asset_id,
                              .width = 64U,
                              .height = 64U,
                              .source_format = AF_IMAGE_SOURCE_PNG,
                              .color_space = AF_IMAGE_COLOR_SRGB,
                              .compression = AF_IMAGE_COMPRESSION_LOSSLESS,
                              .trim_enabled = false,
                              .mipmaps_enabled = true,
                              .sprite_borders = {7U, 7U, 7U, 7U}};
        (void)snprintf(asset.name, sizeof(asset.name), "Smoke Frame %u", index + 1U);
        (void)snprintf(asset.source_path, sizeof(asset.source_path), "assets/smoke_frame_%u.png",
                       index + 1U);
        result = af_image_add(&editor->project.image_data, &asset,
                              index == 0U ? first_pixels : second_pixels, &asset_ids[index],
                              &deduplicated);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        ++editor->project.next_asset_id;
    }
    for (uint32_t index = 0U; index < 2U; ++index) {
        AfSceneNode node = {0};
        AfImageBinding binding = {0};
        result = af_project_allocate_id(&editor->project, &node_ids[index]);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        node =
            (AfSceneNode){.id = node_ids[index],
                          .parent_id = AF_INVALID_NODE_ID,
                          .type = AF_NODE_IMAGE,
                          .local_transform = {.position = {180.0F + (float)index * 430.0F, 190.0F},
                                              .scale = {1.0F, 1.0F}},
                          .bounds = {330.0F, 220.0F},
                          .fill = {1.0F, 1.0F, 1.0F, 1.0F},
                          .opacity = 1.0F,
                          .layer_order = 20 + (int32_t)index,
                          .visible = true};
        result = af_project_add_node(&editor->project, &node,
                                     index == 0U ? "Image Flipbook" : "Nine Slice Image");
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        {
            const AfLayoutNode layout = af_editor_centered_pivot_layout(&node);
            result = af_project_set_layout(&editor->project, &layout);
            if (result != AF_RESULT_SUCCESS) {
                return result;
            }
        }
        binding = (AfImageBinding){.node_id = node.id,
                                   .asset_id = asset_ids[index],
                                   .display_mode = index == 0U ? AF_IMAGE_DISPLAY_STRETCH
                                                               : AF_IMAGE_DISPLAY_NINE_SLICE,
                                   .borders = {7U, 7U, 7U, 7U},
                                   .loop = true};
        result = af_image_set_binding(&editor->project.image_data, &binding);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    {
        const AfImageSequence sequence = {.id = editor->project.next_asset_id,
                                          .frame_count = 2U,
                                          .frames_per_second = 4.0F,
                                          .loop = true,
                                          .name = "Smoke Flipbook"};
        AfImageBinding *binding = NULL;
        result = af_image_add_sequence(&editor->project.image_data, &sequence, asset_ids);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        ++editor->project.next_asset_id;
        binding = af_image_find_binding(&editor->project.image_data, node_ids[0]);
        binding->asset_id = 0U;
        binding->sequence_id = sequence.id;
        binding->frames_per_second = sequence.frames_per_second;
    }
    editor->selected_asset_id = asset_ids[0];
    editor->asset_browser_visible = true;
    af_editor_select_only(editor, node_ids[0]);
    return af_scene_evaluate_world(&editor->project.scene);
}
