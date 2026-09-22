#version 450

layout(location = 0) in vec4 in_matrix;
layout(location = 1) in vec4 in_translation_bounds;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec4 in_parameters;
layout(location = 4) in uint in_paint;

layout(push_constant) uniform Canvas {
    vec2 viewport_size;
} canvas;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec2 out_bounds;
layout(location = 2) out vec4 out_color;
layout(location = 3) out vec4 out_parameters;
layout(location = 4) flat out uint out_paint;

const vec2 POSITIONS[6] = vec2[6](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
);

void main() {
    vec2 uv = POSITIONS[gl_VertexIndex];
    float outline_margin = max(in_parameters.w, 0.0) * 0.5;
    vec2 local = uv * (in_translation_bounds.zw + vec2(outline_margin * 2.0)) -
                 vec2(outline_margin);
    vec2 world = vec2(
        in_matrix.x * local.x + in_matrix.y * local.y + in_translation_bounds.x,
        in_matrix.z * local.x + in_matrix.w * local.y + in_translation_bounds.y
    );
    vec2 ndc = world * 2.0 / canvas.viewport_size - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    out_uv = local;
    out_bounds = in_translation_bounds.zw;
    out_color = in_color;
    out_parameters = in_parameters;
    out_paint = in_paint;
}
