#version 450
layout(location=0, xfb_buffer=0, xfb_offset=0, xfb_stride=16) out vec4 captured;
void main() {
    const vec2 positions[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3));
    gl_Position = vec4(positions[gl_VertexIndex % 3], 0, 1);
    captured = vec4(float(gl_VertexIndex) + 10, float(gl_InstanceIndex) + 20, 30, 1);
}
