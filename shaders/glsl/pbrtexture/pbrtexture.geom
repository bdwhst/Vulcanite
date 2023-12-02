#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec3 inWorldPos[3];
layout (location = 1) in vec3 inNormal[3];
layout (location = 2) in vec2 inUV[3];
layout (location = 3) in vec4 inTangent[3];
layout (location = 4) in vec4 inClusterInfos[3];
layout(location = 5) in vec4 inClusterGroupInfos[3];
layout (location = 6) in flat uint inObjectId[3];

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 outTangent;
layout (location = 4) out vec4 outClusterInfos;
layout(location = 5) out vec4 outClusterGroupInfos;
layout (location = 6) out flat uint outObjectId;

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

void main() {
    // Emit the primitive
    for(uint i = 0; i < 3; i++){
        outObjectId = objectIds[gl_PrimitiveIDIn * 3 + i];
        outWorldPos = (inModelMats[outObjectId] * vec4(inWorldPos[i], 1.0)).xyz;
        outNormal = mat3(inModelMats[outObjectId]) * inNormal[i];
	    outTangent = vec4(mat3(inModelMats[outObjectId]) * inTangent[i].xyz, inTangent[i].w);
        outUV = inUV[i];
        outClusterInfos = inClusterInfos[i];
        outClusterGroupInfos = inClusterGroupInfos[i];

        gl_Position = ubo.projection * ubo.view * vec4(outWorldPos, 1.0);
        EmitVertex();
    }

    EndPrimitive();
}
