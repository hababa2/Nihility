#include "ShaderIncludes/ShaderTypes.h"

struct PushConstants
{
	float4x4 viewProj;
	float tileSize;
	float camZoom;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct VertexInput
{
	float3 position;
	uint typeData;
};

struct VertexOutput
{
	float4 position : SV_Position;
	float2 worldPos;
	float3 color;
	nointerpolation uint edgeMask;
};

float3 GetCollisionColor(uint type)
{
    switch (type)
    {
    case 1: return float3(0.0, 1.0, 0.0); // Solid (Green)
    case 2: return float3(1.0, 1.0, 0.0); // One-Way (Yellow)
    case 3: return float3(0.0, 0.0, 1.0); // Climbable (Blue)
    case 4: return float3(0.0, 1.0, 1.0); // Fluid (Cyan)
    case 5: return float3(1.0, 1.0, 1.0); // Slippery (White)
    case 6: return float3(1.0, 0.0, 0.0); // Hazard (Red)
    case 7: return float3(1.0, 0.0, 1.0); // Trigger (Magenta)
    default: return float3(1.0, 1.0, 1.0); // Fallback
    }
}

[PipelineState("TriangleList", "None")]
[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    uint edgeFlags = input.typeData & 15;
    uint colType = input.typeData >> 4;

	VertexOutput output;
	output.position = mul(pc.viewProj, float4(input.position, 1.0));
    output.worldPos = input.position.xy;
    output.color = GetCollisionColor(colType);
    output.edgeMask = edgeFlags;

	return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
	float2 tileLocal = input.worldPos - floor(input.worldPos / pc.tileSize) * pc.tileSize;

	float thickness = pc.camZoom * 2;

	bool drawTop = (input.edgeMask & 1) != 0 && (tileLocal.y <= thickness);
	bool drawRight = (input.edgeMask & 2) != 0 && (tileLocal.x >= pc.tileSize - thickness);
	bool drawBottom = (input.edgeMask & 4) != 0 && (tileLocal.y >= pc.tileSize - thickness);
	bool drawLeft = (input.edgeMask & 8) != 0 && (tileLocal.x <= thickness);

	if (drawTop || drawRight || drawBottom || drawLeft)
	{
		return float4(input.color, 0.8);
	}

	return float4(input.color, 0.25);
}