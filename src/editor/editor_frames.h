#ifndef AF_EDITOR_FRAMES_H
#define AF_EDITOR_FRAMES_H

#include "editor/editor_commands.h"

uint32_t af_editor_frame_rate(const AfProjectClip *clip);
const AfProjectTrack *af_editor_frame_at(const AfProject *project, uint64_t clip_id,
                                         uint32_t frame);
const AfProjectTrack *af_editor_frame_owner(const AfProject *project, uint64_t node_id);
/* A zero source creates an empty drawing; otherwise copies the source drawing subtree. */
AfResult af_editor_command_frame(AfProject *project, uint64_t clip_id, uint32_t frame,
                                 uint64_t source_node, AfEditorCommand *command);
AfResult af_editor_command_frame_hold(AfProject *project, uint64_t track_id, uint32_t length,
                                      AfEditorCommand *command);
AfResult af_editor_command_frame_rate(AfProject *project, uint64_t clip_id, uint32_t fps,
                                      AfEditorCommand *command);

#endif
