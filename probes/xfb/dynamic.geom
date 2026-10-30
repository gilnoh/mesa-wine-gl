#version 450
layout(points) in;
layout(points, max_vertices=2) out;
layout(location=0) in vec4 value[];
layout(location=0, xfb_buffer=0, xfb_offset=0, xfb_stride=16) out vec4 captured;
void main() {
    int count = (gl_PrimitiveIDIn % 2) + 1;
    for (int i=0; i<count; ++i) {
        captured = value[0] + vec4(float(i)*100.0, 0, 0, 0);
        gl_Position = gl_in[0].gl_Position;
        gl_PointSize = 1.0;
        EmitVertex();
        EndPrimitive();
    }
}
