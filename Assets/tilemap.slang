#include "ShaderIncludes/ShaderTypes.h"

[[vk::binding(0, 0)]]
[GlobalTextures]
Sampler2D globalTextures[];

struct PushConstants
{
	float4x4 viewProj;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct VertexInput
{
	float3 position;
	float2 uv;
	float4 color;
	uint texIndex;
};

struct VertexOutput
{
	float4 position : SV_Position;
	float2 uv;
	float4 color;
	nointerpolation uint fragTexIndex;
};

[PipelineState("TriangleList", "None")]
[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
	VertexOutput output;

	output.position = mul(pc.viewProj, float4(input.position, 1.0));
	output.uv = input.uv;
	output.color = input.color;
	output.fragTexIndex = input.texIndex;

	return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return input.color;
	//return globalTextures[NonUniformResourceIndex(input.fragTexIndex)].Sample(input.fragUV);
}