# Animation

Tracks target stable node IDs and one typed property: position, rotation, scale, opacity, fill color,
or corner radius. Keyframes store seconds, up to four value components, interpolation, and cubic
Bézier easing controls. No pointer is serialized.

Sampling before or after a track uses its endpoint. Exact times select the last keyframe at that
time, making duplicate-time behavior deterministic. Step holds the left value; linear interpolates
components; cubic Bézier solves easing X deterministically and applies easing Y to component
interpolation.

Playback supports play, pause, stop, seek, looping, speed, forward, and reverse. Looping uses a
positive modulo over clip duration. Each runtime update restores the package's base scene, evaluates
animation at the current time, then computes parent-to-child world transforms. The same package,
time, inputs, and seed produce the same result, and normal update allocates no general heap memory.

Advanced authoring supports clip duplication/trimming/splitting, markers and regions, nested
timelines, time remapping, hold frames, blend/additive layers, transitions, speed curves, motion
paths, ghost frames, retiming, and reusable easing presets. Shape tracks cover path points, strokes,
gradient stops/transforms, and masks. Bone tracks and fixed-step procedural tracks can be baked to
ordinary keyframes. State machines evaluate typed inputs, events, completion, exit time, and nested
states under strict per-update transition limits.
