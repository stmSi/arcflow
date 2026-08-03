# Vector rendering

Rectangles, rounded rectangles, and ellipses use an instanced batch of six-vertex
quads. No primitive is tessellated. Instance records contain a 2D affine transform, bounds, solid
color, primitive kind, corner radius, inherited opacity, and layer order. CPU insertion sorting
preserves layer order before one instanced draw.

The vertex shader converts local pixel coordinates through the world affine transform into a
top-left, Y-down orthographic canvas. Canvas dimensions come from the current swapchain extent.
Rounded rectangles and ellipses compute coverage from fragment derivatives; ordinary rectangles use
the quad coverage.

Arbitrary paths are adaptively flattened on the CPU with fixed subdivision-depth,
flattened-point, and contour limits. Scan-band tessellation handles self intersections and compound
contours using either non-zero or even-odd winding. Tessellated local vertices are cached by stable
path ID and geometry generation; transform, opacity, fill-color, and layer changes reuse geometry.
Mixed analytic/path draw items are stable-sorted by layer and paths use a dedicated pipeline.

Input colors are straight RGBA authoring values. The fragment shader multiplies alpha by inherited
opacity and coverage, premultiplies RGB, and outputs premultiplied RGBA. Blending is `ONE` against
`ONE_MINUS_SRC_ALPHA` for both color and alpha. The swapchain prefers an sRGB format. Shader color
values are treated as linear and encoded by the sRGB attachment.

Vector styling supports bounded dashed strokes with butt/round/square caps and miter/round/bevel
joins, plus linear, radial, and conic gradients. Geometry and paint generations are tracked
separately so paint-only changes reuse tessellation. Composition plans support rectangle,
rounded-rectangle, and path clips; alpha/inverted/nested masks; common premultiplied blend modes;
bounded filters; cached offscreen layers; and pooled targets. Shape morphing and skinning always
write into separate output storage so cached source geometry remains immutable.
