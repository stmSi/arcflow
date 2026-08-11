#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(push_constant) uniform Canvas {
    vec2 viewport_size;
} canvas;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

void main() {
    vec2 ndc = in_position * 2.0 / canvas.viewport_size - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    out_uv = in_uv;
    out_color = in_color;
}
