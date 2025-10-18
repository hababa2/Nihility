#include "SpriteComponent.hpp"

#include "Rendering/VulkanInclude.hpp"

#include "Resources.hpp"

#include "Rendering/Renderer.hpp"

Material Sprite::spriteMaterial;
Shader Sprite::spriteVertexShader;
Shader Sprite::spriteFragmentShader;
Vector<SpriteInstance> Sprite::spriteInstances(10000);
Vector<Sprite> Sprite::components(10000, {});
Freelist Sprite::freeComponents(10000);
bool Sprite::initialized = false;

bool Sprite::Initialize()
{
	if (!initialized)
	{
		initialized = true;

		VkPushConstantRange pushConstant{};
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(GlobalPushConstant);

		PipelineLayout spritePipelineLayout;

		spritePipelineLayout.Create({ Resources::DummyDescriptorSet(), Resources::BindlessTexturesDescriptorSet() }, { pushConstant });

		spriteVertexShader.Create("shaders/sprite.vert.spv", ShaderStage::Vertex);
		spriteFragmentShader.Create("shaders/sprite.frag.spv", ShaderStage::Fragment);

		Vector<VkVertexInputBindingDescription> inputs = {
			{ 0, sizeof(SpriteVertex), VK_VERTEX_INPUT_RATE_VERTEX },
			{ 1, sizeof(SpriteInstance), VK_VERTEX_INPUT_RATE_INSTANCE}
		};

		Vector<VkVertexInputAttributeDescription> attributes = {
			{ 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteVertex, position) },
			{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteVertex, texcoord) },

			{ 2, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteInstance, position) },
			{ 3, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteInstance, scale) },
			{ 4, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteInstance, rotation) },
			{ 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SpriteInstance, instColor) },
			{ 6, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteInstance, instTexcoord) },
			{ 7, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(SpriteInstance, instTexcoordScale) },
			{ 8, 1, VK_FORMAT_R32_UINT, offsetof(SpriteInstance, textureIndex) },
			{ 9, 1, VK_FORMAT_R32_UINT, offsetof(SpriteInstance, spriteIndex) },
		};

		Pipeline spritePipeline;
		spritePipeline.Create(spritePipelineLayout, { PolygonMode::Fill }, { spriteVertexShader, spriteFragmentShader }, inputs, attributes);
		spriteMaterial.Create(spritePipelineLayout, spritePipeline, { Resources::DummyDescriptorSet(), Resources::BindlessTexturesDescriptorSet() },
			{ PushConstant{ Renderer::GetGlobalPushConstant(), sizeof(GlobalPushConstant), 0, VK_SHADER_STAGE_VERTEX_BIT } });

		SpriteVertex vertices[4] = {
			{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
			{ { -1.0f,  1.0f }, { 0.0f, 0.0f } },
			{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },
			{ {  1.0f, -1.0f }, { 1.0f, 1.0f } }
		};

		U32 indices[6] = { 0, 1, 2, 2, 3, 0 };

		spriteMaterial.UploadVertices(vertices, sizeof(SpriteVertex) * 4, 0);
		spriteMaterial.UploadIndices(indices, sizeof(U32) * 6, 0);

		World::UpdateFns += Update;
		World::RenderFns += Render;
	}

	return false;
}

bool Sprite::Shutdown()
{
	if (initialized)
	{
		initialized = false;
		spriteVertexShader.Destroy();
		spriteFragmentShader.Destroy();
		spriteMaterial.Destroy();
	}

	return false;
}

bool Sprite::Update(Camera& camera, Vector<Entity>& entities)
{
	int i = 0;
	for (Sprite& sprite : components)
	{
		if (sprite.entityIndex == U32_MAX) { continue; }
		const Entity& entity = entities[sprite.entityIndex];
		SpriteInstance& instance = spriteInstances[sprite.instanceIndex];

		instance.position = entity.position;
		instance.scale = entity.scale;
		instance.rotation = entity.rotation;
		++i;
	}

	spriteMaterial.ClearInstances();

	if (spriteInstances.Size())
	{
		spriteMaterial.UploadInstances(spriteInstances.Data(), (U32)(spriteInstances.Size() * sizeof(SpriteInstance)), 0);
	}

	return false;
}

bool Sprite::Render(CommandBuffer commandBuffer)
{
	if (freeComponents.Size()) { spriteMaterial.Bind(commandBuffer); }

	return false;
}

ComponentRef<Sprite> Sprite::AddTo(const EntityRef& entity, const ResourceRef<Texture>& texture, const Vector4& color, const Vector2& textureCoord, const Vector2& textureScale)
{
	if (freeComponents.Full()) { Logger::Error("Max Sprite Instances Reached!"); return nullptr; }

	U32 instanceId;
	Sprite& sprite = Create(instanceId);
	sprite.entityIndex = entity.EntityId();
	sprite.instanceIndex = instanceId;
	
	SpriteInstance& instance = instanceId == spriteInstances.Size() ? spriteInstances.Push({}) : spriteInstances[instanceId];

	instance.instColor = color;
	instance.instTexcoord = textureCoord;
	instance.instTexcoordScale = textureScale;
	instance.textureIndex = texture.Handle();
	instance.spriteIndex = instanceId;

	return { entity.EntityId(), instanceId };
}

void Sprite::RemoveFrom(const EntityRef& entity)
{
	 ComponentRef<Sprite> sprite = GetRef(entity);
	 if (sprite)
	 {
		 spriteInstances[sprite->instanceIndex].textureIndex = U16_MAX;
		 spriteInstances[sprite->instanceIndex].scale = Vector2::Zero;

		 Destroy(*sprite);
	 }
}

void Sprite::SetColor(const Vector4& color)
{
	spriteInstances[instanceIndex].instColor = color;
}

void Sprite::SetTexture(const ResourceRef<Texture>& texture, const Vector2& textureCoord, const Vector2& textureScale)
{
	spriteInstances[instanceIndex].textureIndex = texture.Handle();
	spriteInstances[instanceIndex].instTexcoord = textureCoord;
	spriteInstances[instanceIndex].instTexcoordScale = textureScale;
}