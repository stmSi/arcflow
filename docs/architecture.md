# Architecture

Dependency direction: **Arcflow → AF Framework → third-party libraries**.
The framework is a Git submodule in `framework/` and builds independently.

| Layer | Owns |
| --- | --- |
| Framework foundation | Memory, files, logging, math |
| Framework engine | Scenes, paths, styles, layout, text, animation, particles |
| Framework UI | Drawing, inputs, widget feedback, actions, shortcuts |
| Framework platform/Vulkan | SDL windows, input, GPU resources, rendering |
| Arcflow | Projects, packages, editing/undo, templates, exports, plugins, updates |

Framework headers use `af/`; CMake targets use `af::`.
Boundary tests reject application includes. SDK tests build an independent consumer.

`src/editor/editor_app_draw.inc` connects editor state to `AfUiContext`.
`src/renderer/runtime_adapter.c` converts borrowed Vulkan handles for Arcflow's runtime.
Compatibility headers forward legacy result, event, action, and input types.

Editable projects compile into validated `.afp` runtime packages.
Playback copies base nodes, applies animation, and evaluates transforms without allocation.
Coordinates start at the top left: X right, Y down, angles in radians.
Child transforms compose as `world = parent * local`.

See [AF Framework](https://github.com/stmSi/af-framework#readme) for builds, targets, and SDK use.
