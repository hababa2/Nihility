#include "ShaderIncludes/ShaderTypes.h"

[[vk::binding(0, 0)]]
[GlobalTextures]
Sampler2D globalTextures[];

[[vk::binding(0, 1)]] StructuredBuffer<SpriteData> spriteBuffer;

struct VertexInput
{
    float2 position;
    float2 uv;
    float4 color;
    uint texIndex;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 fragUV;
    float4 fragColor;
    nointerpolation uint fragTexIndex;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;

    output.position = float4(input.position, 0.0, 1.0);

    output.fragUV = input.uv;
    output.fragColor = input.color;
    output.fragTexIndex = input.texIndex;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    if (input.fragTexIndex == 0xFFFFFFFF)
    {
        return input.fragColor;
    }
    else
    {
        float4 texColor = globalTextures[NonUniformResourceIndex(input.fragTexIndex)].Sample(input.fragUV);
        return texColor;
    }
}