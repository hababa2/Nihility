#include "ShaderIncludes/ShaderTypes.h"

[[vk::push_constant]] ConstantBuffer<GridPushConstants> pc;

struct VertexOutput
{
	float4 position : SV_Position;
};

[PipelineState("TriangleList", "None")]
[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID)
{
	float2 positions[6] = {
		float2(-1, -1), float2(1, -1), float2(1, 1),
		float2(1, 1), float2(-1, 1), float2(-1, -1)
	};

	VertexOutput output;
	output.position = float4(positions[vertexID], 0.0, 1.0);
	return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float2 pixelCoord = input.position.xy - pc.offset;
    float2 worldPos = (pixelCoord * pc.camZoom * pc.scale) + pc.camPos;

    float2 halfThickness = pc.camZoom * pc.scale * 0.5;

    float chunkSize = pc.tileSize * pc.chunkMult;

    float2 chunkLocal = fmod(abs(worldPos), chunkSize);
    float2 chunkDist = min(chunkLocal, chunkSize - chunkLocal);

    float2 tileLocal = fmod(abs(worldPos), pc.tileSize);
    float2 tileDist = min(tileLocal, pc.tileSize - tileLocal);

    if (chunkDist.x <= halfThickness.x || chunkDist.y <= halfThickness.y)
    {
        return float4(1.0, 0.8, 0.0, 0.6);
    }

    if (tileDist.x <= halfThickness.x || tileDist.y <= halfThickness.y)
    {
        return float4(1.0, 1.0, 1.0, 0.15);
    }

    discard;
    return float4(0.0, 0.0, 0.0, 0.0);
}