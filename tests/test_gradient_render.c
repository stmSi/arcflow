/* Native GPU regression: compare rendered pixels with the CPU paint evaluator.
 * Run with a Vulkan-capable SDL display; intentionally separate from headless CTest. */
#include "project/image_import.h"
#include "renderer/vk_renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK_GPU(condition)                                                                       \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Gradient render check failed: %d\n", __LINE__);                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)
static float linear(float c) {
    return c <= .04045F ? c / 12.92F : powf((c + .055F) / 1.055F, 2.4F);
}
static float srgb(float c) {
    return c <= .0031308F ? c * 12.92F : 1.055F * powf(c, 1 / 2.4F) - .055F;
}
int main(int argc, char **argv) {
    const char *capture = argc > 1 ? argv[1] : "/tmp/arcflow-gradient-render.png";
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfPlatformWindow window = {0};
    AfVkRenderer *renderer = NULL;
    AfDecodedImage image = {0};
    bool passed = false;
    AfSceneNode nodes[12] = {0};
    AfStyle styles[12];
    AfPath paths[3];
    AfPathCommand commands[15];
    AfPathData path_data = {.paths = paths,
                            .path_count = 3,
                            .path_capacity = 3,
                            .commands = commands,
                            .command_count = 15,
                            .command_capacity = 15};
    AfStyleData style_data = {.styles = styles, .count = 12, .capacity = 12};
    AfScene scene;
    af_scene_init(&scene, nodes, 12);
    scene.count = 12;
    scene.styles = &style_data;
    scene.paths = &path_data;
    for (uint32_t row = 0; row < 4; ++row)
        for (uint32_t col = 0; col < 3; ++col) {
            uint32_t i = row * 3 + col;
            nodes[i] = (AfSceneNode){
                .id = i + 1,
                .parent_id = AF_INVALID_NODE_ID,
                .type = (AfNodeType)(row + 1),
                .local_transform = {.position = {30 + (float)col * 235, 30 + (float)row * 130},
                                    .scale = {1, 1}},
                .bounds = {180, 100},
                .fill = {1, 0, 0, 1},
                .opacity = 1,
                .corner_radius = 16,
                .visible = true};
            styles[i] = af_style_default(i + 1, i + 1, nodes[i].fill);
            styles[i].fill = (AfPaint){.type = (AfPaintType)(col + 1),
                                       .solid = {1, 0, 0, 1},
                                       .start = {0, 0},
                                       .end = {180, 0},
                                       .end_radius = 90,
                                       .transform = {.m00 = 1, .m11 = 1},
                                       .stop_count = 3,
                                       .stops = {{.offset = 0, .color = {1, 0, 0, 1}},
                                                 {.offset = .37F, .color = {0, 1, 0, 1}},
                                                 {.offset = 1, .color = {0, 0, 1, 1}}}};
            if (col)
                styles[i].fill.start = (AfVec2){90, 50};
            if (row == 3) {
                nodes[i].type = AF_NODE_PATH;
                paths[col] = (AfPath){.id = i + 1,
                                      .node_id = i + 1,
                                      .geometry_generation = 1,
                                      .command_start = col * 5,
                                      .command_count = 5};
                commands[col * 5] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {0, 0}};
                commands[col * 5 + 1] = (AfPathCommand){.type = AF_PATH_LINE, .point = {180, 0}};
                commands[col * 5 + 2] = (AfPathCommand){.type = AF_PATH_LINE, .point = {180, 100}};
                commands[col * 5 + 3] = (AfPathCommand){.type = AF_PATH_LINE, .point = {0, 100}};
                commands[col * 5 + 4] = (AfPathCommand){.type = AF_PATH_CLOSE};
            }
        }
    CHECK_GPU(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK_GPU(af_platform_window_create("Gradient pixel regression", 800, 600, &window) ==
              AF_RESULT_SUCCESS);
    CHECK_GPU(af_vk_renderer_create(&window, &renderer) == AF_RESULT_SUCCESS);
    for (uint32_t phase = 0; phase < 3; ++phase) {
        if (phase)
            for (uint32_t i = 0; i < 12; ++i) {
                styles[i].fill.stops[1].color = (AfColor){1, 0, 1, 1};
                (void)af_style_mark_paint_changed(&styles[i]);
            }
        if (phase == 2) {
            for (uint32_t i = 0; i < 12; ++i) {
                nodes[i].local_transform.rotation = .1F;
                nodes[i].local_transform.scale = (AfVec2){.9F, .8F};
                nodes[i].opacity = .8F;
                styles[i].fill_opacity = .75F;
                styles[i].fill.transform = (AfAffine2){
                    .m00 = .8F, .m01 = .1F, .m10 = .05F, .m11 = 1.1F, .tx = 8, .ty = -2};
                styles[i].fill.stop_count = 16;
                for (uint32_t stop = 0; stop < 16; ++stop)
                    styles[i].fill.stops[stop] =
                        (AfGradientStop){.offset = (float)stop / 15,
                                         .color = {(float)(stop % 3) / 2, (float)(stop % 5) / 4,
                                                   (float)(stop % 7) / 6, .3F + (float)stop / 22}};
                styles[i].stroke.enabled = true;
                styles[i].stroke.width = styles[i].stroke.end_width = 12;
                styles[i].stroke_opacity = .65F;
                styles[i].stroke_paint = styles[i].fill;
                (void)af_style_mark_stroke_geometry_changed(&styles[i]);
                (void)af_style_mark_paint_changed(&styles[i]);
            }
            CHECK_GPU(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
        }
        for (uint32_t frame = 0; frame < 4; ++frame) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
            }
            if (frame == 3)
                CHECK_GPU(af_vk_renderer_capture_next(renderer, capture) == AF_RESULT_SUCCESS);
            CHECK_GPU(af_vk_renderer_draw(renderer, &scene) == AF_RESULT_SUCCESS);
            if (phase == 1 || frame > 0)
                CHECK_GPU(af_vk_renderer_frame_metrics(renderer).path_rebuilds == 0);
        }
        CHECK_GPU(!af_vk_renderer_has_validation_errors(renderer));
        CHECK_GPU(af_image_decode_file(capture, &allocator, &image) == AF_RESULT_SUCCESS);
        CHECK_GPU(image.width >= 700 && image.height >= 520);
        const AfVec2 samples[] = {{45, 35}, {66, 50}, {110, 65}, {145, 45}, {80, -3}};
        for (uint32_t i = 0; i < 12; ++i)
            for (uint32_t p = 0; p < (phase == 2 ? 5U : 4U); ++p) {
                AfColor expected;
                const AfVec2 world =
                    af_affine_transform_point(nodes[i].world_transform, samples[p]);
                uint32_t x = (uint32_t)world.x, y = (uint32_t)world.y;
                AfAffine2 inverse;
                CHECK_GPU(af_affine_inverse(nodes[i].world_transform, &inverse));
                const AfVec2 local =
                    af_affine_transform_point(inverse, (AfVec2){(float)x + .5F, (float)y + .5F});
                CHECK_GPU(
                    af_paint_sample(p == 4 ? &styles[i].stroke_paint : &styles[i].fill, local,
                                    nodes[i].world_opacity * (p == 4 ? styles[i].stroke_opacity
                                                                     : styles[i].fill_opacity),
                                    &expected) == AF_RESULT_SUCCESS);
                const uint8_t *pixel = image.pixels + ((size_t)y * image.width + x) * 4;
                const float channels[] = {expected.r, expected.g, expected.b};
                for (uint32_t c = 0; c < 3; ++c)
                    if (fabsf((float)pixel[c] -
                              srgb(linear(channels[c]) * expected.a +
                                   linear((float)image.pixels[c] / 255) * (1 - expected.a)) *
                                  255) > 5) {
                        fprintf(stderr,
                                "phase=%u object=%u sample=%u channel=%u pixel=%u expected=%.1f\n",
                                phase, i, p, c, pixel[c], (double)channels[c] * 255);
                        goto cleanup;
                    }
            }
        af_decoded_image_destroy(&image);
        CHECK_GPU(af_vk_renderer_frame_metrics(renderer).path_rebuilds == 0);
    }
    nodes[0].local_transform.scale = (AfVec2){0, 0};
    CHECK_GPU(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK_GPU(af_vk_renderer_draw(renderer, &scene) == AF_RESULT_SUCCESS);
    passed = true;
cleanup:
    af_decoded_image_destroy(&image);
    af_vk_renderer_destroy(renderer);
    af_platform_window_destroy(&window);
    if (passed)
        puts("Gradient pixels match CPU stops across shapes/paths and color edits; no geometry "
             "rebuilds.");
    return passed ? 0 : 1;
}
