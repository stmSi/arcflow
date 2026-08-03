# Arcflow Runtime Package

Arcflow Runtime Package files use `.afp`, but validation relies on contents rather than extension.
All fields use explicit little-endian fixed-width encoding; raw C structures and pointers are never
serialized.

The 64-byte header contains the eight-byte `ARCFLOW\0` magic, format and minimum-runtime versions,
endianness marker, header size, total size, section-table offset/count, required-feature flags, and a
32-bit FNV-1a checksum covering bytes after the header. Format version 6 has node, clip, track,
keyframe, string, path, path-command, style, layout, component, and state-machine sections. Each
32-byte section entry records type, offset, byte size, alignment, and element count.

Validation is side-effect free and runs before any decode or GPU creation. It checks file limits,
overflow, table bounds, section alignment and overlap, record sizes and counts, unique node IDs,
parent references, hierarchy depth/cycles, clip and track ranges, node references, ordered keyframe
times, interpolation values, path ownership and command sequencing, finite bounded coordinates,
string termination, version compatibility, required features, and the checksum.

Package bytes are untrusted. The decoder only runs after validation and reads fields byte by byte.
Unknown required features and unsupported versions are rejected. Current packages contain runtime
vector styling, layout, reusable components, and state machines. They contain no embedded textures,
fonts, shaders, scripts, external paths, or editor metadata.
