# Arcflow

Arcflow 1.0 is a C17/Vulkan 1.3 runtime and toolchain for deterministic,
resolution-independent 2D animation. It ships the stable editor, standalone player, headless
compiler, runtime C API, Host SDK 1.0, production examples, and reproducible release pipeline from
the 30-milestone foundation roadmap.

## Build and run

Source dependencies and Linux shader/validation tools are pinned in `third_party/`, so they survive
workspace restarts. The host still needs a Vulkan 1.3 driver/loader, CMake, and Ninja.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/arcflow_player
./build/debug/arcflow_editor assets/editor_demo.afproj
```

The build generates `build/debug/assets/arcflow_demo.afp`. Pass another `.afp` path as the first
argument. `--frames N` is available for automated smoke runs.
Add `--resize-smoke` to force swapchain recreation during that run.

The editor provides resizable docked panels, primitive tools, transform gizmos, a dope sheet, and a
curve editor. A selected shape exposes red/green move axes, blue axis/corner scale handles, magenta
X/Y skew handles, and an amber rotation handle. `Shift` preserves the scale ratio or snaps rotation
to 15-degree steps. In the timeline, choose the labeled property and click `+ FIRST KEY` (`K`). Move
the playhead and manipulate the object on the canvas; once a track exists, Arcflow creates or
updates the next key automatically. Track rows show both the object and property and can be clicked
to select their target. Drag or box-select keys; use `Ctrl+C`/`Ctrl+V`/`Ctrl+D`, `Delete`,
`Ctrl+[`/`Ctrl+]`, and
`Shift+R` to edit timing. `S`, `L`, and `B` choose step, linear, and cubic interpolation; `F7`–`F9`
apply easing presets; `F6` toggles curves; Space plays; and the ruler scrubs or edits the loop range.
The red playhead can also be grabbed and dragged from anywhere along its full vertical line.
Timeline edits share bounded `Ctrl+Z`/`Ctrl+Y` history with scene edits. Scene shortcuts include
`1`–`4`, `F2`, arrows, `Q`/`E`, `-`/`+`, brackets, `U`, `Ctrl+S`, and `F5`. `4` creates an editable
path. Selecting a point opens a canvas action bar for point keys, line/curve conversion, insertion,
open/close, and deletion; its visible points and Bézier handles remain directly draggable. The same
actions have keyboard shortcuts: `I` inserts, `Delete` removes, `C` converts line/cubic points, `O`
opens or closes a contour, `Shift+R` reverses it, `Shift+M` adds an open contour, and `F` switches
non-zero/even-odd filling. Use `Shift+Delete` to remove the path node.

Use **File > Export...** (`Ctrl+Shift+E`) for deliverables. Export targets include the lossless Arcflow
runtime package (`.afp`), Lottie JSON (`.json`), dotLottie v2 (`.lottie`), the current vector frame
(`.svg`), animated GIF, and H.264 MP4. Vector interchange reports compatibility warnings; GIF and
MP4 evaluate the selected clip at 30 FPS and require FFmpeg. Destinations are replaced only after a
successful export.

Compile without opening the editor using:

```sh
./build/debug/arcflow_compiler assets/editor_demo.afproj build/debug/assets/editor_demo.afp
./build/debug/arcflow_player build/debug/assets/editor_demo.afp
./build/debug/arcflow_compiler --release-audit assets/editor_demo.afproj
./build/debug/arcflow_compiler --migrate-project old.afproj migrated.afproj
```

Build and verify a complete stable release candidate with:

```sh
cmake --preset release
cmake --build --preset release --target arcflow_production_acceptance
```

See [building](docs/building.md), [exporting](docs/exporting.md), [architecture](docs/architecture.md), and the
[runtime API](docs/runtime-api.md). The final automated and external acceptance gates are in the
[production release guide](docs/production-release.md). Severity, blocker policy, and project-audit
details remain in [beta stabilization](docs/beta-stabilization.md); enforced CPU budgets are in
[performance targets](docs/performance.md), and required parser mutation thresholds are in
[deterministic fuzzing](docs/fuzzing.md).
