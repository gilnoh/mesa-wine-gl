#version 450
layout(location=0) out vec4 value;
void main() {
    value = vec4(float(gl_VertexIndex) + 10.0, float(gl_InstanceIndex) + 20.0, 30.0, 1.0);
    gl_Position = vec4(0, 0, 0, 1);
    gl_PointSize = 1.0;
}
