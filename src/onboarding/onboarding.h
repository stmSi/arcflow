#ifndef AF_ONBOARDING_H
#define AF_ONBOARDING_H

#include <stddef.h>
#include <stdint.h>

#define AF_ONBOARDING_MAX_STEPS 6U

typedef enum AfOnboardingTopic {
    AF_ONBOARDING_INSTALLATION = 0,
    AF_ONBOARDING_QUICK_START,
    AF_ONBOARDING_VECTOR_ANIMATION,
    AF_ONBOARDING_INTERACTIVE_BUTTON,
    AF_ONBOARDING_RESPONSIVE_UI,
    AF_ONBOARDING_PARTICLE_EFFECT,
    AF_ONBOARDING_RUNTIME_INTEGRATION,
    AF_ONBOARDING_VULKAN_INTEGRATION,
    AF_ONBOARDING_FILE_FORMATS,
    AF_ONBOARDING_C_API,
    AF_ONBOARDING_TROUBLESHOOTING,
    AF_ONBOARDING_PERFORMANCE,
    AF_ONBOARDING_SECURITY,
    AF_ONBOARDING_MIGRATION,
    AF_ONBOARDING_SHORTCUTS,
    AF_ONBOARDING_LIMITATIONS,
    AF_ONBOARDING_TOPIC_COUNT
} AfOnboardingTopic;

typedef struct AfOnboardingGuide {
    const char *slug;
    const char *title;
    const char *summary;
    const char *steps[AF_ONBOARDING_MAX_STEPS];
    uint32_t step_count;
} AfOnboardingGuide;

uint32_t af_onboarding_guide_count(void);
const AfOnboardingGuide *af_onboarding_guide(AfOnboardingTopic topic);
const AfOnboardingGuide *af_onboarding_guide_find(const char *slug);
AfOnboardingTopic af_onboarding_guide_topic(const AfOnboardingGuide *guide);

#endif
