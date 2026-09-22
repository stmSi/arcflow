static AfEditorTooltip af_editor_find_tooltip(AfEditorState *editor, int width, int height) {
    const AfVec2 mouse = editor->mouse_position;
    const float panel_x = (float)width - editor->right_width + 18.0F;
    const float timeline_top = af_editor_timeline_top(editor, height);
    const float center_right = (float)width - editor->right_width;
    AfEditorTooltip tooltip = {0};
    if (editor->drag_mode != AF_EDITOR_DRAG_NONE || editor->rename_active ||
        editor->inspector_field != AF_EDITOR_INSPECTOR_NONE) {
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
    if (editor->top_menu != AF_EDITOR_MENU_NONE || editor->choice_menu) {
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
    if (mouse.y >= AF_EDITOR_MENU_HEIGHT && mouse.y < editor->toolbar_height &&
        mouse.x >= editor->left_width && mouse.x < center_right) {
        const float local_x = mouse.x - af_editor_toolbar_origin(editor, width);
        const float tools_width = (float)AF_EDITOR_TOOL_COUNT * 42.0F;
        if (local_x >= 0.0F && local_x < tools_width) {
            const uint32_t tool = (uint32_t)(local_x / 42.0F);
            const float button_x = (float)tool * 42.0F;
            static const char *const summaries[AF_EDITOR_TOOL_COUNT] = {
                "Select, move, scale, rotate, skew, and reposition pivots.",
                "Edit path anchors and animated Bézier handles.",
                "Cut a selected line or curve into two editable contours.",
                "Remove the clicked segment while preserving the surrounding contours.",
                "Drag a straight knife through every segment that should be split.",
                "Drag an editable rectangle directly on the canvas.",
                "Drag a live rounded rectangle with an editable radius.",
                "Drag an ellipse; hold Shift for a circle.",
                "Drag an open stroked line; Shift snaps its angle.",
                "Click anchors or drag symmetric Bézier handles; Enter finishes.",
                "Draw a bounded freehand stroke with automatic simplification.",
                "Drag a six-sided editable polygon.",
                "Drag a five-point editable star.",
                "Drag an open three-quarter Bézier arc.",
                "Drag a bounded three-turn editable spiral.",
                "Click for artistic text, drag a paragraph frame, or click existing text to edit."};
            static const char *const shortcuts[AF_EDITOR_TOOL_COUNT] = {
                "V", "Shift+V", "X", "D", "J", "R", "Shift+R", "O",
                "L", "P",       "B", "Y", "A", "C", "I",       "T"};
            if (tool < AF_EDITOR_TOOL_COUNT && local_x < button_x + 38.0F) {
                return (AfEditorTooltip){
                    .id = 10U + tool,
                    .title = af_editor_vector_tool_name((AfEditorVectorTool)tool),
                    .summary = summaries[tool],
                    .details = af_editor_vector_tool_is_construction((AfEditorVectorTool)tool)
                                   ? "The result is undoable and uses Arcflow's animatable vector "
                                     "properties."
                                   : "Tool state stays active until another tool is selected.",
                    .shortcut = shortcuts[tool]};
            }
        }
        if (local_x >= tools_width + 8.0F && local_x < tools_width + 96.0F) {
            return mouse.y < AF_EDITOR_MENU_HEIGHT + 22.0F
                       ? (AfEditorTooltip){.id = 30U,
                                           .title = "Canvas Grid",
                                           .summary = "Show or hide the canvas grid.",
                                           .details = "The guide follows canvas zoom and is "
                                                      "excluded from export.",
                                           .shortcut = "G"}
                       : (AfEditorTooltip){
                             .id = 31U,
                             .title = "Snapping",
                             .summary = "Enable or disable construction and editing snaps.",
                             .details = "Construction points snap to the current grid interval.",
                             .shortcut = "N"};
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
        if (af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_COLOR_TARGET_Y, 72.0F,
                                    AF_EDITOR_COLOR_TARGET_HEIGHT)) {
            return (AfEditorTooltip){.id = UINT64_C(0x1fff0),
                                     .title = "Fill Color",
                                     .summary = "Send the color controls to the object fill.",
                                     .details =
                                         "The blue target shows which paint the picker edits.",
                                     .shortcut = "Click Fill, then drag the color controls"};
        }
        if (af_editor_point_in_rect(mouse, panel_x + 78.0F, AF_EDITOR_COLOR_TARGET_Y, 96.0F,
                                    AF_EDITOR_COLOR_TARGET_HEIGHT)) {
            return (AfEditorTooltip){.id = UINT64_C(0x1fff1),
                                     .title = "Outline Color",
                                     .summary = "Send the color controls to the object outline.",
                                     .details = "Enable Outline below if the paint is not visible.",
                                     .shortcut = "Click Outline, then drag the color controls"};
        }
        if (af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_COLOR_SV_Y, AF_EDITOR_COLOR_SV_WIDTH,
                                    AF_EDITOR_COLOR_SV_HEIGHT)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x20000),
                .title = "Saturation / Value",
                .summary = "Drag to choose color intensity and brightness.",
                .details =
                    "Horizontal movement changes saturation; vertical movement changes value.",
                .shortcut = "One drag creates one undoable color edit"};
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
                .title = editor->color_target_outline ? "Outline Alpha" : "Fill Alpha",
                .summary = "Drag to set color transparency.",
                .details =
                    editor->color_target_outline
                        ? "Alpha changes the outline paint without changing object opacity."
                        : "Alpha changes the fill color while object opacity remains independent.",
                .shortcut = "Left is transparent; right is opaque"};
        }
        if (af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_COLOR_HEX_Y,
                                    editor->right_width - 36.0F,
                                    AF_EDITOR_INSPECTOR_FIELD_HEIGHT) ||
            af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_COLOR_COMPONENT_Y,
                                    editor->right_width - 36.0F,
                                    AF_EDITOR_INSPECTOR_FIELD_HEIGHT)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x21000),
                .title = "Exact Color",
                .summary = "Click to enter hexadecimal or 0-255 RGBA values.",
                .details = "Hex accepts #RGB, #RGBA, #RRGGBB, and #RRGGBBAA.",
                .shortcut = "Enter applies · Esc cancels · Tab moves to the next field"};
        }
        if (af_editor_point_in_rect(mouse, panel_x + editor->right_width - 36.0F - 76.0F,
                                    AF_EDITOR_INSPECTOR_LABEL_Y - 4.0F, 76.0F, 17.0F)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x21003),
                .title = "Ratio Lock",
                .summary = "Link width and height, and link scale X and Y.",
                .details = "Changes preserve the selected object's current proportions.",
                .shortcut = "Click RATIO to toggle it for typing, scrubbing, and −/+"};
        }
        if (af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_INSPECTOR_FIELD_Y,
                                    editor->right_width - 36.0F,
                                    5.0F * AF_EDITOR_INSPECTOR_FIELD_STEP)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x21001),
                .title = "Exact Property Value",
                .summary = "Click to type, or drag horizontally to scrub the value.",
                .details = "Position, scale, rotation, opacity, and radius follow existing "
                           "animation tracks; dimensions and layer edit the authored object.",
                .shortcut = "−/+ step · Shift fine drag · Ctrl coarse drag"};
        }
        if (af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_CONVERT_PATH_Y,
                                    editor->right_width - 36.0F, 26.0F)) {
            if (af_editor_is_text(editor, editor->selected_id))
                return (AfEditorTooltip){
                    .id = UINT64_C(0x20030),
                    .title = "Convert text to path",
                    .summary = "Replace editable text with editable glyph outlines.",
                    .details =
                        "Ctrl+Z restores text. Use T or double-click to edit the text content.",
                    .shortcut = "T edits text"};
            return (AfEditorTooltip){
                .id = UINT64_C(0x20030),
                .title = "Vector geometry",
                .summary = "Convert a primitive into editable lines and Bezier points.",
                .details =
                    "Conversion keeps the object's transform and style. New stars and polygons "
                    "keep their shape settings until converted. Ctrl+Z restores the shape.",
                .shortcut = "Double-click a path to edit its points"};
        }
        if (af_editor_point_in_rect(mouse, panel_x, AF_EDITOR_STYLE_BAR_Y,
                                    editor->right_width - 36.0F,
                                    AF_EDITOR_INSPECTOR_FIELD_HEIGHT)) {
            return (AfEditorTooltip){
                .id = UINT64_C(0x21002),
                .title = "Exact Outline Width",
                .summary = "Click to type, drag to scrub, or use −/+.",
                .details = "The value is clamped to the supported 0-1000 px range and is undoable.",
                .shortcut = "Shift fine drag · Ctrl coarse drag"};
        }
        if (mouse.y >= AF_EDITOR_STYLE_ROW_Y && mouse.y < AF_EDITOR_STYLE_ROW_Y + 28.0F &&
            mouse.x >= panel_x) {
            const uint32_t item = (uint32_t)((mouse.x - panel_x) / 38.0F);
            static const char *const titles[] = {"Outline",   "Outline Width -", "Outline Width +",
                                                 "Fill Mode", "Line Cap",        "Line Join"};
            static const char *const summaries[] = {
                "Toggle the vector outline.",       "Make the outline thinner.",
                "Make the outline thicker.",        "Cycle the fill paint mode.",
                "Cycle the outline end-cap style.", "Cycle the outline corner-join style."};
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
        const float local_x = (mouse.x - editor->left_width) /
                              af_editor_timeline_controls_scale(editor, width);
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
                    .details = "Click the property name to open its menu. Arrows step through "
                               "properties valid for the selected object or vector point.",
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
            if (af_editor_timeline_compact_clips(editor, width) &&
                mouse.x >= center_right - 74.0F && mouse.x < center_right - 6.0F) {
                return (AfEditorTooltip){.id = UINT64_C(0x30200),
                                         .title = "Next Animation Clip",
                                         .summary = "Click to cycle through animation clips.",
                                         .details = "The compact timeline shows the active clip.",
                                         .shortcut = "New clip: Alt+C"};
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
        if (row < af_editor_asset_visible_rows(editor) &&
            asset_index < editor->project.image_data.asset_count) {
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
    if (mouse.x < editor->left_width && mouse.y >= editor->toolbar_height + 8.0F) {
        const uint32_t row =
            (uint32_t)((mouse.y - editor->toolbar_height - 8.0F) / AF_EDITOR_ROW_HEIGHT);
        const uint32_t hierarchy_index = editor->hierarchy_scroll + row;
        if (hierarchy_index < editor->hierarchy_count) {
            const uint32_t node_index = editor->hierarchy_order[hierarchy_index];
            const AfSceneNode *node = &editor->project.scene.nodes[node_index];
            return (AfEditorTooltip){
                .id = UINT64_C(0x4000000000000000) | node->id,
                .title = editor->project.names[node_index],
                .summary = "Select or drag this object in the scene hierarchy.",
                .details = "Drop on a group to move inside; insertion lines choose the parent and order.",
                .shortcut = "Shift range / Ctrl toggle / Right click actions"};
        }
        return tooltip;
    }
    if (editor->selected_count == 1U && mouse.x >= editor->left_width && mouse.x < center_right &&
        mouse.y >= editor->toolbar_height && mouse.y < timeline_top) {
        const AfSceneNode *selected =
            af_scene_find_const(&editor->preview_scene, editor->selected_id);
        AfEditorGizmoGeometry geometry = {0};
        if (selected != NULL &&
            af_editor_gizmo_geometry(selected, af_editor_node_pivot(editor, selected),
                                     af_editor_camera(editor, width, height), &geometry)) {
            const AfEditorGizmoHandle handle = af_editor_gizmo_pick(&geometry, mouse, 10.0F);
            if (handle == AF_EDITOR_GIZMO_PIVOT) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x50001),
                    .title = "Transform Pivot",
                    .summary = "Drag the point used by rotation and scaling.",
                    .details = "Moving the pivot keeps the artwork visually stationary and saves "
                               "the pivot with the project.",
                    .shortcut = "Drag the gold ring; rotation uses the updated point"};
            }
            if (handle == AF_EDITOR_GIZMO_SCALE_X || handle == AF_EDITOR_GIZMO_SCALE_Y ||
                handle == AF_EDITOR_GIZMO_SCALE_XY) {
                return (AfEditorTooltip){
                    .id = UINT64_C(0x50002),
                    .title = "Scale",
                    .summary = "Drag to resize from the opposite handle.",
                    .details = "Shift preserves the original aspect ratio. Ctrl scales around the "
                               "object center; combine them for centered proportional scaling.",
                    .shortcut = "Shift ratio · Ctrl center · Shift+Ctrl both"};
            }
            if (handle == AF_EDITOR_GIZMO_ROTATE) {
                return (AfEditorTooltip){.id = UINT64_C(0x50003),
                                         .title = "Rotate",
                                         .summary = "Rotate around the editable pivot.",
                                         .details = "Move the gold pivot ring to change the center "
                                                    "of rotation without moving the artwork.",
                                         .shortcut = "Shift snaps to 15° increments"};
            }
        }
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
