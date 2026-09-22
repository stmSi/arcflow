// std430 layout shared with AfGpuPaint. Index zero is the solid-color fast path.
struct Paint {
    vec4 inverse_x;
    vec4 inverse_y;
    vec4 endpoints;
    vec4 parameters;
    vec4 colors[16];
    vec4 offsets[4];
};
layout(std430, set = 0, binding = 0) readonly buffer Paints { Paint paints[]; };
float paint_offset(uint index, uint stop) {
    return paints[index].offsets[stop / 4U][stop % 4U];
}
vec4 sample_paint(uint index, vec4 solid) {
    if (index == 0U) return solid;
    vec3 screen = vec3(gl_FragCoord.xy, 1.0);
    vec2 local = vec2(dot(paints[index].inverse_x.xyz, screen),
                      dot(paints[index].inverse_y.xyz, screen));
    vec4 endpoints = paints[index].endpoints;
    vec4 parameters = paints[index].parameters;
    float position;
    if (parameters.w == 1.0) {
        vec2 delta = endpoints.zw - endpoints.xy;
        float denominator = dot(delta, delta);
        position = denominator <= 0.000001 ? 0.0 : dot(local - endpoints.xy, delta) / denominator;
    } else if (parameters.w == 2.0) {
        float span = parameters.y - parameters.x;
        position = abs(span) <= 0.000001 ? 0.0 : (length(local - endpoints.xy) - parameters.x) / span;
    } else {
        vec2 delta = local - endpoints.xy;
        float angle = dot(delta, delta) == 0.0 ? 0.0 : atan(delta.y, delta.x);
        position = fract((angle - parameters.z) / 6.283185307179586);
    }
    uint count = uint(paints[index].inverse_y.w);
    vec4 color = paints[index].colors[count - 1U];
    if (position <= paint_offset(index, 0U)) color = paints[index].colors[0];
    else {
        for (uint stop = 1U; stop < count; ++stop) {
            float end = paint_offset(index, stop);
            if (position <= end) {
                float begin = paint_offset(index, stop - 1U);
                float span = end - begin;
                color = mix(paints[index].colors[stop - 1U], paints[index].colors[stop],
                            span <= 0.000001 ? 1.0 : (position - begin) / span);
                break;
            }
        }
    }
    color.a *= paints[index].inverse_x.w;
    return color;
}
