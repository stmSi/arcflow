#ifndef AF_PROJECT_TEMPLATES_H
#define AF_PROJECT_TEMPLATES_H

#include "project/project.h"

#include <stdint.h>

typedef enum AfProjectTemplateId {
    AF_PROJECT_TEMPLATE_BLANK = 0,
    AF_PROJECT_TEMPLATE_GUIDED_BASICS,
    AF_PROJECT_TEMPLATE_VECTOR_ANIMATION,
    AF_PROJECT_TEMPLATE_INTERACTIVE_BUTTON,
    AF_PROJECT_TEMPLATE_RESPONSIVE_UI,
    AF_PROJECT_TEMPLATE_PARTICLE_EFFECT,
    AF_PROJECT_TEMPLATE_COUNT
} AfProjectTemplateId;

typedef enum AfProjectTemplateFeature {
    AF_PROJECT_TEMPLATE_FEATURE_ANIMATION = 1U << 0U,
    AF_PROJECT_TEMPLATE_FEATURE_VECTOR = 1U << 1U,
    AF_PROJECT_TEMPLATE_FEATURE_INTERACTION = 1U << 2U,
    AF_PROJECT_TEMPLATE_FEATURE_LAYOUT = 1U << 3U,
    AF_PROJECT_TEMPLATE_FEATURE_PARTICLES = 1U << 4U
} AfProjectTemplateFeature;

typedef struct AfProjectTemplateInfo {
    AfProjectTemplateId id;
    const char *slug;
    const char *name;
    const char *description;
    const char *feature_label;
    uint32_t features;
} AfProjectTemplateInfo;

uint32_t af_project_template_count(void);
const AfProjectTemplateInfo *af_project_template_info(AfProjectTemplateId id);
const AfProjectTemplateInfo *af_project_template_find(const char *slug);
AfResult af_project_template_build(AfProjectTemplateId id, AfAllocator *allocator,
                                   uint32_t capacity, AfProject *out_project);

#endif
