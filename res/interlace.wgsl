struct UniformsInterlace {
    size: vec2f,
}

@group(0) @binding(0) var the_sampler: sampler;
@group(0) @binding(1) var even_field: texture_2d<f32>;
@group(0) @binding(2) var odd_field: texture_2d<f32>;
@group(0) @binding(3) var<uniform> the_uniforms: UniformsInterlace;

@fragment
fn fs_main(@builtin(position) pos: vec4f) -> @location(0) vec4f {
    let split = u32(floor(pos.y)) % 2;
    var color: vec4f;
    let coord = pos.xy / the_uniforms.size;
    if (split == 0) {
        color = textureSampleLevel(even_field, the_sampler, coord, 0);
    } else {
        color = textureSampleLevel(odd_field, the_sampler, coord, 0);
    }
    return color;
}
