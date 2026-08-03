# Architecture

Arcflow keeps editable projects, compiler IR, and runtime packages as distinct representations. The
compiler validates the editable scene, path, style, layout, component, state-machine, and animation
model, copies runtime-relevant data into compiler IR, and writes a package without editor state.

Module boundaries:

- `core`: allocators, arenas, errors, safe arithmetic, handles, files, logging, hashes, and math.
- `scene`: stable node IDs, hierarchy validation, primitive/path references, and world transforms.
- `path`: bounded validation, adaptive flattening, hit testing, tessellation, and generations.
- `style`: strokes, dash patterns, gradients, inherited paint, and style presets.
- `layout`: anchors, pivots, safe areas, size constraints, and horizontal/vertical groups.
- `text`: UTF-8 validation, HarfBuzz shaping, FreeType faces, fallback, and bounded glyph atlases.
- `component`: shared definitions, nested instances, typed overrides, and instance playback state.
- `interaction`: transform-aware hit testing, pointer capture, focus, and bounded event queues.
- `state_machine`: typed inputs and bounded deterministic transition evaluation.
- `animation`: typed tracks, deterministic sampling, and allocation-free playback.
- `composition`: clips, masks, filters, blend modes, offscreen caching, and target pooling.
- `deformation`: path morphing, trim paths, shape keys, mesh deformation, and path following.
- `rigging`: bone hierarchies, skinning, bounded constraints, and fixed-step procedural motion.
- `assets`: explicit little-endian package encoding, validation, and decoding.
- `project`: human-readable editable project serialization and compiler-IR construction.
- `editor`: editor interaction state and a bounded, stable-ID command history.
- `runtime`: opaque public handles and playback instances; it has no SDL dependency.
- `platform`: SDL window and event integration.
- `renderer`: the only internal module that owns or records Vulkan resources and commands.
- `compiler`, `player`, and `editor`: executable front ends.

Scene and animation code never sees internal Vulkan objects. Runtime package data is decoded into
owned arrays, then each instance owns mutable scene nodes. An update copies immutable base nodes,
applies typed tracks, and evaluates world transforms without allocation.

Coordinates use a top-left origin, positive X right, positive Y down, and radians. A child local
transform is composed after its parent (`world = parent * local`). Evaluation order is parent
layout, node layout, base transform, animation override, then final world transform.

Milestones 1–13 cover vector authoring/playback through rigging and procedural motion. GPU particles
and their dependent VFX systems remain outside the current boundary.
