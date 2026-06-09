#include "ShaderIncludes/ShaderTypes.h"

[[vk::binding(0, 0)]]
[GlobalTextures]
Sampler2D globalTextures[];

struct VertexInput
{
	float2 position;
	float2 uv;
	float4 color;
	float boldness;
	uint texIndex;
};

struct VertexOutput
{
	float4 position : SV_Position;
	float2 fragUV;
	float4 fragColor;
	float fragBoldness;
	nointerpolation uint fragTexIndex;
};

[PipelineState("TriangleList", "None")]
[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
	VertexOutput output;

	output.position = float4(input.position, 0.0, 1.0);
	output.fragUV = input.uv;
	output.fragColor = input.color;
	output.fragBoldness = input.boldness;
	output.fragTexIndex = input.texIndex;

	return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
	float3 msdf = globalTextures[NonUniformResourceIndex(input.fragTexIndex)].Sample(input.fragUV).rgb;

	float sd = max(min(msdf.r, msdf.g), min(max(msdf.r, msdf.g), msdf.b));

	if (sd <= 0.001) { discard; }

	float distance = sd - input.fragBoldness;
	float edgeWidth = fwidth(distance);
	float alpha = smoothstep(-edgeWidth, edgeWidth, distance) * input.fragColor.a;

	if (alpha <= 0.0) { discard; }

	return float4(input.fragColor.rgb, alpha);
}