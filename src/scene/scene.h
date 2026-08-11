#ifndef AF_SCENE_H
#define AF_SCENE_H

#include "arcflow/arcflow_result.h"
#include "assets/image.h"
#include "core/math.h"
#include "particle/particle.h"
#include "path/path.h"
#include "style/style.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_INVALID_NODE_ID UINT64_MAX
#define AF_MAX_NODES 4096U
#define AF_MAX_HIERARCHY_DEPTH 64U

typedef enum AfNodeType {
    AF_NODE_GROUP = 0,
    AF_NODE_RECTANGLE = 1,
    AF_NODE_ROUNDED_RECTANGLE = 2,
    AF_NODE_ELLIPSE = 3,
    AF_NODE_PATH = 4,
    AF_NODE_IMAGE = 5
} AfNodeType;

typedef struct AfSceneNode {
    uint64_t id;
    uint64_t parent_id;
    AfNodeType type;
    AfTransform2D local_transform;
    AfVec2 bounds;
    AfColor fill;
    float opacity;
    float corner_radius;
    int32_t layer_order;
    bool visible;
    AfAffine2 world_transform;
    float world_opacity;
    bool world_visible;
    AfVec2 resolved_bounds;
} AfSceneNode;

struct AfLayoutData;

typedef struct AfScene {
    AfSceneNode *nodes;
    uint32_t count;
    uint32_t capacity;
    const AfPathData *paths;
    const AfStyleData *styles;
    const struct AfLayoutData *layouts;
    const AfParticleData *particles;
    const AfImageData *images;
} AfScene;

void af_scene_init(AfScene *scene, AfSceneNode *nodes, uint32_t capacity);
AfResult af_scene_add_node(AfScene *scene, const AfSceneNode *node);
AfSceneNode *af_scene_find(AfScene *scene, uint64_t id);
const AfSceneNode *af_scene_find_const(const AfScene *scene, uint64_t id);
AfResult af_scene_reparent(AfScene *scene, uint64_t id, uint64_t parent_id);
AfResult af_scene_delete_node(AfScene *scene, uint64_t id);
AfResult af_scene_validate(const AfScene *scene);
AfResult af_scene_evaluate_world(AfScene *scene);

#endif
