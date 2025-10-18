#include "Lighting.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"
#include "Resources/Resources.hpp"

LightingPushConstant Lighting::pushConstant;

Material Lighting::cascadeRenderMaterial;
Shader Lighting::cascadeRenderVertexShader;
Shader Lighting::cascadeRenderFragmentShader;

Material Lighting::cascadeMergeMaterial;
Shader Lighting::cascadeMergeVertexShader;
Shader Lighting::cascadeMergeFragmentShader;

bool Lighting::Initialize()
{
	static constexpr U32 Cascade0ProbeSpacing = 2;
	static constexpr Vector2Int Cascade0AngularResolution = { 4, 8 };

	Vector4Int area = Renderer::RenderSize();

	pushConstant.worldTextureDimensions = (Vector2)area.zw();
	pushConstant.cascadeTextureDimensions = { pushConstant.cascade0ProbeResolution.x * pushConstant.cascade0AnglarResolution.x * 2.0f, (F32)pushConstant.cascade0ProbeResolution.y * pushConstant.cascade0AnglarResolution.y };
	pushConstant.cascade0AnglarResolution = Cascade0AngularResolution;
	pushConstant.cascade0ProbeResolution = area.zw() / Cascade0ProbeSpacing;
	pushConstant.cascadeTextureIndex;

	VkPushConstantRange pushConstant{};
	pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstant.offset = 0;
	pushConstant.size = sizeof(LightingPushConstant);

	PipelineLayout cascadeRenderPipelineLayout;

	cascadeRenderPipelineLayout.Create({ Resources::DummyDescriptorSet(), Resources::BindlessTexturesDescriptorSet() }, { pushConstant });

	cascadeRenderVertexShader.Create("shaders/cascade_render.vert.spv", ShaderStage::Vertex);
	cascadeRenderFragmentShader.Create("shaders/cascade_render.frag.spv", ShaderStage::Fragment);

	Pipeline cascadeRenderPipeline;
	cascadeRenderPipeline.Create(cascadeRenderPipelineLayout, { PolygonMode::Fill }, { cascadeRenderVertexShader, cascadeRenderFragmentShader }, {}, {});
	cascadeRenderMaterial.Create(cascadeRenderPipelineLayout, cascadeRenderPipeline, { Resources::DummyDescriptorSet(), Resources::BindlessTexturesDescriptorSet() },
		{ PushConstant{ &pushConstant, sizeof(LightingPushConstant), 0, VK_SHADER_STAGE_FRAGMENT_BIT } });

	return true;
}

void Lighting::Shutdown()
{
	cascadeRenderVertexShader.Destroy();
	cascadeRenderFragmentShader.Destroy();
	cascadeRenderMaterial.Destroy();
}

void Lighting::Render(CommandBuffer commandBuffer)
{
	cascadeRenderMaterial.Bind(commandBuffer);
}