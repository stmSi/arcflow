# Arcflow

Arcflow 1.0 is a C17/Vulkan 1.3 editor, player, compiler, and runtime for 2D animation.
Reusable UI, engine, platform, and rendering code lives in
[AF Framework](https://github.com/stmSi/af-framework), pinned as the `framework/` submodule.

## Build and run

Requires CMake 3.25+, Ninja, and a Vulkan 1.3 driver/loader.

```sh
git clone --recurse-submodules https://github.com/stmSi/arcflow.git
cd arcflow
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/arcflow_editor assets/editor_demo.afproj
./build/debug/arcflow_player
```

For an existing checkout, run `git submodule update --init --recursive`.
Pinned dependencies are in `third_party/`; use `tools/bootstrap-dependencies.sh` if missing.
To use another framework checkout, set `-DARCFLOW_FRAMEWORK_SOURCE_DIR=/path/to/af-framework`.

Browser build and GitHub Pages deployment: [Web guide](docs/web.md).

## Editor basics

- `V` toggles Pick/Node; `Shift+V` selects Node. `R` draws rectangles, `A` stars, `T` text.
- Inspector values support typing, horizontal dragging, and `−`/`+` steps, including RGB/hex.
- Particle values use the same controls; scroll the Inspector to reach lower rows.
- `Ctrl+S` saves; `Ctrl+Z`/`Ctrl+Y` undo/redo; `Ctrl+Shift+P` opens commands.
- File > Export produces `.afp`, SVG, Lottie, GIF, or MP4. GIF/MP4 require FFmpeg.

## Docs

[Build](docs/building.md) · [Architecture](docs/architecture.md) ·
[Framework](https://github.com/stmSi/af-framework#readme) · [Runtime API](docs/runtime-api.md) ·
[Export](docs/exporting.md) ·
[Release](docs/production-release.md)
