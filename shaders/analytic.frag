#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec2 in_bounds;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec3 in_parameters;

layout(location = 0) out vec4 out_color;

float rounded_rectangle_coverage(vec2 uv, vec2 size, float radius) {
    vec2 half_size = size * 0.5;
    float safe_radius = clamp(radius, 0.0, min(half_size.x, half_size.y));
    vec2 point = uv * size - half_size;
    vec2 q = abs(point) - (half_size - vec2(safe_radius));
    float distance = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - safe_radius;
    return 1.0 - smoothstep(-fwidth(distance), fwidth(distance), distance);
}

float ellipse_coverage(vec2 uv) {
    vec2 point = uv * 2.0 - 1.0;
    float distance = length(point) - 1.0;
    return 1.0 - smoothstep(-fwidth(distance), fwidth(distance), distance);
}

void main() {
    int kind = int(in_parameters.x + 0.5);
    float coverage = 1.0;
    if (kind == 2) {
        coverage = rounded_rectangle_coverage(in_uv, in_bounds, in_parameters.y);
    } else if (kind == 3) {
        coverage = ellipse_coverage(in_uv);
    }
    float alpha = clamp(in_color.a * in_parameters.z * coverage, 0.0, 1.0);
    out_color = vec4(in_color.rgb * alpha, alpha);
}
