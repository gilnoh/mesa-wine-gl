#version 450
layout(points) in;
layout(points, max_vertices=1) out;
layout(location=0) in vec4 value[];
layout(location=0, xfb_buffer=0, xfb_offset=0, xfb_stride=16) out vec4 captured;
void main() {
    captured = value[0];
    gl_Position = gl_in[0].gl_Position;
    gl_PointSize = 1.0;
    EmitVertex();
    EndPrimitive();
}
