# Pinned dependencies

- SDL 3.4.12 (`release-3.4.12`, zlib license)
- Vulkan-Headers 1.3.296 (`v1.3.296`, Apache-2.0 license)
- volk Vulkan SDK 1.3.296 (`vulkan-sdk-1.3.296.0`, MIT license)
- Vulkan Validation Layers 1.4.350.1 (Apache-2.0 license, Linux x64 runtime package)
- shaderc 2026.2 and glslang/SPIRV-Tools 1.4.350.1 (Apache-2.0 licenses, Linux x64 tools)

All Milestone 1 source and Linux x64 build dependencies are vendored here. A Vulkan loader and a
conforming Vulkan 1.3 driver are operating-system runtime requirements. On other platforms,
`glslc` and `spirv-val` must be available as build tools.
Use `tools/run-player-validation.sh` to load the project-local validation layer.
