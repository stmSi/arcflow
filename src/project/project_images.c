#include "project/image_import.h"

#include "core/file_io.h"
#include "core/hash.h"

#include <SDL3/SDL.h>

#include <ctype.h>
#include <stdalign.h>
#include <stdio.h>
#include <string.h>

static bool af_project_directory(const char *project_path, char *output, size_t capacity) {
    const char *slash = NULL;
    size_t length = 0U;
    if (project_path == NULL || project_path[0] == '\0' || output == NULL || capacity < 2U) {
        return false;
    }
    slash = strrchr(project_path, '/');
#if defined(_WIN32)
    {
        const char *backslash = strrchr(project_path, '\\');
        if (backslash != NULL && (slash == NULL || backslash > slash)) {
            slash = backslash;
        }
    }
#endif
    length = slash == NULL ? 1U : (size_t)(slash - project_path);
    if (length == 0U) {
        length = 1U;
    }
    if (length >= capacity) {
        return false;
    }
    if (slash == NULL) {
        output[0] = '.';
    } else {
        memcpy(output, project_path, length);
    }
    output[length] = '\0';
    return true;
}

static bool af_project_asset_source_path(const char *project_path, const char *relative,
                                         char *output, size_t capacity) {
    char directory[AF_IMAGE_PATH_CAPACITY];
    int written = 0;
    if (!af_project_directory(project_path, directory, sizeof(directory)) ||
        !af_image_path_is_project_relative(relative)) {
        return false;
    }
    written = snprintf(output, capacity, "%s/%s", directory, relative);
    return written > 0 && (size_t)written < capacity;
}

static const char *af_project_image_extension(const char *path) {
    const char *extension = path == NULL ? NULL : strrchr(path, '.');
    if (extension == NULL) {
        return NULL;
    }
    if (SDL_strcasecmp(extension, ".png") == 0) {
        return ".png";
    }
    if (SDL_strcasecmp(extension, ".jpg") == 0 || SDL_strcasecmp(extension, ".jpeg") == 0) {
        return ".jpg";
    }
    if (SDL_strcasecmp(extension, ".ktx2") == 0) {
        return ".ktx2";
    }
    return SDL_strcasecmp(extension, ".svg") == 0 ? ".svg" : NULL;
}

static void af_project_image_name(const char *path, char output[AF_IMAGE_NAME_CAPACITY]) {
    const char *base = path;
    const char *slash = strrchr(path, '/');
    size_t write = 0U;
    if (slash != NULL) {
        base = slash + 1U;
    }
#if defined(_WIN32)
    {
        const char *backslash = strrchr(base, '\\');
        if (backslash != NULL) {
            base = backslash + 1U;
        }
    }
#endif
    while (base[write] != '\0' && base[write] != '.' && write + 1U < AF_IMAGE_NAME_CAPACITY) {
        const unsigned char byte = (unsigned char)base[write];
        output[write] = (char)(isalnum(byte) || byte == '_' || byte == '-' ? byte : '_');
        ++write;
    }
    if (write == 0U) {
        memcpy(output, "Image", 6U);
    } else {
        output[write] = '\0';
    }
}

static AfResult af_project_copy_image_source(const char *project_path, const char *source_path,
                                             uint64_t source_hash, char *out_relative,
                                             size_t relative_capacity) {
    const char *extension = af_project_image_extension(source_path);
    char directory[AF_IMAGE_PATH_CAPACITY];
    char assets_directory[AF_IMAGE_PATH_CAPACITY];
    char destination[AF_IMAGE_PATH_CAPACITY * 2U];
    int written = 0;
    if (extension == NULL || !af_project_directory(project_path, directory, sizeof(directory))) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    written = snprintf(assets_directory, sizeof(assets_directory), "%s/assets", directory);
    if (written <= 0 || (size_t)written >= sizeof(assets_directory) ||
        !SDL_CreateDirectory(assets_directory)) {
        return AF_RESULT_IO_ERROR;
    }
    written = snprintf(out_relative, relative_capacity, "assets/image_%016llx%s",
                       (unsigned long long)source_hash, extension);
    if (written <= 0 || (size_t)written >= relative_capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    written = snprintf(destination, sizeof(destination), "%s/%s", directory, out_relative);
    if (written <= 0 || (size_t)written >= sizeof(destination)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (!SDL_CopyFile(source_path, destination)) {
        SDL_PathInfo info;
        if (!SDL_GetPathInfo(destination, &info) || info.type != SDL_PATHTYPE_FILE) {
            return AF_RESULT_IO_ERROR;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_import_image(AfProject *project, const char *project_path,
                                 const char *source_path, uint64_t *out_asset_id,
                                 uint64_t *out_node_id) {
    AfDecodedImage decoded = {0};
    AfFileData source = {0};
    AfImageAsset asset = {0};
    AfImageBinding binding = {0};
    AfSceneNode node = {0};
    char relative[AF_IMAGE_PATH_CAPACITY];
    uint64_t asset_id = 0U;
    uint64_t node_id = 0U;
    uint64_t source_hash = 0U;
    bool deduplicated = false;
    AfResult result = AF_RESULT_SUCCESS;
    if (project == NULL || project_path == NULL || project_path[0] == '\0' || source_path == NULL ||
        source_path[0] == '\0') {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result =
        af_file_read_limited(source_path, AF_MAX_IMAGE_SOURCE_SIZE, project->allocator, &source);
    if (result == AF_RESULT_SUCCESS) {
        source_hash = af_hash_bytes(source.data, source.size);
        result = af_image_decode_file(source_path, project->allocator, &decoded);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_copy_image_source(project_path, source_path, source_hash, relative,
                                              sizeof(relative));
    }
    if (result == AF_RESULT_SUCCESS) {
        asset_id = project->next_asset_id++;
        asset = (AfImageAsset){.id = asset_id,
                               .width = decoded.width,
                               .height = decoded.height,
                               .source_format = decoded.format,
                               .color_space = decoded.color_space,
                               .compression = AF_IMAGE_COMPRESSION_LOSSLESS,
                               .trim_enabled = true,
                               .mipmaps_enabled = true};
        af_project_image_name(source_path, asset.name);
        (void)snprintf(asset.source_path, sizeof(asset.source_path), "%s", relative);
        result =
            af_image_add(&project->image_data, &asset, decoded.pixels, &asset_id, &deduplicated);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_allocate_id(project, &node_id);
    }
    if (result == AF_RESULT_SUCCESS) {
        const AfImageAsset *stored = af_image_find_const(&project->image_data, asset_id);
        node =
            (AfSceneNode){.id = node_id,
                          .parent_id = AF_INVALID_NODE_ID,
                          .type = AF_NODE_IMAGE,
                          .local_transform = {.position = {100.0F, 100.0F}, .scale = {1.0F, 1.0F}},
                          .bounds = {(float)stored->width, (float)stored->height},
                          .fill = {1.0F, 1.0F, 1.0F, 1.0F},
                          .opacity = 1.0F,
                          .visible = true};
        result = af_project_add_node(project, &node, asset.name);
    }
    if (result == AF_RESULT_SUCCESS) {
        binding = (AfImageBinding){.node_id = node_id,
                                   .asset_id = asset_id,
                                   .display_mode = AF_IMAGE_DISPLAY_STRETCH,
                                   .loop = true};
        result = af_image_set_binding(&project->image_data, &binding);
        if (result != AF_RESULT_SUCCESS) {
            (void)af_project_remove_node_exact(project, node_id);
        }
    }
    if (result != AF_RESULT_SUCCESS && !deduplicated && asset_id != 0U) {
        (void)af_image_remove(&project->image_data, asset_id);
    }
    if (result == AF_RESULT_SUCCESS) {
        if (out_asset_id != NULL) {
            *out_asset_id = asset_id;
        }
        if (out_node_id != NULL) {
            *out_node_id = node_id;
        }
    }
    af_decoded_image_destroy(&decoded);
    af_file_data_destroy(project->allocator, &source);
    return result;
}

AfResult af_project_reimport_images(AfProject *project, const char *project_path,
                                    uint32_t *out_missing_count) {
    uint32_t missing = 0U;
    if (project == NULL || project_path == NULL || project_path[0] == '\0') {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->image_data.asset_count; ++index) {
        AfImageAsset *asset = &project->image_data.assets[index];
        AfDecodedImage decoded = {0};
        char source_path[AF_IMAGE_PATH_CAPACITY * 2U];
        if (!af_project_asset_source_path(project_path, asset->source_path, source_path,
                                          sizeof(source_path)) ||
            af_image_decode_file(source_path, project->allocator, &decoded) != AF_RESULT_SUCCESS) {
            asset->missing = true;
            ++missing;
            continue;
        }
        if (decoded.width != asset->width || decoded.height != asset->height) {
            af_decoded_image_destroy(&decoded);
            asset->missing = true;
            ++missing;
            continue;
        }
        af_deallocate(project->allocator, asset->pixels);
        asset->pixels = decoded.pixels;
        decoded.pixels = NULL;
        asset->pixel_size = decoded.width * decoded.height * 4U;
        asset->content_hash = af_hash_bytes(asset->pixels, asset->pixel_size);
        asset->color_space = decoded.color_space;
        asset->source_format = decoded.format;
        asset->missing = false;
        (void)af_image_trim(asset);
        af_decoded_image_destroy(&decoded);
    }
    ++project->image_data.generation;
    if (out_missing_count != NULL) {
        *out_missing_count = missing;
    }
    return missing == 0U ? AF_RESULT_SUCCESS : AF_RESULT_NOT_FOUND;
}

AfResult af_project_repair_image_source(AfProject *project, const char *project_path,
                                        uint64_t asset_id, const char *replacement_path) {
    AfDecodedImage decoded = {0};
    AfFileData source = {0};
    AfImageAsset staged = {0};
    AfImageAsset *asset = NULL;
    char relative[AF_IMAGE_PATH_CAPACITY];
    uint64_t source_hash = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (project == NULL || project_path == NULL || project_path[0] == '\0' || asset_id == 0U ||
        replacement_path == NULL || replacement_path[0] == '\0') {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    asset = af_image_find(&project->image_data, asset_id);
    if (asset == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    result = af_file_read_limited(replacement_path, AF_MAX_IMAGE_SOURCE_SIZE, project->allocator,
                                  &source);
    if (result == AF_RESULT_SUCCESS) {
        source_hash = af_hash_bytes(source.data, source.size);
        result = af_image_decode_file(replacement_path, project->allocator, &decoded);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_copy_image_source(project_path, replacement_path, source_hash, relative,
                                              sizeof(relative));
    }
    if (result == AF_RESULT_SUCCESS) {
        staged = *asset;
        staged.width = decoded.width;
        staged.height = decoded.height;
        staged.trim_x = 0U;
        staged.trim_y = 0U;
        staged.trim_width = decoded.width;
        staged.trim_height = decoded.height;
        staged.atlas_index = 0U;
        staged.atlas_x = 0U;
        staged.atlas_y = 0U;
        staged.pixel_size = decoded.width * decoded.height * 4U;
        staged.pixels = decoded.pixels;
        staged.source_format = decoded.format;
        staged.color_space = decoded.color_space;
        staged.content_hash = af_hash_bytes(staged.pixels, staged.pixel_size);
        staged.missing = false;
        (void)snprintf(staged.source_path, sizeof(staged.source_path), "%s", relative);
        af_project_image_name(replacement_path, staged.name);
        result = af_image_trim(&staged);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_deallocate(project->allocator, asset->pixels);
        *asset = staged;
        decoded.pixels = NULL;
        ++project->image_data.generation;
    }
    af_decoded_image_destroy(&decoded);
    af_file_data_destroy(project->allocator, &source);
    return result;
}
