#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;
layout(location = 2) flat in uint in_output;

layout(location = 0) out vec4 out_color;

void main() {
    vec2 point = fract(in_uv) * 2.0 - 1.0;
    float distance_to_edge = length(point) - 1.0;
    float coverage = in_output == 2U
                         ? 1.0
                         : 1.0 - smoothstep(-fwidth(distance_to_edge), fwidth(distance_to_edge),
                                            distance_to_edge);
    float alpha = clamp(in_color.a * coverage, 0.0, 1.0);
    out_color = vec4(in_color.rgb * alpha, alpha);
}
