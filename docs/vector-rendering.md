# Vector rendering

Rectangles, rounded rectangles, and ellipses use an instanced batch of six-vertex
quads. No primitive is tessellated. Instance records contain a 2D affine transform, bounds, solid
color, primitive kind, corner radius, inherited opacity, optional centered-outline width, and layer
order. A visible outline adds one expanded signed-distance instance after its fill. A stable merge
sort preserves layer and painter order before drawing; adjacent compatible buffer ranges on the
same layer are batched, including instanced analytic shapes.

The vertex shader converts local pixel coordinates through the world affine transform into a
top-left, Y-down orthographic canvas. Canvas dimensions come from the current swapchain extent.
Rectangles, rounded rectangles, and ellipses compute signed-distance coverage from fragment
derivatives. The same shader evaluates either filled interior coverage or the ring coverage for a
centered outline.

Arbitrary paths are adaptively flattened on the CPU with fixed subdivision-depth,
flattened-point, and contour limits. Scan-band tessellation handles self intersections and compound
contours using either non-zero or even-odd winding. Tessellated local vertices are cached by stable
path ID and geometry generation; transform, opacity, fill-color, and layer changes reuse geometry.
Mixed analytic/path draw items are stable-sorted by layer and paths use a dedicated pipeline.

Input colors are straight sRGB RGBA authoring values. Fragment shaders decode authored RGB to
linear light, multiply alpha by inherited opacity and coverage, and output premultiplied RGBA.
Blending is `ONE` against `ONE_MINUS_SRC_ALPHA` for both color and alpha. The swapchain prefers an
sRGB format, which encodes the linear blended result for display. The UNORM fallback explicitly
encodes the shader result so its presentation remains color-correct.

Path styling supports bounded dashed strokes with butt/round/square caps and miter/round/bevel
joins. Vector paints support solid, linear, radial, and conic gradients. Geometry and paint
generations are tracked separately so paint-only changes reuse tessellation. Composition plans
support rectangle, rounded-rectangle, and path clips; alpha/inverted/nested masks; common
premultiplied blend modes; bounded filters; cached offscreen layers; and pooled targets. Shape
morphing and skinning always write into separate output storage so cached source geometry remains
immutable.
