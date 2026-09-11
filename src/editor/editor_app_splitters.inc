static float af_editor_toolbar_scroll_limit(const AfEditorState *editor, int width) {
    return fmaxf(0.0F, (float)AF_EDITOR_TOOL_COUNT * 42.0F + 116.0F -
                           ((float)width - editor->left_width - editor->right_width));
}

static float af_editor_toolbar_origin(const AfEditorState *editor, int width) {
    return editor->left_width + 14.0F -
           fminf(editor->toolbar_scroll, af_editor_toolbar_scroll_limit(editor, width));
}

static AfEditorDragMode af_editor_splitter_at(const AfEditorState *editor, int width, int height) {
    const AfVec2 p = editor->mouse_position;
    const float right = (float)width - editor->right_width;
    const float timeline = (float)height - editor->status_height - editor->timeline_height;
    if (p.y < AF_EDITOR_MENU_HEIGHT) {
        return AF_EDITOR_DRAG_NONE;
    }
    if (fabsf(p.x - editor->left_width) <= 5.0F)
        return AF_EDITOR_DRAG_LEFT_DOCK;
    if (fabsf(p.x - right) <= 5.0F)
        return AF_EDITOR_DRAG_RIGHT_DOCK;
    if (p.x < right && fabsf(p.y - timeline) <= 5.0F)
        return AF_EDITOR_DRAG_TIMELINE_HEIGHT;
    if (p.x < editor->left_width && editor->asset_browser_visible &&
        fabsf(p.y - (timeline - editor->asset_height)) <= 5.0F)
        return AF_EDITOR_DRAG_ASSET_HEIGHT;
    if (p.x > editor->left_width && p.x < right) {
        if (fabsf(p.y - editor->toolbar_height) <= 4.0F)
            return AF_EDITOR_DRAG_TOOLBAR_HEIGHT;
        if (fabsf(p.y - ((float)height - editor->status_height)) <= 4.0F)
            return AF_EDITOR_DRAG_STATUS_HEIGHT;
        if (p.y > timeline + AF_EDITOR_TIMELINE_RULER_HEIGHT &&
            fabsf(p.x - editor->left_width - editor->timeline_header_width) <= 4.0F)
            return AF_EDITOR_DRAG_TIMELINE_HEADER;
    }
    return AF_EDITOR_DRAG_NONE;
}

static bool af_editor_is_splitter(AfEditorDragMode mode) {
    return mode == AF_EDITOR_DRAG_LEFT_DOCK || mode == AF_EDITOR_DRAG_RIGHT_DOCK ||
           mode == AF_EDITOR_DRAG_TIMELINE_HEIGHT || mode == AF_EDITOR_DRAG_ASSET_HEIGHT ||
           mode == AF_EDITOR_DRAG_TOOLBAR_HEIGHT || mode == AF_EDITOR_DRAG_STATUS_HEIGHT ||
           mode == AF_EDITOR_DRAG_TIMELINE_HEADER;
}

static void af_editor_render_splitters(AfEditorState *editor, int width, int height) {
    const AfEditorDragMode mode = af_editor_is_splitter(editor->drag_mode) ? editor->drag_mode
                                  : editor->drag_mode == AF_EDITOR_DRAG_NONE
                                      ? af_editor_splitter_at(editor, width, height)
                                      : AF_EDITOR_DRAG_NONE;
    const float right = (float)width - editor->right_width;
    const float timeline = (float)height - editor->status_height - editor->timeline_height;
    const AfColor idle = {0.25F, 0.3F, 0.38F, 1};
    const AfColor hover = {0.25F, 0.73F, 1, 1};
    const AfEditorDragMode modes[] = {AF_EDITOR_DRAG_LEFT_DOCK,       AF_EDITOR_DRAG_RIGHT_DOCK,
                                      AF_EDITOR_DRAG_TIMELINE_HEIGHT, AF_EDITOR_DRAG_TOOLBAR_HEIGHT,
                                      AF_EDITOR_DRAG_STATUS_HEIGHT,   AF_EDITOR_DRAG_ASSET_HEIGHT,
                                      AF_EDITOR_DRAG_TIMELINE_HEADER};
    const float rects[][4] = {
        {editor->left_width - 2, AF_EDITOR_MENU_HEIGHT, 4, (float)height - AF_EDITOR_MENU_HEIGHT},
        {right - 2, AF_EDITOR_MENU_HEIGHT, 4, (float)height - AF_EDITOR_MENU_HEIGHT},
        {0, timeline - 2, right, 4},
        {editor->left_width, editor->toolbar_height - 2, right - editor->left_width, 4},
        {editor->left_width, (float)height - editor->status_height - 2, right - editor->left_width,
         4},
        {0, timeline - editor->asset_height - 2, editor->left_width, 4},
        {editor->left_width + editor->timeline_header_width - 2,
         timeline + AF_EDITOR_TIMELINE_RULER_HEIGHT, 4,
         editor->timeline_height - AF_EDITOR_TIMELINE_RULER_HEIGHT}};
    for (uint32_t index = 0U; index < 7U; ++index) {
        if (modes[index] == AF_EDITOR_DRAG_ASSET_HEIGHT && !editor->asset_browser_visible)
            continue;
        af_editor_add_rect(editor, rects[index][0], rects[index][1], rects[index][2],
                           rects[index][3], mode == modes[index] ? hover : idle, 100145);
    }
    if (editor->resize_horizontal_cursor == NULL) {
        editor->resize_horizontal_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
        editor->resize_vertical_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
    }
    SDL_Cursor *cursor = SDL_GetDefaultCursor();
    if (mode == AF_EDITOR_DRAG_LEFT_DOCK || mode == AF_EDITOR_DRAG_RIGHT_DOCK ||
        mode == AF_EDITOR_DRAG_TIMELINE_HEADER)
        cursor = editor->resize_horizontal_cursor;
    else if (mode != AF_EDITOR_DRAG_NONE)
        cursor = editor->resize_vertical_cursor;
    if (cursor != NULL)
        SDL_SetCursor(cursor);
}
