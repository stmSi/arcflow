#ifndef AF_LAYOUT_H
#define AF_LAYOUT_H

#include "arcflow/arcflow_result.h"
#include "core/math.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_LAYOUT_NODES 4096U

typedef enum AfCanvasScaleMode {
    AF_CANVAS_SCALE_NONE = 0,
    AF_CANVAS_SCALE_FIT,
    AF_CANVAS_SCALE_FILL,
    AF_CANVAS_SCALE_STRETCH
} AfCanvasScaleMode;

typedef enum AfLayoutValueMode {
    AF_LAYOUT_VALUE_INTRINSIC = 0,
    AF_LAYOUT_VALUE_FIXED,
    AF_LAYOUT_VALUE_PERCENT
} AfLayoutValueMode;

typedef enum AfLayoutDirection {
    AF_LAYOUT_DIRECTION_NONE = 0,
    AF_LAYOUT_DIRECTION_HORIZONTAL,
    AF_LAYOUT_DIRECTION_VERTICAL
} AfLayoutDirection;

typedef enum AfLayoutAlignment {
    AF_LAYOUT_ALIGN_START = 0,
    AF_LAYOUT_ALIGN_CENTER,
    AF_LAYOUT_ALIGN_END,
    AF_LAYOUT_ALIGN_STRETCH
} AfLayoutAlignment;

typedef enum AfAspectConstraint {
    AF_ASPECT_NONE = 0,
    AF_ASPECT_FIT,
    AF_ASPECT_WIDTH_CONTROLS_HEIGHT,
    AF_ASPECT_HEIGHT_CONTROLS_WIDTH
} AfAspectConstraint;

typedef struct AfInsets {
    float left;
    float top;
    float right;
    float bottom;
} AfInsets;

typedef struct AfLayoutValue {
    AfLayoutValueMode mode;
    float value;
} AfLayoutValue;

typedef struct AfLayoutNode {
    uint64_t node_id;
    AfVec2 anchor_min;
    AfVec2 anchor_max;
    AfVec2 pivot;
    AfVec2 offset;
    AfLayoutValue width;
    AfLayoutValue height;
    AfVec2 minimum_size;
    AfVec2 maximum_size;
    float aspect_ratio;
    AfAspectConstraint aspect;
    AfLayoutDirection direction;
    AfLayoutAlignment child_alignment;
    float spacing;
    AfInsets padding;
} AfLayoutNode;

typedef struct AfLayoutData {
    AfLayoutNode *nodes;
    uint32_t count;
    uint32_t capacity;
    AfVec2 reference_size;
    AfVec2 target_size;
    AfInsets safe_area;
    AfCanvasScaleMode scale_mode;
} AfLayoutData;

struct AfScene;

AfLayoutNode af_layout_node_default(uint64_t node_id);
const AfLayoutNode *af_layout_find_const(const AfLayoutData *data, uint64_t node_id);
AfResult af_layout_node_validate(const AfLayoutNode *layout);
AfResult af_layout_validate(const AfLayoutData *data, const struct AfScene *scene);
AfResult af_layout_evaluate_scene(struct AfScene *scene);

#endif
