#ifndef SHADER_TYPES
#define SHADER_TYPES

#ifdef __cplusplus
#include "Core/Containers.hpp"
#include <glm/glm.hpp>

typedef glm::mat4 float4x4;
typedef glm::vec4 float4;
typedef glm::vec3 float3;
typedef glm::vec2 float2;
typedef unsigned int uint;
typedef String string;

#else
[__AttributeUsage(_AttributeTargets.Var)]
struct GlobalTexturesAttribute {};

[__AttributeUsage(_AttributeTargets.Function)]
struct PipelineState
{
	string topology;
	string cullMode;
};
#endif

struct SpriteData
{
	float4x4 transform;
	float4 color;
	uint textureIndex;
	uint zIndex;

	uint pad0;
	uint pad1;
};

struct GridPushConstants
{
	float2 camPos;
	float2 camZoom;
	float2 offset;
	float scale;
	float tileSize;
	float chunkMult;
	float padding;
};

#endif