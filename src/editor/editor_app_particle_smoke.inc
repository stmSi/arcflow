static AfResult af_editor_particle_inputs_smoke(AfEditorState *editor, int width) {
#define PARTICLE_CHECK(condition)                                                                  \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            af_log(AF_LOG_ERROR, "Particle input smoke failed at line %u", (unsigned)__LINE__);    \
            return AF_RESULT_INVALID_ARGUMENT;                                                     \
        }                                                                                          \
    } while (0)
    PARTICLE_CHECK(af_editor_create_template(editor, AF_PROJECT_TEMPLATE_PARTICLE_EFFECT));
    editor->welcome_visible = false;
    PARTICLE_CHECK(editor->project.particle_data.count != 0);
    af_editor_select_only(editor, editor->project.particle_data.emitters[0].node_id);
    af_editor_update_preview(editor);
    uint32_t tested = 0;
    for (uint32_t page = 0; page < AF_EDITOR_PARTICLE_PAGE_COUNT; ++page) {
        editor->particle_page = page;
        for (uint32_t row = 0; row < 9; ++row) {
            const AfEditorInspectorField field =
                (AfEditorInspectorField)(AF_EDITOR_INSPECTOR_PARTICLE_FIRST + page * 9 + row);
            AfEditorParticleParameter parameter;
            if (!af_editor_particle_parameter(editor, field, &parameter))
                continue;
            float x, w;
            PARTICLE_CHECK(af_editor_particle_field_rect(editor, width, field, &x, &w));
            const uint32_t visible_row = row == 8 ? (page == 1 ? 6U : 7U) : row;
            PARTICLE_CHECK(af_editor_inspector_field_at(
                               editor, width, x + 10,
                               AF_EDITOR_PARTICLE_ROW_Y +
                                   (float)visible_row * AF_EDITOR_PARTICLE_ROW_STEP + 10) == field);
            double target =
                fmin(parameter.maximum, af_editor_particle_value(&parameter) + parameter.step);
            if (parameter.integer)
                target = round(target);
            af_editor_begin_inspector_edit(editor, field);
            (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%.12g",
                           target);
            PARTICLE_CHECK(af_editor_finish_inspector_edit(editor, true));
            PARTICLE_CHECK(fabs(af_editor_particle_value(&parameter) - target) < 0.0001);
            PARTICLE_CHECK(af_particle_data_validate(&editor->project.particle_data,
                                                     &editor->project.scene) == AF_RESULT_SUCCESS);
            ++tested;
        }
    }
    PARTICLE_CHECK(tested > 40);
    editor->particle_page = 0;
    const AfEditorInspectorField seed =
        (AfEditorInspectorField)(AF_EDITOR_INSPECTOR_PARTICLE_FIRST + 7);
    af_editor_begin_inspector_edit(editor, seed);
    (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%u", UINT32_MAX);
    PARTICLE_CHECK(af_editor_finish_inspector_edit(editor, true));
    PARTICLE_CHECK(af_project_find_node_emitter(&editor->project, editor->selected_id)->seed ==
                   UINT32_MAX);
    PARTICLE_CHECK(af_editor_step_inspector_value(editor, seed, -1));
    PARTICLE_CHECK(af_project_find_node_emitter(&editor->project, editor->selected_id)->seed ==
                   UINT32_MAX - 1U);
    const AfEditorInspectorField spawn =
        (AfEditorInspectorField)(AF_EDITOR_INSPECTOR_PARTICLE_FIRST + 1);
    AfEditorParticleParameter parameter;
    PARTICLE_CHECK(af_editor_particle_parameter(editor, spawn, &parameter));
    const double before = af_editor_particle_value(&parameter);
    af_editor_begin_inspector_scrub(editor, spawn, 100, 100);
    af_editor_update_inspector_scrub(editor, 140);
    af_editor_update_inspector_scrub(editor, 148);
    const double after = af_editor_particle_value(&parameter);
    PARTICLE_CHECK(after > before);
    editor->drag_mode = AF_EDITOR_DRAG_NONE;
    editor->inspector_scrub_field = AF_EDITOR_INSPECTOR_NONE;
    PARTICLE_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    PARTICLE_CHECK(af_editor_particle_parameter(editor, spawn, &parameter));
    PARTICLE_CHECK(fabs(af_editor_particle_value(&parameter) - before) < .0001);
    PARTICLE_CHECK(af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    PARTICLE_CHECK(af_editor_particle_parameter(editor, spawn, &parameter));
    PARTICLE_CHECK(fabs(af_editor_particle_value(&parameter) - after) < .0001);
    af_editor_begin_inspector_edit(editor, spawn);
    (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "invalid");
    PARTICLE_CHECK(!af_editor_finish_inspector_edit(editor, true));
    PARTICLE_CHECK(fabs(af_editor_particle_value(&parameter) - after) < .0001);
    PARTICLE_CHECK(af_editor_finish_inspector_edit(editor, false));
    af_editor_status(editor,
                     "Particle inputs: typing, limits, steppers, dragging and undo accepted");
    editor->inspector_scroll = 140;
#undef PARTICLE_CHECK
    return AF_RESULT_SUCCESS;
}
