#pragma once

#include "Defines.hpp"

#include "Resources/Material.hpp"
#include "Math/Math.hpp"

struct LightingPushConstant
{
	Vector2 worldTextureDimensions;
	Vector2 cascadeTextureDimensions;
	Vector2Int cascade0AnglarResolution;
	Vector2Int cascade0ProbeResolution;
	U32 cascadeTextureIndex;
};

class NH_API Lighting
{
public:


private:
	static bool Initialize();
	static void Shutdown();
	static void Render(CommandBuffer commandBuffer);

	static LightingPushConstant pushConstant;

	static Material cascadeRenderMaterial;
	static Shader cascadeRenderVertexShader;
	static Shader cascadeRenderFragmentShader;

	static Material cascadeMergeMaterial;
	static Shader cascadeMergeVertexShader;
	static Shader cascadeMergeFragmentShader;

	STATIC_CLASS(Lighting);
	friend class Engine;
};