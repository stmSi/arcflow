static AfEditorTooltip af_editor_find_tooltip(AfEditorState *editor, int width, int height) {
    const AfVec2 mouse = editor->mouse_position;
    const float panel_x = (float)width - editor->right_width + 18.0F;
    const float timeline_top = af_editor_timeline_top(editor, height);
    const float center_right = (float)width - editor->right_width;
    AfEditorTooltip tooltip = {0};
    if (editor->drag_mode != AF_EDITOR_DRAG_NONE || editor->rename_active) {
        return tooltip;
    }
    if (editor->context_menu != AF_EDITOR_CONTEXT_NONE) {
        const AfEditorContextMenuInfo *info = af_editor_context_info(editor->context_menu);
        const AfVec2 origin = af_editor_context_origin(editor, width, height);
        const float first_y = origin.y + AF_EDITOR_CONTEXT_HEADER_HEIGHT + 4.0F;
        if (info != NULL && mouse.x >= origin.x && mouse.x < origin.x + AF_EDITOR_CONTEXT_WIDTH &&
            mouse.y >= first_y) {
            const uint32_t item = (uint32_t)((mouse.y - first_y) / AF_EDITOR_CONTEXT_ITEM_HEIGHT);
            if (item < info->item_count) {
                return af_editor_context_tooltip(info->items[item],
                                                 UINT64_C(0x10000) +
                                                     (uint64_t)editor->context_menu * 16U + item);
            }
        }
        return tooltip;
    }
    if (editor->top_menu != AF_EDITOR_MENU_NONE) {
        return tooltip;
    }
    if (fabsf(mouse.x - editor->left_width) <= 5.0F) {
        return (AfEditorTooltip){.id = 1U,
                                 .title = "Resize Scene Hierarchy",
                                 .summary = "Drag to change the left panel width.",
                                 .details =
                                     "The canvas and timeline resize around the docked panel.",
                                 .shortcut = "Drag the vertical divider"};
    }
    if (fabsf(mouse.x - center_right) <= 5.0F) {
        return (AfEditorTooltip){.id = 2U,
                                 .title = "Resize Inspector",
                                 .summary = "Drag to change the right panel width.",
                                 .details =
                                     "Inspector text and controls adapt to the available width.",
                                 .shortcut = "Drag the vertical divider"};
    }
    if (fabsf(mouse.y - timeline_top) <= 5.0F && mouse.x >= editor->left_width &&
        mouse.x < center_right) {
        return (AfEditorTooltip){.id = 3U,
                                 .title = "Resize Timeline",
                                 .summary = "Drag to change the timeline height.",
                                 .details =
                                     "Expands the dope sheet or curve editor without hiding docks.",
                                 .shortcut = "Drag the horizontal divider"};
    }
    if (mouse.y >= AF_EDITOR_MENU_HEIGHT && mouse.y < AF_EDITOR_TOOLBAR_HEIGHT &&
        mouse.x >= editor->left_width) {
        const float local_x = mouse.x - editor->left_width - 14.0F;
        static const AfEditorTooltip tools[] = {
            {.id = 10U,
             .title = "Rectangle Tool",
             .summary = "Create a rectangle at the canvas center.",
             .details = "The new object is selected immediately and every edit is undoable.",
             .shortcut = "Shortcut: 1"},
            {.id = 11U,
             .title = "Rounded Rectangle Tool",
             .summary = "Create a rectangle with rounded corners.",
             .details = "Change its radius later with the Inspector or object context menu.",
             .shortcut = "Shortcut: 2"},
            {.id = 12U,
             .title = "Ellipse Tool",
             .summary = "Create an ellipse at the canvas center.",
             .details = "Use equal width and height for a circle; its fill remains editable.",
             .shortcut = "Shortcut: 3"},
            {.id = 13U,
             .title = "Vector Path Tool",
             .summary = "Create an editable Bézier path.",
             .details = "Select it to edit anchors and controls or use its object context menu.",
             .shortcut = "Shortcut: 4"},
            {.id = 14U,
             .title = "Canvas Grid",
             .summary = "Show or hide the canvas grid.",
             .details = "The guide follows canvas zoom and is excluded from exported packages.",
             .shortcut = "Shortcut: G"},
            {.id = 15U,
             .title = "Snapping",
             .summary = "Enable or disable editing snap increments.",
             .details = "Affects object movement and timeline edits for predictable alignment.",
             .shortcut = "Shortcut: N"}};
        if (local_x >= 0.0F && local_x < 34.0F) {
            return tools[0];
        }
        if (local_x >= 42.0F && local_x < 76.0F) {
            return tools[1];
        }
        if (local_x >= 84.0F && local_x < 118.0F) {
            return tools[2];
        }
        if (local_x >= 126.0F && local_x < 160.0F) {
            return tools[3];
        }
        if (local_x >= 172.0F && local_x < 260.0F) {
            return mouse.y < AF_EDITOR_MENU_HEIGHT + 22.0F ? tools[4] : tools[5];
        }
        return tooltip;
    }
    if (mouse.x >= center_right) {
        const AfSceneNode *selected =
            af_scene_find_const(&editor->project.scene, editor->selected_id);
        if (selected != NULL && selected->type == AF_NODE_IMAGE) {
            if (mouse.y >= 150.0F && mouse.y < 340.0F) {
                const uint32_t row = (uint32_t)((mouse.y - 150.0F) / 38.0F);
                static const char *const titles[] = {"Texture Trimming", "Mip Generation",
                                                     "Color Space", "Compression Setting",
                                                     "Image Display Mode"};
                static const char *const summaries[] = {
                    "Remove transparent edge pixels from atlas storage.",
                    "Generate downsampled levels for stable minification.",
                    "Choose sRGB color or linear data sampling.",
                    "Choose raw or lossless package storage metadata.",
                    "Switch between stretch and nine-slice rendering."};
                if (row < 5U) {
                    return (AfEditorTooltip){
                        .id = UINT64_C(0x26000) + row,
                        .title = titles[row],
                        .summary = summaries[row],
                        .details = "The atlas is rebuilt deterministically after this change.",
                        .shortcut = "Click to toggle the setting"};
                }
            }
            if (mouse.y >= 375.0F && mouse.y < 519.0F) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x26100) + (uint64_t)((mouse.y - 375.0F) / 36.0F),
                    .title = "Nine-Slice Border",
                    .summary = "Adjust the preserved edge width for this sprite.",
                    .details = "Corners remain fixed while center regions stretch to object size.",
                    .shortcut = "Use - and + to edit pixels"};
            }
            if (mouse.y >= 548.0F && mouse.y < 578.0F) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x26200),
                    .title = "Image Flipbook",
                    .summary = "Create or remove a sequence on this image object.",
                    .details = "Creation uses all imported assets in browser order as frames.",
                    .shortcut = "Import at least two images first"};
            }
            if (mouse.y >= 586.0F && mouse.y < 612.0F) {
                return (AfEditorTooltip){.id = UINT64_C(0x26201),
                                         .title = "Flipbook Rate",
                                         .summary = "Change playback frames per second.",
                                         .details =
                                             "Each bound object can override sequence speed.",
                                         .shortcut = "Use - and + from 1 to 120 FPS"};
            }
            if (mouse.y >= 622.0F && mouse.y < 650.0F) {
                return (AfEditorTooltip){.id = UINT64_C(0x26202),
                                         .title = "Flipbook Loop",
                                         .summary = "Toggle looping for this image object.",
                                         .details =
                                             "When disabled, playback holds the final frame.",
                                         .shortcut = "Click to toggle"};
            }
            return (AfEditorTooltip){
                .id = UINT64_C(0x26210),
                .title = "Image Inspector",
                .summary = "Configure the selected image resource and its scene binding.",
                .details =
                    "Source, atlas, display, border, mip, color, and sequence controls are here.",
                .shortcut = "Right click the object for image-specific actions"};
        }
        if (af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_COLOR_SV_Y, AF_EDITOR_COLOR_SV_WIDTH,
                                    AF_EDITOR_COLOR_SV_HEIGHT)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x20000),
                .title = "Saturation / Value",
                .summary = "Drag to choose color intensity and brightness.",
                .details =
                    "Horizontal movement changes saturation; vertical movement changes value.",
                .shortcut = "One drag creates one undoable fill edit"};
        }
        if (af_editor_point_in_rect(mouse, panel_x + AF_EDITOR_COLOR_HUE_X, AF_EDITOR_COLOR_SV_Y,
                                    AF_EDITOR_COLOR_HUE_WIDTH, AF_EDITOR_COLOR_SV_HEIGHT)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x20001),
                .title = "Hue",
                .summary = "Drag to choose the base color family.",
                .details = "Hue is preserved while saturation, value, and alpha stay unchanged.",
                .shortcut = "Drag the vertical rainbow strip"};
        }
        if (af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_COLOR_ALPHA_Y,
                                    AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH,
                                    AF_EDITOR_COLOR_ALPHA_HEIGHT)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x20002),
                .title = "Fill Alpha",
                .summary = "Drag to set color transparency.",
                .details = "Alpha changes the fill color while object opacity remains independent.",
                .shortcut = "Left is transparent; right is opaque"};
        }
        for (uint32_t row = 0U; row < 4U; ++row) {
            const float row_y =
                AF_EDITOR_INSPECTOR_SCALAR_Y + (float)row * AF_EDITOR_INSPECTOR_SCALAR_STEP;
            static const char *const titles[] = {"Opacity", "Corner Radius", "Layer Order",
                                                 "Rotation"};
            static const char *const summaries[] = {
                "Decrease or increase object opacity.", "Decrease or increase corner rounding.",
                "Move the object backward or forward.", "Rotate the object in small increments."};
            static const char *const details[] = {
                "Opacity is clamped from fully transparent to fully visible.",
                "Radius is bounded by the selected shape and remains animatable.",
                "Layer order changes drawing order without changing the parent hierarchy.",
                "Rotation edits merge while repeated and can be animated on the timeline."};
            static const char *const shortcuts[] = {"Value range: 0% to 100%",
                                                    "Inspector or object context menu",
                                                    "Shortcuts: [ and ]", "Shortcuts: Q and E"};
            if (af_editor_point_in_rect(mouse, panel_x, row_y, 70.0F, 26.0F)) {
                return (AfEditorTooltip){.id = UINT64_C(0x21000) + row,
                                         .title = titles[row],
                                         .summary = summaries[row],
                                         .details = details[row],
                                         .shortcut = shortcuts[row]};
            }
        }
        if (mouse.y >= AF_EDITOR_STYLE_ROW_Y && mouse.y < AF_EDITOR_STYLE_ROW_Y + 28.0F &&
            mouse.x >= panel_x) {
            const uint32_t item = (uint32_t)((mouse.x - panel_x) / 38.0F);
            static const char *const titles[] = {"Stroke",    "Stroke Width -", "Stroke Width +",
                                                 "Fill Mode", "Line Cap",       "Line Join"};
            static const char *const summaries[] = {
                "Toggle the vector stroke.",       "Make the stroke thinner.",
                "Make the stroke thicker.",        "Cycle the fill paint mode.",
                "Cycle the stroke end-cap style.", "Cycle the stroke corner-join style."};
            if (item < 6U) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x22000) + item,
                    .title = titles[item],
                    .summary = summaries[item],
                    .details = "Applies to the selected vector style and is undoable.",
                    .shortcut = "Style data is compiled into the runtime package"};
            }
        }
        if (mouse.y >= AF_EDITOR_LAYOUT_ROW_Y && mouse.y < AF_EDITOR_LAYOUT_ROW_Y + 28.0F &&
            mouse.x >= panel_x) {
            const uint32_t item = (uint32_t)((mouse.x - panel_x) / 38.0F);
            static const char *const titles[] = {"Anchors",         "Width Sizing",
                                                 "Height Sizing",   "Flow Direction",
                                                 "Child Alignment", "Aspect Constraint"};
            static const char *const summaries[] = {"Cycle responsive anchor presets.",
                                                    "Cycle fixed, percent, and content width.",
                                                    "Cycle fixed, percent, and content height.",
                                                    "Cycle free, row, and column flow.",
                                                    "Cycle how child objects align.",
                                                    "Cycle aspect-ratio constraints."};
            if (item < 6U) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x23000) + item,
                    .title = titles[item],
                    .summary = summaries[item],
                    .details = "Responsive layout is evaluated against the current canvas.",
                    .shortcut = "Click repeatedly to cycle available modes"};
            }
        }
        if (mouse.y >= AF_EDITOR_PARTICLE_PAGE_Y && mouse.y < AF_EDITOR_PARTICLE_PAGE_Y + 23.0F &&
            mouse.x >= panel_x) {
            const uint32_t page = (uint32_t)((mouse.x - panel_x) / 32.0F);
            static const char *const page_titles[] = {"Emission",  "Motion",    "Forces",
                                                      "Collision", "Rendering", "Sub-Emitters",
                                                      "VFX LOD"};
            if (page < AF_EDITOR_PARTICLE_PAGE_COUNT) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x23800) + page,
                    .title = page_titles[page],
                    .summary = "Open this particle-control page.",
                    .details =
                        "Pages keep the complete VFX interface readable without panel overflow.",
                    .shortcut = "Click a page, then use labeled - and + controls"};
            }
        }
        if (mouse.y >= AF_EDITOR_PARTICLE_ROW_Y &&
            mouse.y < AF_EDITOR_PARTICLE_ROW_Y +
                          AF_EDITOR_PARTICLE_ROW_COUNT * AF_EDITOR_PARTICLE_ROW_STEP &&
            mouse.x >= panel_x && mouse.x < panel_x + 65.0F) {
            const uint32_t row =
                (uint32_t)((mouse.y - AF_EDITOR_PARTICLE_ROW_Y) / AF_EDITOR_PARTICLE_ROW_STEP);
            static const char *const
                titles[AF_EDITOR_PARTICLE_PAGE_COUNT][AF_EDITOR_PARTICLE_ROW_COUNT] = {
                    {"Emitter Enabled", "Spawn Rate", "Minimum Lifetime", "Maximum Lifetime",
                     "Particle Capacity", "Emitter Shape", "Startup Burst", "Random Seed"},
                    {"Minimum X Velocity", "Maximum X Velocity", "Minimum Y Velocity",
                     "Maximum Y Velocity", "Horizontal Gravity", "Vertical Gravity",
                     "Horizontal Wind", "Particle Drag"},
                    {"Curl Turbulence", "Noise Frequency", "Radial Force", "Vortex Force",
                     "Attractor Strength", "Attractor Radius", "Repulsor Strength",
                     "Vector Field Strength"},
                    {"Collision Plane", "Plane Offset", "Restitution", "Collision Friction",
                     "Depth Collision", "Minimum Depth", "Maximum Depth", "Depth Velocity"},
                    {"Particle Output", "Trail Length", "Trail Width", "Mesh Scale",
                     "Decal Rotation", "Distortion", "Soft Fade", "Light Radius"},
                    {"Flipbook Columns", "Flipbook Rows", "Flipbook Frame Rate", "Sub-Emitter",
                     "Birth Spawn Count", "Death Spawn Count", "Recursion Limit", "Event Burst"},
                    {"Maximum Particles", "Spawn Quality", "Simulation Quality", "VFX Renderer",
                     "LOD Start Distance", "LOD End Distance", "Host Quality", "View Distance"}};
            if (row < AF_EDITOR_PARTICLE_ROW_COUNT) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x24000) + (uint64_t)editor->particle_page * 16U + row,
                    .title = titles[editor->particle_page][row],
                    .summary = "Adjust this particle-system parameter.",
                    .details = "The value is bounded, validated, saved, compiled, and undoable.",
                    .shortcut = "Use - and + to change the value"};
            }
        }
        return (AfEditorTooltip){
            .id = UINT64_C(0x25000),
            .title = "Inspector",
            .summary = "Edit the selected object's properties.",
            .details = "Right click for reset, ordering, duplication, and delete actions.",
            .shortcut = "Changes are stored in bounded undo history"};
    }
    if (mouse.y >= timeline_top) {
        const float local_x = mouse.x - editor->left_width;
        if (mouse.y < timeline_top + 42.0F) {
            if (local_x >= 10.0F && local_x < 56.0F) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x30000),
                    .title = "Play / Pause",
                    .summary = "Preview the active animation clip.",
                    .details = "Playback advances the playhead and respects the clip loop region.",
                    .shortcut = "Shortcut: Space"};
            }
            if (local_x >= 62.0F && local_x < 114.0F) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x30001),
                    .title = "Reset Preview",
                    .summary = "Stop preview and return to time zero.",
                    .details =
                        "The scene updates immediately to the first evaluated animation frame.",
                    .shortcut = "Shortcut: Home moves the playhead to zero"};
            }
            if (local_x >= 120.0F && local_x < 178.0F) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x30002),
                    .title = "Curve Editor",
                    .summary = "Toggle curve and dope-sheet views.",
                    .details = "Curve view exposes cubic Bézier tangent handles for selected keys.",
                    .shortcut = "Shortcut: F6"};
            }
            if (local_x >= 188.0F && local_x < 390.0F) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x30100) + (uint64_t)editor->selected_property,
                    .title = "Animated Property",
                    .summary = "Choose which property the key button records.",
                    .details = "Use the arrows to cycle through properties valid for the selected "
                               "object or vector point.",
                    .shortcut = "Canvas handles choose position, scale, skew, or rotation for you"};
            }
            if (local_x >= 398.0F && local_x < 514.0F) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x30120),
                    .title = "Add or Update Keyframe",
                    .summary = "Record the selected property at the playhead.",
                    .details =
                        "After the first key, move the playhead and drag the object; Arcflow "
                        "creates the next key automatically.",
                    .shortcut = "Shortcut: K"};
            }
            for (uint32_t index = 0U; index < editor->project.clip_count && index < 8U; ++index) {
                const float x = center_right - 74.0F * (float)(index + 1U);
                if (mouse.x >= x && mouse.x < x + 68.0F) {
                    return (AfEditorTooltip){
                        .id = UINT64_C(0x30200) + index,
                        .title = editor->project.clips[index].name,
                        .summary = "Select this animation clip.",
                        .details =
                            "Switches the visible tracks and returns its playhead to time zero.",
                        .shortcut = "New clip: Alt+C"};
                }
            }
        }
        if (mouse.y < timeline_top + AF_EDITOR_TIMELINE_RULER_HEIGHT) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x30300),
                .title = "Timeline Ruler",
                .summary = "Click or drag to scrub the playhead.",
                .details = "Drag the upper red marker to scrub. In the lower green lane, drag an "
                           "endpoint to resize the loop or drag the bar to move it.",
                .shortcut = "Wheel zooms; Shift+wheel pans"};
        }
        return (AfEditorTooltip){
            .id = UINT64_C(0x30400),
            .title = editor->curve_visible ? "Animation Curves" : "Dope Sheet",
            .summary = editor->curve_visible ? "Edit easing curves and tangent handles."
                                             : "Select, box-select, and move keyframes.",
            .details = editor->curve_visible
                           ? "Cubic keys expose bounded in and out tangent controls."
                           : "Drag the red playhead line anywhere along its full height to scrub. "
                             "Ctrl toggles key selection; dragging a key moves it.",
            .shortcut = "Drag playhead to scrub; right click for keyframe actions"};
    }
    if (mouse.x < editor->left_width && editor->asset_browser_visible &&
        mouse.y >= af_editor_asset_browser_top(editor, height)) {
        const float row_start = af_editor_asset_browser_top(editor, height) + 48.0F;
        const uint32_t row =
            mouse.y < row_start ? UINT32_MAX : (uint32_t)((mouse.y - row_start) / 27.0F);
        const uint32_t asset_index = editor->asset_scroll + row;
        if (row < 5U && asset_index < editor->project.image_data.asset_count) {
            const AfImageAsset *asset = &editor->project.image_data.assets[asset_index];
            return (AfEditorTooltip){
                .id = UINT64_C(0x4100000000000000) | asset->id,
                .title = asset->name,
                .summary = asset->missing ? "Source file is missing; reimport is required."
                                          : "Select this imported image asset.",
                .details = "The row shows format, dimensions, and scene dependency count.",
                .shortcut = "Wheel scrolls / Right click opens asset actions"};
        }
        return tooltip;
    }
    if (mouse.x < editor->left_width && mouse.y >= AF_EDITOR_TOOLBAR_HEIGHT + 8.0F) {
        const uint32_t row =
            (uint32_t)((mouse.y - AF_EDITOR_TOOLBAR_HEIGHT - 8.0F) / AF_EDITOR_ROW_HEIGHT);
        const uint32_t hierarchy_index = editor->hierarchy_scroll + row;
        if (hierarchy_index < editor->hierarchy_count) {
            const uint32_t node_index = editor->hierarchy_order[hierarchy_index];
            const AfSceneNode *node = &editor->project.scene.nodes[node_index];
            return (AfEditorTooltip){
                .id = UINT64_C(0x4000000000000000) | node->id,
                .title = editor->project.names[node_index],
                .summary = "Select this object in the scene hierarchy.",
                .details = "Shift selects a contiguous range; Ctrl toggles individual objects.",
                .shortcut = "Shift range / Ctrl toggle / Right click actions"};
        }
        return tooltip;
    }
    return tooltip;
}

static void af_editor_render_tooltip(AfEditorState *editor, int width, int height) {
    const AfEditorTooltip tooltip = af_editor_find_tooltip(editor, width, height);
    const uint64_t ticks = SDL_GetTicksNS();
    const bool detailed = editor->tooltip_smoke || (SDL_GetModState() & SDL_KMOD_ALT) != 0U;
    const float popup_width = detailed ? AF_EDITOR_TOOLTIP_DETAIL_WIDTH : AF_EDITOR_TOOLTIP_WIDTH;
    const float popup_height = detailed ? 142.0F : 78.0F;
    float x = editor->mouse_position.x + 16.0F;
    float y = editor->mouse_position.y + 18.0F;
    if (tooltip.id == 0U || tooltip.title == NULL) {
        editor->hovered_tooltip_id = 0U;
        return;
    }
    if (editor->hovered_tooltip_id != tooltip.id) {
        editor->hovered_tooltip_id = tooltip.id;
        editor->tooltip_started_ticks = ticks;
        if (!detailed) {
            return;
        }
    }
    if (!detailed && ticks - editor->tooltip_started_ticks < AF_EDITOR_TOOLTIP_DELAY_NS) {
        return;
    }
    x = af_editor_clamp(x, 8.0F, (float)width - popup_width - 8.0F);
    y = af_editor_clamp(y, 8.0F, (float)height - popup_height - 8.0F);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 5.0F, .ty = y + 6.0F},
                        (AfVec2){popup_width, popup_height}, (AfColor){0.0F, 0.0F, 0.0F, 0.5F},
                        8.0F, 300000);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){popup_width, popup_height}, (AfColor){0.085F, 0.095F, 0.12F, 0.99F}, 8.0F, 300010);
    af_editor_add_rect(editor, x, y, 4.0F, popup_height,
                       detailed ? (AfColor){0.72F, 0.46F, 1.0F, 1.0F}
                                : (AfColor){0.28F, 0.68F, 1.0F, 1.0F},
                       300020);
    af_editor_add_text(editor, x + 14.0F, y + 12.0F, tooltip.title, 1.25F, popup_width - 28.0F,
                       (AfColor){0.92F, 0.94F, 0.98F, 1.0F}, 300030);
    af_editor_add_text(editor, x + 14.0F, y + 34.0F, tooltip.summary, 0.82F, popup_width - 28.0F,
                       (AfColor){0.7F, 0.75F, 0.84F, 1.0F}, 300030);
    if (detailed) {
        af_editor_add_rect(editor, x + 14.0F, y + 57.0F, popup_width - 28.0F, 1.0F,
                           (AfColor){0.25F, 0.28F, 0.35F, 1.0F}, 300025);
        af_editor_add_text(editor, x + 14.0F, y + 67.0F, "MORE DETAILS", 0.7F, popup_width - 28.0F,
                           (AfColor){0.72F, 0.54F, 0.98F, 1.0F}, 300030);
        af_editor_add_text(editor, x + 14.0F, y + 87.0F, tooltip.details, 0.78F,
                           popup_width - 28.0F, (AfColor){0.76F, 0.8F, 0.87F, 1.0F}, 300030);
        af_editor_add_text(editor, x + 14.0F, y + 114.0F, tooltip.shortcut, 0.75F,
                           popup_width - 28.0F, (AfColor){0.42F, 0.76F, 1.0F, 1.0F}, 300030);
    } else {
        af_editor_add_text(editor, x + 14.0F, y + 58.0F, "HOLD ALT FOR MORE DETAILS", 0.62F,
                           popup_width - 28.0F, (AfColor){0.48F, 0.66F, 0.9F, 1.0F}, 300030);
    }
}
