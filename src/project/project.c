#include "project/project.h"

#include "animation/animation.h"
#include "core/file_io.h"
#include "core/safe_math.h"
#include "project/image_import.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AF_PROJECT_FIELD_ID = 1U << 0U,
    AF_PROJECT_FIELD_PARENT = 1U << 1U,
    AF_PROJECT_FIELD_TYPE = 1U << 2U,
    AF_PROJECT_FIELD_NAME = 1U << 3U,
    AF_PROJECT_FIELD_POSITION = 1U << 4U,
    AF_PROJECT_FIELD_ROTATION = 1U << 5U,
    AF_PROJECT_FIELD_SCALE = 1U << 6U,
    AF_PROJECT_FIELD_BOUNDS = 1U << 7U,
    AF_PROJECT_FIELD_FILL = 1U << 8U,
    AF_PROJECT_FIELD_OPACITY = 1U << 9U,
    AF_PROJECT_FIELD_RADIUS = 1U << 10U,
    AF_PROJECT_FIELD_LAYER = 1U << 11U,
    AF_PROJECT_FIELD_VISIBLE = 1U << 12U,
    AF_PROJECT_REQUIRED_FIELDS = (1U << 13U) - 1U
};

enum {
    AF_PROJECT_CLIP_ID = 1U << 0U,
    AF_PROJECT_CLIP_NAME = 1U << 1U,
    AF_PROJECT_CLIP_DURATION = 1U << 2U,
    AF_PROJECT_CLIP_LOOP = 1U << 3U,
    AF_PROJECT_CLIP_REGION = 1U << 4U,
    AF_PROJECT_CLIP_REQUIRED = (1U << 5U) - 1U,
    AF_PROJECT_TRACK_ID = 1U << 0U,
    AF_PROJECT_TRACK_CLIP = 1U << 1U,
    AF_PROJECT_TRACK_NODE = 1U << 2U,
    AF_PROJECT_TRACK_PROPERTY = 1U << 3U,
    AF_PROJECT_TRACK_REQUIRED = (1U << 4U) - 1U,
    AF_PROJECT_KEY_ID = 1U << 0U,
    AF_PROJECT_KEY_TRACK = 1U << 1U,
    AF_PROJECT_KEY_TIME = 1U << 2U,
    AF_PROJECT_KEY_VALUE = 1U << 3U,
    AF_PROJECT_KEY_INTERPOLATION = 1U << 4U,
    AF_PROJECT_KEY_EASING = 1U << 5U,
    AF_PROJECT_KEY_REQUIRED = (1U << 6U) - 1U,
    AF_PROJECT_PATH_ID = 1U << 0U,
    AF_PROJECT_PATH_NODE = 1U << 1U,
    AF_PROJECT_PATH_FILL_RULE = 1U << 2U,
    AF_PROJECT_PATH_GENERATION = 1U << 3U,
    AF_PROJECT_PATH_REQUIRED = (1U << 4U) - 1U
};

typedef struct AfProjectTextWriter {
    char *data;
    size_t capacity;
    size_t length;
} AfProjectTextWriter;

// clang-format off
#include "project/project_model.inc"
#include "project/project_authoring.inc"
#include "project/project_image_tokens.inc"
#include "project/project_bindings.inc"
#include "project/project_serialize.inc"
#include "project/project_parse_values.inc"
#include "project/project_parse_entities.inc"
#include "project/project_parse_images.inc"
#include "project/project_deserialize.inc"
#include "project/project_compile.inc"
// clang-format on
