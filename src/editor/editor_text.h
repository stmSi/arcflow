#ifndef AF_EDITOR_TEXT_H
#define AF_EDITOR_TEXT_H
#include "editor/editor_commands.h"
#include "text/text.h"

/* id == 0 creates a root text object at position; otherwise edits existing text.
 * Caller loads the selected family/style as font_id. No project mutation. */
AfResult af_editor_command_text(AfProject *project, AfTextEngine *engine, uint64_t font_id,
                                uint64_t id, AfVec2 position, const AfProjectText *text,
                                AfEditorCommand *command);
#endif
