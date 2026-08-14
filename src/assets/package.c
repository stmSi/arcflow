#include "assets/package.h"

#include "core/safe_math.h"

#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

const uint8_t AF_PACKAGE_MAGIC[8] = {'A', 'R', 'C', 'F', 'L', 'O', 'W', '\0'};

enum {
    AF_SECTION_NODES = 1U,
    AF_SECTION_CLIPS = 2U,
    AF_SECTION_TRACKS = 3U,
    AF_SECTION_KEYFRAMES = 4U,
    AF_SECTION_STRINGS = 5U,
    AF_SECTION_PATHS = 6U,
    AF_SECTION_PATH_COMMANDS = 7U,
    AF_SECTION_STYLES = 8U,
    AF_SECTION_LAYOUTS = 9U,
    AF_SECTION_COMPONENTS = 10U,
    AF_SECTION_STATE_MACHINES = 11U,
    AF_SECTION_PARTICLES = 12U,
    AF_SECTION_IMAGES = 13U,
    AF_SECTION_BINDINGS = 14U,
    AF_SECTION_COUNT = 14U,
    AF_NODE_RECORD_SIZE = 80U,
    AF_CLIP_RECORD_SIZE = 24U,
    AF_TRACK_RECORD_SIZE = 32U,
    AF_KEYFRAME_RECORD_SIZE = 40U,
    AF_PATH_RECORD_SIZE = 40U,
    AF_PATH_COMMAND_RECORD_SIZE = 32U,
    AF_PAINT_RECORD_SIZE = 400U,
    AF_STYLE_RECORD_SIZE = 952U,
    AF_LAYOUT_HEADER_SIZE = 48U,
    AF_LAYOUT_RECORD_SIZE = 128U,
    AF_COMPONENT_HEADER_SIZE = 32U,
    AF_COMPONENT_DEFINITION_RECORD_SIZE = 40U,
    AF_COMPONENT_REFERENCE_RECORD_SIZE = 16U,
    AF_COMPONENT_VALUE_RECORD_SIZE = 88U,
    AF_COMPONENT_INSTANCE_RECORD_SIZE = 48U,
    AF_STATE_MACHINE_HEADER_SIZE = 32U,
    AF_STATE_MACHINE_RECORD_SIZE = 40U,
    AF_STATE_RECORD_SIZE = 48U,
    AF_STATE_TRANSITION_RECORD_SIZE = 64U,
    AF_STATE_INPUT_RECORD_SIZE = 32U,
    AF_STATE_CONDITION_RECORD_SIZE = 32U,
    AF_PARTICLE_HEADER_SIZE = 64U,
    AF_PARTICLE_RECORD_SIZE = 512U,
    AF_IMAGE_SECTION_HEADER_SIZE = 64U,
    AF_IMAGE_ASSET_RECORD_SIZE = 96U,
    AF_IMAGE_BINDING_RECORD_SIZE = 64U,
    AF_IMAGE_SEQUENCE_RECORD_SIZE = 96U,
    AF_IMAGE_ATLAS_RECORD_SIZE = 96U,
    AF_BINDING_HEADER_SIZE = 32U,
    AF_BINDING_PROPERTY_RECORD_SIZE = 480U,
    AF_BINDING_TARGET_RECORD_SIZE = 32U
};

typedef struct AfSectionView {
    uint32_t type;
    uint64_t offset;
    uint64_t size;
    uint32_t alignment;
    uint32_t count;
} AfSectionView;

// clang-format off
#include "assets/package_wire.inc"
#include "assets/package_bindings.inc"
#include "assets/package_validate_records.inc"
#include "assets/package_images.inc"
#include "assets/package_validate_graphs.inc"
#include "assets/package_encode.inc"
#include "assets/package_write.inc"
#include "assets/package_decode.inc"
// clang-format on
