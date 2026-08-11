#version 450

struct Particle {
    vec4 position_age;
    vec4 velocity_rotation;
    vec4 sizes;
    vec4 color;
    uvec4 meta;
    vec4 previous_depth;
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
    vec4 advanced0;
    vec4 force_center;
    vec4 attractor;
    vec4 repulsor;
    vec4 vector_field;
    vec4 collision_plane;
    vec4 collision_extra;
    vec4 depth_velocity;
    vec4 render0;
    vec4 render1;
    uvec4 sub_settings;
    uvec4 event_settings;
};

layout(std430, binding = 0) readonly buffer Particles { Particle particles[]; };
layout(std430, binding = 1) readonly buffer AliveA { uint alive_a[]; };
layout(std430, binding = 2) readonly buffer AliveB { uint alive_b[]; };
layout(std430, binding = 5) readonly buffer Emitters { Emitter emitters[]; };

layout(push_constant) uniform ParticleView {
    vec2 viewport_size;
    uint parity;
    uint reserved;
    vec4 transform;
    vec4 translation;
} view;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;
layout(location = 2) flat out uint out_output;
layout(location = 3) out vec4 out_effect;

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
    vec2 uv = UVS[gl_VertexIndex];
    vec2 corner = POSITIONS[gl_VertexIndex] * particle.sizes.z;
    if (particle.meta.y == 5U) {
        corner *= emitter.render0.z;
    } else if (particle.meta.y == 10U) {
        corner *= emitter.render1.z;
    }
    float sine = sin(particle.velocity_rotation.z);
    float cosine = cos(particle.velocity_rotation.z);
    if (particle.meta.y == 6U) {
        sine = sin(particle.velocity_rotation.z + emitter.render0.w);
        cosine = cos(particle.velocity_rotation.z + emitter.render0.w);
    }
    vec2 rotated = vec2(cosine * corner.x - sine * corner.y,
                        sine * corner.x + cosine * corner.y);
    vec2 world = particle.position_age.xy + rotated;
    if (particle.meta.y == 3U || particle.meta.y == 4U) {
        vec2 direction = particle.position_age.xy - particle.previous_depth.xy;
        if (length(direction) < 0.00001) {
            direction = particle.velocity_rotation.xy;
        }
        direction = length(direction) > 0.00001 ? normalize(direction) : vec2(1.0, 0.0);
        vec2 normal = vec2(-direction.y, direction.x);
        float width = emitter.render0.y;
        if (particle.meta.y == 4U) {
            width *= 0.65 + 0.35 * sin(particle.position_age.z * 8.0);
        }
        world = mix(particle.position_age.xy - direction * emitter.render0.x,
                    particle.position_age.xy, uv.x) + normal * ((uv.y - 0.5) * width);
    } else if (particle.meta.y == 7U || particle.meta.y == 8U) {
        world.x += sin(world.y * 0.08 + particle.position_age.z * 12.0) * emitter.render1.x;
    }
    world = vec2(view.transform.x * world.x + view.transform.y * world.y,
                 view.transform.z * world.x + view.transform.w * world.y) +
            view.translation.xy;
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
    out_effect = emitter.render1;
}
