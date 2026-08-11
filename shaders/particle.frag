#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;
layout(location = 2) flat in uint in_output;
layout(location = 3) in vec4 in_effect;

layout(location = 0) out vec4 out_color;

void main() {
    vec2 point = fract(in_uv) * 2.0 - 1.0;
    float distance_to_edge = length(point) - 1.0;
    float edge_width = max(fwidth(distance_to_edge), 0.001);
    float coverage = (in_output == 2U || in_output == 3U || in_output == 4U || in_output == 6U)
                         ? 1.0
                         : 1.0 - smoothstep(-edge_width, edge_width, distance_to_edge);
    vec3 color = in_color.rgb;
    if (in_output == 5U) {
        coverage = 1.0 - smoothstep(-edge_width, edge_width,
                                    abs(point.x) + abs(point.y) - 1.0);
    } else if (in_output == 7U) {
        color = mix(color, vec3(1.0, 0.42, 0.08), 0.35 + 0.25 * sin(in_uv.y * 18.0));
        coverage *= 0.45;
    } else if (in_output == 8U) {
        color = mix(color, vec3(0.5, 0.85, 1.0), 0.55);
        coverage *= 0.28 + min(abs(in_effect.x), 1.0) * 0.2;
    } else if (in_output == 9U) {
        float softness = clamp(in_effect.y / (in_effect.y + 8.0), 0.05, 0.95);
        coverage = 1.0 - smoothstep(-softness, 0.0, distance_to_edge);
    } else if (in_output == 10U) {
        coverage = pow(clamp(1.0 - length(point), 0.0, 1.0), 2.0);
        color *= 1.8;
    }
    float alpha = clamp(in_color.a * coverage, 0.0, 1.0);
    out_color = vec4(color * alpha, alpha);
}
