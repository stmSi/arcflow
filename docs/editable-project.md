# Editable projects

Arcflow editable projects use the `.afproj` extension and a bounded, line-oriented text format.
The current format is version 14 and starts with `arcflow_project 14`. It declares reference-canvas
and stable-ID counters, stores nodes in `node`/`end` blocks, paths in typed `path`/`end_path` blocks,
and clips, tracks, and keys in typed animation blocks. Earlier supported versions are migrated by
loading their known fields and applying bounded defaults for fields introduced later. Saving always
emits the current format. Path blocks contain bounded move, line, cubic, and close commands plus fill
rule and geometry generation. Node transforms store position, rotation, scale, and bounded X/Y skew
angles. Version 11 and earlier projects load with zero skew. Node and clip names, loop regions, and
editor-friendly hierarchy data remain in this file. Selection, panel layout, command history, and
other transient state are not serialized.

Version 13 adds `polygon <sides> <inset>` to node blocks. Zero sides/inset denotes raw geometry;
3–64 sides retains polygon/star authoring settings, and a positive inset is the inner/outer radius
ratio of a star (at most 0.95). Generated paths remain stored explicitly, so runtime compilation
uses ordinary vector geometry. Older projects default to zero and keep their existing path edits.
Converting a parametric shape to a raw path clears these settings; undo restores them.

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

Use `arcflow_compiler --migrate-project INPUT.afproj OUTPUT.afproj` for an explicit migration. The
compiler writes no output unless the migrated project produces the exact same validated release
package bytes and normalized editable-project data as the source. Project-relative assets must be
available relative to the destination; keep the output beside the source or copy its asset directory
first.

## Editable text (format 14)

A path node may include `text_settings` (mode, font size, paragraph width, path offset),
`text_utf8` (lowercase hex-encoded UTF-8), and `text_format` (font family, bold, italic, alignment,
letter spacing, line spacing). All three fields are required together. Modes 1/2/3 are artistic,
paragraph and path text; font families 0/1/2 are bundled Liberation Sans/Serif/Mono. Alignment
0/1/2 means left/center/right. Zero line spacing means the default 1.2 multiplier. Path text also
stores up to 32 `text_guide` commands (type, endpoint, control1, control2) as a copied single contour.
Text content is limited to 511 UTF-8 bytes. Font size is 1–1000; letter spacing is bounded to
±1000 object units, and line spacing to 10 times the font size.

Normal path blocks hold the generated glyph outlines. Rendering and runtime compilation consume
those outlines without a font dependency. Editing regenerates geometry and text metadata together;
conversion to curves removes the text fields through undoable snapshots. Older formats still load,
and runtime-package format remains 13. Invalid/missing text fields are rejected during parsing.
