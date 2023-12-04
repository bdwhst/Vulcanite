#version 450

layout(triangles, equal_spacing, ccw) in;

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

layout (location = 0) in vec3 inPos[];

layout (location = 0) out flat uint outID;

void main()
{
	uvec3 currID = inID[gl_PrimitiveID];
	outID = 0;
	outID |= ((currID.y&0xFFFFFF)<<8);
	outID |= (currID.z&0xFF);
    if(gl_TessCoord.x>0.99)
	{
		gl_Position = ubo.projection*ubo.view*inModelMats[currID.x]*vec4(inPos[0],1);
	}
	else if(gl_TessCoord.y>0.99)
	{
		gl_Position = ubo.projection*ubo.view*inModelMats[currID.x]*vec4(inPos[1],1);
	}
	else
	{
		gl_Position = ubo.projection*ubo.view*inModelMats[currID.x]*vec4(inPos[2],1);
	}
}