#version 450
layout(location=0, xfb_buffer=0, xfb_offset=0, xfb_stride=16) out vec4 captured;
void main() {
    captured = vec4(float(gl_VertexIndex) + 10, float(gl_InstanceIndex) + 20, 30, 1);
    gl_Position = vec4(0, 0, 0, 1);
    gl_PointSize = 1.0;
}
