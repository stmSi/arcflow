#version 450

layout(set = 0, binding = 0) uniform sampler2D image_atlas;

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;

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

void main() {
    vec4 sampled = texture(image_atlas, in_uv);
    float alpha = clamp(sampled.a * in_color.a, 0.0, 1.0);
    vec3 linear_color = sampled.rgb * srgb_to_linear(in_color.rgb);
    out_color = premultiplied_output(linear_color, alpha);
}
