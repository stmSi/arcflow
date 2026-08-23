# Security

Projects and packages are untrusted. The editor and compiler limit editable projects to 32 MiB,
validate finite/ranged values, required fields, stable identifiers, parent references, hierarchy
cycles/depth, names, canvas bounds, bounded skew angles, versions, and node limits before use. Unknown optional project
fields are ignored for schema migration; malformed known data is rejected. Project data never
supplies scripts, shaders, or external paths. The player validates package bytes before loading.

Package validation enforces a 64 MiB file limit and fixed limits for sections, nodes, hierarchy
depth, clips, tracks, keyframes, paths, path commands, and strings. Arithmetic is checked before
ranges are formed.
Sections must be aligned, in bounds, outside the header/table, non-overlapping, uniquely typed, and
consistent with their element counts. Checksums, versions, endianness, required features, stable
identifiers, hierarchy references/cycles, track references, and keyframe ordering are checked before
allocation.

Package decoding uses fixed-width little-endian readers. It does not cast untrusted bytes to C
structures, follow serialized pointers, access external paths, or create GPU resources. Path
validation rejects non-finite coordinates, malformed command sequences, overlapping command ranges,
missing path nodes, and inputs beyond fixed command, contour, flattening, tessellation, and
subdivision limits. Recoverable malformed input returns `AfResult` rather than asserting.
Component and state-machine validation also rejects missing references, excessive nesting, cyclic
graphs, invalid typed overrides/conditions, and transition counts above fixed limits. Layout,
interaction, composition, deformation, rigging, and procedural APIs validate finite values and
fixed capacities. Runtime assets cannot introduce scripts or shader include paths.
