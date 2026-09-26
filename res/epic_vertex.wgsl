@vertex
fn vs_main(@builtin(vertex_index) idx: u32) -> @builtin(position) vec4f {

switch (idx) {
case 0: {
    // Bottom left
    return vec4f(-10, -10, 0, 1);
}
case 1: {
    // Bottom right
    return vec4f(10, -10, 0, 1);
}
default: {
    // Top
    return vec4f(0, 10, 0, 1);
}
}

}
