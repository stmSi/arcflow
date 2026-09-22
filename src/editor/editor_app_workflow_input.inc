static bool af_editor_workflow_mouse_down(AfEditorState *editor, int width, int height, float x,
                                          float y) {
    if (editor->text_panel_open) {
        af_editor_text_panel_mouse(editor, width, height, x, y);
        return true;
    }
    if (editor->offset_panel_open) {
        const float left = ((float)width - 540.0F) * 0.5F, top = ((float)height - 210.0F) * 0.5F;
        if (y >= top + 166 && y <= top + 204 && x >= left && x <= left + 540) {
            if (x < left + 270)
                af_editor_apply_offset_panel(editor);
            else
                editor->offset_panel_open = false;
        }
        return true;
    }
    if (af_editor_onboarding_mouse_down(editor, width, height, x, y)) {
        return true;
    }
    if (editor->export_panel_visible) {
        static const AfEditorDialogAction actions[] = {
            AF_EDITOR_DIALOG_EXPORT_PACKAGE,   AF_EDITOR_DIALOG_EXPORT_LOTTIE,
            AF_EDITOR_DIALOG_EXPORT_DOTLOTTIE, AF_EDITOR_DIALOG_EXPORT_SVG,
            AF_EDITOR_DIALOG_EXPORT_GIF,       AF_EDITOR_DIALOG_EXPORT_MP4};
        const float box_x = ((float)width - 780.0F) * 0.5F;
        const float box_y = ((float)height - 500.0F) * 0.5F;
        if (x >= box_x + 24.0F && y >= box_y + 78.0F) {
            const uint32_t column = (uint32_t)((x - box_x - 24.0F) / 250.0F);
            const uint32_t row = (uint32_t)((y - box_y - 78.0F) / 178.0F);
            const uint32_t index = row * 3U + column;
            const float card_x = box_x + 24.0F + (float)column * 250.0F;
            const float card_y = box_y + 78.0F + (float)row * 178.0F;
            if (column < 3U && row < 2U && index < 6U && x < card_x + 232.0F &&
                y < card_y + 160.0F) {
                editor->export_panel_visible = false;
                af_editor_show_project_dialog(editor, actions[index]);
            }
        }
        if (x < box_x || x >= box_x + 780.0F || y < box_y || y >= box_y + 500.0F) {
            editor->export_panel_visible = false;
            af_editor_status(editor, "Export canceled");
        }
        return true;
    }
    if (editor->platform_panel_visible) {
        editor->platform_panel_visible = false;
        return true;
    }
    if (editor->update_panel_visible && !editor->dialog_active) {
        editor->update_panel_visible = false;
        return true;
    }
    if (editor->template_panel_visible) {
        const float box_x = ((float)width - 780.0F) * 0.5F;
        const float box_y = ((float)height - 410.0F) * 0.5F;
        if (x >= box_x + 18.0F && x < box_x + 768.0F && y >= box_y + 75.0F && y < box_y + 352.0F) {
            const uint32_t column = (uint32_t)((x - box_x - 18.0F) / 250.0F);
            const uint32_t row = (uint32_t)((y - box_y - 75.0F) / 145.0F);
            const uint32_t index = row * 3U + column;
            const float card_x = box_x + 18.0F + (float)column * 250.0F;
            const float card_y = box_y + 75.0F + (float)row * 145.0F;
            if (index < af_project_template_count() && x < card_x + 238.0F && y < card_y + 132.0F) {
                editor->template_selected_index = index;
                editor->template_panel_visible = false;
                (void)af_editor_create_template(editor, index);
            }
        }
        return true;
    }
    if (editor->recent_panel_visible) {
        const float box_x = ((float)width - 650.0F) * 0.5F;
        const float box_y = ((float)height - 390.0F) * 0.5F;
        if (x >= box_x + 14.0F && x < box_x + 636.0F && y >= box_y + 76.0F) {
            const uint32_t row = (uint32_t)((y - box_y - 76.0F) / 32.0F);
            const float row_y = box_y + 76.0F + (float)row * 32.0F;
            if (row < editor->preferences.recent_project_count && row < 9U && y < row_y + 29.0F) {
                editor->recent_selected_index = row;
                af_editor_open_recent_index(editor, row);
            }
        }
        return true;
    }
    if (editor->command_palette.open) {
        const float box_x = ((float)width - AF_EDITOR_WORKFLOW_OVERLAY_WIDTH) * 0.5F;
        const float first_y = 170.0F;
        if (x >= box_x + 12.0F && x < box_x + AF_EDITOR_WORKFLOW_OVERLAY_WIDTH - 12.0F &&
            y >= first_y) {
            const uint32_t row = (uint32_t)((y - first_y) / 38.0F);
            const float row_y = first_y + (float)row * 38.0F;
            const uint32_t first_result =
                (editor->command_palette.selected_index / AF_EDITOR_WORKFLOW_PALETTE_ROWS) *
                AF_EDITOR_WORKFLOW_PALETTE_ROWS;
            if (first_result + row < editor->command_palette.result_count &&
                row < AF_EDITOR_WORKFLOW_PALETTE_ROWS && y < row_y + 34.0F) {
                const AfEditorActionId action_id =
                    editor->command_palette.results[first_result + row].action_id;
                af_editor_command_palette_close(&editor->command_palette);
                (void)af_editor_action_execute(&editor->actions, action_id);
            }
        }
        return true;
    }
    if (editor->shortcuts_overlay) {
        const float box_width = 680.0F;
        const float box_height = 498.0F;
        const float box_x = ((float)width - box_width) * 0.5F;
        const float box_y = ((float)height - box_height) * 0.5F;
        uint32_t start =
            editor->shortcut_selected_index >= AF_EDITOR_WORKFLOW_SHORTCUT_ROWS / 2U
                ? editor->shortcut_selected_index - AF_EDITOR_WORKFLOW_SHORTCUT_ROWS / 2U
                : 0U;
        if (start + AF_EDITOR_WORKFLOW_SHORTCUT_ROWS > editor->actions.count) {
            start = editor->actions.count > AF_EDITOR_WORKFLOW_SHORTCUT_ROWS
                        ? editor->actions.count - AF_EDITOR_WORKFLOW_SHORTCUT_ROWS
                        : 0U;
        }
        if (x >= box_x + 12.0F && x < box_x + box_width - 12.0F && y >= box_y + 70.0F &&
            y < box_y + 70.0F + AF_EDITOR_WORKFLOW_SHORTCUT_ROWS * 33.0F) {
            const uint32_t row = (uint32_t)((y - box_y - 70.0F) / 33.0F);
            const float row_y = box_y + 70.0F + (float)row * 33.0F;
            if (start + row < editor->actions.count && y < row_y + 30.0F) {
                editor->shortcut_selected_index = start + row;
                editor->shortcut_capture = true;
            }
        }
        return true;
    }
    if (editor->recovery_overlay) {
        const float box_x = ((float)width - 540.0F) * 0.5F;
        const float box_y = ((float)height - 270.0F) * 0.5F;
        if (x >= box_x + 22.0F && x < box_x + 518.0F && y >= box_y + 90.0F && y < box_y + 128.0F) {
            const uint32_t current = editor->preferences.autosave_interval_seconds;
            editor->preferences.autosave_interval_seconds =
                current == 300U ? 0U : (current == 0U ? 30U : (current == 30U ? 120U : 300U));
            return true;
        }
        if (y >= box_y + 148.0F && y < box_y + 190.0F && x >= box_x + 22.0F && x < box_x + 258.0F) {
            if (editor->recovery_candidate.status == AF_EDITOR_RECOVERY_STATUS_AVAILABLE &&
                af_editor_recovery_restore(&editor->recovery, AF_PROJECT_FORMAT_VERSION,
                                           &editor->recovery_candidate,
                                           &editor->allocator) == AF_RESULT_SUCCESS) {
                editor->recovery_overlay = false;
                (void)af_editor_open_project_path(editor, editor->project_path);
                af_editor_status(editor, "Autosave restored; previous project kept as a backup");
            } else {
                af_editor_status(editor, "No valid autosave is available to restore");
            }
            return true;
        }
        if (y >= box_y + 148.0F && y < box_y + 190.0F && x >= box_x + 282.0F &&
            x < box_x + 518.0F) {
            (void)af_editor_recovery_discard(&editor->recovery);
            editor->recovery_candidate.status = AF_EDITOR_RECOVERY_STATUS_NONE;
            editor->recovery_overlay = false;
            af_editor_status(editor, "Autosave recovery data discarded");
            return true;
        }
        return true;
    }
    if (editor->preset_panel_visible) {
        const float box_x = ((float)width - 430.0F) * 0.5F;
        const float box_y = ((float)height - 330.0F) * 0.5F;
        if (x >= box_x + 18.0F && x < box_x + 412.0F && y >= box_y + 55.0F && y < box_y + 93.0F) {
            const AfStyle *selected =
                af_style_find_node_const(&editor->project.style_data, editor->selected_id);
            if (selected != NULL) {
                AfStyle preset = *selected;
                preset.id = editor->project.next_id++;
                preset.node_id = 0U;
                preset.parent_style_id = 0U;
                if (af_project_add_style_preset(&editor->project, &preset) == AF_RESULT_SUCCESS) {
                    af_editor_mark_dirty(editor);
                    af_editor_status(editor, "Style preset saved in this project");
                }
            }
            return true;
        }
        if (x >= box_x + 18.0F && x < box_x + 412.0F && y >= box_y + 106.0F) {
            const uint32_t requested = (uint32_t)((y - box_y - 106.0F) / 32.0F);
            const float row_y = box_y + 106.0F + (float)requested * 32.0F;
            uint32_t row = 0U;
            if (y >= row_y + 28.0F) {
                return true;
            }
            for (uint32_t index = 0U; index < editor->project.style_data.count; ++index) {
                const AfStyle *preset = &editor->project.style_data.styles[index];
                if (preset->node_id != 0U) {
                    continue;
                }
                if (row++ == requested) {
                    af_editor_begin_selection_modify(editor, false);
                    for (uint32_t selected = 0U; selected < editor->selected_count; ++selected) {
                        AfStyle *style = af_project_find_node_style(&editor->project,
                                                                    editor->selected_ids[selected]);
                        if (style != NULL) {
                            const uint64_t id = style->id;
                            const uint64_t node_id = style->node_id;
                            *style = *preset;
                            style->id = id;
                            style->node_id = node_id;
                        }
                    }
                    (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
                    af_editor_status(editor, "Style preset applied to selection");
                    break;
                }
            }
            return true;
        }
        return true;
    }
    if (editor->profiler_visible) {
        const float panel_x =
            (float)width - editor->right_width - AF_EDITOR_WORKFLOW_PANEL_WIDTH - 12.0F;
        if (x >= panel_x && x < panel_x + AF_EDITOR_WORKFLOW_PANEL_WIDTH &&
            y >= editor->toolbar_height + 12.0F &&
            y < editor->toolbar_height + 12.0F + AF_EDITOR_WORKFLOW_PANEL_HEIGHT) {
            editor->profiler_visible = false;
            return true;
        }
    }
    if (editor->validation_visible) {
        const float panel_x = editor->left_width + 12.0F;
        const float panel_y = editor->toolbar_height + 12.0F;
        if (x >= panel_x && x < panel_x + AF_EDITOR_WORKFLOW_PANEL_WIDTH && y >= panel_y &&
            y < panel_y + 40.0F) {
            editor->validation_visible = false;
            return true;
        }
        if (x >= panel_x + 12.0F && x < panel_x + AF_EDITOR_WORKFLOW_PANEL_WIDTH - 12.0F &&
            y >= panel_y + 156.0F && y < panel_y + 194.0F) {
            uint64_t affected = 0U;
            (void)af_editor_missing_asset_count(editor, &affected);
            if (affected != 0U) {
                af_editor_select_only(editor, affected);
                editor->asset_browser_visible = true;
                for (uint32_t asset = 0U; asset < editor->project.image_data.asset_count; ++asset) {
                    if (editor->project.image_data.assets[asset].missing) {
                        editor->selected_asset_id = editor->project.image_data.assets[asset].id;
                        break;
                    }
                }
                af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_REPAIR_IMAGE);
                af_editor_status(editor, "Select a replacement file for the missing resource");
            }
            return true;
        }
    }
    if (x < editor->left_width && y >= 48.0F && y < 75.0F) {
        editor->hierarchy_search_active = true;
        (void)af_platform_text_input_set(&editor->window, true);
        return true;
    }
    return false;
}

static bool af_editor_workflow_key(AfEditorState *editor, const SDL_KeyboardEvent *event) {
    const uint32_t key = af_editor_shortcut_key(event->key);
    const uint32_t modifiers = af_editor_shortcut_modifiers(event->mod);
    if (!event->down) {
        return false;
    }
    if (af_editor_onboarding_key(editor, event)) {
        return true;
    }
    if (editor->export_panel_visible) {
        AfEditorDialogAction export_action = AF_EDITOR_DIALOG_NONE;
        if (event->key == SDLK_ESCAPE) {
            editor->export_panel_visible = false;
            af_editor_status(editor, "Export canceled");
        } else if (event->key == SDLK_1) {
            export_action = AF_EDITOR_DIALOG_EXPORT_PACKAGE;
        } else if (event->key == SDLK_2) {
            export_action = AF_EDITOR_DIALOG_EXPORT_LOTTIE;
        } else if (event->key == SDLK_3) {
            export_action = AF_EDITOR_DIALOG_EXPORT_DOTLOTTIE;
        } else if (event->key == SDLK_4) {
            export_action = AF_EDITOR_DIALOG_EXPORT_SVG;
        } else if (event->key == SDLK_5) {
            export_action = AF_EDITOR_DIALOG_EXPORT_GIF;
        } else if (event->key == SDLK_6) {
            export_action = AF_EDITOR_DIALOG_EXPORT_MP4;
        }
        if (export_action != AF_EDITOR_DIALOG_NONE) {
            editor->export_panel_visible = false;
            af_editor_show_project_dialog(editor, export_action);
        }
        return true;
    }
    if (editor->platform_panel_visible) {
        if (event->key == SDLK_ESCAPE) {
            editor->platform_panel_visible = false;
        }
        return true;
    }
    if (editor->update_panel_visible) {
        if (event->key == SDLK_ESCAPE) {
            editor->update_panel_visible = false;
        }
        return true;
    }
    if (editor->template_panel_visible) {
        if (event->key == SDLK_ESCAPE) {
            editor->template_panel_visible = false;
        } else if (event->key == SDLK_LEFT && editor->template_selected_index > 0U) {
            --editor->template_selected_index;
        } else if (event->key == SDLK_RIGHT &&
                   editor->template_selected_index + 1U < af_project_template_count()) {
            ++editor->template_selected_index;
        } else if (event->key == SDLK_UP && editor->template_selected_index >= 3U) {
            editor->template_selected_index -= 3U;
        } else if (event->key == SDLK_DOWN &&
                   editor->template_selected_index + 3U < af_project_template_count()) {
            editor->template_selected_index += 3U;
        } else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            const uint32_t index = editor->template_selected_index;
            editor->template_panel_visible = false;
            (void)af_editor_create_template(editor, index);
        }
        return true;
    }
    if (editor->recent_panel_visible) {
        if (event->key == SDLK_ESCAPE) {
            editor->recent_panel_visible = false;
        } else if (event->key == SDLK_UP && editor->recent_selected_index > 0U) {
            --editor->recent_selected_index;
        } else if (event->key == SDLK_DOWN &&
                   editor->recent_selected_index + 1U < editor->preferences.recent_project_count) {
            ++editor->recent_selected_index;
        } else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            af_editor_open_recent_index(editor, editor->recent_selected_index);
        }
        return true;
    }
    if (editor->shortcut_capture) {
        if (event->key == SDLK_ESCAPE) {
            editor->shortcut_capture = false;
            return true;
        }
        if (key != 0U && editor->shortcut_selected_index < editor->actions.count) {
            AfEditorActionId conflict = 0U;
            AfEditorAction *action = &editor->actions.actions[editor->shortcut_selected_index];
            const AfEditorShortcut shortcut = {key, modifiers};
            if (af_editor_action_set_shortcut(&editor->actions, action->id, shortcut, &conflict) ==
                AF_RESULT_SUCCESS) {
                (void)af_editor_preferences_set_shortcut(&editor->preferences, action->id,
                                                         shortcut);
                af_editor_status(editor, "Shortcut updated");
            } else {
                af_editor_status(editor, "Shortcut conflicts with another active command");
            }
            editor->shortcut_capture = false;
        }
        return true;
    }
    if (editor->shortcuts_overlay) {
        if (event->key == SDLK_ESCAPE) {
            editor->shortcuts_overlay = false;
        } else if (event->key == SDLK_UP && editor->shortcut_selected_index > 0U) {
            --editor->shortcut_selected_index;
        } else if (event->key == SDLK_DOWN &&
                   editor->shortcut_selected_index + 1U < editor->actions.count) {
            ++editor->shortcut_selected_index;
        } else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            editor->shortcut_capture = true;
        }
        return true;
    }
    if (editor->recovery_overlay) {
        if (event->key == SDLK_ESCAPE) {
            editor->recovery_overlay = false;
        }
        return true;
    }
    if (editor->preset_panel_visible) {
        if (event->key == SDLK_ESCAPE) {
            editor->preset_panel_visible = false;
        }
        return true;
    }
    if (editor->command_palette.open) {
        AfEditorCommandPaletteKey palette_key = AF_EDITOR_COMMAND_PALETTE_CANCEL;
        AfEditorActionId action_id = 0U;
        bool handled = true;
        if (event->key == SDLK_UP) {
            palette_key = AF_EDITOR_COMMAND_PALETTE_UP;
        } else if (event->key == SDLK_DOWN) {
            palette_key = AF_EDITOR_COMMAND_PALETTE_DOWN;
        } else if (event->key == SDLK_HOME) {
            palette_key = AF_EDITOR_COMMAND_PALETTE_HOME;
        } else if (event->key == SDLK_END) {
            palette_key = AF_EDITOR_COMMAND_PALETTE_END;
        } else if (event->key == SDLK_PAGEUP) {
            palette_key = AF_EDITOR_COMMAND_PALETTE_PAGE_UP;
        } else if (event->key == SDLK_PAGEDOWN) {
            palette_key = AF_EDITOR_COMMAND_PALETTE_PAGE_DOWN;
        } else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            palette_key = AF_EDITOR_COMMAND_PALETTE_ACTIVATE;
        } else if (event->key == SDLK_ESCAPE) {
            palette_key = AF_EDITOR_COMMAND_PALETTE_CANCEL;
        } else if (event->key == SDLK_BACKSPACE) {
            af_editor_backspace_utf8(editor->command_palette.query);
            (void)af_editor_command_palette_set_query(&editor->command_palette,
                                                      editor->command_palette.query);
            return true;
        } else {
            handled = false;
        }
        if (handled &&
            af_editor_command_palette_handle_key(&editor->command_palette, palette_key,
                                                 &action_id) == AF_RESULT_SUCCESS &&
            action_id != 0U) {
            (void)af_editor_action_execute(&editor->actions, action_id);
        }
        return true;
    }
    if (editor->hierarchy_search_active) {
        if (event->key == SDLK_ESCAPE) {
            editor->hierarchy_search[0] = '\0';
            editor->hierarchy_search_active = false;
            (void)af_platform_text_input_set(&editor->window, false);
        } else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            editor->hierarchy_search_active = false;
            (void)af_platform_text_input_set(&editor->window, false);
        } else if (event->key == SDLK_BACKSPACE) {
            af_editor_backspace_utf8(editor->hierarchy_search);
        }
        return true;
    }
    if ((modifiers & (AF_EDITOR_SHORTCUT_MODIFIER_CTRL | AF_EDITOR_SHORTCUT_MODIFIER_SHIFT)) ==
            (AF_EDITOR_SHORTCUT_MODIFIER_CTRL | AF_EDITOR_SHORTCUT_MODIFIER_SHIFT) &&
        key == (uint32_t)'P') {
        af_editor_execute_workflow_action(editor, AF_EDITOR_MENU_ACTION_COMMAND_PALETTE,
                                          editor->window_width, editor->window_height);
        return true;
    }
    if (af_editor_choice_key(editor, event))
        return true;
    if (event->key == SDLK_DELETE && editor->gradient_stop_focus &&
        editor->gradient_target == editor->selected_id && editor->selected_count == 1 &&
        !editor->timeline_focus && !editor->color_target_outline &&
        (editor->active_tool == AF_EDITOR_TOOL_SELECT ||
         editor->active_tool == AF_EDITOR_TOOL_TEXT)) {
        const AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
        if (style && style->fill.type != AF_PAINT_SOLID) {
            af_editor_gradient_action(editor, 4);
            return true;
        }
    }
    {
        AfEditorActionId action_id = 0U;
        uint32_t contexts = editor->timeline_focus ? AF_EDITOR_ACTION_CONTEXT_TIMELINE
                                                   : AF_EDITOR_ACTION_CONTEXT_CANVAS |
                                                         AF_EDITOR_ACTION_CONTEXT_HIERARCHY;
        if (key != 0U &&
            af_editor_action_resolve_shortcut(&editor->actions, (AfEditorShortcut){key, modifiers},
                                              contexts, &action_id) == AF_RESULT_SUCCESS) {
            (void)af_editor_action_execute(&editor->actions, action_id);
            return true;
        }
    }
    return false;
}

static bool af_editor_append_workflow_text(char *buffer, size_t capacity, const char *text) {
    const size_t current = strlen(buffer);
    const size_t incoming = text == NULL ? 0U : strlen(text);
    if (incoming == 0U || current + incoming >= capacity || strchr(text, '\n') != NULL ||
        strchr(text, '\r') != NULL) {
        return true;
    }
    memcpy(buffer + current, text, incoming + 1U);
    return true;
}

static bool af_editor_workflow_text(AfEditorState *editor, const char *text) {
    if (editor->offset_panel_open) {
        if (editor->offset_replace)
            editor->offset_buffer[0] = '\0';
        editor->offset_replace = false;
        (void)af_editor_append_workflow_text(editor->offset_buffer, sizeof(editor->offset_buffer),
                                             text);
        return true;
    }
    if (editor->command_palette.open) {
        (void)af_editor_append_workflow_text(editor->command_palette.query,
                                             sizeof(editor->command_palette.query), text);
        (void)af_editor_command_palette_set_query(&editor->command_palette,
                                                  editor->command_palette.query);
        return true;
    }
    if (editor->hierarchy_search_active) {
        return af_editor_append_workflow_text(editor->hierarchy_search,
                                              sizeof(editor->hierarchy_search), text);
    }
    return false;
}

static void af_editor_workflow_drop_file(AfEditorState *editor, const char *path) {
    const char *extension = path == NULL ? NULL : strrchr(path, '.');
    if (path == NULL || extension == NULL) {
        af_editor_status(editor, "Dropped file has no supported extension");
        return;
    }
    if (strcmp(extension, ".afproj") == 0) {
        (void)af_editor_open_project_path(editor, path);
        return;
    }
    if (editor->project_path[0] == '\0') {
        af_editor_status(editor, "Save the project before dropping image assets");
        return;
    }
    {
        uint64_t asset_id = 0U;
        uint64_t node_id = 0U;
        const AfResult result = af_project_import_image(&editor->project, editor->project_path,
                                                        path, &asset_id, &node_id);
        if (result == AF_RESULT_SUCCESS) {
            editor->selected_asset_id = asset_id;
            editor->asset_browser_visible = true;
            af_editor_select_only(editor, node_id);
            af_editor_mark_dirty(editor);
            (void)af_image_build_atlases(&editor->project.image_data);
            af_editor_status(editor, "Dropped image imported into the project");
        } else {
            af_editor_status(editor, "Dropped asset is unsupported or invalid");
        }
    }
}
