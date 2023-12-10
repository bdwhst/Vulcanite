#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec3 inPos[3];

layout (location = 0) out flat uint outID;

layout(set = 0, binding = 0) buffer readonly ModelMatIn{
	mat4 inModelMats[];
};

layout(set = 0, binding = 1) buffer readonly ObjectIdIn{
	uvec3 inID[ ]; //objectID,clusterID,triangleID
};

layout (set = 0, binding = 2) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
	vec3 camPos;
} ubo;


void main()
{
    uvec3 currID = inID[gl_PrimitiveIDIn];
    mat4 model = inModelMats[currID.x];
    uint packedID = 0;
	packedID |= ((currID.y&0x7FFF)<<17);//15 bits of clusterID
    packedID |= ((currID.x&0x7FF)<<6);//11 bits of objectID
	packedID |= (currID.z&0x3F);//6 bits of triangleID
    for(uint i = 0; i < 3; i++){
        outID = packedID;
        gl_Position = ubo.projection * ubo.view * model * vec4(inPos[i], 1.0);
        EmitVertex();
    }

    EndPrimitive();
}