static void af_editor_collaboration_release(AfEditorState *editor) {
    if (editor->project_lock.acquired) {
        (void)af_project_lock_release(&editor->project_lock);
    }
    editor->project_lock = (AfProjectLock){0};
    editor->project_fingerprint = (AfProjectFingerprint){0};
    editor->project_lock_owner[0] = '\0';
    editor->project_read_only = false;
    editor->external_change_visible = false;
    editor->external_check_ticks = 0U;
}

static void af_editor_collaboration_track_path(AfEditorState *editor) {
    AfResult lock_result = AF_RESULT_SUCCESS;
    af_editor_collaboration_release(editor);
    if (editor->project_path[0] == '\0') {
        return;
    }
    (void)af_project_fingerprint_file(editor->project_path, AF_PROJECT_MAX_FILE_SIZE,
                                      &editor->project_fingerprint);
    lock_result = af_project_lock_acquire(editor->project_path, "arcflow-editor",
                                          &editor->project_lock, editor->project_lock_owner,
                                          sizeof(editor->project_lock_owner));
    if (lock_result == AF_RESULT_STALE_HANDLE) {
        editor->project_read_only = true;
    } else if (lock_result != AF_RESULT_SUCCESS) {
        (void)snprintf(editor->project_lock_owner, sizeof(editor->project_lock_owner),
                       "lock-unavailable");
        editor->project_read_only = true;
    }
}

static void af_editor_collaboration_saved(AfEditorState *editor) {
    if (editor->project_path[0] == '\0') {
        return;
    }
    if (af_project_fingerprint_file(editor->project_path, AF_PROJECT_MAX_FILE_SIZE,
                                    &editor->project_fingerprint) == AF_RESULT_SUCCESS) {
        editor->external_change_visible = false;
        editor->external_check_ticks = 0U;
    }
}

static void af_editor_collaboration_update(AfEditorState *editor, uint64_t now_ns) {
    AfProjectFingerprint current = {0};
    if (editor->project_path[0] == '\0' ||
        (editor->external_check_ticks != 0U &&
         now_ns - editor->external_check_ticks < UINT64_C(1000000000))) {
        return;
    }
    editor->external_check_ticks = now_ns;
    if (af_project_fingerprint_file(editor->project_path, AF_PROJECT_MAX_FILE_SIZE, &current) ==
            AF_RESULT_SUCCESS &&
        af_project_fingerprint_changed(&editor->project_fingerprint, &current) &&
        !editor->external_change_visible) {
        editor->external_change_visible = true;
        editor->validation_visible = true;
        af_editor_status(editor,
                         editor->dirty
                             ? "Project changed on disk; reload or Save As to avoid overwriting"
                             : "Project changed on disk; use File > Reload From Disk");
    }
}
