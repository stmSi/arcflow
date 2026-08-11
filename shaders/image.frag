#version 450

layout(set = 0, binding = 0) uniform sampler2D image_atlas;

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 sampled = texture(image_atlas, in_uv) * in_color;
    out_color = vec4(sampled.rgb * sampled.a, sampled.a);
}
