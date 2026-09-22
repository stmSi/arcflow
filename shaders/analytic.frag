#version 450
#extension GL_GOOGLE_include_directive : require
#include "paint.glsl"

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec2 in_bounds;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec4 in_parameters;

layout(location = 4) flat in uint in_paint;
layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const uint OUTPUT_IS_SRGB = 1U;

vec3 srgb_to_linear(vec3 color) {
    color = max(color, vec3(0.0));
    bvec3 use_low = lessThanEqual(color, vec3(0.04045));
    vec3 low = color / 12.92;
    vec3 high = pow((color + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, use_low);
}

vec3 linear_to_srgb(vec3 color) {
    color = max(color, vec3(0.0));
    bvec3 use_low = lessThanEqual(color, vec3(0.0031308));
    vec3 low = color * 12.92;
    vec3 high = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, use_low);
}

vec4 premultiplied_output(vec3 linear_color, float alpha) {
    vec3 output_color = OUTPUT_IS_SRGB != 0U ? linear_color : linear_to_srgb(linear_color);
    return vec4(output_color * alpha, alpha);
}

float rounded_rectangle_distance(vec2 local, vec2 size, float radius) {
    vec2 half_size = size * 0.5;
    float safe_radius = clamp(radius, 0.0, min(half_size.x, half_size.y));
    vec2 point = local - half_size;
    vec2 q = abs(point) - (half_size - vec2(safe_radius));
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - safe_radius;
}

float ellipse_distance(vec2 local, vec2 size) {
    vec2 radius = max(size * 0.5, vec2(0.0001));
    vec2 point = local - radius;
    float normalized = length(point / radius);
    float gradient = length(point / (radius * radius));
    return gradient > 0.0001 ? normalized * (normalized - 1.0) / gradient
                             : -min(radius.x, radius.y);
}

void main() {
    int kind = int(in_parameters.x + 0.5);
    float distance = kind == 3 ? ellipse_distance(in_uv, in_bounds)
                               : rounded_rectangle_distance(
                                     in_uv, in_bounds, kind == 2 ? in_parameters.y : 0.0);
    float antialias = max(fwidth(distance), 0.0001);
    float coverage = in_parameters.w > 0.0
                         ? 1.0 - smoothstep(in_parameters.w * 0.5 - antialias,
                                            in_parameters.w * 0.5 + antialias, abs(distance))
                         : 1.0 - smoothstep(-antialias, antialias, distance);
    vec4 color = sample_paint(in_paint, in_color);
    float alpha = clamp(color.a * in_parameters.z * coverage, 0.0, 1.0);
    out_color = premultiplied_output(srgb_to_linear(color.rgb), alpha);
}
