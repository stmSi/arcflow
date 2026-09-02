#include "animation/advanced_animation.h"
#include "animation/animation.h"
#include "arcflow/arcflow_runtime.h"
#include "assets/package.h"
#include "component/component.h"
#include "composition/composition.h"
#include "core/allocator.h"
#include "core/arena.h"
#include "core/array.h"
#include "core/file_io.h"
#include "core/handles.h"
#include "core/hash.h"
#include "core/math.h"
#include "core/png.h"
#include "core/safe_math.h"
#include "deformation/deformation.h"
#include "editor/editor_commands.h"
#include "editor/editor_gizmo.h"
#include "editor/editor_inspector.h"
#include "editor/editor_vector_tools.h"
#include "interaction/interaction.h"
#include "layout/layout.h"
#include "particle/particle.h"
#include "path/path.h"
#include "project/image_import.h"
#include "project/project.h"
#include "rigging/rigging.h"
#include "runtime/runtime_internal.h"
#include "scene/scene.h"
#include "state_machine/state_machine.h"
#include "style/style.h"
#include "text/text.h"

#include <volk.h>

#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expression);   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_NEAR(a, b, epsilon) CHECK(fabsf((a) - (b)) <= (epsilon))

typedef struct AfTestFixture {
    AfSceneNode nodes[3];
    AfScene scene;
    AfAnimationClip clips[1];
    AfAnimationTrack tracks[2];
    AfKeyframe keys[5];
    AfAnimationData animation;
} AfTestFixture;

// clang-format off
#include "test_core.inc"
#include "test_file_io_atomic.inc"
#include "test_systems.inc"
#include "test_rigging.inc"
#include "test_package.inc"
#include "test_project.inc"
#include "test_images.inc"
#include "test_editor.inc"
#include "test_gpu.inc"
// clang-format on
int main(void) {
    static const AfTestCase tests[] = {
        {"core", af_test_core},
        {"file limits", af_test_file_limit},
        {"atomic project files", af_test_atomic_file_io},
        {"PNG image export", af_test_png_writer},
        {"math and transforms", af_test_math},
        {"scene hierarchy", af_test_scene},
        {"GPU particle data", af_test_particles},
        {"animation", af_test_animation},
        {"vector paths", af_test_paths},
        {"vector styling", af_test_vector_styling},
        {"responsive layout", af_test_responsive_layout},
        {"text shaping and cache", af_test_text_shaping},
        {"reusable components", af_test_components},
        {"interaction and events", af_test_interaction},
        {"animation state machines", af_test_state_machines},
        {"advanced animation", af_test_advanced_animation},
        {"masks and composition", af_test_composition},
        {"shape animation and deformation", af_test_shape_deformation},
        {"rigging and procedural motion", af_test_rigging_and_procedural_motion},
        {"runtime package", af_test_package},
        {"runtime handles and allocations", af_test_runtime},
        {"editable projects", af_test_project},
        {"image asset pipeline", af_test_images},
        {"timeline authoring", af_test_timeline_authoring},
        {"editor transform gizmo", af_test_editor_gizmo},
        {"editor inspector values", af_test_editor_inspector_values},
        {"editor vector tools", af_test_editor_vector_tools},
        {"editor commands", af_test_editor_commands},
        {"Vulkan pipeline smoke", af_test_gpu_smoke}};
    size_t index = 0U;
    size_t passed = 0U;
    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (tests[index].function()) {
            ++passed;
            (void)printf("[PASS] %s\n", tests[index].name);
        } else {
            (void)printf("[FAIL] %s\n", tests[index].name);
        }
    }
    (void)printf("%zu/%zu tests passed\n", passed, sizeof(tests) / sizeof(tests[0]));
    return passed == sizeof(tests) / sizeof(tests[0]) ? 0 : 1;
}
