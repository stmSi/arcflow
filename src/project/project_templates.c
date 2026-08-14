#include "project/project_templates.h"

#include <stdio.h>
#include <string.h>

static const AfProjectTemplateInfo af_project_templates[AF_PROJECT_TEMPLATE_COUNT] = {
    {.id = AF_PROJECT_TEMPLATE_BLANK,
     .slug = "blank",
     .name = "Blank Canvas",
     .description = "Empty 1280 x 720 scene with a Main Clip",
     .feature_label = "START CLEAN",
     .features = 0U},
    {.id = AF_PROJECT_TEMPLATE_GUIDED_BASICS,
     .slug = "guided-basics",
     .name = "Guided Basics",
     .description = "Shapes and a ready-to-preview position animation",
     .feature_label = "CREATE + ANIMATE",
     .features = AF_PROJECT_TEMPLATE_FEATURE_ANIMATION},
    {.id = AF_PROJECT_TEMPLATE_VECTOR_ANIMATION,
     .slug = "vector-animation",
     .name = "Vector Animation",
     .description = "Animated anchor and cubic Bezier handles",
     .feature_label = "POINT TRACKS",
     .features = AF_PROJECT_TEMPLATE_FEATURE_ANIMATION | AF_PROJECT_TEMPLATE_FEATURE_VECTOR},
    {.id = AF_PROJECT_TEMPLATE_INTERACTIVE_BUTTON,
     .slug = "interactive-button",
     .name = "Interactive Button",
     .description = "Button hierarchy with press feedback animation",
     .feature_label = "INPUT + EVENTS",
     .features = AF_PROJECT_TEMPLATE_FEATURE_ANIMATION | AF_PROJECT_TEMPLATE_FEATURE_INTERACTION},
    {.id = AF_PROJECT_TEMPLATE_RESPONSIVE_UI,
     .slug = "responsive-ui",
     .name = "Responsive UI",
     .description = "Safe-area layout with anchored percent sizing",
     .feature_label = "LAYOUT",
     .features = AF_PROJECT_TEMPLATE_FEATURE_LAYOUT},
    {.id = AF_PROJECT_TEMPLATE_PARTICLE_EFFECT,
     .slug = "particle-effect",
     .name = "Particle Effect",
     .description = "Trail emitter with turbulence, vortex, and LOD",
     .feature_label = "VFX",
     .features = AF_PROJECT_TEMPLATE_FEATURE_PARTICLES}};

static AfSceneNode af_project_template_node(uint64_t id, uint64_t parent_id, AfNodeType type,
                                            AfVec2 position, AfVec2 bounds, AfColor color,
                                            int32_t layer) {
    return (AfSceneNode){.id = id,
                         .parent_id = parent_id,
                         .type = type,
                         .local_transform = {.position = position, .scale = {1.0F, 1.0F}},
                         .bounds = bounds,
                         .fill = color,
                         .opacity = 1.0F,
                         .corner_radius = type == AF_NODE_ROUNDED_RECTANGLE ? 24.0F : 0.0F,
                         .layer_order = layer,
                         .visible = true};
}

static AfResult af_project_template_clip(AfProject *project, uint64_t id, const char *name,
                                         float duration) {
    const AfProjectClip clip = {
        .id = id, .duration = duration, .loop_start = 0.0F, .loop_end = duration, .loop = true};
    AfProjectClip named = clip;
    (void)snprintf(named.name, sizeof(named.name), "%s", name);
    return af_project_add_clip(project, &named);
}

static AfResult af_project_template_track(AfProject *project, uint64_t track_id, uint64_t clip_id,
                                          uint64_t node_id, AfPropertyType property,
                                          uint32_t element_index, uint64_t first_key_id,
                                          uint64_t second_key_id, float duration,
                                          const float start[4], const float end[4]) {
    const AfProjectTrack track = {.id = track_id,
                                  .clip_id = clip_id,
                                  .node_id = node_id,
                                  .property = property,
                                  .element_index = element_index};
    AfProjectKeyframe keys[2] = {{.id = first_key_id,
                                  .track_id = track_id,
                                  .keyframe = {.time = 0.0F,
                                               .interpolation = AF_INTERPOLATION_CUBIC_BEZIER,
                                               .easing = {0.42F, 0.0F, 0.58F, 1.0F}}},
                                 {.id = second_key_id,
                                  .track_id = track_id,
                                  .keyframe = {.time = duration,
                                               .interpolation = AF_INTERPOLATION_CUBIC_BEZIER,
                                               .easing = {0.42F, 0.0F, 0.58F, 1.0F}}}};
    AfResult result = af_project_add_track(project, &track);
    memcpy(keys[0].keyframe.value, start, sizeof(keys[0].keyframe.value));
    memcpy(keys[1].keyframe.value, end, sizeof(keys[1].keyframe.value));
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_keyframe(project, &keys[0]);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_keyframe(project, &keys[1]);
    }
    return result;
}

static AfResult af_project_template_blank(AfProject *project) {
    return af_project_template_clip(project, 100U, "Main Clip", 4.0F);
}

static AfResult af_project_template_guided(AfProject *project) {
    const AfSceneNode root =
        af_project_template_node(1U, AF_INVALID_NODE_ID, AF_NODE_GROUP, (AfVec2){0.0F, 0.0F},
                                 (AfVec2){1280.0F, 720.0F}, (AfColor){0.0F, 0.0F, 0.0F, 0.0F}, 0);
    const AfSceneNode card =
        af_project_template_node(2U, 1U, AF_NODE_ROUNDED_RECTANGLE, (AfVec2){220.0F, 230.0F},
                                 (AfVec2){300.0F, 180.0F}, (AfColor){0.16F, 0.48F, 0.92F, 1.0F}, 1);
    const AfSceneNode accent =
        af_project_template_node(3U, 1U, AF_NODE_ELLIPSE, (AfVec2){650.0F, 270.0F},
                                 (AfVec2){120.0F, 120.0F}, (AfColor){0.34F, 0.9F, 0.62F, 1.0F}, 2);
    const float start[4] = {220.0F, 230.0F, 0.0F, 0.0F};
    const float end[4] = {520.0F, 230.0F, 0.0F, 0.0F};
    AfResult result = af_project_add_node(project, &root, "Scene");
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(project, &card, "Animated Card");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(project, &accent, "Accent");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_clip(project, 100U, "Main Clip", 4.0F);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_track(project, 101U, 100U, 2U, AF_PROPERTY_POSITION, 0U, 102U,
                                           103U, 4.0F, start, end);
    }
    return result;
}

static AfResult af_project_template_vector(AfProject *project) {
    static const AfPathCommand commands[] = {{.type = AF_PATH_MOVE, .point = {0.0F, 180.0F}},
                                             {.type = AF_PATH_CUBIC,
                                              .point = {240.0F, 40.0F},
                                              .control1 = {45.0F, 20.0F},
                                              .control2 = {170.0F, 170.0F}},
                                             {.type = AF_PATH_CUBIC,
                                              .point = {480.0F, 180.0F},
                                              .control1 = {310.0F, -80.0F},
                                              .control2 = {430.0F, 20.0F}},
                                             {.type = AF_PATH_LINE, .point = {240.0F, 360.0F}},
                                             {.type = AF_PATH_CLOSE}};
    const AfSceneNode root =
        af_project_template_node(1U, AF_INVALID_NODE_ID, AF_NODE_GROUP, (AfVec2){0.0F, 0.0F},
                                 (AfVec2){1280.0F, 720.0F}, (AfColor){0.0F, 0.0F, 0.0F, 0.0F}, 0);
    const AfSceneNode node =
        af_project_template_node(2U, 1U, AF_NODE_PATH, (AfVec2){360.0F, 160.0F},
                                 (AfVec2){480.0F, 360.0F}, (AfColor){0.69F, 0.35F, 0.96F, 1.0F}, 1);
    const AfPath path = {.id = 500U,
                         .node_id = 2U,
                         .command_count = 5U,
                         .fill_rule = AF_PATH_FILL_EVEN_ODD,
                         .geometry_generation = 1U};
    const float point_start[4] = {240.0F, 40.0F, 0.0F, 0.0F};
    const float point_end[4] = {250.0F, 200.0F, 0.0F, 0.0F};
    const float in_start[4] = {170.0F, 170.0F, 0.0F, 0.0F};
    const float in_end[4] = {120.0F, 260.0F, 0.0F, 0.0F};
    const float out_start[4] = {45.0F, 20.0F, 0.0F, 0.0F};
    const float out_end[4] = {80.0F, -70.0F, 0.0F, 0.0F};
    AfResult result = af_project_add_node(project, &root, "Scene");
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(project, &node, "Animated Vector");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_path(project, &path, commands);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_clip(project, 100U, "Vector Motion", 4.0F);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_track(project, 101U, 100U, 2U, AF_PROPERTY_PATH_POINT, 1U,
                                           102U, 103U, 4.0F, point_start, point_end);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_track(project, 104U, 100U, 2U, AF_PROPERTY_PATH_CONTROL_IN, 1U,
                                           105U, 106U, 4.0F, in_start, in_end);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_track(project, 107U, 100U, 2U, AF_PROPERTY_PATH_CONTROL_OUT,
                                           1U, 108U, 109U, 4.0F, out_start, out_end);
    }
    return result;
}

static AfResult af_project_template_button(AfProject *project) {
    const AfSceneNode root =
        af_project_template_node(1U, AF_INVALID_NODE_ID, AF_NODE_GROUP, (AfVec2){390.0F, 250.0F},
                                 (AfVec2){500.0F, 220.0F}, (AfColor){0.0F, 0.0F, 0.0F, 0.0F}, 0);
    AfSceneNode focus =
        af_project_template_node(2U, 1U, AF_NODE_ROUNDED_RECTANGLE, (AfVec2){-8.0F, -8.0F},
                                 (AfVec2){436.0F, 156.0F}, (AfColor){0.34F, 0.78F, 1.0F, 0.45F}, 1);
    AfSceneNode button =
        af_project_template_node(3U, 1U, AF_NODE_ROUNDED_RECTANGLE, (AfVec2){0.0F, 0.0F},
                                 (AfVec2){420.0F, 140.0F}, (AfColor){0.12F, 0.42F, 0.82F, 1.0F}, 2);
    const AfSceneNode icon =
        af_project_template_node(4U, 1U, AF_NODE_ELLIPSE, (AfVec2){42.0F, 38.0F},
                                 (AfVec2){64.0F, 64.0F}, (AfColor){0.78F, 0.94F, 1.0F, 1.0F}, 3);
    const float scale_start[4] = {1.0F, 1.0F, 0.0F, 0.0F};
    const float scale_end[4] = {0.94F, 0.94F, 0.0F, 0.0F};
    const float color_start[4] = {0.12F, 0.42F, 0.82F, 1.0F};
    const float color_end[4] = {0.08F, 0.28F, 0.62F, 1.0F};
    AfResult result = AF_RESULT_SUCCESS;
    focus.corner_radius = 34.0F;
    button.corner_radius = 28.0F;
    result = af_project_add_node(project, &root, "Button Group");
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(project, &focus, "Focus Ring");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(project, &button, "Button Background");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(project, &icon, "Button Icon");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_clip(project, 100U, "Press Feedback", 0.18F);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_track(project, 101U, 100U, 1U, AF_PROPERTY_SCALE, 0U, 102U,
                                           103U, 0.18F, scale_start, scale_end);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_track(project, 104U, 100U, 3U, AF_PROPERTY_FILL_COLOR, 0U,
                                           105U, 106U, 0.18F, color_start, color_end);
    }
    return result;
}

static AfResult af_project_template_responsive(AfProject *project) {
    const AfSceneNode root =
        af_project_template_node(1U, AF_INVALID_NODE_ID, AF_NODE_GROUP, (AfVec2){0.0F, 0.0F},
                                 (AfVec2){1280.0F, 720.0F}, (AfColor){0.0F, 0.0F, 0.0F, 0.0F}, 0);
    const AfSceneNode header =
        af_project_template_node(2U, 1U, AF_NODE_ROUNDED_RECTANGLE, (AfVec2){0.0F, 0.0F},
                                 (AfVec2){1184.0F, 112.0F}, (AfColor){0.12F, 0.2F, 0.34F, 1.0F}, 1);
    const AfSceneNode content =
        af_project_template_node(3U, 1U, AF_NODE_ROUNDED_RECTANGLE, (AfVec2){0.0F, 132.0F},
                                 (AfVec2){760.0F, 460.0F}, (AfColor){0.18F, 0.48F, 0.78F, 1.0F}, 2);
    const AfSceneNode action =
        af_project_template_node(4U, 1U, AF_NODE_ROUNDED_RECTANGLE, (AfVec2){800.0F, 132.0F},
                                 (AfVec2){384.0F, 200.0F}, (AfColor){0.3F, 0.75F, 0.58F, 1.0F}, 3);
    AfLayoutNode root_layout = af_layout_node_default(1U);
    AfLayoutNode header_layout = af_layout_node_default(2U);
    AfLayoutNode content_layout = af_layout_node_default(3U);
    AfLayoutNode action_layout = af_layout_node_default(4U);
    AfResult result = AF_RESULT_SUCCESS;
    project->canvas_size = (AfVec2){1280.0F, 720.0F};
    project->layout_data.reference_size = project->canvas_size;
    project->layout_data.target_size = project->canvas_size;
    project->layout_data.safe_area =
        (AfInsets){.left = 48.0F, .top = 36.0F, .right = 48.0F, .bottom = 36.0F};
    project->layout_data.scale_mode = AF_CANVAS_SCALE_FIT;
    root_layout.width = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_PERCENT, .value = 1.0F};
    root_layout.height = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_PERCENT, .value = 1.0F};
    root_layout.direction = AF_LAYOUT_DIRECTION_VERTICAL;
    root_layout.spacing = 20.0F;
    root_layout.padding = (AfInsets){.left = 16.0F, .top = 12.0F, .right = 16.0F, .bottom = 12.0F};
    header_layout.width = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_PERCENT, .value = 1.0F};
    header_layout.height = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_FIXED, .value = 112.0F};
    content_layout.anchor_min = (AfVec2){0.0F, 0.0F};
    content_layout.anchor_max = (AfVec2){0.0F, 0.0F};
    content_layout.width = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_PERCENT, .value = 0.64F};
    content_layout.height = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_PERCENT, .value = 0.68F};
    content_layout.minimum_size = (AfVec2){320.0F, 240.0F};
    action_layout.anchor_min = (AfVec2){1.0F, 0.0F};
    action_layout.anchor_max = action_layout.anchor_min;
    action_layout.pivot = (AfVec2){1.0F, 0.0F};
    action_layout.offset = (AfVec2){-16.0F, 144.0F};
    action_layout.width = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_PERCENT, .value = 0.28F};
    action_layout.height = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_FIXED, .value = 200.0F};
    action_layout.minimum_size = (AfVec2){220.0F, 120.0F};
    result = af_project_add_node(project, &root, "Safe Area Root");
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(project, &header, "Responsive Header");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(project, &content, "Percent Content");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(project, &action, "Anchored Action");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_set_layout(project, &root_layout);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_set_layout(project, &header_layout);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_set_layout(project, &content_layout);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_set_layout(project, &action_layout);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_clip(project, 100U, "Main Clip", 4.0F);
    }
    return result;
}

static AfResult af_project_template_particles(AfProject *project) {
    const AfSceneNode emitter_node =
        af_project_template_node(1U, AF_INVALID_NODE_ID, AF_NODE_ELLIPSE, (AfVec2){560.0F, 320.0F},
                                 (AfVec2){96.0F, 96.0F}, (AfColor){0.34F, 0.88F, 0.76F, 1.0F}, 1);
    AfParticleEmitter emitter = af_particle_emitter_default(100U, 1U);
    AfResult result = af_project_add_node(project, &emitter_node, "Trail Emitter");
    emitter.type = AF_PARTICLE_EMITTER_CIRCLE;
    emitter.output = AF_PARTICLE_OUTPUT_TRAIL;
    emitter.capacity = 4096U;
    emitter.spawn_rate = 96.0F;
    emitter.turbulence_strength = 145.0F;
    emitter.vortex_force = 82.0F;
    emitter.trail_length = 58.0F;
    emitter.trail_width = 7.0F;
    project->particle_data.lod = af_vfx_lod_default();
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_emitter(project, &emitter);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_template_clip(project, 200U, "Particle Preview", 4.0F);
    }
    return result;
}

uint32_t af_project_template_count(void) { return AF_PROJECT_TEMPLATE_COUNT; }

const AfProjectTemplateInfo *af_project_template_info(AfProjectTemplateId id) {
    return (uint32_t)id < AF_PROJECT_TEMPLATE_COUNT ? &af_project_templates[id] : NULL;
}

const AfProjectTemplateInfo *af_project_template_find(const char *slug) {
    if (slug == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < AF_PROJECT_TEMPLATE_COUNT; ++index) {
        if (strcmp(slug, af_project_templates[index].slug) == 0) {
            return &af_project_templates[index];
        }
    }
    return NULL;
}

AfResult af_project_template_build(AfProjectTemplateId id, AfAllocator *allocator,
                                   uint32_t capacity, AfProject *out_project) {
    AfProject project = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if ((uint32_t)id >= AF_PROJECT_TEMPLATE_COUNT || allocator == NULL || out_project == NULL ||
        capacity < 4U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_project = (AfProject){0};
    result = af_project_create(&project, allocator, capacity);
    if (result == AF_RESULT_SUCCESS) {
        (void)snprintf(project.metadata.project_id, sizeof(project.metadata.project_id),
                       "arcflow-%s", af_project_templates[id].slug);
        switch (id) {
        case AF_PROJECT_TEMPLATE_BLANK:
            result = af_project_template_blank(&project);
            break;
        case AF_PROJECT_TEMPLATE_GUIDED_BASICS:
            result = af_project_template_guided(&project);
            break;
        case AF_PROJECT_TEMPLATE_VECTOR_ANIMATION:
            result = af_project_template_vector(&project);
            break;
        case AF_PROJECT_TEMPLATE_INTERACTIVE_BUTTON:
            result = af_project_template_button(&project);
            break;
        case AF_PROJECT_TEMPLATE_RESPONSIVE_UI:
            result = af_project_template_responsive(&project);
            break;
        case AF_PROJECT_TEMPLATE_PARTICLE_EFFECT:
            result = af_project_template_particles(&project);
            break;
        case AF_PROJECT_TEMPLATE_COUNT:
            result = AF_RESULT_INVALID_ARGUMENT;
            break;
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_scene_evaluate_world(&project.scene);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_validate(&project);
    }
    if (result == AF_RESULT_SUCCESS) {
        *out_project = project;
        project = (AfProject){0};
    }
    af_project_destroy(&project);
    return result;
}
