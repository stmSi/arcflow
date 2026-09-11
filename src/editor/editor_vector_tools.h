#ifndef AF_EDITOR_VECTOR_TOOLS_H
#define AF_EDITOR_VECTOR_TOOLS_H

#include "core/math.h"
#include "path/path.h"
#include "scene/scene.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY 512U

typedef enum AfEditorVectorTool {
    AF_EDITOR_TOOL_SELECT = 0,
    AF_EDITOR_TOOL_NODE,
    AF_EDITOR_TOOL_SCISSORS,
    AF_EDITOR_TOOL_SEGMENT_DELETE,
    AF_EDITOR_TOOL_KNIFE,
    AF_EDITOR_TOOL_RECTANGLE,
    AF_EDITOR_TOOL_ROUNDED_RECTANGLE,
    AF_EDITOR_TOOL_ELLIPSE,
    AF_EDITOR_TOOL_LINE,
    AF_EDITOR_TOOL_PEN,
    AF_EDITOR_TOOL_PENCIL,
    AF_EDITOR_TOOL_POLYGON,
    AF_EDITOR_TOOL_STAR,
    AF_EDITOR_TOOL_ARC,
    AF_EDITOR_TOOL_SPIRAL,
    AF_EDITOR_TOOL_COUNT
} AfEditorVectorTool;

typedef struct AfEditorToolRect {
    AfVec2 position;
    AfVec2 size;
} AfEditorToolRect;

typedef struct AfEditorKnifeResult {
    uint32_t command_count;
    uint32_t cut_count;
    uint32_t first_cut_command;
    uint32_t source_commands[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
    AfVec2 cut_points[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
} AfEditorKnifeResult;

/* Closed geometry in the primitive's local coordinate system. Zero means unsupported,
 * invalid dimensions, or insufficient capacity; output is unchanged in those cases. */
uint32_t af_editor_vector_primitive_path(AfNodeType type, AfVec2 size, float radius,
                                         AfPathCommand *out_commands, uint32_t capacity);

const char *af_editor_vector_tool_name(AfEditorVectorTool tool);
const char *af_editor_vector_tool_label(AfEditorVectorTool tool);
bool af_editor_vector_tool_is_construction(AfEditorVectorTool tool);
bool af_editor_vector_tool_rect(AfVec2 start, AfVec2 current, bool from_center,
                                bool constrain_ratio, AfEditorToolRect *out_rect);
uint32_t af_editor_vector_tool_polygon(AfVec2 size, uint32_t sides, float inset,
                                       AfPathCommand *out_commands, uint32_t capacity);
uint32_t af_editor_vector_tool_arc(AfVec2 size, AfPathCommand *out_commands, uint32_t capacity);
uint32_t af_editor_vector_tool_spiral(AfVec2 size, uint32_t turns, AfPathCommand *out_commands,
                                      uint32_t capacity);
uint32_t af_editor_vector_tool_simplify(const AfVec2 *points, uint32_t point_count, float tolerance,
                                        AfVec2 *out_points, uint32_t capacity);

/* Cuts at most once per source segment, including implicit closing edges. Coordinates are local
 * to the path. Closed contours open at the cut instead of acquiring a seam at the original MOVE.
 * Input/output may alias; neither output is changed on failure or when no segment is crossed. */
AfResult af_editor_vector_tool_knife(const AfPathCommand *commands, uint32_t command_count,
                                     AfVec2 start, AfVec2 end, AfPathCommand *out_commands,
                                     uint32_t capacity, AfEditorKnifeResult *out_result);

#endif
