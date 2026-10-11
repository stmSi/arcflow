/* Arcflow supplies document buffers, interaction state and appearance to the reusable UI. */
static AfUiContext *af_editor_ui(AfEditorState *editor) {
    AfUiContext *ui = &editor->ui;
    ui->scene = &editor->render_scene;
    ui->paths = &editor->render_path_data;
    ui->text_engine = editor->text_engine;
    ui->glyphs = editor->text_glyphs;
    ui->lines = editor->text_lines;
    ui->glyph_capacity = AF_EDITOR_TEXT_GLYPH_CAPACITY;
    ui->line_capacity = AF_EDITOR_TEXT_LINE_CAPACITY;
    ui->font_id = AF_EDITOR_TEXT_FONT_ID;
    ui->node_id_base = AF_EDITOR_UI_NODE_ID_BASE;
    ui->pointer = editor->mouse_position;
    ui->pointer_down = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0;
    ui->frame = editor->rendered_frames;
    ui->reduced_motion = editor->preferences.accessibility.reduced_motion != AF_REDUCED_MOTION_OFF;
    ui->high_contrast = editor->preferences.accessibility.high_contrast;
    ui->contrast_border = AF_UI_MUTED;
    const bool covered = editor->command_palette.open || editor->top_menu != AF_EDITOR_MENU_NONE ||
                         editor->context_menu != AF_EDITOR_CONTEXT_NONE || editor->choice_menu ||
                         editor->shortcuts_overlay || editor->welcome_visible ||
                         editor->template_panel_visible || editor->recent_panel_visible ||
                         editor->export_panel_visible || editor->offset_panel_open;
    ui->interaction_blocked_below_layer = covered ? 210000 : INT32_MIN;
    return ui;
}

static void af_editor_add_shape(AfEditorState *editor, AfNodeType type, AfAffine2 transform,
                                AfVec2 bounds, AfColor color, float radius, int32_t layer) {
    af_ui_add_shape(af_editor_ui(editor), type, transform, bounds, color, radius, layer);
}

static void af_editor_add_rect(AfEditorState *editor, float x, float y, float width, float height,
                               AfColor color, int32_t layer) {
    af_ui_add_rect(af_editor_ui(editor), x, y, width, height, color, layer);
}

static bool af_editor_hovered_rect(const AfEditorState *editor, float x, float y, float width,
                                   float height) {
    return af_ui_point_in_rect(editor->mouse_position, x, y, width, height);
}

static void af_editor_add_surface(AfEditorState *editor, float x, float y, float width,
                                  float height, AfColor color, float radius, int32_t layer) {
    af_ui_add_surface(af_editor_ui(editor), x, y, width, height, color, radius, layer);
}

static void af_editor_add_button_rect(AfEditorState *editor, float x, float y, float width,
                                      float height, AfColor color, int32_t layer) {
    af_ui_add_button_rect(af_editor_ui(editor), x, y, width, height, color, layer);
}

static void af_editor_add_button_rounded(AfEditorState *editor, float x, float y, float width,
                                         float height, AfColor color, float radius, int32_t layer) {
    af_ui_add_button_rounded(af_editor_ui(editor), x, y, width, height, color, radius, layer);
}

static void af_editor_add_line(AfEditorState *editor, AfVec2 start, AfVec2 end, float thickness,
                               AfColor color, int32_t layer) {
    af_ui_add_line(af_editor_ui(editor), start, end, thickness, color, layer);
}

static void af_editor_add_text(AfEditorState *editor, float x, float y, const char *text,
                               float scale, float maximum_width, AfColor color, int32_t layer) {
    af_ui_add_text(af_editor_ui(editor), x, y, text, scale, maximum_width, color, layer);
}

static void af_editor_finish_text_path(AfEditorState *editor, uint32_t command_start, AfVec2 origin,
                                       AfColor color, int32_t layer) {
    af_ui_finish_text_path(af_editor_ui(editor), command_start, origin, color, layer);
}

/* Exercise cache identity across movement, UI insertion, duplicate labels and edits. */
static AfResult af_editor_text_cache_smoke(AfEditorState *editor) {
    const uint32_t node_start = editor->render_scene.count;
    const uint32_t path_start = editor->render_path_data.path_count;
    const uint32_t command_start = editor->render_path_data.command_count;
    const AfColor color = {1, 1, 1, 1};
    AfPath original = {0};
    bool valid = true;
    af_editor_add_text(editor, 20, 30, "CACHE", 1, 200, color, 1);
    if (editor->render_path_data.path_count != path_start + 1U) {
        valid = false;
    } else {
        original = editor->render_path_data.paths[path_start];
    }
    editor->render_scene.count = node_start;
    editor->render_path_data.path_count = path_start;
    editor->render_path_data.command_count = command_start;
    af_editor_add_rect(editor, 0, 0, 10, 10, color, 0);
    af_editor_add_text(editor, -150.25F, 420.5F, "CACHE", 1, 200, (AfColor){0, 1, 0, 1}, 2);
    if (editor->render_path_data.path_count != path_start + 1U) {
        valid = false;
    } else {
        const AfPath *moved = &editor->render_path_data.paths[path_start];
        const AfSceneNode *node = &editor->render_scene.nodes[node_start + 1U];
        valid = valid && original.id == moved->id && original.node_id != moved->node_id &&
                original.geometry_generation == moved->geometry_generation &&
                original.command_count == moved->command_count &&
                node->world_transform.tx == -150.25F && node->world_transform.ty == 420.5F;
    }
    af_editor_add_text(editor, 50, 60, "CACHE", 1, 200, color, 1);
    af_editor_add_text(editor, 50, 60, "EDIT", 1, 200, color, 1);
    if (editor->render_path_data.path_count != path_start + 3U) {
        valid = false;
    } else {
        const AfPath *duplicate = &editor->render_path_data.paths[path_start + 1U];
        const AfPath *edited = &editor->render_path_data.paths[path_start + 2U];
        valid = valid && duplicate->id != original.id &&
                duplicate->geometry_generation == original.geometry_generation &&
                edited->id != original.id &&
                edited->geometry_generation != original.geometry_generation;
    }
    editor->render_scene.count = node_start;
    editor->render_path_data.path_count = path_start;
    editor->render_path_data.command_count = command_start;
    if (!valid) {
        af_log(AF_LOG_ERROR, "Text cache identity smoke failed");
    }
    return valid ? AF_RESULT_SUCCESS : AF_RESULT_INVALID_ARGUMENT;
}

static const AfEditorContextMenuInfo *af_editor_context_info(AfEditorContextMenu menu) {
    static const AfEditorContextMenuInfo menus[] = {
        {0},
        {.title = "SCENE",
         .items = {"DUPLICATE", "RENAME", "UNPARENT", "DELETE", "GROUP SELECTION"},
         .item_count = 5U},
        {.title = "CANVAS",
         .items = {"ADD RECTANGLE", "ADD ROUNDED RECT", "ADD ELLIPSE", "ADD PATH", "TOGGLE GRID",
                   "TOGGLE SNAP"},
         .item_count = 6U},
        {.title = "INSPECTOR",
         .items = {"RESET TRANSFORM", "RESET APPEARANCE", "BRING FORWARD", "SEND BACKWARD",
                   "DUPLICATE", "DELETE"},
         .item_count = 6U},
        {.title = "TIMELINE",
         .items = {"ADD KEYFRAME", "DELETE KEYFRAMES", "COPY KEYFRAMES", "PASTE KEYFRAMES",
                   "TOGGLE CURVES", "NEW CLIP"},
         .item_count = 6U},
        {.title = "TOOLS",
         .items = {"SAVE PROJECT", "COMPILE PACKAGE", "TOGGLE GRID", "TOGGLE SNAP", "UNDO", "REDO"},
         .item_count = 6U},
        {.title = "RECTANGLE",
         .items = {"DUPLICATE", "RENAME", "MAKE ROUNDED", "FIT TO CANVAS", "TOGGLE PARTICLES",
                   "BRING FORWARD", "DELETE", "CONVERT TO PATH"},
         .item_count = 8U},
        {.title = "ROUNDED RECTANGLE",
         .items = {"DUPLICATE", "RENAME", "INCREASE RADIUS", "MAKE RECTANGLE", "TOGGLE PARTICLES",
                   "BRING FORWARD", "DELETE", "CONVERT TO PATH"},
         .item_count = 8U},
        {.title = "ELLIPSE",
         .items = {"DUPLICATE", "RENAME", "MAKE CIRCLE", "FIT TO CANVAS", "TOGGLE PARTICLES",
                   "BRING FORWARD", "DELETE", "CONVERT TO PATH"},
         .item_count = 8U},
        {.title = "VECTOR PATH",
         .items = {"DUPLICATE", "RENAME", "TOGGLE FILL RULE", "EDIT POINTS", "TOGGLE PARTICLES",
                   "REVERSE CONTOUR", "DELETE"},
         .item_count = 7U},
        {.title = "GROUP",
         .items = {"DUPLICATE", "RENAME", "ADD RECT CHILD", "UNPARENT", "TOGGLE PARTICLES",
                   "DELETE"},
         .item_count = 6U},
        {.title = "IMAGE",
         .items = {"DUPLICATE", "RENAME", "SELECT ASSET", "REPAIR / REIMPORT...", "CREATE FLIPBOOK",
                   "TOGGLE NINE-SLICE", "DELETE"},
         .item_count = 7U},
        {.title = "IMAGE ASSET",
         .items = {"SELECT USERS", "CREATE FLIPBOOK", "REPAIR / REIMPORT...", "PACK ATLASES",
                   "DELETE UNUSED ASSET"},
         .item_count = 5U},
        {.title = "PARTICLE EMITTER",
         .items = {"RESET EMITTER", "TRIGGER BURST", "TOGGLE ENABLED", "NEXT OUTPUT", "NEXT SHAPE",
                   "OPEN LOD PAGE", "REMOVE EMITTER"},
         .item_count = 7U}};
    return menu > AF_EDITOR_CONTEXT_NONE && menu <= AF_EDITOR_CONTEXT_PARTICLE ? &menus[menu]
                                                                               : NULL;
}

static AfEditorContextMenu af_editor_context_for_node(const AfSceneNode *node) {
    if (node == NULL) {
        return AF_EDITOR_CONTEXT_CANVAS;
    }
    switch (node->type) {
    case AF_NODE_RECTANGLE:
        return AF_EDITOR_CONTEXT_RECTANGLE;
    case AF_NODE_ROUNDED_RECTANGLE:
        return AF_EDITOR_CONTEXT_ROUNDED_RECTANGLE;
    case AF_NODE_ELLIPSE:
        return AF_EDITOR_CONTEXT_ELLIPSE;
    case AF_NODE_PATH:
        return AF_EDITOR_CONTEXT_PATH;
    case AF_NODE_GROUP:
        return AF_EDITOR_CONTEXT_GROUP;
    case AF_NODE_IMAGE:
        return AF_EDITOR_CONTEXT_IMAGE;
    default:
        return AF_EDITOR_CONTEXT_CANVAS;
    }
}
