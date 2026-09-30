# Exporting from Arcflow Editor

Open **File > Export...** (`Ctrl+Shift+E`) to configure and preview the output before choosing a
destination. Set **Width** and **Height** (1–16,384 pixels), choose **Canvas** or **Selection**, and
use **Fit content** to crop the viewport around the chosen artwork, including negative coordinates.
**Page size** restores the document viewport. Dimensions change the output boundary without scaling
the artwork or changing the project canvas. MP4 dimensions must be even.

The preview clips artwork to the output boundary and counts objects that extend outside it. A
permanent boundary also marks the document page in the drawing canvas. SVG, GIF, and MP4 use a
compatible vector preview with the same object-omission rules as the SVG exporter; this is not a
preview of the GIF palette or H.264 encoding. Lottie/dotLottie show the Arcflow scene and disclose
conversion warnings, rather than promising exact rendering in a third-party player. Compatibility
checks refresh when settings change. The white preview paper is not an exported background; MP4
does not support transparency.

Selection includes selected descendants, preserves parent transforms and animation, and retains
empty structural groups where flow layouts need space from unselected siblings. Inherited
appearances are resolved in the export copy. Fit Content uses curve extrema and conservative stroke
padding. The source project, decoded image pixels, and undo history remain unchanged. Native
packages retain responsive layout settings; their playback viewport is controlled by the host,
rather than a fixed pixel crop.

Choose one of these formats, then press **Export...** to open the native save dialog:

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
  frame at the configured export viewport size. Solid-filled and solid-stroked rectangles, rounded rectangles,
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
