#version 450

layout (location = 0) in flat uint outID;
layout (location = 0) out uvec4 color;


void main()
{
    color.x=outID;
}