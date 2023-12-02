#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inClusterInfos;
layout(location = 5) in vec4 inClusterGroupInfos;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
	//mat4 normal;
	vec3 camPos;
} ubo;

layout(binding = 10) buffer readonly ObjectIdIn{
	uint objectIds[];
};

layout(binding = 11) buffer readonly ModelMatIn{
	mat4 inModelMats[];
};

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 outTangent;
layout (location = 4) out vec4 outClusterInfos;
layout(location = 5) out vec4 outClusterGroupInfos;
layout (location = 6) out flat uint outObjectId;

void main() 
{
	outWorldPos = inPos;
	outNormal = inNormal;
	outTangent = inTangent;
	outUV = inUV;
	outClusterInfos = inClusterInfos;
	outClusterGroupInfos = inClusterGroupInfos;
	//gl_Position =  ubo.projection * ubo.view * vec4(outWorldPos, 1.0);
}
