#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inClusterInfos;
layout(location = 5) in vec4 inClusterGroupInfos;


layout (location = 0) out vec3 outPos;

void main()
{
    outPos = inPos;
}