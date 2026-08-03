# Building

Supported configurations are Debug, Development, Release, AddressSanitizer, and
UndefinedBehaviorSanitizer. Linux x64 is verified by the project presets; the project code and
vendored dependencies are structured for Windows x64, where a Vulkan loader and shader tools must
also be on `PATH`.

```sh
cmake --preset development
cmake --build --preset development
ctest --preset development
```

Use `debug`, `release`, `asan`, or `ubsan` in place of `development`. The optional
`arcflow_format_check` target runs clang-format without modifying files.
Sanitizer presets omit the GPU smoke case because third-party Vulkan drivers retain process-global
allocations; the Debug preset runs that GPU check.

Run the player with the project-local Linux validation runtime using:

```sh
tools/run-player-validation.sh --resize-smoke --frames 12
```

The same local validation environment can run the editor:

```sh
tools/run-editor-validation.sh assets/editor_demo.afproj --resize-smoke --frames 12
```

Pinned dependencies live under `third_party/`: SDL 3.4.12, Vulkan-Headers 1.3.296, volk from Vulkan
SDK 1.3.296, FreeType 2.13.3, HarfBuzz 10.2.0, and Linux x64 shader/validation tools. If a checkout omits them, run
`tools/bootstrap-dependencies.sh`. Dependencies are never downloaded at application startup or
normal CMake configuration. Exact versions and licenses are recorded in
`third_party/DEPENDENCIES.md`.

Shaders are compiled offline for Vulkan 1.3 with `glslc` and validated by `spirv-val` on every shader
rebuild. Release playback never compiles shaders.

Runtime requirements are a Vulkan loader and a driver supporting Vulkan 1.3, dynamic rendering,
synchronization 2, timeline semaphores, swapchains, and presentation. Debug validation is enabled
when `VK_LAYER_KHRONOS_validation` is installed and falls back safely when it is absent.
