#include "player/player_app.h"

#include "arcflow/arcflow_runtime.h"
#include "assets/package.h"
#include "core/allocator.h"
#include "core/file_io.h"
#include "core/log.h"
#include "particle/particle.h"
#include "platform/platform_sdl.h"
#include "renderer/vk_renderer.h"
#include "runtime/runtime_internal.h"

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AF_PLAYER_CAPTURE_PATH_CAPACITY 1024U

int af_player_run(const char *title, int argument_count, char **arguments) {
    const char *package_path = ARCFLOW_DEMO_PACKAGE;
    AfAllocator allocator = {0};
    AfFileData file = {0};
    AfPlatformWindow window = {0};
    AfVkRenderer *renderer = NULL;
    AfRuntime *runtime = NULL;
    AfPackageHandle package = 0U;
    AfInstanceHandle instance = 0U;
    const AfScene *scene = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    bool running = true;
    uint64_t previous_ticks = 0U;
    uint32_t maximum_frames = 0U;
    uint32_t rendered_frames = 0U;
    bool resize_smoke = false;
    const char *output_path = NULL;
    const char *sequence_prefix = NULL;
    char sequence_path[AF_PLAYER_CAPTURE_PATH_CAPACITY];
    char default_package_path[AF_PLAYER_CAPTURE_PATH_CAPACITY];
    int argument_index = 1;
    int exit_code = 1;
    for (argument_index = 1; argument_index < argument_count; ++argument_index) {
        if (strcmp(arguments[argument_index], "--frames") == 0 &&
            argument_index + 1 < argument_count) {
            maximum_frames = (uint32_t)strtoul(arguments[++argument_index], NULL, 10);
        } else if (strcmp(arguments[argument_index], "--resize-smoke") == 0) {
            resize_smoke = true;
        } else if (strcmp(arguments[argument_index], "--output") == 0 &&
                   argument_index + 1 < argument_count) {
            output_path = arguments[++argument_index];
        } else if (strcmp(arguments[argument_index], "--sequence") == 0 &&
                   argument_index + 1 < argument_count) {
            sequence_prefix = arguments[++argument_index];
        } else {
            package_path = arguments[argument_index];
        }
    }
    if (output_path != NULL && sequence_prefix != NULL) {
        af_log(AF_LOG_ERROR, "Use either --output or --sequence, not both");
        goto cleanup;
    }
    if (output_path != NULL && maximum_frames == 0U) {
        maximum_frames = 1U;
    } else if (sequence_prefix != NULL && maximum_frames == 0U) {
        maximum_frames = 60U;
    }
    af_allocator_init_default(&allocator);
    result = af_file_read_limited(package_path, AF_PACKAGE_MAX_SIZE, &allocator, &file);
    if (result != AF_RESULT_SUCCESS && strcmp(package_path, ARCFLOW_DEMO_PACKAGE) == 0 &&
        af_platform_resource_path("assets/arcflow_demo.afp", default_package_path,
                                  sizeof(default_package_path)) == AF_RESULT_SUCCESS) {
        package_path = default_package_path;
        result = af_file_read_limited(package_path, AF_PACKAGE_MAX_SIZE, &allocator, &file);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_log(AF_LOG_ERROR, "Cannot read package '%s': %s", package_path,
               af_result_string(result));
        goto cleanup;
    }
    result = af_package_validate(file.data, file.size);
    if (result != AF_RESULT_SUCCESS) {
        af_log(AF_LOG_ERROR, "Package validation failed for '%s': %s", package_path,
               af_result_string(result));
        goto cleanup;
    }
    result = af_platform_window_create(title, 1280, 720, &window);
    if (result != AF_RESULT_SUCCESS) {
        goto cleanup;
    }
    result = af_vk_renderer_create(&window, &renderer);
    if (result != AF_RESULT_SUCCESS) {
        af_log(AF_LOG_ERROR, "Vulkan renderer initialization failed: %s", af_result_string(result));
        goto cleanup;
    }
    {
        const AfRuntimeCreateInfo create_info = af_vk_renderer_runtime_create_info(renderer);
        result = af_runtime_create(&create_info, &runtime);
    }
    if (result != AF_RESULT_SUCCESS ||
        (result = af_package_load(runtime, file.data, file.size, &package)) != AF_RESULT_SUCCESS ||
        (result = af_instance_create(runtime, package, &instance)) != AF_RESULT_SUCCESS ||
        (result = af_instance_play(runtime, instance)) != AF_RESULT_SUCCESS) {
        af_log(AF_LOG_ERROR, "Arcflow Runtime initialization failed: %s", af_result_string(result));
        goto cleanup;
    }
    previous_ticks = SDL_GetTicksNS();
    while (running) {
        SDL_Event event;
        uint64_t current_ticks = 0U;
        float delta_time = 0.0F;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
                       event.type == SDL_EVENT_WINDOW_RESIZED ||
                       event.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED ||
                       event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
                int width = 0;
                int height = 0;
                af_vk_renderer_request_resize(renderer);
                AfPlatformWindowMetrics metrics = {0};
                if (af_platform_window_metrics(&window, &metrics) == AF_RESULT_SUCCESS &&
                    metrics.pixel_width > 0 && metrics.pixel_height > 0) {
                    width = metrics.pixel_width;
                    height = metrics.pixel_height;
                    const AfCanvasInfo canvas = {.width = (float)width, .height = (float)height};
                    (void)af_instance_resize(runtime, instance, &canvas);
                }
            }
        }
        if (!running) {
            break;
        }
        current_ticks = SDL_GetTicksNS();
        delta_time = output_path != NULL || sequence_prefix != NULL
                         ? AF_PARTICLE_FIXED_STEP
                         : (float)((double)(current_ticks - previous_ticks) / 1000000000.0);
        previous_ticks = current_ticks;
        if (delta_time > 0.1F) {
            delta_time = 0.1F;
        }
        result = af_instance_update(runtime, instance, delta_time);
        if (result == AF_RESULT_SUCCESS) {
            result = af_instance_get_scene(runtime, instance, &scene);
        }
        if (result == AF_RESULT_SUCCESS && sequence_prefix != NULL) {
            const int length = snprintf(sequence_path, sizeof(sequence_path), "%s-%06u.png",
                                        sequence_prefix, rendered_frames + 1U);
            result = length < 0 || (size_t)length >= sizeof(sequence_path)
                         ? AF_RESULT_LIMIT_EXCEEDED
                         : af_vk_renderer_capture_next(renderer, sequence_path);
        } else if (result == AF_RESULT_SUCCESS && output_path != NULL &&
                   rendered_frames + 1U == maximum_frames) {
            result = af_vk_renderer_capture_next(renderer, output_path);
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_vk_renderer_draw_timed(renderer, scene, delta_time);
        }
        if (result != AF_RESULT_SUCCESS) {
            af_log(AF_LOG_ERROR, "Playback failed: %s", af_result_string(result));
            goto cleanup;
        }
        ++rendered_frames;
        if (resize_smoke && rendered_frames == 2U) {
            (void)SDL_SetWindowSize(window.window, 960, 540);
            af_vk_renderer_request_resize(renderer);
        }
        if (maximum_frames != 0U && rendered_frames >= maximum_frames) {
            running = false;
        }
    }
    if (af_vk_renderer_has_validation_errors(renderer)) {
        af_log(AF_LOG_ERROR, "Vulkan validation reported errors");
        goto cleanup;
    }
    exit_code = 0;

cleanup:
    if (runtime != NULL && instance != 0U) {
        (void)af_instance_destroy(runtime, instance);
    }
    if (runtime != NULL && package != 0U) {
        (void)af_package_destroy(runtime, package);
    }
    af_runtime_destroy(runtime);
    af_vk_renderer_destroy(renderer);
    af_platform_window_destroy(&window);
    af_file_data_destroy(&allocator, &file);
    return exit_code;
}
