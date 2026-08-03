#ifndef ARCFLOW_EVENTS_H
#define ARCFLOW_EVENTS_H

#include <stdint.h>

typedef enum AfRuntimeEventType {
    AF_EVENT_POINTER_ENTER = 0,
    AF_EVENT_POINTER_LEAVE,
    AF_EVENT_POINTER_MOVE,
    AF_EVENT_POINTER_DOWN,
    AF_EVENT_POINTER_UP,
    AF_EVENT_CLICK,
    AF_EVENT_DOUBLE_CLICK,
    AF_EVENT_DRAG_START,
    AF_EVENT_DRAG,
    AF_EVENT_DRAG_END,
    AF_EVENT_FOCUS,
    AF_EVENT_BLUR,
    AF_EVENT_KEYBOARD_ACTIVATE,
    AF_EVENT_CONTROLLER_NAVIGATE,
    AF_EVENT_ANIMATION
} AfRuntimeEventType;

typedef struct AfRuntimeEvent {
    AfRuntimeEventType type;
    uint64_t node_id;
    uint64_t name_id;
    float x;
    float y;
    double timestamp;
    int32_t controller_direction;
} AfRuntimeEvent;

#endif
