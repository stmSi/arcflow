#ifndef AF_EDITOR_EXPORT_H
#define AF_EDITOR_EXPORT_H
#include "project/project.h"
typedef struct AfEditorExportOptions {
    AfVec2 size;
    AfVec2 origin;
    bool selection_only;
} AfEditorExportOptions;
bool af_editor_export_includes(const AfProject *project, uint64_t id, const uint64_t *ids,
                               uint32_t count);
AfResult af_editor_export_project(const AfProject *source, const AfEditorExportOptions *options,
                                  const uint64_t *ids, uint32_t count, AfAllocator *allocator,
                                  AfProject *output);
#endif
