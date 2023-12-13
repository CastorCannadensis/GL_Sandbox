#version 430 core

layout(binding = 0) uniform usampler2DArray sheet;

in vec2 tCoord;
flat in uint id;

out uvec4 color;

void main(void)
{
	vec3 idx = vec3(tCoord, id);
	uvec4 c = texture(sheet, idx);
	

	color = c;
}