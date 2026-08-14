#ifndef AF_EDITOR_WORKFLOW_H
#define AF_EDITOR_WORKFLOW_H

#include "arcflow/arcflow_result.h"
#include "scene/scene.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_EDITOR_WORKFLOW_MAX_GUIDES 64U
#define AF_EDITOR_WORKFLOW_MAX_CLIPBOARD_NODES 64U
#define AF_EDITOR_WORKFLOW_MAX_FILTER_RESULTS 4096U

typedef enum AfEditorAlignMode {
    AF_EDITOR_ALIGN_LEFT = 0,
    AF_EDITOR_ALIGN_CENTER_X,
    AF_EDITOR_ALIGN_RIGHT,
    AF_EDITOR_ALIGN_TOP,
    AF_EDITOR_ALIGN_CENTER_Y,
    AF_EDITOR_ALIGN_BOTTOM
} AfEditorAlignMode;

typedef enum AfEditorDistributeMode {
    AF_EDITOR_DISTRIBUTE_HORIZONTAL = 0,
    AF_EDITOR_DISTRIBUTE_VERTICAL
} AfEditorDistributeMode;

typedef struct AfEditorGuide {
    float position;
    bool vertical;
} AfEditorGuide;

typedef struct AfEditorWorkspace {
    float left_width;
    float right_width;
    float timeline_height;
    bool hierarchy_visible;
    bool inspector_visible;
    bool timeline_visible;
    bool assets_visible;
    bool profiling_visible;
    bool validation_visible;
    AfEditorGuide guides[AF_EDITOR_WORKFLOW_MAX_GUIDES];
    uint32_t guide_count;
} AfEditorWorkspace;

typedef struct AfEditorClipboard {
    AfSceneNode nodes[AF_EDITOR_WORKFLOW_MAX_CLIPBOARD_NODES];
    char names[AF_EDITOR_WORKFLOW_MAX_CLIPBOARD_NODES][64];
    uint32_t count;
} AfEditorClipboard;

void af_editor_workspace_default(AfEditorWorkspace *workspace);
AfResult af_editor_workspace_validate(AfEditorWorkspace *workspace, float window_width,
                                      float window_height);
AfResult af_editor_workspace_serialize(const AfEditorWorkspace *workspace, char *output,
                                       size_t output_capacity, size_t *out_size);
AfResult af_editor_workspace_parse(const char *text, size_t text_size,
                                   AfEditorWorkspace *out_workspace);
AfResult af_editor_align_nodes(AfScene *scene, const uint64_t *node_ids, uint32_t node_count,
                               AfEditorAlignMode mode);
AfResult af_editor_distribute_nodes(AfScene *scene, const uint64_t *node_ids, uint32_t node_count,
                                    AfEditorDistributeMode mode);
AfResult af_editor_filter_nodes(const AfScene *scene, const char (*names)[64], const char *query,
                                uint32_t *out_indices, uint32_t capacity, uint32_t *out_count);
AfResult af_editor_clipboard_copy(const AfScene *scene, const char (*names)[64],
                                  const uint64_t *node_ids, uint32_t node_count,
                                  AfEditorClipboard *clipboard);
AfResult af_editor_clipboard_paste(const AfEditorClipboard *clipboard, AfScene *scene,
                                   char (*names)[64], uint64_t *next_id, AfVec2 offset,
                                   uint64_t *out_ids, uint32_t out_capacity, uint32_t *out_count);

#endif
