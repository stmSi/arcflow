#version 450

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main() {
    float alpha = clamp(in_color.a, 0.0, 1.0);
    out_color = vec4(in_color.rgb * alpha, alpha);
}
