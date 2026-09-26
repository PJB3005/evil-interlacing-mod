struct UniformsBlit {
    size: vec2f,
}

@group(0) @binding(0) var the_texture: texture_2d<f32>;
@group(0) @binding(1) var the_sampler: sampler;
@group(0) @binding(2) var<uniform> the_uniforms: UniformsBlit;

@fragment
fn fs_main(@builtin(position) pos: vec4f) -> @location(0) vec4f {
    return textureSample(the_texture, the_sampler, pos.xy / the_uniforms.size);
}
