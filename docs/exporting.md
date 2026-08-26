# Exporting from Arcflow Editor

Open **File > Export...** (`Ctrl+Shift+E`) and choose the output for its destination:

- **Runtime Package (`.afp`)** preserves clips, keyframes, vectors, images, particles, layouts,
  components, state machines, and bindings. The editor compiles a production-stripped package,
  validates the finished bytes, and only then atomically replaces the destination. Use this output
  with Arcflow Player, the runtime C API, or Host SDK integrations.
- **Lottie JSON (`.json`)** exports the selected clip as portable vector animation at 60 FPS.
  Rectangles, rounded rectangles, ellipses, static Bézier paths, hierarchy, solid fill/stroke, and
  animated position, rotation, scale, opacity, fill color, and corner radius are represented.
  Step, linear, and cubic Bézier keyframe interpolation are retained. Animated path points,
  particles, images, gradients, variable-width/dashed strokes, and two-axis skew have compatibility
  fallbacks or warnings.
- **dotLottie (`.lottie`)** packages the same Lottie animation in a dotLottie v2 ZIP container with
  a valid root `manifest.json` and `a/animation.json`. Use it when a player or delivery service
  prefers one portable animation file.
- **Static Vector (`.svg`)** evaluates the selected clip at the current playhead time and exports that
  frame at the project canvas size. Solid-filled and solid-stroked rectangles, rounded rectangles,
  ellipses, and paths are represented directly. Features without a static SVG representation, such
  as particles, images, gradients, variable-width strokes, and some composed effects, are omitted
  and counted as compatibility warnings in the editor status.
- **Animated GIF (`.gif`)** evaluates the complete selected clip at 30 FPS, writes temporary SVG
  frames beside the destination, and encodes a looping palette-optimized GIF with FFmpeg.
- **MP4 Video (`.mp4`)** uses the same evaluated 30 FPS frames and encodes H.264 with YUV420p pixel
  format and fast-start metadata for broad browser and device compatibility.

Export does not save or modify the editable project. Unsaved in-memory edits are included in the
output. The save dialog supplies the expected extension when it is omitted. Package, SVG, Lottie,
and dotLottie writes are atomic and retain one rotating backup. GIF and MP4 encode to a unique
temporary destination and replace the requested file only after FFmpeg exits successfully. Temporary
frames and partial media output are removed after success or failure.

GIF and MP4 require `ffmpeg` on `PATH`. The FFmpeg build must provide SVG decoding (commonly through
`librsvg`); MP4 additionally requires the `libx264` encoder. Raster media currently inherits the
static SVG compatibility boundary, although animated transforms, colors, radii, and path points are
evaluated independently for every frame. A clip above 2,048 output frames is rejected instead of
creating an unexpectedly large temporary sequence.

`F5` remains the quick development compile command. It writes the non-stripped runtime package next
to a saved project; use **File > Export... > Runtime Package** for a release deliverable or a custom
destination.
