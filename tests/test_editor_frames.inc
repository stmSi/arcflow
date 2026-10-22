static bool af_test_editor_frames(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfProject project = {0}, reopened = {0};
    AfEditorHistory history = {0};
    AfEditorCommand command;
    CHECK(af_project_create(&project, &allocator, 64) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_create(&history, &allocator, AF_EDITOR_HISTORY_DEFAULT_BYTES) ==
          AF_RESULT_SUCCESS);
    AfProjectClip clip = {.id = 100, .name = "Drawings", .duration = 1, .loop_end = 1};
    CHECK(af_project_add_clip(&project, &clip) == AF_RESULT_SUCCESS);
    CHECK(af_editor_frame_rate(&clip) == 24);
    CHECK(af_editor_command_frame(&project, 100, 0, 0, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    const uint64_t first = command.target_id;
    const uint64_t first_track = command.track_after[0].track.id;
    CHECK(af_editor_frame_at(&project, 100, 0)->node_id == first);
    CHECK(af_editor_frame_at(&project, 100, 1) == NULL);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 0 && project.track_count == 0 && project.keyframe_count == 0);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    AfSceneNode shape = {.id = 20,
                         .parent_id = first,
                         .type = AF_NODE_RECTANGLE,
                         .local_transform = {.position = {20, 30}, .scale = {1, 1}},
                         .bounds = {100, 80},
                         .fill = {1, 0, 0, 1},
                         .opacity = 1,
                         .visible = true};
    CHECK(af_project_add_node(&project, &shape, "Rectangle") == AF_RESULT_SUCCESS);
    /* Use real vector geometry when duplicating a drawing. */
    CHECK(af_editor_command_convert_path(&project, 20, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    AfStyle *source_style = af_project_find_node_style(&project, 20);
    source_style->parent_style_id = af_project_find_node_style(&project, first)->id;
    source_style->inherit_flags = AF_STYLE_INHERIT_FILL;
    CHECK(af_editor_command_frame(&project, 100, 1, first, &command) == AF_RESULT_SUCCESS);
    const uint64_t second = command.target_id, copy = command.after[1].node.id;
    CHECK(command.path_after_count == 1);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_frame_owner(&project, copy)->node_id == second);
    CHECK(af_project_find_node_style(&project, copy)->parent_style_id ==
          af_project_find_node_style(&project, second)->id);
    CHECK(af_path_find_node_const(&project.path_data, copy)->command_count ==
          af_path_find_node_const(&project.path_data, 20)->command_count);
    CHECK_NEAR(af_scene_find_const(&project.scene, copy)->local_transform.position.x, 20, .001F);
    AfSceneNode sampled[64];
    AfScene preview = project.scene;
    preview.nodes = sampled;
    memcpy(sampled, project.scene.nodes, project.scene.count * sizeof(*sampled));
    CHECK(af_project_animation_apply(&project, 100, 0, &preview) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find_const(&preview, 20)->world_opacity, 1, .001F);
    CHECK_NEAR(af_scene_find_const(&preview, copy)->world_opacity, 0, .001F);
    CHECK(af_project_animation_apply(&project, 100, 1.0F / 24, &preview) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find_const(&preview, 20)->world_opacity, 0, .001F);
    CHECK_NEAR(af_scene_find_const(&preview, copy)->world_opacity, 1, .001F);
    CHECK(af_project_animation_apply(&project, 100, 2.0F / 24, &preview) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find_const(&preview, copy)->world_opacity, 0, .001F);
    CHECK(af_editor_command_frame_hold(&project, first_track, 2, &command) ==
          AF_RESULT_INVALID_ARGUMENT);
    const uint64_t second_track = af_editor_frame_at(&project, 100, 1)->id;
    CHECK(af_editor_command_frame_hold(&project, second_track, 3, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_frame_at(&project, 100, 3)->node_id == second);
    CHECK(af_editor_command_frame(&project, 100, 3, 0, &command) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_editor_command_frame_rate(&project, 100, 12, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_frame_rate(&project.clips[0]) == 12);
    CHECK(af_project_animation_apply(&project, 100, 1.0F / 24, &preview) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find_const(&preview, 20)->world_opacity, 1, .001F);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_frame_rate(&project.clips[0]) == 24);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_frame(&project, 100, 12, 0, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.clips[0].duration > 1);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(project.clips[0].duration, 1, .001F);
    AfPackageBlob text = {0}, package = {0};
    CHECK(af_project_serialize(&project, &allocator, &text) == AF_RESULT_SUCCESS);
    CHECK(af_project_deserialize(text.data, text.size, &allocator, &reopened) == AF_RESULT_SUCCESS);
    CHECK(reopened.clips[0].frame_rate == 12);
    CHECK(af_editor_frame_at(&reopened, 100, 3)->cel_length == 3);
    CHECK(af_project_compile(&reopened, &allocator, &package) == AF_RESULT_SUCCESS);
    AfLoadedPackage runtime = {0};
    CHECK(af_package_decode(package.data, package.size, &allocator, &runtime) == AF_RESULT_SUCCESS);
    CHECK(af_animation_apply(&runtime.animation, &runtime.animation.clips[0], 2.0F / 12,
                             &runtime.scene) == AF_RESULT_SUCCESS);
    CHECK(af_scene_evaluate_world(&runtime.scene) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find_const(&runtime.scene, copy)->world_opacity, 1, .001F);
    CHECK_NEAR(af_scene_find_const(&runtime.scene, 20)->world_opacity, 0, .001F);
    af_loaded_package_destroy(&allocator, &runtime);
    char lottie[16384];
    size_t lottie_size = 0;
    CHECK(af_interchange_export_lottie_json(
              &reopened, 100, &(AfLottieExportOptions){.frame_rate = 60}, lottie, sizeof(lottie),
              &lottie_size, NULL) == AF_RESULT_SUCCESS);
    CHECK(strstr(lottie, "\"ip\":5,\"op\":20") != NULL);
    af_package_blob_destroy(&allocator, &package);
    af_package_blob_destroy(&allocator, &text);
    CHECK(af_editor_command_delete(&project, second, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_frame_at(&project, 100, 1) == NULL);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_frame_at(&project, 100, 3)->node_id == second);
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
    const uint32_t nodes_before = project.scene.count, tracks_before = project.track_count;
    const uint32_t keys_before = project.keyframe_count, capacity = project.track_capacity;
    project.track_capacity = project.track_count;
    CHECK(af_editor_command_frame(&project, 100, 24, 0, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) !=
          AF_RESULT_SUCCESS);
    project.track_capacity = capacity;
    CHECK(project.scene.count == nodes_before && project.track_count == tracks_before &&
          project.keyframe_count == keys_before);
    CHECK_NEAR(project.clips[0].duration, 1, .001F);
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
    /* A normal drawing can contain more than eight pen strokes. */
    const AfPath original_path = *af_path_find_node_const(&project.path_data, 20);
    AfPathCommand points[5];
    CHECK(original_path.command_count == 5);
    memcpy(points, &project.path_data.commands[original_path.command_start], sizeof(points));
    for (uint32_t i = 0; i < 9; ++i) {
        AfSceneNode stroke = *af_scene_find_const(&project.scene, 20);
        CHECK(af_project_allocate_id(&project, &stroke.id) == AF_RESULT_SUCCESS);
        CHECK(af_project_add_node(&project, &stroke, "Stroke") == AF_RESULT_SUCCESS);
        AfPath path = original_path;
        path.id = path.node_id = stroke.id;
        CHECK(af_project_add_path(&project, &path, points) == AF_RESULT_SUCCESS);
    }
    CHECK(af_editor_command_frame(&project, 100, 5, first, &command) == AF_RESULT_SUCCESS);
    CHECK(command.path_after_count == 10);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    af_project_destroy(&reopened);
    return true;
}
