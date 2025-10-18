#version 450

layout (location = 0) out vec2 outTexcoord;

void main()
{
	gl_Position = vec4(vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 + -1.0, 0, 1.0);
	outTexcoord = 0.5 * gl_Position.xy + vec2(0.5);
}