#version 450 core

layout(vertices = 3) out;

layout (location = 0) in vec3 inPos[];
layout (location = 0) out vec3 outPos[];

void main() {
	outPos[gl_InvocationID] = inPos[gl_InvocationID];
    if (gl_InvocationID == 0) {
        gl_TessLevelInner[0] = 1.0;
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = 1.0;
        gl_TessLevelOuter[2] = 1.0;
    }
}
