static bool af_test_components(void) {
    AfTestFixture fixture;
    AfComponentDefinition definitions[4] = {{.id = 100U,
                                             .root_node_id = 1U,
                                             .clip_id = 10U,
                                             .reference_start = 0U,
                                             .reference_count = 1U,
                                             .property_start = 0U,
                                             .property_count = 5U},
                                            {.id = 200U,
                                             .root_node_id = 2U,
                                             .clip_id = 10U,
                                             .reference_start = 1U,
                                             .property_start = 5U}};
    AfComponentReference references[8] = {{.owner_definition_id = 100U, .component_id = 200U}};
    AfComponentProperty properties[16] = {{.definition_id = 100U,
                                           .id = 1U,
                                           .type = AF_COMPONENT_PROPERTY_NUMBER,
                                           .default_value.number = 2.0F},
                                          {.definition_id = 100U,
                                           .id = 2U,
                                           .type = AF_COMPONENT_PROPERTY_BOOLEAN,
                                           .default_value.boolean = true},
                                          {.definition_id = 100U,
                                           .id = 3U,
                                           .type = AF_COMPONENT_PROPERTY_COLOR,
                                           .default_value.color = {0.1F, 0.2F, 0.3F, 1.0F}},
                                          {.definition_id = 100U,
                                           .id = 4U,
                                           .type = AF_COMPONENT_PROPERTY_IMAGE,
                                           .default_value.image_id = 55U},
                                          {.definition_id = 100U,
                                           .id = 5U,
                                           .type = AF_COMPONENT_PROPERTY_TEXT,
                                           .default_value.text = "label"}};
    AfComponentInstance instances[2] = {{.id = 300U,
                                         .definition_id = 100U,
                                         .node_id = 2U,
                                         .override_start = 0U,
                                         .override_count = 2U,
                                         .playback_speed = 1.0F,
                                         .independent_playback = true},
                                        {.id = 301U,
                                         .definition_id = 100U,
                                         .node_id = 3U,
                                         .override_start = 2U,
                                         .playback_time = 0.25F,
                                         .playback_speed = 1.0F}};
    AfComponentOverride overrides[4] = {{.instance_id = 300U,
                                         .property_id = 1U,
                                         .type = AF_COMPONENT_PROPERTY_NUMBER,
                                         .value.number = 7.0F},
                                        {.instance_id = 300U,
                                         .property_id = 5U,
                                         .type = AF_COMPONENT_PROPERTY_TEXT,
                                         .value.text = "overridden"}};
    AfComponentData data = {.definitions = definitions,
                            .definition_count = 2U,
                            .definition_capacity = 4U,
                            .references = references,
                            .reference_count = 1U,
                            .reference_capacity = 8U,
                            .properties = properties,
                            .property_count = 5U,
                            .property_capacity = 16U,
                            .instances = instances,
                            .instance_count = 2U,
                            .instance_capacity = 2U,
                            .overrides = overrides,
                            .override_count = 2U,
                            .override_capacity = 4U};
    AfComponentInstanceState states[2];
    AfComponentRuntime runtime;
    AfComponentValue value;
    AfDetachedComponent detached;
    AfComponentEditSession edit_session;
    AfAllocator allocator;
    AfPackageBlob blob = {0};
    AfLoadedPackage loaded = {0};
    AfPackageSource source;
    af_test_fixture_init(&fixture);
    af_allocator_init_default(&allocator);
    CHECK(af_component_validate(&data, &fixture.scene, &fixture.animation) == AF_RESULT_SUCCESS);
    CHECK(af_component_runtime_init(&runtime, &data, &fixture.animation, states, 2U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_component_runtime_update(&runtime, 0.5F) == AF_RESULT_SUCCESS);
    CHECK_NEAR(states[0].playback_time, 0.5F, 0.0001F);
    CHECK_NEAR(states[1].playback_time, 0.25F, 0.0001F);
    CHECK(af_component_runtime_set_playing(&runtime, 300U, false) == AF_RESULT_SUCCESS);
    CHECK(af_component_runtime_update(&runtime, 1.0F) == AF_RESULT_SUCCESS);
    CHECK_NEAR(states[0].playback_time, 0.5F, 0.0001F);
    CHECK(af_component_resolve_property(&data, 300U, 1U, AF_COMPONENT_PROPERTY_NUMBER, &value) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(value.number, 7.0F, 0.0001F);
    CHECK(af_component_resolve_property(&data, 301U, 5U, AF_COMPONENT_PROPERTY_TEXT, &value) ==
          AF_RESULT_SUCCESS);
    CHECK(strcmp(value.text, "label") == 0);
    CHECK(af_component_detach(&data, 300U, &detached) == AF_RESULT_SUCCESS);
    CHECK(detached.root_node_id == 1U && detached.override_count == 2U);
    CHECK(af_component_edit_begin(&data, 100U, &edit_session) == AF_RESULT_SUCCESS);
    CHECK(edit_session.active && edit_session.definition_id == 100U);
    af_component_edit_end(&edit_session);
    CHECK(!edit_session.active);
    CHECK(af_component_duplicate_definition(&data, 100U, 201U) == AF_RESULT_SUCCESS);
    CHECK(data.definition_count == 3U && data.property_count == 10U && data.reference_count == 2U);
    data.definition_count = 2U;
    data.property_count = 5U;
    data.reference_count = 2U;
    definitions[1].reference_count = 1U;
    references[1] = (AfComponentReference){.owner_definition_id = 200U, .component_id = 100U};
    CHECK(af_component_validate(&data, &fixture.scene, &fixture.animation) ==
          AF_RESULT_INVALID_ARGUMENT);
    definitions[1].reference_count = 0U;
    data.reference_count = 1U;
    source = (AfPackageSource){
        .scene = &fixture.scene, .animation = &fixture.animation, .components = &data};
    CHECK(af_package_write(&source, &allocator, &blob) == AF_RESULT_SUCCESS);
    CHECK(af_package_validate(blob.data, blob.size) == AF_RESULT_SUCCESS);
    CHECK(af_package_decode(blob.data, blob.size, &allocator, &loaded) == AF_RESULT_SUCCESS);
    CHECK(loaded.component_data.definition_count == 2U &&
          loaded.component_data.instance_count == 2U &&
          loaded.component_data.definitions[0].id == 100U &&
          strcmp(loaded.component_data.overrides[1].value.text, "overridden") == 0);
    af_loaded_package_destroy(&allocator, &loaded);
    af_package_blob_destroy(&allocator, &blob);
    return true;
}

static bool af_test_interaction(void) {
    AfSceneNode nodes[3] = {{.id = 1U,
                             .parent_id = AF_INVALID_NODE_ID,
                             .type = AF_NODE_GROUP,
                             .local_transform = {.position = {10.0F, 10.0F}, .scale = {1.0F, 1.0F}},
                             .bounds = {80.0F, 80.0F},
                             .opacity = 1.0F,
                             .visible = true},
                            {.id = 2U,
                             .parent_id = 1U,
                             .type = AF_NODE_RECTANGLE,
                             .local_transform = {.position = {5.0F, 5.0F}, .scale = {1.0F, 1.0F}},
                             .bounds = {100.0F, 100.0F},
                             .opacity = 1.0F,
                             .visible = true},
                            {.id = 3U,
                             .parent_id = AF_INVALID_NODE_ID,
                             .type = AF_NODE_ELLIPSE,
                             .local_transform = {.position = {30.0F, 30.0F}, .scale = {1.0F, 1.0F}},
                             .bounds = {30.0F, 30.0F},
                             .opacity = 1.0F,
                             .layer_order = 2,
                             .visible = true}};
    AfScene scene = {.nodes = nodes, .count = 3U, .capacity = 3U};
    AfInteractionNode interaction_nodes[3] = {
        {.node_id = 1U, .enabled = false, .clip_children = true},
        {.node_id = 2U, .enabled = true, .focusable = true, .opacity_threshold = 0.01F},
        {.node_id = 3U, .enabled = true, .focusable = true, .opacity_threshold = 0.01F}};
    AfInteractionSystem system;
    AfInteractionData data = {.nodes = interaction_nodes, .count = 3U};
    AfRuntimeEvent event;
    uint32_t event_count = 0U;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_validate(&data, &scene) == AF_RESULT_SUCCESS);
    af_interaction_init(&system, interaction_nodes, 3U);
    CHECK(af_interaction_hit_test(&system, &scene, 35.0F, 35.0F) == 3U);
    interaction_nodes[2].pointer_passthrough = true;
    CHECK(af_interaction_hit_test(&system, &scene, 35.0F, 35.0F) == 2U);
    CHECK(af_interaction_hit_test(&system, &scene, 100.0F, 20.0F) == 0U);
    CHECK(af_interaction_pointer_move(&system, &scene, 20.0F, 20.0F, 1.0) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_pointer_down(&system, &scene, 20.0F, 20.0F, 1.1) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_pointer_move(&system, &scene, 200.0F, 200.0F, 1.2) == AF_RESULT_SUCCESS);
    CHECK(system.captured_node_id == 2U && system.dragging);
    CHECK(af_interaction_pointer_up(&system, &scene, 200.0F, 200.0F, 1.3) == AF_RESULT_SUCCESS);
    while (af_interaction_poll(&system, &event) == AF_RESULT_SUCCESS) {
        ++event_count;
    }
    CHECK(event_count >= 9U);
    CHECK(af_interaction_pointer_down(&system, &scene, 20.0F, 20.0F, 2.0) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_pointer_up(&system, &scene, 20.0F, 20.0F, 2.1) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_pointer_down(&system, &scene, 20.0F, 20.0F, 2.2) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_pointer_up(&system, &scene, 20.0F, 20.0F, 2.3) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_keyboard_activate(&system, 2.4) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_controller_navigate(&system, &scene, 1, 2.5) == AF_RESULT_SUCCESS);
    while (af_interaction_poll(&system, &event) == AF_RESULT_SUCCESS) {
        if (event.type == AF_EVENT_DOUBLE_CLICK) {
            ++event_count;
        }
    }
    CHECK(event_count >= 10U);
    af_interaction_init(&system, interaction_nodes, 3U);
    for (uint32_t index = 0U; index < AF_MAX_RUNTIME_EVENTS; ++index) {
        CHECK(af_interaction_queue_animation_event(&system, 1000U + index, 2U, (double)index) ==
              AF_RESULT_SUCCESS);
    }
    CHECK(af_interaction_queue_animation_event(&system, 9999U, 2U, 999.0) ==
          AF_RESULT_LIMIT_EXCEEDED);
    CHECK(af_interaction_poll(&system, &event) == AF_RESULT_SUCCESS);
    CHECK(event.type == AF_EVENT_ANIMATION && event.name_id == 1000U);
    nodes[0].visible = false;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_hit_test(&system, &scene, 20.0F, 20.0F) == 0U);
    return true;
}

static bool af_test_state_machines(void) {
    AfTestFixture fixture;
    AfStateMachineDefinition machines[2] = {
        {.id = 100U,
         .entry_state_id = 1000U,
         .state_start = 0U,
         .state_count = 4U,
         .transition_start = 0U,
         .transition_count = 3U},
        {.id = 200U, .entry_state_id = 2000U, .state_start = 4U, .state_count = 1U}};
    AfStateDefinition states[5] = {
        {.id = 1000U, .machine_id = 100U, .clip_id = 10U, .type = AF_STATE_ANIMATION},
        {.id = 1001U,
         .machine_id = 100U,
         .clip_id = 10U,
         .tag_id = 77U,
         .type = AF_STATE_ANIMATION},
        {.id = 1002U, .machine_id = 100U, .type = AF_STATE_EXIT},
        {.id = 1003U,
         .machine_id = 100U,
         .nested_machine_id = 200U,
         .type = AF_STATE_NESTED_MACHINE},
        {.id = 2000U, .machine_id = 200U, .clip_id = 10U, .type = AF_STATE_ANIMATION}};
    AfStateTransition transitions[4] = {{.source_state_id = 1000U,
                                         .target_state_id = 1001U,
                                         .condition_start = 0U,
                                         .condition_count = 1U,
                                         .priority = 1},
                                        {.source_state_id = 1001U,
                                         .target_state_id = 1000U,
                                         .condition_start = 1U,
                                         .condition_count = 1U,
                                         .priority = 1},
                                        {.target_state_id = 1002U,
                                         .event_id = 99U,
                                         .priority = 10,
                                         .flags = AF_TRANSITION_REQUIRE_EVENT,
                                         .duration = 0.2F,
                                         .easing = {0.25F, 0.1F, 0.25F, 1.0F}}};
    AfStateInputDefinition input_definitions[4] = {
        {.machine_id = 100U,
         .id = 1U,
         .type = AF_STATE_INPUT_BOOLEAN,
         .default_value.boolean = false},
        {.machine_id = 100U, .id = 2U, .type = AF_STATE_INPUT_INTEGER, .default_value.integer = 0},
        {.machine_id = 100U, .id = 3U, .type = AF_STATE_INPUT_FLOAT, .default_value.real = 0.0F},
        {.machine_id = 100U, .id = 4U, .type = AF_STATE_INPUT_TRIGGER}};
    AfTransitionCondition conditions[2] = {{.input_id = 1U,
                                            .type = AF_STATE_INPUT_BOOLEAN,
                                            .comparison = AF_STATE_COMPARE_EQUAL,
                                            .value.boolean = true},
                                           {.input_id = 4U,
                                            .type = AF_STATE_INPUT_TRIGGER,
                                            .comparison = AF_STATE_COMPARE_EQUAL,
                                            .value.boolean = true}};
    AfStateMachineData data = {.machines = machines,
                               .machine_count = 2U,
                               .machine_capacity = 2U,
                               .states = states,
                               .state_count = 5U,
                               .state_capacity = 5U,
                               .transitions = transitions,
                               .transition_count = 3U,
                               .transition_capacity = 4U,
                               .inputs = input_definitions,
                               .input_count = 4U,
                               .input_capacity = 4U,
                               .conditions = conditions,
                               .condition_count = 2U,
                               .condition_capacity = 2U};
    AfStateMachineStatus statuses[2];
    AfStateInputState inputs[4];
    AfStateMachineRuntime runtime;
    AfStateMachineStatus debug;
    AfComponentData no_components = {0};
    AfAllocator allocator;
    AfPackageBlob blob = {0};
    AfLoadedPackage loaded = {0};
    AfRuntime *host_runtime = NULL;
    AfPackageHandle package = 0U;
    AfInstanceHandle instance = 0U;
    AfRuntimeCreateInfo create_info = {.frames_in_flight = 2U};
    AfPackageSource source;
    af_test_fixture_init(&fixture);
    af_allocator_init_default(&allocator);
    CHECK(af_state_machine_validate(&data, &fixture.animation, &no_components) ==
          AF_RESULT_SUCCESS);
    CHECK(af_state_machine_runtime_init(&runtime, &data, &fixture.animation, statuses, 2U, inputs,
                                        4U) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_set_boolean(&runtime, 1U, true) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_set_integer(&runtime, 2U, 8) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_set_float(&runtime, 3U, 0.75F) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_update(&runtime, 0.1F) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_debug(&runtime, 100U, &debug) == AF_RESULT_SUCCESS);
    CHECK(debug.current_state_id == 1001U && debug.transitions_last_update == 1U);
    CHECK(af_state_machine_set_boolean(&runtime, 1U, false) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_fire_trigger(&runtime, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_update(&runtime, 0.0F) == AF_RESULT_SUCCESS);
    CHECK(statuses[0].current_state_id == 1000U && !inputs[3].triggered);
    CHECK(af_state_machine_notify_event(&runtime, 99U) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_update(&runtime, 0.0F) == AF_RESULT_SUCCESS);
    CHECK(statuses[0].pending_state_id == 1002U);
    CHECK(af_state_machine_update(&runtime, 0.1F) == AF_RESULT_SUCCESS);
    CHECK(statuses[0].pending_state_id == 1002U);
    CHECK(af_state_machine_update(&runtime, 0.1F) == AF_RESULT_SUCCESS);
    CHECK(statuses[0].current_state_id == 1002U && statuses[0].exited);
    source = (AfPackageSource){
        .scene = &fixture.scene, .animation = &fixture.animation, .state_machines = &data};
    CHECK(af_package_write(&source, &allocator, &blob) == AF_RESULT_SUCCESS);
    CHECK(af_package_decode(blob.data, blob.size, &allocator, &loaded) == AF_RESULT_SUCCESS);
    CHECK(loaded.state_machine_data.machine_count == 2U &&
          loaded.state_machine_data.transition_count == 3U &&
          loaded.state_machine_data.inputs[3].type == AF_STATE_INPUT_TRIGGER);
    af_loaded_package_destroy(&allocator, &loaded);
    CHECK(af_runtime_create(&create_info, &host_runtime) == AF_RESULT_SUCCESS);
    CHECK(af_package_load(host_runtime, blob.data, blob.size, &package) == AF_RESULT_SUCCESS);
    CHECK(af_instance_create(host_runtime, package, &instance) == AF_RESULT_SUCCESS);
    CHECK(af_instance_set_boolean_input(host_runtime, instance, 1U, 1) == AF_RESULT_SUCCESS);
    CHECK(af_instance_set_integer_input(host_runtime, instance, 2U, 9) == AF_RESULT_SUCCESS);
    CHECK(af_instance_set_float_input(host_runtime, instance, 3U, 0.5F) == AF_RESULT_SUCCESS);
    CHECK(af_instance_update(host_runtime, instance, 0.01F) == AF_RESULT_SUCCESS);
    CHECK(af_instance_set_boolean_input(host_runtime, instance, 1U, 0) == AF_RESULT_SUCCESS);
    CHECK(af_instance_fire_trigger(host_runtime, instance, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_instance_update(host_runtime, instance, 0.01F) == AF_RESULT_SUCCESS);
    CHECK(af_runtime_frame_allocation_count(host_runtime) == 0U);
    CHECK(af_instance_destroy(host_runtime, instance) == AF_RESULT_SUCCESS);
    CHECK(af_package_destroy(host_runtime, package) == AF_RESULT_SUCCESS);
    af_runtime_destroy(host_runtime);
    af_package_blob_destroy(&allocator, &blob);
    states[3].nested_machine_id = 100U;
    CHECK(af_state_machine_validate(&data, &fixture.animation, &no_components) ==
          AF_RESULT_INVALID_ARGUMENT);
    states[3].nested_machine_id = 200U;
    transitions[0] = (AfStateTransition){.source_state_id = 1000U, .target_state_id = 1001U};
    transitions[1] = (AfStateTransition){.source_state_id = 1001U, .target_state_id = 1000U};
    machines[0].transition_count = 2U;
    CHECK(af_state_machine_runtime_init(&runtime, &data, &fixture.animation, statuses, 2U, inputs,
                                        4U) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_update(&runtime, 0.0F) == AF_RESULT_LIMIT_EXCEEDED);
    return true;
}

static bool af_test_advanced_animation(void) {
    AfTestFixture fixture;
    AfAnimationClip clips[4] = {0};
    AfAnimationTrack tracks[8] = {0};
    AfKeyframe keys[32] = {0};
    AfAnimationData animation = {.clips = clips,
                                 .clip_count = 1U,
                                 .tracks = tracks,
                                 .track_count = 2U,
                                 .keyframes = keys,
                                 .keyframe_count = 5U};
    AfClipMarker markers[2] = {{.clip_id = 10U, .event_id = 900U, .time = 0.5F}};
    AfTimelineRegion regions[2] = {
        {.clip_id = 10U, .id = 901U, .start_time = 0.25F, .end_time = 1.5F}};
    AfNestedTimeline nested[2] = {{.owner_clip_id = 10U,
                                   .child_clip_id = 20U,
                                   .start_time = 0.0F,
                                   .duration = 2.0F,
                                   .child_start_time = 0.0F,
                                   .child_end_time = 2.0F,
                                   .speed = 1.0F,
                                   .hold = true}};
    AfAnimationLayer layers[2] = {{.clip_id = 10U,
                                   .time = 1.0F,
                                   .weight = 0.5F,
                                   .transition_duration = 0.25F,
                                   .order = 0,
                                   .enabled = true},
                                  {.clip_id = 20U,
                                   .time = 0.5F,
                                   .weight = 0.25F,
                                   .order = 1,
                                   .additive = true,
                                   .enabled = true}};
    AfSpeedPoint speed_points[2] = {{.clip_id = 10U, .time = 0.0F, .speed = 1.0F},
                                    {.clip_id = 10U, .time = 1.0F, .speed = 0.5F}};
    AfAdvancedAnimationData advanced = {.markers = markers,
                                        .marker_count = 1U,
                                        .marker_capacity = 2U,
                                        .regions = regions,
                                        .region_count = 1U,
                                        .region_capacity = 2U,
                                        .nested = nested,
                                        .nested_count = 1U,
                                        .nested_capacity = 2U,
                                        .layers = layers,
                                        .layer_count = 2U,
                                        .layer_capacity = 2U,
                                        .speed_points = speed_points,
                                        .speed_point_count = 2U,
                                        .speed_point_capacity = 2U};
    AfAnimationClip snapshot_clips[4];
    AfAnimationTrack snapshot_tracks[8];
    AfKeyframe snapshot_keys[32];
    AfAnimationEditSnapshot snapshot = {.clips = snapshot_clips,
                                        .clip_capacity = 4U,
                                        .tracks = snapshot_tracks,
                                        .track_capacity = 8U,
                                        .keyframes = snapshot_keys,
                                        .keyframe_capacity = 32U};
    AfMotionPathPoint motion[8];
    float ghosts[8];
    float mapped_time = 0.0F;
    uint32_t output_count = 0U;
    const uint32_t retime_indices[2] = {0U, 1U};
    const float custom[4] = {0.42F, 0.0F, 0.58F, 1.0F};
    af_test_fixture_init(&fixture);
    memcpy(clips, fixture.clips, sizeof(fixture.clips));
    memcpy(tracks, fixture.tracks, sizeof(fixture.tracks));
    memcpy(keys, fixture.keys, sizeof(fixture.keys));
    CHECK(af_advanced_snapshot_capture(&animation, &snapshot) == AF_RESULT_SUCCESS);
    CHECK(af_advanced_clip_duplicate(&animation, 4U, 8U, 32U, 10U, 20U, 100U) == AF_RESULT_SUCCESS);
    CHECK(animation.clip_count == 2U && animation.track_count == 4U &&
          animation.keyframe_count == 10U);
    CHECK(af_advanced_animation_validate(&advanced, &animation) == AF_RESULT_SUCCESS);
    CHECK(af_advanced_map_nested_time(&advanced, &animation, 10U, 20U, 1.0F, false, &mapped_time) ==
          AF_RESULT_SUCCESS);
    CHECK(mapped_time >= 0.0F && mapped_time <= 2.0F);
    CHECK(af_advanced_apply_layers(&advanced, &animation, &fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(isfinite(fixture.nodes[1].local_transform.position.x));
    CHECK(af_advanced_motion_path(&animation, 10U, 2U, 8U, motion, 8U, &output_count) ==
          AF_RESULT_SUCCESS);
    CHECK(output_count == 8U && motion[0].time == 0.0F && motion[7].time == 2.0F);
    CHECK(af_advanced_motion_path_edit(&animation, 11U, 1U, (AfVec2){22.0F, 23.0F}) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(animation.keyframes[1].value[0], 22.0F, 0.0001F);
    CHECK(af_advanced_retime_keys(&animation, retime_indices, 2U, 0.5F, 1.0F, true) ==
          AF_RESULT_SUCCESS);
    CHECK(animation.keyframes[0].time <= animation.keyframes[1].time);
    CHECK(af_advanced_clip_split(&animation, 4U, 8U, 32U, 10U, 30U, 200U, 1.0F) ==
          AF_RESULT_SUCCESS);
    CHECK(animation.clip_count == 3U && clips[0].duration == 1.0F && clips[2].duration == 1.0F);
    CHECK(af_advanced_ghost_frames(1.0F, 1.0F / 60.0F, 2U, 3U, ghosts, 8U, &output_count) ==
          AF_RESULT_SUCCESS);
    CHECK(output_count == 5U && ghosts[0] < 1.0F && ghosts[4] > 1.0F);
    for (uint32_t preset = AF_EASING_EASE_IN; preset <= AF_EASING_CUSTOM_CUBIC; ++preset) {
        const float eased = af_advanced_easing_evaluate((AfEasingPreset)preset, custom, 0.5F);
        CHECK(isfinite(eased));
    }
    CHECK(af_advanced_snapshot_restore(&animation, &snapshot) == AF_RESULT_SUCCESS);
    CHECK(animation.clip_count == 1U && animation.track_count == 2U &&
          animation.keyframe_count == 5U);
    CHECK(af_advanced_clip_duplicate(&animation, 4U, 8U, 32U, 10U, 20U, 100U) == AF_RESULT_SUCCESS);
    nested[1] = (AfNestedTimeline){.owner_clip_id = 20U,
                                   .child_clip_id = 10U,
                                   .duration = 1.0F,
                                   .child_end_time = 1.0F,
                                   .speed = 1.0F};
    advanced.nested_count = 2U;
    CHECK(af_advanced_animation_validate(&advanced, &animation) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_composition(void) {
    AfTestFixture fixture;
    AfCompositionClip clips[2] = {
        {.id = 10U, .node_id = 1U, .type = AF_CLIP_ROUNDED_RECTANGLE, .corner_radius = 8.0F},
        {.id = 11U, .node_id = 3U, .type = AF_CLIP_RECTANGLE}};
    AfCompositionMask masks[2] = {
        {.id = 20U, .owner_node_id = 2U, .mask_node_id = 3U, .alpha = 1.0F},
        {.id = 21U,
         .owner_node_id = 2U,
         .mask_node_id = 3U,
         .parent_mask_id = 20U,
         .alpha = 0.75F,
         .inverted = true}};
    AfLayerFilter filters[5] = {
        {.type = AF_FILTER_BLUR, .radius = 4.0F},
        {.type = AF_FILTER_DROP_SHADOW,
         .radius = 8.0F,
         .offset = {3.0F, 5.0F},
         .color = {0.0F, 0.0F, 0.0F, 0.5F}},
        {.type = AF_FILTER_INNER_SHADOW, .radius = 3.0F},
        {.type = AF_FILTER_GLOW, .radius = 6.0F, .color = {1.0F, 0.5F, 0.2F, 1.0F}},
        {.type = AF_FILTER_COLOR_ADJUSTMENT}};
    AfCompositionLayer layers[2] = {{.id = 100U,
                                     .node_id = 2U,
                                     .clip_id = 10U,
                                     .mask_id = 21U,
                                     .generation = 1U,
                                     .filter_start = 0U,
                                     .filter_count = 5U,
                                     .group_opacity = 0.6F,
                                     .blend_mode = AF_BLEND_NORMAL,
                                     .cache_enabled = true},
                                    {.id = 101U,
                                     .node_id = 3U,
                                     .parent_layer_id = 100U,
                                     .clip_id = 11U,
                                     .generation = 1U,
                                     .group_opacity = 1.0F,
                                     .blend_mode = AF_BLEND_SCREEN,
                                     .offscreen = true}};
    AfCompositionData data = {.layers = layers,
                              .layer_count = 2U,
                              .layer_capacity = 2U,
                              .clips = clips,
                              .clip_count = 2U,
                              .clip_capacity = 2U,
                              .masks = masks,
                              .mask_count = 2U,
                              .mask_capacity = 2U,
                              .filters = filters,
                              .filter_count = 5U,
                              .filter_capacity = 5U};
    AfRenderTargetPool pool = {0};
    AfCompositionCommand commands[2];
    AfCompositionPlan plan = {.commands = commands, .capacity = 2U};
    AfColor destination = {0.2F, 0.3F, 0.4F, 0.8F};
    AfColor source = {0.3F, 0.2F, 0.1F, 0.5F};
    af_test_fixture_init(&fixture);
    fixture.nodes[0].bounds = (AfVec2){300.0F, 300.0F};
    CHECK(af_scene_evaluate_world(&fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(af_composition_validate(&data, &fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(af_composition_build_plan(&data, &fixture.scene, 640U, 480U, &pool, &plan) ==
          AF_RESULT_SUCCESS);
    CHECK(plan.count == 2U && commands[0].target_index >= 0 && commands[1].target_index >= 0 &&
          !commands[0].target_reused);
    CHECK(af_composition_build_plan(&data, &fixture.scene, 640U, 480U, &pool, &plan) ==
          AF_RESULT_SUCCESS);
    CHECK(commands[0].target_reused && commands[1].target_reused);
    fixture.nodes[2].local_transform.position.x += 10.0F;
    CHECK(af_scene_evaluate_world(&fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(af_composition_build_plan(&data, &fixture.scene, 640U, 480U, &pool, &plan) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(commands[0].mask_transform.tx, fixture.nodes[2].world_transform.tx, 0.0001F);
    for (uint32_t mode = AF_BLEND_NORMAL; mode <= AF_BLEND_SCREEN; ++mode) {
        const AfColor blended = af_composition_blend((AfBlendMode)mode, source, destination);
        CHECK(isfinite(blended.r) && isfinite(blended.g) && isfinite(blended.b) &&
              isfinite(blended.a));
    }
    {
        const AfVec2 expansion = af_composition_filter_expansion(&filters[1]);
        CHECK(expansion.x >= 16.0F && expansion.y >= 16.0F);
    }
    CHECK(af_render_target_acquire(&pool, AF_MAX_RENDER_TARGET_DIMENSION + 1U, 16U, 0U, 0U, false,
                                   &(uint32_t){0}, &(bool){false}) == AF_RESULT_INVALID_ARGUMENT);
    layers[1].blend_mode = AF_BLEND_UNSUPPORTED;
    CHECK(af_composition_validate(&data, &fixture.scene) == AF_RESULT_UNSUPPORTED_FEATURE);
    layers[1].blend_mode = AF_BLEND_SCREEN;
    layers[0].parent_layer_id = 101U;
    CHECK(af_composition_validate(&data, &fixture.scene) == AF_RESULT_INVALID_ARGUMENT);
    layers[0].parent_layer_id = 0U;
    masks[0].parent_mask_id = 21U;
    CHECK(af_composition_validate(&data, &fixture.scene) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_shape_deformation(void) {
    AfPath paths[3] = {{.id = 1U,
                        .node_id = 1U,
                        .command_start = 0U,
                        .command_count = 4U,
                        .fill_rule = AF_PATH_FILL_NON_ZERO,
                        .geometry_generation = 1U},
                       {.id = 2U,
                        .node_id = 2U,
                        .command_start = 4U,
                        .command_count = 4U,
                        .fill_rule = AF_PATH_FILL_NON_ZERO,
                        .geometry_generation = 1U},
                       {.id = 3U,
                        .node_id = 3U,
                        .command_start = 8U,
                        .command_count = 3U,
                        .fill_rule = AF_PATH_FILL_NON_ZERO,
                        .geometry_generation = 1U}};
    AfPathCommand commands[11] = {{.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {10.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {10.0F, 10.0F}},
                                  {.type = AF_PATH_CLOSE},
                                  {.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {20.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {20.0F, 20.0F}},
                                  {.type = AF_PATH_CLOSE},
                                  {.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {5.0F, 5.0F}},
                                  {.type = AF_PATH_LINE, .point = {10.0F, 0.0F}}};
    AfPathData path_data = {.paths = paths,
                            .path_count = 3U,
                            .path_capacity = 3U,
                            .commands = commands,
                            .command_count = 11U,
                            .command_capacity = 11U};
    AfPathCommand morphed[4];
    AfMorphCache morph = {.commands = morphed, .command_capacity = 4U};
    AfVec2 trimmed[64];
    AfPathTessellationScratch scratch;
    AfTransform2D followed;
    uint32_t trimmed_count = 0U;
    AfStyle style = af_style_default(10U, 1U, (AfColor){1.0F, 1.0F, 1.0F, 1.0F});
    AfStyleData style_data = {.styles = &style, .count = 1U, .capacity = 1U};
    AfCompositionMask mask = {.id = 20U, .owner_node_id = 1U, .mask_node_id = 2U, .alpha = 1.0F};
    AfCompositionData composition = {.mask_count = 1U, .mask_capacity = 1U, .masks = &mask};
    AfShapeAnimationTrack shape_tracks[5] = {{.target_id = 1U,
                                              .property = AF_SHAPE_PROPERTY_PATH_POINT,
                                              .element_index = 1U,
                                              .sample_start = 0U,
                                              .sample_count = 2U},
                                             {.target_id = 10U,
                                              .property = AF_SHAPE_PROPERTY_STROKE_WIDTH,
                                              .sample_start = 2U,
                                              .sample_count = 2U},
                                             {.target_id = 10U,
                                              .property = AF_SHAPE_PROPERTY_GRADIENT_STOP,
                                              .sample_start = 4U,
                                              .sample_count = 2U},
                                             {.target_id = 10U,
                                              .property = AF_SHAPE_PROPERTY_GRADIENT_TRANSFORM,
                                              .sample_start = 6U,
                                              .sample_count = 2U},
                                             {.target_id = 20U,
                                              .property = AF_SHAPE_PROPERTY_MASK_ALPHA,
                                              .sample_start = 8U,
                                              .sample_count = 2U}};
    AfShapeAnimationSample samples[10] = {
        {.time = 0.0F, .value = {10.0F, 0.0F}},
        {.time = 1.0F, .value = {12.0F, 2.0F}},
        {.time = 0.0F, .value = {2.0F}},
        {.time = 1.0F, .value = {6.0F}},
        {.time = 0.0F, .value = {0.0F, 1.0F, 0.0F, 0.0F, 1.0F}},
        {.time = 1.0F, .value = {0.5F, 0.0F, 1.0F, 0.0F, 1.0F}},
        {.time = 0.0F, .value = {1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F}},
        {.time = 1.0F, .value = {1.0F, 0.0F, 0.0F, 1.0F, 5.0F, 3.0F}},
        {.time = 0.0F, .value = {1.0F}},
        {.time = 1.0F, .value = {0.5F}}};
    AfShapeAnimationData shape_animation = {.tracks = shape_tracks,
                                            .track_count = 5U,
                                            .track_capacity = 5U,
                                            .samples = samples,
                                            .sample_count = 10U,
                                            .sample_capacity = 10U};
    AfVec2 mesh_vertices[4] = {{0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}};
    uint32_t mesh_indices[6] = {0U, 1U, 2U, 0U, 2U, 3U};
    AfDeformMesh mesh = {.vertices = mesh_vertices,
                         .vertex_count = 4U,
                         .vertex_capacity = 4U,
                         .indices = mesh_indices,
                         .index_count = 6U,
                         .index_capacity = 6U};
    AfShapeKey shape_keys[2] = {
        {.id = 100U, .vertex_start = 0U, .vertex_count = 4U, .delta_start = 0U},
        {.id = 101U, .vertex_start = 0U, .vertex_count = 4U, .delta_start = 4U}};
    AfVec2 deltas[8] = {{0.0F, 0.0F}, {0.5F, 0.0F},   {0.5F, 0.5F},    {0.0F, 0.5F},
                        {0.0F, 0.0F}, {-0.25F, 0.0F}, {-0.25F, 0.25F}, {0.0F, 0.25F}};
    AfShapeKeyData key_data = {.keys = shape_keys,
                               .key_count = 2U,
                               .key_capacity = 2U,
                               .deltas = deltas,
                               .delta_count = 8U,
                               .delta_capacity = 8U};
    AfShapeKeyInfluence influences[2] = {{.key_id = 100U, .weight = 0.5F},
                                         {.key_id = 101U, .weight = 0.25F}};
    AfVec2 deformed[4];
    AfVec2 interpolated[4];
    uint32_t interpolated_count = 0U;
    style.fill.type = AF_PAINT_LINEAR_GRADIENT;
    style.fill.stop_count = 1U;
    style.fill.stops[0] = (AfGradientStop){.color = {1.0F, 0.0F, 0.0F, 1.0F}};
    style.stroke.enabled = true;
    style.stroke.width = 2.0F;
    CHECK(af_path_data_validate(&path_data) == AF_RESULT_SUCCESS);
    CHECK(af_shape_morph_validate(&path_data, 1U, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_shape_morph_update(&path_data, 1U, 2U, 0.5F, &morph) == AF_RESULT_SUCCESS);
    CHECK(morph.rebuild_count == 1U && morph.command_count == 4U);
    CHECK_NEAR(morphed[1].point.x, 15.0F, 0.0001F);
    CHECK(af_shape_morph_update(&path_data, 1U, 2U, 0.5F, &morph) == AF_RESULT_SUCCESS);
    CHECK(morph.rebuild_count == 1U);
    CHECK(af_trim_path(&path_data, 1U, 0.2F, 0.8F, 0.25F, &scratch, trimmed, 64U, &trimmed_count) ==
          AF_RESULT_SUCCESS);
    CHECK(trimmed_count >= 2U);
    CHECK(af_trim_path(&path_data, 3U, 0.1F, 0.9F, 0.25F, &scratch, trimmed, 64U, &trimmed_count) ==
          AF_RESULT_SUCCESS);
    CHECK(af_path_follow(&path_data, 1U, 0.5F, 0.25F, &scratch, &followed) == AF_RESULT_SUCCESS);
    CHECK(isfinite(followed.position.x) && isfinite(followed.rotation));
    CHECK(af_shape_animation_validate(&shape_animation, &path_data, &style_data, &composition) ==
          AF_RESULT_SUCCESS);
    CHECK(af_shape_animation_apply(&shape_animation, 0.5F, &path_data, &style_data, &composition) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(commands[1].point.x, 11.0F, 0.0001F);
    CHECK_NEAR(style.stroke.width, 4.0F, 0.0001F);
    CHECK_NEAR(mask.alpha, 0.75F, 0.0001F);
    {
        const uint64_t path_generation = paths[0].geometry_generation;
        const uint64_t paint_generation = style.paint_generation;
        const uint64_t stroke_generation = style.stroke_geometry_generation;
        CHECK(af_shape_animation_apply(&shape_animation, 0.5F, &path_data, &style_data,
                                       &composition) == AF_RESULT_SUCCESS);
        CHECK(paths[0].geometry_generation == path_generation &&
              style.paint_generation == paint_generation &&
              style.stroke_geometry_generation == stroke_generation);
    }
    CHECK(af_deform_mesh_validate(&mesh, &key_data) == AF_RESULT_SUCCESS);
    CHECK(af_deform_mesh_apply(&mesh, &key_data, influences, 2U, deformed, 4U) ==
          AF_RESULT_SUCCESS);
    CHECK(deformed[1].x > mesh_vertices[1].x);
    CHECK(af_shape_key_interpolate(&key_data, 100U, 101U, 0.5F, interpolated, 4U,
                                   &interpolated_count) == AF_RESULT_SUCCESS);
    CHECK(interpolated_count == 4U && isfinite(interpolated[2].x));
    commands[5].type = AF_PATH_CUBIC;
    CHECK(af_shape_morph_validate(&path_data, 1U, 2U) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}
