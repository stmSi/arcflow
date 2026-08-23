# Arcflow

Arcflow is a C17/Vulkan 1.3 runtime and toolchain for deterministic,
resolution-independent 2D animation. The repository implements the planned engine, editor, asset,
runtime SDK, release, and onboarding foundations through Milestone 28. Milestone 29 beta
stabilization is active.

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
to 15-degree steps. Select a colored property button and press `K` to add a key. Drag or box-select
keys; use `Ctrl+C`/`Ctrl+V`/`Ctrl+D`, `Delete`, `Ctrl+[`/`Ctrl+]`, and
`Shift+R` to edit timing. `S`, `L`, and `B` choose step, linear, and cubic interpolation; `F7`–`F9`
apply easing presets; `F6` toggles curves; Space plays; and the ruler scrubs or edits the loop range.
Timeline edits share bounded `Ctrl+Z`/`Ctrl+Y` history with scene edits. Scene shortcuts include
`1`–`4`, `F2`, arrows, `Q`/`E`, `-`/`+`, brackets, `U`, `Ctrl+S`, and `F5`. `4` creates an editable
path. Select its visible points and handles to drag them; `I` inserts, `Delete` removes, `C`
converts line/cubic points, `O` opens or closes a contour, `Shift+R` reverses it, `Shift+M` adds an
open contour, and `F` switches non-zero/even-odd filling. Use `Shift+Delete` to remove the path node.

Compile without opening the editor using:

```sh
./build/debug/arcflow_compiler assets/editor_demo.afproj build/debug/assets/editor_demo.afp
./build/debug/arcflow_player build/debug/assets/editor_demo.afp
./build/debug/arcflow_compiler --beta-audit assets/editor_demo.afproj
./build/debug/arcflow_compiler --migrate-project old.afproj migrated.afproj
```

See [building](docs/building.md), [architecture](docs/architecture.md), and the
[runtime API](docs/runtime-api.md). Beta severity, blocker policy, and project-audit details are in
[beta stabilization](docs/beta-stabilization.md); enforced CPU budgets are in
[performance targets](docs/performance.md), and required parser mutation thresholds are in
[deterministic fuzzing](docs/fuzzing.md).
