# Animation

Tracks target stable node IDs and one typed property: position, rotation, scale, skew, opacity, fill
color, corner radius, a path point/control, outline color, or gradient geometry/stop color/offset.
Keyframes store seconds, up to four value components, interpolation, and cubic
Bézier easing controls. No pointer is serialized.

## Drawing frames

Click **Frames** (`Shift+F6`) to draw with the existing vector tools. Click a numbered cell or use
`[` / `]` to step. **Blank** starts an empty drawing; **Duplicate** copies the current drawing into
the next available frame. Each copy is independently editable. **Hold** extends a drawing into
empty cells; **FPS** sets playback speed. Both have −/+ controls.

**Onion skin** previews the previous and next drawings in blue and pink. **Fit clip length** trims
playback and exports to the drawings and existing animation keys. Frame numbers start at 1 (time 0).
Drawings, timing, and undo work in desktop and browser projects; ordinary objects remain backgrounds.

## Authoring workflow

Select an object, position the playhead, choose the labeled property in the timeline, and click
`+ FIRST KEY` (or press `K`). Move the playhead and manipulate the object with its canvas handles;
after a track exists, canvas and inspector edits create or update the key at that time automatically.
The dope sheet identifies every track by object and property, and clicking a row selects its target.
Drag the upper red playhead marker or its vertical track line to scrub. Drag either endpoint of the
lower green loop bar to resize the playback range, or drag the bar itself to move the range. Use the graph view
after selecting the track whose easing needs adjustment.

In **Curves** (`F6`), click or box-select keys and drag them horizontally to retime.
`Shift`/`Ctrl`-click adds or removes keys; `Ctrl+A` selects the displayed track's keys.
Choose **Linear**, **Hold**, **Ease in**, **Ease out**, **Ease both**, or **Overshoot**, then drag the
handles attached to the selected key's next segment. Easing applies to all components of that segment.
Switch **X/Y** or **RGBA** to inspect each component. Values scale automatically, including overshoot;
**Fit graph** (`F`) fits the track in time. Wheel zooms and `Shift`+wheel pans.
`Delete`, copy/paste, and duplicate operate on keys while the timeline is focused. `Esc` cancels a
key/handle drag; `Ctrl+Z` undoes a finished edit. Constant segments and last keys have no easing handles.

For paths, select a visible point and use the canvas action bar to key the point, convert its incoming
segment between line and cubic, insert a point, open or close the contour, or delete the segment
point. Cubic in/out handles remain directly draggable and can be keyed independently from the
property chooser.

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

## Animation delivery

The editor exports the selected clip to Lottie JSON or a dotLottie v2 container for portable vector
playback. Core shape layers, hierarchy, transforms, solid styling, and standard keyframe easing map
directly. Arcflow's `.afp` remains the lossless target for particles, images, advanced path motion,
layouts, bindings, and runtime behavior. GIF and H.264 MP4 provide a rendered 30 FPS fallback for
players that do not support vector animation; see [exporting](exporting.md) for dependencies and the
current compatibility boundary.
