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

The editor provides resizable docked panels, persistent vector tools, transform gizmos, a dope
sheet, and a curve editor. The toolbar includes Pick, Node, Scissors, Segment Delete, Rectangle,
Rounded Rectangle, Ellipse, Line, Pen, Pencil, Polygon, Star, Arc, and Spiral tools. Construction happens directly on the
canvas; `Shift` constrains shapes/angles, `Ctrl` draws shapes from their center, Pen finishes with a
double-click or `Enter`, and `Esc` cancels an active construction. Pencil strokes are bounded and
simplified into editable paths. Generated path points and Bézier handles use the same animation
workflow as other vector paths. A selected shape exposes red/green move axes, blue axis/corner
scale handles, magenta
X/Y skew handles, an amber rotation handle, and a draggable gold pivot. `Shift` preserves the scale
ratio or snaps rotation to 15-degree steps; `Ctrl` scales around the object's center, and
`Shift+Ctrl` combines both scale behaviors. In the timeline, choose the labeled property and click
`+ FIRST KEY` (`K`). Move the playhead and manipulate the object on the canvas; once a track exists,
Arcflow creates or updates the next key automatically. Track rows show both the object and property
and can be clicked to select their target. Drag or box-select keys; use
`Ctrl+C`/`Ctrl+V`/`Ctrl+D`, `Delete`,
`Ctrl+[`/`Ctrl+]`, and
`Shift+R` to edit timing. `S`, `L`, and `B` choose step, linear, and cubic interpolation; `F7`–`F9`
apply easing presets; `F6` toggles curves; Space plays; and the ruler scrubs or edits the loop range.
The red playhead can also be grabbed and dragged from anywhere along its full vertical line.
Tool shortcuts are `V` Pick, `F10` Node, `X` Scissors, `D` Segment Delete, `R` Rectangle,
`Shift+R` Rounded Rectangle, `O` Ellipse, `L` Line, `P` Pen, `B` Pencil, `Y` Polygon, `A` Star,
`C` Arc, and `I` Spiral. In Node mode, the
existing `C`, `O`, `I`, `F`, `Shift+R`, and `Shift+M` path-editing shortcuts keep their contextual
meaning.
Inspector values are directly editable: click the HEX/RGBA, position, width/height, scale,
rotation, opacity, radius, layer, or stroke-width field and type. `Enter` applies, `Esc` cancels,
`Tab` advances, and `Shift+Tab` moves backward; standard `Ctrl+A/C/X/V` editing works. Hex colors
accept `#RGB`, `#RGBA`, `#RRGGBB`, and `#RRGGBBAA`; RGBA component fields use 0–255 values.
Drag a numeric field horizontally to scrub it (`Shift` for fine control, `Ctrl` for coarse control),
or use its embedded `−`/`+` buttons for quick steps. `RATIO` links width/height and scale X/Y while
typing, scrubbing, or stepping.
Timeline edits share bounded `Ctrl+Z`/`Ctrl+Y` history with scene edits. Scene shortcuts include
`1`–`4`, `F2`, arrows, `Q`/`E`, `-`/`+`, brackets, `U`, `Ctrl+S`, and `F5`. `4` creates an editable
path. Selecting a point opens a canvas action bar for point keys, line/curve conversion, insertion,
open/close, multi-node deletion, X/Y alignment and distribution, and handle smoothing. Shift-click
adds nodes, Ctrl-click toggles them, a canvas marquee selects nodes, and `Ctrl+A` selects every node.
Drag a selected node to move the whole node selection. Drag directly on a line or curve to reshape
the segment; Shift-dragging a Bézier handle preserves a smooth tangent and Ctrl-dragging makes the
opposite handle symmetric. `S` smooths selected handles and `Shift+S` makes them symmetric. Scissors
cuts the clicked line or cubic into editable contours with one undo; Segment Delete removes only the
clicked segment and preserves the surrounding open contours. The same
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

See [building](docs/building.md), [exporting](docs/exporting.md),
[architecture](docs/architecture.md), and the [runtime API](docs/runtime-api.md). The final automated
and external acceptance gates are in the [production release guide](docs/production-release.md).
Severity, blocker policy, and project-audit details remain in
[beta stabilization](docs/beta-stabilization.md); enforced CPU budgets are in
[performance targets](docs/performance.md), and required parser mutation thresholds are in
[deterministic fuzzing](docs/fuzzing.md).
