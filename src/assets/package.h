#ifndef AF_PACKAGE_H
#define AF_PACKAGE_H

#include "af/animation/animation.h"
#include "af/binding/binding.h"
#include "af/component/component.h"
#include "af/core/allocator.h"
#include "af/layout/layout.h"
#include "af/scene/scene.h"
#include "af/state_machine/state_machine.h"
#include "arcflow/arcflow_result.h"
#include "arcflow/arcflow_version.h"

#include <stddef.h>
#include <stdint.h>

#define AF_PACKAGE_ENDIAN_MARKER 0x01020304U
#define AF_PACKAGE_HEADER_SIZE 64U
#define AF_PACKAGE_SECTION_SIZE 32U
#define AF_PACKAGE_MAX_SIZE (64U * 1024U * 1024U)
#define AF_PACKAGE_MAX_SECTIONS 16U

extern const uint8_t AF_PACKAGE_MAGIC[8];

typedef struct AfPackageSource {
    const AfScene *scene;
    const AfAnimationData *animation;
    const AfComponentData *components;
    const AfStateMachineData *state_machines;
    const AfBindingData *bindings;
    bool strip_debug_names;
} AfPackageSource;

typedef struct AfPackageBlob {
    uint8_t *data;
    size_t size;
} AfPackageBlob;

typedef struct AfLoadedPackage {
    AfScene scene;
    AfAnimationData animation;
    AfPathData path_data;
    AfStyleData style_data;
    AfLayoutData layout_data;
    AfComponentData component_data;
    AfStateMachineData state_machine_data;
    AfParticleData particle_data;
    AfImageData image_data;
    AfBindingData binding_data;
    AfSceneNode *base_nodes;
} AfLoadedPackage;

AfResult af_package_write(const AfPackageSource *source, AfAllocator *allocator,
                          AfPackageBlob *out_blob);
void af_package_blob_destroy(AfAllocator *allocator, AfPackageBlob *blob);
AfResult af_package_validate(const void *data, size_t size);
AfResult af_package_decode(const void *data, size_t size, AfAllocator *allocator,
                           AfLoadedPackage *out_package);
void af_loaded_package_destroy(AfAllocator *allocator, AfLoadedPackage *package);

#endif
