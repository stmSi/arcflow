#include "af/core/allocator.h"
#include "onboarding/onboarding.h"
#include "project/project.h"
#include "project/project_templates.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    AfAllocator allocator = {0};
    CHECK(af_onboarding_guide_count() == AF_ONBOARDING_TOPIC_COUNT);
    for (uint32_t index = 0U; index < af_onboarding_guide_count(); ++index) {
        const AfOnboardingGuide *guide = af_onboarding_guide((AfOnboardingTopic)index);
        CHECK(guide != NULL && guide->slug != NULL && guide->slug[0] != '\0');
        CHECK(guide->title != NULL && guide->title[0] != '\0');
        CHECK(guide->summary != NULL && guide->summary[0] != '\0');
        CHECK(guide->step_count > 0U && guide->step_count <= AF_ONBOARDING_MAX_STEPS);
        CHECK(af_onboarding_guide_find(guide->slug) == guide);
        CHECK(af_onboarding_guide_topic(guide) == (AfOnboardingTopic)index);
        for (uint32_t step = 0U; step < guide->step_count; ++step) {
            CHECK(guide->steps[step] != NULL && guide->steps[step][0] != '\0');
        }
    }
    CHECK(af_onboarding_guide_find("not-a-topic") == NULL);
    CHECK(af_onboarding_guide_topic(NULL) == AF_ONBOARDING_TOPIC_COUNT);

    af_allocator_init_default(&allocator);
    CHECK(af_project_template_count() == AF_PROJECT_TEMPLATE_COUNT);
    for (uint32_t index = 0U; index < af_project_template_count(); ++index) {
        const AfProjectTemplateInfo *info = af_project_template_info((AfProjectTemplateId)index);
        AfProject project = {0};
        AfProject loaded = {0};
        AfPackageBlob text = {0};
        AfPackageBlob first = {0};
        AfPackageBlob second = {0};
        CHECK(info != NULL && info->slug != NULL && info->name != NULL);
        CHECK(af_project_template_find(info->slug) == info);
        CHECK(af_project_template_build(info->id, &allocator, 32U, &project) == AF_RESULT_SUCCESS);
        /* Templates return an owning project, not references into their builder's stack. */
        CHECK(project.scene.paths == &project.path_data);
        CHECK(project.scene.styles == &project.style_data);
        CHECK(project.scene.layouts == &project.layout_data);
        CHECK(project.scene.particles == &project.particle_data);
        CHECK(project.scene.images == &project.image_data);
        CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
        CHECK(strcmp(project.metadata.project_id, "arcflow-project") != 0);
        CHECK(af_project_serialize(&project, &allocator, &text) == AF_RESULT_SUCCESS);
        CHECK(af_project_deserialize(text.data, text.size, &allocator, &loaded) ==
              AF_RESULT_SUCCESS);
        CHECK(af_project_compile_release(&project, &allocator, &first) == AF_RESULT_SUCCESS);
        CHECK(af_project_compile_release(&project, &allocator, &second) == AF_RESULT_SUCCESS);
        CHECK(first.size == second.size && memcmp(first.data, second.data, first.size) == 0);
        if ((info->features & AF_PROJECT_TEMPLATE_FEATURE_VECTOR) != 0U) {
            CHECK(project.path_data.path_count != 0U && project.track_count >= 3U);
        }
        if ((info->features & AF_PROJECT_TEMPLATE_FEATURE_LAYOUT) != 0U) {
            CHECK(project.layout_data.count != 0U);
        }
        if ((info->features & AF_PROJECT_TEMPLATE_FEATURE_PARTICLES) != 0U) {
            CHECK(project.particle_data.count != 0U);
        }
        af_package_blob_destroy(&allocator, &second);
        af_package_blob_destroy(&allocator, &first);
        af_project_destroy(&loaded);
        af_package_blob_destroy(&allocator, &text);
        af_project_destroy(&project);
    }
    CHECK(af_project_template_find("not-a-template") == NULL);
    (void)printf("%u guides and %u templates verified\n", af_onboarding_guide_count(),
                 af_project_template_count());
    return 0;
}
