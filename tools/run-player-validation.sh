#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
layer_root="$project_dir/third_party/Vulkan-ValidationLayers-runtime/usr"

if [ ! -f "$layer_root/lib/libVkLayer_khronos_validation.so" ]; then
    echo "Project-local Vulkan validation layer is missing." >&2
    exit 1
fi

VK_LAYER_PATH="$layer_root/share/vulkan/explicit_layer.d"
LD_LIBRARY_PATH="$layer_root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export VK_LAYER_PATH LD_LIBRARY_PATH

exec "$project_dir/build/debug/arcflow_player" "$@"
