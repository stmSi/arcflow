#include "animation/animation.h"
#include "layout/layout.h"
#include "project/project.h"
#include "renderer/render_items.h"

#include <SDL3/SDL_timer.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fixed CPU workloads. Each report is the median of seven batches of eight evaluations.
 * Setup and allocation are outside the timed region; timings are informational. */
static AfSceneNode nodes[2048];
static AfLayoutNode layouts[512];
static AfProjectTrack tracks[256];
static AfProjectKeyframe keys[4096];
static AfKeyframe runtime_keys[4096];
static AfVkRenderItem render_source[4096];
static AfVkRenderItem render_items[4096];
static AfVkRenderItem render_scratch[4096];
static uint32_t render_batches;
static AfScene scene;
static AfProject project;
static AfProjectClip clip = {.id = 1, .duration = 16};
static AfAnimationTrack track = {.keyframe_count = 4096};
static AfAnimationData animation = {.keyframes = runtime_keys, .keyframe_count = 4096};
static volatile float sink;

static double af_hotpath_now(void) { return (double)SDL_GetTicksNS(); }

static void af_hotpath_run(int kind) {
    if (kind < 2) {
        if (af_scene_evaluate_world(&scene) != AF_RESULT_SUCCESS) {
            abort();
        }
        sink = nodes[0].world_transform.tx;
    } else if (kind == 2) {
        if (af_project_animation_apply(&project, 1, 8.5f, &scene) != AF_RESULT_SUCCESS) {
            abort();
        }
        sink = nodes[0].world_transform.tx;
    } else if (kind == 3) {
        float value[4];
        for (int i = 0; i < 256; i++) {
            if (af_animation_sample_track(&animation, &track, 2048.5f, value) !=
                AF_RESULT_SUCCESS) {
                abort();
            }
        }
        sink = value[0];
    } else {
        memcpy(render_items, render_source, sizeof(render_items));
        render_batches = af_vk_prepare_render_items(render_items, render_scratch, 4096U);
        sink = (float)render_batches;
    }
}

int main(void) {
    AfLayoutData layout = {.nodes = layouts,
                           .count = 512,
                           .capacity = 512,
                           .reference_size = {1000, 1000},
                           .target_size = {1000, 1000}};
    static const char *const names[] = {
        "scene_2048",       "layout_512",        "editor_animation_256x16",
        "runtime_256x4096", "render_mixed_4096", "render_ordered_4096"};
    for (int kind = 0; kind < 6; kind++) {
        unsigned count = kind == 1 ? 512 : kind == 2 ? 256 : 2048;
        for (unsigned i = 0; i < count; i++) {
            nodes[i] = (AfSceneNode){.id = (uint64_t)i + 1,
                                     .parent_id = i + 1 == count ? AF_INVALID_NODE_ID : count,
                                     .local_transform = {.scale = {1, 1}},
                                     .bounds = {1, 2},
                                     .opacity = 1,
                                     .visible = true};
        }
        scene = (AfScene){.nodes = nodes, .count = count, .capacity = count};
        if (kind == 1) {
            for (unsigned i = 0; i < 512; i++) {
                layouts[i] = af_layout_node_default(i + 1);
            }
            layouts[511].direction = AF_LAYOUT_DIRECTION_HORIZONTAL;
            layouts[511].spacing = 0.5f;
            scene.layouts = &layout;
        }
        if (kind == 2) {
            for (unsigned i = 0; i < 256; i++) {
                tracks[i] = (AfProjectTrack){.id = (uint64_t)i + 2,
                                             .clip_id = 1,
                                             .node_id = (uint64_t)i + 1,
                                             .property = AF_PROPERTY_POSITION};
                for (unsigned k = 0; k < 16; k++) {
                    keys[k * 256 + i] =
                        (AfProjectKeyframe){.id = (uint64_t)k * 256 + i + 300,
                                            .track_id = (uint64_t)i + 2,
                                            .keyframe = {.time = (float)k, .value = {(float)k, 1}}};
                }
            }
            project = (AfProject){.clips = &clip,
                                  .clip_count = 1,
                                  .tracks = tracks,
                                  .track_count = 256,
                                  .keyframes = keys,
                                  .keyframe_count = 4096};
        }
        for (unsigned i = 0; i < 4096; i++) {
            runtime_keys[i] = (AfKeyframe){.time = (float)i, .value = {(float)i}};
        }
        for (uint32_t i = 0; i < 4096U; i++) {
            render_source[i] = (AfVkRenderItem){.type = AF_VK_RENDER_ANALYTIC,
                                                .layer_order = kind == 4 ? (int32_t)(i * 73U % 31U)
                                                                         : (int32_t)(i / 256U),
                                                .first = i,
                                                .count = 1U};
        }
        double samples[7];
        af_hotpath_run(kind);
        for (int s = 0; s < 7; s++) {
            double t = af_hotpath_now();
            for (int j = 0; j < 8; j++) {
                af_hotpath_run(kind);
            }
            samples[s] = (af_hotpath_now() - t) / 8;
        }
        for (int i = 1; i < 7; i++) {
            for (int j = i; j > 0 && samples[j] < samples[j - 1]; j--) {
                double t = samples[j];
                samples[j] = samples[j - 1];
                samples[j - 1] = t;
            }
        }
        printf("{\"workload\":\"%s\",\"median_ns\":%.0f,\"batches\":%u}\n", names[kind], samples[3],
               kind >= 4 ? render_batches : 0U);
    }
    return sink < 0;
}
