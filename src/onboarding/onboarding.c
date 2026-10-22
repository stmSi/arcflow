#include "onboarding/onboarding.h"

#include <string.h>

#include "arcflow/onboarding_guides.inc"

uint32_t af_onboarding_guide_count(void) { return AF_ONBOARDING_TOPIC_COUNT; }

const AfOnboardingGuide *af_onboarding_guide(AfOnboardingTopic topic) {
    return (uint32_t)topic < AF_ONBOARDING_TOPIC_COUNT ? &af_onboarding_guides[topic] : NULL;
}

const AfOnboardingGuide *af_onboarding_guide_find(const char *slug) {
    if (slug == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < AF_ONBOARDING_TOPIC_COUNT; ++index) {
        if (strcmp(slug, af_onboarding_guides[index].slug) == 0) {
            return &af_onboarding_guides[index];
        }
    }
    return NULL;
}

AfOnboardingTopic af_onboarding_guide_topic(const AfOnboardingGuide *guide) {
    for (uint32_t index = 0U; guide != NULL && index < AF_ONBOARDING_TOPIC_COUNT; ++index) {
        if (guide == &af_onboarding_guides[index]) {
            return (AfOnboardingTopic)index;
        }
    }
    return AF_ONBOARDING_TOPIC_COUNT;
}
