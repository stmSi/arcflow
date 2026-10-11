typedef struct AfEditorParticleParameter {
    const char *label;
    const void *address;
    double minimum;
    double maximum;
    double step;
    double scale;
    bool integer;
} AfEditorParticleParameter;

static AfResult af_editor_commit_modify(AfEditorState *editor, const AfProjectNodeSnapshot *before,
                                        AfEditorMergeKey merge_key, bool merge);

static bool af_editor_particle_parameter(const AfEditorState *editor, AfEditorInspectorField field,
                                         AfEditorParticleParameter *parameter) {
    const AfParticleEmitter *e =
        af_project_find_node_emitter_const(&editor->project, editor->selected_id);
    if (e == NULL || field < AF_EDITOR_INSPECTOR_PARTICLE_FIRST ||
        field > AF_EDITOR_INSPECTOR_PARTICLE_LAST)
        return false;
    const uint32_t offset = (uint32_t)(field - AF_EDITOR_INSPECTOR_PARTICLE_FIRST);
    const uint32_t page = offset / 9U, row = offset % 9U;
    const AfVfxLodSettings *lod = &editor->project.particle_data.lod;
    *parameter =
        (AfEditorParticleParameter){.minimum = -1000000, .maximum = 1000000, .step = 1, .scale = 1};
#define AF_PARTICLE_FLOAT(label_, member, min_, max_, step_)                                       \
    do {                                                                                           \
        parameter->label = label_;                                                                 \
        parameter->address = &e->member;                                                           \
        parameter->minimum = min_;                                                                 \
        parameter->maximum = max_;                                                                 \
        parameter->step = step_;                                                                   \
    } while (0)
#define AF_PARTICLE_UINT(label_, member, min_, max_, step_)                                        \
    do {                                                                                           \
        AF_PARTICLE_FLOAT(label_, member, min_, max_, step_);                                      \
        parameter->integer = true;                                                                 \
    } while (0)
    if (page == 0) {
        switch (row) {
        case 1:
            AF_PARTICLE_FLOAT("SPAWN / S", spawn_rate, 0, 1000000, 8);
            break;
        case 2:
            AF_PARTICLE_FLOAT("LIFE MIN", lifetime_min, .001, e->lifetime_max, .1);
            break;
        case 3:
            AF_PARTICLE_FLOAT("LIFE MAX", lifetime_max, e->lifetime_min, 1000000, .1);
            break;
        case 4:
            AF_PARTICLE_UINT(
                "CAPACITY", capacity, 1,
                AF_MAX_PARTICLES -
                    (af_particle_total_capacity(&editor->project.particle_data) - e->capacity),
                256);
            break;
        case 6:
            AF_PARTICLE_UINT("BURST", burst_count, 0, e->capacity, 16);
            break;
        case 7:
            AF_PARTICLE_UINT("SEED", seed, 0, UINT32_MAX, 1);
            break;
        default:
            return false;
        }
    } else if (page == 1) {
        switch (row) {
        case 0:
            AF_PARTICLE_FLOAT("VX MIN", velocity_min.x, -1000000, e->velocity_max.x, 10);
            break;
        case 1:
            AF_PARTICLE_FLOAT("VX MAX", velocity_max.x, e->velocity_min.x, 1000000, 10);
            break;
        case 2:
            AF_PARTICLE_FLOAT("VY MIN", velocity_min.y, -1000000, e->velocity_max.y, 10);
            break;
        case 3:
            AF_PARTICLE_FLOAT("VY MAX", velocity_max.y, e->velocity_min.y, 1000000, 10);
            break;
        case 4:
            AF_PARTICLE_FLOAT("GRAVITY X", gravity.x, -1000000, 1000000, 10);
            break;
        case 5:
            AF_PARTICLE_FLOAT("GRAVITY Y", gravity.y, -1000000, 1000000, 10);
            break;
        case 6:
            AF_PARTICLE_FLOAT("WIND X", wind.x, -1000000, 1000000, 10);
            break;
        case 7:
            AF_PARTICLE_FLOAT("DRAG", drag, 0, 1000000, .05);
            break;
        case 8:
            AF_PARTICLE_FLOAT("WIND Y", wind.y, -1000000, 1000000, 10);
            break;
        default:
            return false;
        }
    } else if (page == 2) {
        switch (row) {
        case 0:
            AF_PARTICLE_FLOAT("TURBULENCE", turbulence_strength, -AF_MAX_PARTICLE_FORCE,
                              AF_MAX_PARTICLE_FORCE, 20);
            break;
        case 1:
            AF_PARTICLE_FLOAT("TURB FREQ", turbulence_frequency, 0, 100, .01);
            break;
        case 2:
            AF_PARTICLE_FLOAT("RADIAL", radial_force, -AF_MAX_PARTICLE_FORCE, AF_MAX_PARTICLE_FORCE,
                              20);
            break;
        case 3:
            AF_PARTICLE_FLOAT("VORTEX", vortex_force, -AF_MAX_PARTICLE_FORCE, AF_MAX_PARTICLE_FORCE,
                              20);
            break;
        case 4:
            AF_PARTICLE_FLOAT("ATTRACT", attractor_strength, -AF_MAX_PARTICLE_FORCE,
                              AF_MAX_PARTICLE_FORCE, 20);
            break;
        case 5:
            AF_PARTICLE_FLOAT("ATTRACT R", attractor_radius, 0, 1000000, 10);
            break;
        case 6:
            AF_PARTICLE_FLOAT("REPULSE", repulsor_strength, -AF_MAX_PARTICLE_FORCE,
                              AF_MAX_PARTICLE_FORCE, 20);
            break;
        case 7:
            AF_PARTICLE_FLOAT("VECTOR", vector_field_strength, -AF_MAX_PARTICLE_FORCE,
                              AF_MAX_PARTICLE_FORCE, 20);
            break;
        default:
            return false;
        }
    } else if (page == 3) {
        switch (row) {
        case 1:
            AF_PARTICLE_FLOAT("PLANE OFFSET", collision_plane_offset, -1000000, 1000000, 10);
            break;
        case 2:
            AF_PARTICLE_FLOAT("BOUNCE", collision_restitution, 0, 1, .05);
            break;
        case 3:
            AF_PARTICLE_FLOAT("FRICTION", collision_friction, 0, 1, .05);
            break;
        case 5:
            AF_PARTICLE_FLOAT("DEPTH MIN", depth_min, -1000000, e->depth_max, .1);
            break;
        case 6:
            AF_PARTICLE_FLOAT("DEPTH MAX", depth_max, e->depth_min, 1000000, .1);
            break;
        case 7:
            AF_PARTICLE_FLOAT("VZ MIN", depth_velocity_min, -1000000, e->depth_velocity_max, .1);
            break;
        case 8:
            AF_PARTICLE_FLOAT("VZ MAX", depth_velocity_max, e->depth_velocity_min, 1000000, .1);
            break;
        default:
            return false;
        }
    } else if (page == 4) {
        switch (row) {
        case 1:
            AF_PARTICLE_FLOAT("TRAIL LENGTH", trail_length, 0, 1000000, 1);
            break;
        case 2:
            AF_PARTICLE_FLOAT("TRAIL WIDTH", trail_width, 0, 1000000, 1);
            break;
        case 3:
            AF_PARTICLE_FLOAT("MESH SCALE", mesh_scale, .1, 1000000, .1);
            break;
        case 4:
            AF_PARTICLE_FLOAT("DECAL ROT", decal_rotation_offset, -1000000, 1000000, .1);
            break;
        case 5:
            AF_PARTICLE_FLOAT("DISTORTION", distortion_strength, -1000000, 1000000, .1);
            break;
        case 6:
            AF_PARTICLE_FLOAT("SOFT FADE", soft_fade_distance, 0, 1000000, .1);
            break;
        case 7:
            AF_PARTICLE_FLOAT("LIGHT RADIUS", light_radius, 0, 1000000, 1);
            break;
        default:
            return false;
        }
    } else if (page == 5) {
        switch (row) {
        case 0:
            AF_PARTICLE_UINT("COLUMNS", flipbook_columns, 1, 64, 1);
            break;
        case 1:
            AF_PARTICLE_UINT("ROWS", flipbook_rows, 1, 64, 1);
            break;
        case 2:
            AF_PARTICLE_FLOAT("FPS", flipbook_frames_per_second, 0, 1000000, 1);
            break;
        case 4:
            AF_PARTICLE_UINT("ON BIRTH", sub_emit_birth_count, 0,
                             e->sub_emitter_id ? AF_MAX_PARTICLE_SUB_EMIT_COUNT : 0, 1);
            break;
        case 5:
            AF_PARTICLE_UINT("ON DEATH", sub_emit_death_count, 0,
                             e->sub_emitter_id ? AF_MAX_PARTICLE_SUB_EMIT_COUNT : 0, 1);
            break;
        case 6:
            AF_PARTICLE_UINT("RECURSION", max_recursion_depth, e->sub_emitter_id ? 1 : 0,
                             AF_MAX_PARTICLE_RECURSION_DEPTH, 1);
            break;
        case 7:
            AF_PARTICLE_UINT("EVENT BURST", event_burst_count, 0, e->capacity, 16);
            break;
        default:
            return false;
        }
    } else if (page == 6) {
        if (row == 3 || row == 8)
            return false;
        static const char *const names[] = {"MAX PARTICLES", "SPAWN %",  "SIM %",  "",
                                            "DIST START",    "DIST END", "HOST %", "VIEW DIST"};
        parameter->label = names[row];
        parameter->minimum = 0;
        parameter->step = 10;
        switch (row) {
        case 0:
            parameter->address = &lod->maximum_particles;
            parameter->integer = true;
            parameter->minimum = 1;
            parameter->maximum = AF_MAX_PARTICLES;
            parameter->step = 256;
            break;
        case 1:
            parameter->address = &lod->spawn_rate_scale;
            parameter->scale = 100;
            parameter->maximum = 100;
            parameter->step = 5;
            break;
        case 2:
            parameter->address = &lod->simulation_frequency_scale;
            parameter->scale = 100;
            parameter->minimum = 1;
            parameter->maximum = 100;
            parameter->step = 5;
            break;
        case 4:
            parameter->address = &lod->distance_start;
            parameter->maximum = lod->distance_end;
            break;
        case 5:
            parameter->address = &lod->distance_end;
            parameter->minimum = lod->distance_start;
            break;
        case 6:
            parameter->address = &lod->host_quality;
            parameter->scale = 100;
            parameter->maximum = 100;
            parameter->step = 5;
            break;
        case 7:
            parameter->address = &lod->view_distance;
            break;
        default:
            return false;
        }
    } else
        return false;
#undef AF_PARTICLE_UINT
#undef AF_PARTICLE_FLOAT
    return true;
}

static double af_editor_particle_value(const AfEditorParticleParameter *parameter) {
    return (parameter->integer ? (double)*(const uint32_t *)parameter->address
                               : (double)*(const float *)parameter->address) *
           parameter->scale;
}

static bool af_editor_particle_field_rect(const AfEditorState *editor, int width,
                                          AfEditorInspectorField field, float *x, float *w) {
    AfEditorParticleParameter parameter;
    if (!af_editor_particle_parameter(editor, field, &parameter))
        return false;
    const uint32_t offset = (uint32_t)(field - AF_EDITOR_INSPECTOR_PARTICLE_FIRST);
    const uint32_t page = offset / 9, row = offset % 9;
    const bool pair = (page == 1 && (row == 6 || row == 8)) || (page == 3 && row >= 7);
    const float panel_x = (float)width - editor->right_width + 18;
    const float available = editor->right_width - 36;
    *w = pair ? (available - 6) * .5F : available - 112;
    *x = panel_x + (pair ? (row == 8 ? *w + 6 : 0) : 112);
    return true;
}

static bool af_editor_apply_particle_number(AfEditorState *editor, AfEditorInspectorField field,
                                            double value, bool merge) {
    AfEditorParticleParameter parameter;
    AfParticleEmitter *emitter =
        af_project_find_node_emitter(&editor->project, editor->selected_id);
    AfProjectNodeSnapshot before = {0};
    if (!isfinite(value) || !af_editor_particle_parameter(editor, field, &parameter) ||
        af_project_snapshot(&editor->project, editor->selected_id, &before) != AF_RESULT_SUCCESS)
        return false;
    value = fmax(parameter.minimum, fmin(parameter.maximum, value));
    if (parameter.integer && floor(value) != value)
        return false;
    const AfParticleEmitter previous = *emitter;
    const AfVfxLodSettings previous_lod = editor->project.particle_data.lod;
    if (parameter.integer)
        *(uint32_t *)(void *)parameter.address = (uint32_t)value;
    else
        *(float *)(void *)parameter.address = (float)(value / parameter.scale);
    if (emitter->burst_count > emitter->capacity)
        emitter->burst_count = emitter->capacity;
    if (emitter->event_burst_count > emitter->capacity)
        emitter->event_burst_count = emitter->capacity;
    if (emitter->attractor_strength != 0 && emitter->attractor_radius <= 0)
        emitter->attractor_radius = 100;
    if (emitter->repulsor_strength != 0 && emitter->repulsor_radius <= 0)
        emitter->repulsor_radius = 100;
    if (emitter->vector_field_strength != 0 && emitter->vector_field_direction.x == 0 &&
        emitter->vector_field_direction.y == 0)
        emitter->vector_field_direction.x = 1;
    if (af_particle_data_validate(&editor->project.particle_data, &editor->project.scene) !=
        AF_RESULT_SUCCESS) {
        *emitter = previous;
        editor->project.particle_data.lod = previous_lod;
        return false;
    }
    if (memcmp(emitter, &previous, sizeof(previous)) == 0 &&
        memcmp(&editor->project.particle_data.lod, &previous_lod, sizeof(previous_lod)) == 0)
        return true;
    if (af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_PARTICLE, merge) !=
        AF_RESULT_SUCCESS) {
        *emitter = previous;
        editor->project.particle_data.lod = previous_lod;
        return false;
    }
    af_editor_mark_dirty(editor);
    return true;
}
