#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ ! -d "$project_dir/third_party/SDL" ]; then
    git clone --depth 1 --branch release-3.4.12 https://github.com/libsdl-org/SDL.git \
        "$project_dir/third_party/SDL"
fi

if [ ! -d "$project_dir/third_party/Vulkan-Headers" ]; then
    git clone --depth 1 --branch v1.3.296 \
        https://github.com/KhronosGroup/Vulkan-Headers.git \
        "$project_dir/third_party/Vulkan-Headers"
fi

if [ ! -d "$project_dir/third_party/freetype" ]; then
    git clone --depth 1 --branch VER-2-13-3 https://github.com/freetype/freetype.git \
        "$project_dir/third_party/freetype"
fi

if [ ! -d "$project_dir/third_party/harfbuzz" ]; then
    git clone --depth 1 --branch 10.2.0 https://github.com/harfbuzz/harfbuzz.git \
        "$project_dir/third_party/harfbuzz"
fi

if [ ! -f "$project_dir/third_party/volk/volk.c" ]; then
    if [ ! -d "$project_dir/third_party/volk-source" ]; then
        git clone --depth 1 --branch vulkan-sdk-1.3.296.0 https://github.com/zeux/volk.git \
            "$project_dir/third_party/volk-source"
    fi
    mkdir -p "$project_dir/third_party/volk"
    cp "$project_dir/third_party/volk-source/volk.c" \
        "$project_dir/third_party/volk-source/volk.h" \
        "$project_dir/third_party/volk-source/LICENSE.md" \
        "$project_dir/third_party/volk/"
fi

validation_dir="$project_dir/third_party/Vulkan-ValidationLayers-runtime"
if [ "$(uname -s)" = "Linux" ] && [ "$(uname -m)" = "x86_64" ] && \
    [ ! -f "$validation_dir/usr/lib/libVkLayer_khronos_validation.so" ]; then
    package=/tmp/vulkan-validation-layers-1.4.350.1-1-x86_64.pkg.tar.zst
    mkdir -p "$validation_dir"
    curl -L --fail \
        https://archive.archlinux.org/packages/v/vulkan-validation-layers/vulkan-validation-layers-1.4.350.1-1-x86_64.pkg.tar.zst \
        -o "$package"
    bsdtar -xf "$package" -C "$validation_dir"
fi

toolchain_dir="$project_dir/third_party/shader-toolchain"
if [ "$(uname -s)" = "Linux" ] && [ "$(uname -m)" = "x86_64" ] && \
    { [ ! -x "$toolchain_dir/usr/bin/glslc" ] || \
      [ ! -x "$toolchain_dir/usr/bin/spirv-val" ]; }; then
    mkdir -p "$toolchain_dir"
    curl -L --fail \
        https://archive.archlinux.org/packages/s/spirv-tools/spirv-tools-1%3A1.4.350.1-1-x86_64.pkg.tar.zst \
        -o /tmp/arcflow-spirv-tools.pkg.tar.zst
    curl -L --fail \
        https://archive.archlinux.org/packages/g/glslang/glslang-1%3A1.4.350.1-1-x86_64.pkg.tar.zst \
        -o /tmp/arcflow-glslang.pkg.tar.zst
    curl -L --fail \
        https://archive.archlinux.org/packages/s/shaderc/shaderc-2026.2-2-x86_64.pkg.tar.zst \
        -o /tmp/arcflow-shaderc.pkg.tar.zst
    bsdtar -xf /tmp/arcflow-spirv-tools.pkg.tar.zst -C "$toolchain_dir"
    bsdtar -xf /tmp/arcflow-glslang.pkg.tar.zst -C "$toolchain_dir"
    bsdtar -xf /tmp/arcflow-shaderc.pkg.tar.zst -C "$toolchain_dir"
fi
