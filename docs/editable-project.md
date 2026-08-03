# Editable projects

Arcflow editable projects use the `.afproj` extension and a bounded, line-oriented text format.
Version 3 starts with `arcflow_project 3`, declares reference-canvas and stable-ID counters, stores
nodes in `node`/`end` blocks, paths in typed `path`/`end_path` blocks, and clips, tracks, and keys in
typed animation blocks. Versions 1 and 2 remain loadable. Path blocks contain bounded move, line,
cubic, and close commands plus fill rule and geometry generation. Node and clip names, loop regions,
and editor-friendly hierarchy data remain in this file. Selection, panel layout, command history,
and other transient state are not serialized.

The loader accepts unknown optional fields to allow schema migration, while rejecting duplicate or
missing required fields, unsupported versions, non-finite/out-of-range values, duplicate IDs,
missing parents, cycles, excessive depth/count/size, and invalid names. Saving validates the entire
project before replacing the requested file. Animation validation also checks clip ranges, property
types, references, easing controls, finite values, and IDs using bounded authoring storage.

The editor command history stores before/after snapshots addressed by stable IDs. Scene, path, and
timeline creation, deletion, duplication, point/handle, contour, timing, interpolation, tangent, and
loop edits use the same apply/undo/redo path. Deleting an animated path node captures its path
commands, tracks, and keys atomically.
Continuous edits can merge, and history allocation is fixed to a byte budget at editor startup.

Compilation validates the project again, creates a separate compiler scene containing only runtime
fields, sorts authoring keys deterministically, then writes a versioned Arcflow Runtime Package.
Editor names, loop-region handles, and selection state cannot enter the package.
