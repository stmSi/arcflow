#ifndef AF_INTERACTION_H
#define AF_INTERACTION_H

#include "arcflow/arcflow_events.h"
#include "arcflow/arcflow_result.h"
#include "scene/scene.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_INTERACTION_NODES AF_MAX_NODES
#define AF_MAX_RUNTIME_EVENTS 256U
#define AF_DOUBLE_CLICK_SECONDS 0.35
#define AF_DRAG_THRESHOLD 3.0F

typedef struct AfInteractionNode {
    uint64_t node_id;
    bool enabled;
    bool pointer_passthrough;
    bool focusable;
    bool clip_children;
    float opacity_threshold;
} AfInteractionNode;

typedef struct AfInteractionData {
    const AfInteractionNode *nodes;
    uint32_t count;
} AfInteractionData;

typedef struct AfEventQueue {
    AfRuntimeEvent events[AF_MAX_RUNTIME_EVENTS];
    uint32_t head;
    uint32_t count;
} AfEventQueue;

typedef struct AfInteractionSystem {
    AfInteractionData data;
    AfEventQueue queue;
    uint64_t hovered_node_id;
    uint64_t pressed_node_id;
    uint64_t captured_node_id;
    uint64_t focused_node_id;
    uint64_t last_click_node_id;
    float press_x;
    float press_y;
    double last_click_time;
    bool dragging;
} AfInteractionSystem;

AfResult af_interaction_validate(const AfInteractionData *data, const AfScene *scene);
void af_interaction_init(AfInteractionSystem *system, const AfInteractionNode *nodes,
                         uint32_t count);
uint64_t af_interaction_hit_test(const AfInteractionSystem *system, const AfScene *scene, float x,
                                 float y);
AfResult af_interaction_pointer_move(AfInteractionSystem *system, const AfScene *scene, float x,
                                     float y, double timestamp);
AfResult af_interaction_pointer_down(AfInteractionSystem *system, const AfScene *scene, float x,
                                     float y, double timestamp);
AfResult af_interaction_pointer_up(AfInteractionSystem *system, const AfScene *scene, float x,
                                   float y, double timestamp);
AfResult af_interaction_keyboard_activate(AfInteractionSystem *system, double timestamp);
AfResult af_interaction_controller_navigate(AfInteractionSystem *system, const AfScene *scene,
                                            int32_t direction, double timestamp);
AfResult af_interaction_queue_animation_event(AfInteractionSystem *system, uint64_t name_id,
                                              uint64_t node_id, double timestamp);
AfResult af_interaction_poll(AfInteractionSystem *system, AfRuntimeEvent *out_event);

#endif
