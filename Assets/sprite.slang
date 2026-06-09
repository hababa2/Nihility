#include "ShaderIncludes/ShaderTypes.h"

[[vk::binding(0, 0)]]
[GlobalTextures]
Sampler2D globalTextures[];

[[vk::binding(0, 1)]] StructuredBuffer<SpriteData> spriteBuffer;

struct VertexInput
{
	uint vertexID : SV_VertexID;
	uint instanceID : SV_InstanceID;
};

struct VertexOutput
{
	float4 position : SV_Position;
	float2 fragUV;
	float4 fragColor;
	nointerpolation uint fragTexIndex;
};

static const float2 quadVertices[4] = {
	float2(-0.5, -0.5), float2(0.5, -0.5),
	float2(-0.5, 0.5), float2(0.5, 0.5)
};

static const float2 quadUVs[4] = {
    float2(0.0, 0.0), float2(1.0, 0.0),
    float2(0.0, 1.0), float2(1.0, 1.0)
};

[PipelineState("TriangleStrip", "None")]
[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
	VertexOutput output;

	SpriteData data = spriteBuffer[input.instanceID];

	output.position = mul(data.transform, float4(quadVertices[input.vertexID], 0.0, 1.0));
	output.fragUV = quadUVs[input.vertexID];
	output.fragColor = data.color;
	output.fragTexIndex = data.textureIndex;

	return output;
}


[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
	float4 texColor = globalTextures[NonUniformResourceIndex(input.fragTexIndex)].Sample(input.fragUV);

	return input.fragColor;
}