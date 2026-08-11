#version 450

struct Particle {
    vec4 position_age;
    vec4 velocity_rotation;
    vec4 sizes;
    vec4 color;
    uvec4 meta;
};

struct Emitter {
    uvec4 meta0;
    uvec4 meta1;
    vec4 transform0;
    vec4 transform1;
    vec4 position_range;
    vec4 velocity_range;
    vec4 lifetime_spawn;
    vec4 rotation_range;
    vec4 size_range;
    vec4 color_start;
    vec4 color_end;
    vec4 forces;
    vec4 flipbook;
    uvec4 settings;
};

layout(std430, binding = 0) readonly buffer Particles { Particle particles[]; };
layout(std430, binding = 1) readonly buffer AliveA { uint alive_a[]; };
layout(std430, binding = 2) readonly buffer AliveB { uint alive_b[]; };
layout(std430, binding = 5) readonly buffer Emitters { Emitter emitters[]; };

layout(push_constant) uniform ParticleView {
    vec2 viewport_size;
    uint parity;
    uint reserved;
} view;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;
layout(location = 2) flat out uint out_output;

const vec2 POSITIONS[6] = vec2[6](
    vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5),
    vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5)
);

const vec2 UVS[6] = vec2[6](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
);

void main() {
    uint particle_index = view.parity == 0U ? alive_a[gl_InstanceIndex] : alive_b[gl_InstanceIndex];
    Particle particle = particles[particle_index];
    Emitter emitter = emitters[particle.meta.x];
    vec2 corner = POSITIONS[gl_VertexIndex] * particle.sizes.z;
    float sine = sin(particle.velocity_rotation.z);
    float cosine = cos(particle.velocity_rotation.z);
    vec2 rotated = vec2(cosine * corner.x - sine * corner.y,
                        sine * corner.x + cosine * corner.y);
    vec2 world = particle.position_age.xy + rotated;
    vec2 ndc = world * 2.0 / view.viewport_size - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    out_uv = UVS[gl_VertexIndex];
    if (particle.meta.y == 2U) {
        uvec2 grid = max(emitter.settings.zw, uvec2(1U));
        uint frame_count = grid.x * grid.y;
        uint frame = uint(particle.position_age.z * emitter.flipbook.x) % frame_count;
        out_uv = (vec2(frame % grid.x, frame / grid.x) + out_uv) / vec2(grid);
    }
    out_color = particle.color;
    out_output = particle.meta.y;
}
