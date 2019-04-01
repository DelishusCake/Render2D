#version 330 core
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec2 i_pos;
layout(location=1) in vec2 i_uv;

out VS_OUT
{
	vec2 uv;
} vs_out;

uniform mat4 u_projection;

void main()
{
	vs_out.uv = i_uv;
	gl_Position = u_projection * vec4(i_pos, 0.f, 1.f);
};