# Building

Requires Python 3.9+, CMake 3.25+, Ninja, and a Vulkan 1.3 driver/loader.

```sh
git submodule update --init --recursive
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Other presets: `development`, `release`, `asan`, `ubsan`, and `windows-x64-cross`.
Sanitizer presets omit GPU smoke tests. `arcflow_format_check` checks formatting.

Pinned dependencies and Linux shader tools live in `third_party/`.
If missing, run `tools/bootstrap-dependencies.sh`; versions and licenses are in
[DEPENDENCIES.md](../third_party/DEPENDENCIES.md). Windows needs shader tools on `PATH`.
Shaders compile offline with `glslc` and are checked with `spirv-val`.

[AF Framework](https://github.com/stmSi/af-framework#readme) builds independently.
Use `-DARCFLOW_FRAMEWORK_SOURCE_DIR=/path/to/af-framework` to select another checkout.

Validation smoke: `tools/run-editor-validation.sh assets/editor_demo.afproj --frames 12`.
Release checks: configure `release`, then build `arcflow_production_acceptance`.
See the [release guide](production-release.md) for CI and hardware requirements.
