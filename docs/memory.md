# Memory

`AfAllocator` is the project allocation boundary. It reports failed aligned allocations and counts
total and per-frame allocations. `AfArena` provides overflow-checked linear allocation; `AfScratch`
restores an arena to its saved offset. Dynamic arrays grow only in authoring or loading paths.

Package loading allocates decoded package arrays. Instance creation allocates one mutable node array;
component states, interaction queues, and state-machine state/input storage are inline bounded
arrays. Package and instance handles use fixed-capacity generation slots, so stale handles are
rejected.

Normal `af_instance_update` starts allocation instrumentation, restores base nodes, advances time,
samples tracks, and evaluates world transforms. It performs zero general heap allocations. The
renderer maps one persistent host-coherent upload buffer at initialization and divides it into two
frame slices. Rendering writes into the current safe slice after waiting for its frame fence.
Each slice also reserves a storage-buffer region for 8,193 paint records (384 bytes each): one solid
sentinel and at most a fill and stroke gradient per node. Fragment shaders evaluate the bounded
16-stop records; no paint allocation or geometry rebuild is needed for color/stop edits. Analytic
instances use 72 bytes and path vertices 28 bytes, including their paint index.

Runtime-package limits are 4,096 nodes, 256 clips, 8,192 tracks, 65,536 keyframes, 512 paths, and
32,768 path commands. Each path has at most 4,096 commands; flattening, contours, subdivision depth,
tessellated vertices, and aggregate cached vertices have fixed caps. Editable projects use bounded
arrays for 64 clips, 1,024 tracks, and 16,384 keys. Timeline selections hold at most 256 keys and path
undo snapshots at most 4,096 aggregate commands per side, while undo storage has a fixed 16 MiB default byte budget. Pen, Knife and Join retain separate 512-command bounds. Editable text metadata is stored alongside project nodes and in node snapshots; runtime nodes remain unchanged. Geometry allocates
only when a path generation changes; transform, paint, opacity, and normal playback do not rebuild it.
Composition targets come from a bounded pool, shape and rig solvers use fixed-capacity scratch
storage, constraints cap iterations and chain length, and procedural updates cap fixed steps per
call. Skinning writes to a caller-owned output buffer and never mutates cached bind geometry.
