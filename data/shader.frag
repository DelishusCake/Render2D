#version 330 core
#extension GL_ARB_separate_shader_objects : enable

in VS_OUT
{
	vec2 uv;
} fs_in;

uniform sampler2D u_sampler;

out vec4 o_frag;

void main()
{
	o_frag = texture(u_sampler, fs_in.uv);
};