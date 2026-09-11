# Performance targets

Arcflow's long-duration CPU gate uses the shipped `vector-animation` template as a stable representative
workload. The gate compiles a stripped release package, validates it, repeatedly loads it, and then
plays 60,000 updates after warm-up. It runs in Debug, sanitizer, and optimized CI configurations.

The Arcflow 1.0 minimum targets are:

* Mean package load no greater than 50 ms over 16 loads.
* Mean runtime update no greater than 1 ms over 60,000 frames.
* Zero general heap allocations in every measured runtime update.
* A valid scene after 1,000 seconds of simulated playback.

Run the gate directly with:

```sh
./build/debug/arcflow_beta_performance_tests
```

It prints one JSON object for CI collection. These CPU thresholds are release blockers when the
same workload hash and supported hardware class regress. They do not substitute for renderer frame
time, GPU memory, integrated/discrete GPU, vendor-driver, or long-duration interactive testing;
those results belong to the hardware matrix before stable release.

`arcflow_release_tests` additionally release-compiles, validates, loads, resizes, and plays all six
shipped templates for an aggregate 3,600 frames while checking the stable Host SDK and zero runtime
frame allocations. The production acceptance target runs both gates.

## Editor, layout, and rendering hot paths

Scene world evaluation now builds a temporary ID index instead of scanning the node array for
parents. Layout uses that index to resolve its node records and computes sibling offsets once per
parent, in scene order. Editor animation gathers the active clip's bracketing keys in one pass,
including deterministic handling of duplicate timestamps and unsorted key storage. Native runtime
tracks use binary search over their sorted keys. These lookups are rebuilt for each evaluation so
edits, undo, hierarchy changes, and playback cannot leave stale cached references.

The renderer appends draw items, stable-sorts the complete list in bounded O(n log n) work, and
combines adjacent items with matching type, layer, atlas, and contiguous buffer ranges. Analytic
shapes use instanced draws. Layer boundaries and equal-layer painter order remain intact, including
particle insertion. Already sorted lists skip sorting. The renderer retains 160 KiB of sort scratch;
scene, layout, and animation lookups use bounded stack scratch and introduce no frame allocations.
Only the occupied portions of lookup state are cleared for small documents.

An optional benchmark reports fixed CPU workloads as JSON lines:

```sh
cmake --build --preset development --target arcflow_hotpath_benchmarks
./build/development/arcflow_hotpath_benchmarks
```

Local before/after measurements on 2026-09-07, Linux x64, Intel Core i5-13420H, optimized `-O2`
builds, median of seven batches of eight evaluations:

| Workload | Before | After |
| --- | ---: | ---: |
| 2,048 nodes with parents late in storage | 1.755 ms | 0.053 ms |
| 512 layout nodes in one horizontal stack | 7.783 ms | 0.033 ms |
| 256 editor tracks, 4,096 interleaved keys | 0.383 ms | 0.034 ms |
| 256 runtime samples from a 4,096-key track | 0.196 ms | 0.003 ms |
| Preparing 4,096 mixed-layer draw items | 1.675 ms | 0.057 ms |

A separate ordered workload with 4,096 contiguous shapes across 16 layers drops from 4,096 draw
items to 16 batches. Its CPU preparation increases from roughly 3 to 7 microseconds to perform
batching; the benefit is fewer Vulkan draw commands. Small 8-node scene/layout and 2-track editor
fixtures also improved in local checks. These are isolated CPU measurements, not an overall frame
rate guarantee, and vary with CPU load and frequency.

Regression coverage checks reversed hierarchy storage, cycles, missing parents, live layout edits,
interleaved keys, duplicate timestamps, stable sorting, atlas/range boundaries, and draw batching.
The vector tools smoke capture also matched a build with batching disabled byte-for-byte.

## Canvas pan frame cost

Editor text outlines now use local coordinates and geometry-based cache IDs. Moving a label,
changing its color, or inserting ruler/grid nodes before it no longer retessellates its outline.
Screen placement stays in the node transform, and duplicate labels retain distinct path records.

Run the middle-button drag check with Vulkan validation enabled:

```sh
tools/run-editor-validation.sh assets/editor_demo.afproj --pan-smoke --frames 180 --output /tmp/pan.png
```

The check first verifies text cache identity across movement, preceding UI insertion, duplicate
labels, and changed text, then sends middle-button press/motion events with reversing pan direction.
It prints mean scene build time, renderer upload/preparation time, frame interval, maximum interval,
and total fill/stroke path rebuilds. Ten warm-up frames and the capture frame are excluded. Saved
dock preferences affect the viewport; use the same layout and project for comparisons.

Local Debug measurements on 2026-09-09, Linux x64, Intel Core i5-13420H, with the Vulkan validation
layer enabled and the same demo/layout, over 169 measured frames:

| Metric | Before | After |
| --- | ---: | ---: |
| Mean frame interval | 47.021 ms | 9.142 ms |
| Maximum frame interval | 74.247 ms | 12.149 ms |
| Mean scene build | 4.485 ms | 3.627 ms |
| Mean renderer upload/preparation | 41.490 ms | 3.203 ms |
| Total path rebuilds | 20,433 | 44 |

These are local frame measurements, not a hardware-independent frame rate guarantee. Newly visible
or edited labels still rebuild as needed. The fix removes the dominant tessellation work during
panning; text shaping and GPU buffer upload still run each frame.

## Editor review: 2026-09-11

The editor skips scene construction and GPU submission while minimized, hidden, or without a
valid drawable. It still processes close/restore events, autosave, and collaboration lock timers,
using a bounded event wait. An X11 minimize/restore/close check measured 88 CPU clock ticks over
one visible second, 1 tick over two minimized seconds, and 83 ticks over one restored second;
closing while minimized completed successfully in 65 ms. Visible idle rendering still rebuilds
and uploads each frame; event-driven invalidation is a remaining optimization opportunity.

The hidden validation panel no longer calls full project validation every frame. The live
profiler (F10) and pan-smoke JSON expose CPU upload/preparation time and GPU timestamp results.
GPU results come from the last completed frame slot after its fence signals, without an extra
GPU wait. Unsupported timestamp queries display N/A. The measured GPU interval includes render
commands but excludes presentation and screenshot readback; `draw_batches` counts scene batches,
excluding particle draws.

Sequential local Debug pan runs, 1440 x 840, the demo and same saved dock layout, 169 samples after
warm-up, Vulkan validation enabled, on an Intel Core i5-13420H:

| Forced Vulkan driver | Build | Upload/preparation | Frame interval | GPU | Scene batches |
| --- | ---: | ---: | ---: | ---: | ---: |
| NVIDIA RTX 4050 Laptop, 610.57.04 | 4.084 ms | 3.744 ms | 9.547 ms | 2.048 ms | 205.3 |
| Intel RPL-P, Mesa 26.1.7 | 3.330 ms | 3.013 ms | 7.193 ms | 1.484 ms | 205.3 |

Both runs completed without Vulkan validation errors and reported 39 path rebuilds total.
These short samples identify CPU scene building/upload as the main remaining frame cost for this
fixture; they are not a general comparison of the two GPUs. To reproduce on this Linux host:

```sh
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json tools/run-editor-validation.sh \
  assets/editor_demo.afproj --pan-smoke --frames 180 --output /tmp/pan-nvidia.png
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json tools/run-editor-validation.sh \
  assets/editor_demo.afproj --pan-smoke --frames 180 --output /tmp/pan-intel.png
```

UI review at 1440 x 840 and 1120 x 680 also corrected the Node shortcut conflict (Shift+V selects
Node; F10 retains the profiler), wheel events affecting neighboring docks, fractional/inverted
wheel zoom, overlapping timeline controls/clip tabs, and loop bars painting into the inspector.
The compact clip tab cycles through all clips; header hit targets and tooltips follow its layout.
The vector-tools smoke includes shortcut, pointer-anchor, wheel routing, compact playback, and
clip-switching assertions alongside construction, path surgery, grouping, reparenting, and undo.
The final vector-tools, vector-preview, inspector/resize, workflow, menu/resize, particle-view,
and image smoke runs all passed with Vulkan validation. The vector-tools workflow also passed
under strict UBSan, and the repository formatting check passed.

A strict UBSan run (`UBSAN_OPTIONS=halt_on_error=1 ctest --preset ubsan`) passed 17 of 18 suites.
Import fuzzing stopped in the unchanged bundled FreeType `src/cff/cffload.c:639`: a zero-length
`FT_MEM_COPY` receives a null source pointer. This dependency finding remains open; the normal
Debug suite passes all 18 tests.
