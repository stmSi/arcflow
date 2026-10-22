#ifndef AF_ONBOARDING_H
#define AF_ONBOARDING_H

#include <stddef.h>
#include <stdint.h>

#include "arcflow/onboarding_topics.h"

typedef struct AfOnboardingGuide {
    const char *slug;
    const char *title;
    const char *summary;
    const char *steps[AF_ONBOARDING_MAX_STEPS];
    const char *details[AF_ONBOARDING_MAX_STEPS];
    uint32_t step_count;
} AfOnboardingGuide;

uint32_t af_onboarding_guide_count(void);
const AfOnboardingGuide *af_onboarding_guide(AfOnboardingTopic topic);
const AfOnboardingGuide *af_onboarding_guide_find(const char *slug);
AfOnboardingTopic af_onboarding_guide_topic(const AfOnboardingGuide *guide);

#endif
