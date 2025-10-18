#include "TilemapComponent.hpp"

#include "Rendering/VulkanInclude.hpp"

#include "Resources.hpp"

#include "Rendering/Renderer.hpp"

DescriptorSet Tilemap::tilemapDescriptor;
Material Tilemap::tilemapMaterial;
Shader Tilemap::tilemapVertexShader;
Shader Tilemap::tilemapFragmentShader;
Buffer Tilemap::tilemapData;
Buffer Tilemap::tilesData;
Vector<Tilemap> Tilemap::components(16, {});
Freelist Tilemap::freeComponents(16);
Vector<TilemapInstance> Tilemap::instanceData;
Vector<TilemapData> Tilemap::tilemapDatas;
U32 Tilemap::nextOffset = 0;
bool Tilemap::initialized = false;

bool Tilemap::Initialize()
{
	if (!initialized)
	{
		initialized = true;

		DescriptorBinding tilemapBinding{
			.type = BindingType::StorageBuffer,
			.stages = (U32)ShaderStage::Fragment,
			.count = 1
		};

		DescriptorBinding tilesBinding{
			.type = BindingType::StorageBuffer,
			.stages = (U32)ShaderStage::Fragment,
			.count = 1
		};

		tilemapDescriptor.Create({ tilemapBinding, tilesBinding }, 0, false);

		PipelineLayout tilemapPipelineLayout;

		tilemapPipelineLayout.Create({ tilemapDescriptor, Resources::BindlessTexturesDescriptorSet() });

		tilemapVertexShader.Create("shaders/tilemap.vert.spv", ShaderStage::Vertex);
		tilemapFragmentShader.Create("shaders/tilemap.frag.spv", ShaderStage::Fragment);

		Vector<VkVertexInputBindingDescription> inputs = {
			{ 0, sizeof(TilemapInstance), VK_VERTEX_INPUT_RATE_INSTANCE },
		};

		Vector<VkVertexInputAttributeDescription> attributes = {
			{ 0, 0, VK_FORMAT_R32_SFLOAT, offsetof(TilemapInstance, depth) },
			{ 1, 0, VK_FORMAT_R32_UINT, offsetof(TilemapInstance, tileOffset) },
		};

		Pipeline tilemapPipeline;
		tilemapPipeline.Create(tilemapPipelineLayout, { PolygonMode::Fill }, { tilemapVertexShader, tilemapFragmentShader }, inputs, attributes);
		tilemapMaterial.Create(tilemapPipelineLayout, tilemapPipeline, { tilemapDescriptor, Resources::BindlessTexturesDescriptorSet() });

		tilemapData.Create(BufferType::Storage, sizeof(TilemapData) * 16);
		Renderer::NameResource(VK_OBJECT_TYPE_BUFFER, tilemapData, "Tilemap Data");
		tilesData.Create(BufferType::Storage, Megabytes(4));
		Renderer::NameResource(VK_OBJECT_TYPE_BUFFER, tilesData, "Tiles Data");

		World::UpdateFns += Update;
		World::RenderFns += Render;
	}

	return false;
}

bool Tilemap::Shutdown()
{
	if (initialized)
	{
		initialized = false;

		for (Tilemap& tilemap : components)
		{
			if (tilemap.entityIndex == U32_MAX) { continue; }
			Memory::Free(&tilemap.tileArray);
		}

		tilemapData.Destroy();
		tilesData.Destroy();

		tilemapVertexShader.Destroy();
		tilemapFragmentShader.Destroy();
		tilemapMaterial.Destroy();
		tilemapDescriptor.Destroy();
	}

	return false;
}

bool Tilemap::Update(Camera& camera, Vector<Entity>& entities)
{
	Vector4Int renderSize = Renderer::RenderSize();

	for (Tilemap& tilemap : components)
	{
		if (tilemap.entityIndex == U32_MAX) { continue; }

		TilemapData& tmd = tilemapDatas[tilemap.instance];

		Vector4Int area = Renderer::RenderSize();

		tmd.eye = camera.Eye().xy() * (renderSize.z / 132.0f) * tilemap.parallax;
		tmd.offset = (Vector2{ tilemap.offset.x, -tilemap.offset.y } + ScreenOffset) * (renderSize.z / 64.0f);
		tmd.tileSize = tilemap.tileSize * (renderSize.z / 64.0f);
	}

	if (instanceData.Size())
	{
		tilemapMaterial.UploadInstances(instanceData.Data(), (U32)(instanceData.Size() * sizeof(TilemapInstance)), 0);
		tilemapData.UploadUniformData(tilemapDatas.Data(), (U32)(tilemapDatas.Size() * sizeof(TilemapData)), 0);
	}

	return false;
}

bool Tilemap::Render(CommandBuffer commandBuffer)
{
	VkDescriptorBufferInfo bufferInfo0 = {
		.buffer = tilemapData,
		.offset = 0,
		.range = VK_WHOLE_SIZE
	};

	VkDescriptorBufferInfo bufferInfo1 = {
		.buffer = tilesData,
		.offset = 0,
		.range = VK_WHOLE_SIZE
	};

	VkWriteDescriptorSet writes[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = tilemapDescriptor,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &bufferInfo0,
			.pTexelBufferView = nullptr
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = tilemapDescriptor,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &bufferInfo1,
			.pTexelBufferView = nullptr
		}
	};

	vkUpdateDescriptorSets(Renderer::GetDevice(), CountOf32(writes), writes, 0, nullptr);

	tilemapMaterial.Bind(commandBuffer);

	return false;
}

ComponentRef<Tilemap> Tilemap::AddTo(const EntityRef& entity, U32 width, U32 height, const Vector2& offset, const Vector2& parallax, F32 depth, const Vector2& tileSize)
{
	Vector4Int renderSize = Renderer::RenderSize();

	U32 instanceId;
	Tilemap& tilemap = Create(instanceId);
	tilemap.entityIndex = entity.EntityId();
	tilemap.parallax = parallax;
	tilemap.instance = (U32)tilemapDatas.Size();
	tilemap.tileSize = tileSize;
	tilemap.offset = offset;

	TilemapData& tmd = tilemapDatas.Push({});

	tmd.width = width;
	tmd.height = height;
	tmd.offset = offset * (renderSize.z / 64.0f);
	
	instanceData.Push({ depth, nextOffset });

	Memory::Allocate(&tilemap.tileArray, tmd.width * tmd.height);

	U16* tiles;
	Memory::Allocate(&tiles, tmd.width * tmd.height);

	for (U32 i = 0; i < tmd.width * tmd.height; ++i)
	{
		tiles[i] = U16_MAX;
	}

	tilemapData.UploadUniformData(tiles, tmd.width * tmd.height * sizeof(U16), nextOffset * sizeof(U16));

	nextOffset += tmd.width * tmd.height;

	Memory::Free(&tiles);

	return { entity.EntityId(), instanceId };
}

void Tilemap::SetTile(const ResourceRef<Texture>& texture, const Vector2Int& position, TileType type)
{
	TilemapData& tmd = tilemapDatas[instance];
	TilemapInstance& tmi = instanceData[instance];

	if ((U32)position.x < tmd.width && (U32)position.y < tmd.height && tileArray[position.x + position.y * tmd.width] != type)
	{
		dirty = true;
		U16 handle = texture.Handle();

		U32 i = (tmi.tileOffset + position.x + position.y * tmd.width);

		tilesData.UploadUniformData(&handle, sizeof(U16), (tmi.tileOffset + position.x + position.y * tmd.width) * sizeof(U16));
		tileArray[position.x + position.y * tmd.width] = type;
	}
}

Vector2Int Tilemap::ScreenToTilemap(const Camera& camera, const Vector2& position)
{
	TilemapData& tmd = tilemapDatas[instance];

	Vector2 pos = Vector2{ position.x + tmd.eye.x - tmd.offset.x, position.y - tmd.eye.y - tmd.offset.y };
		
	return Vector2Int{ (I32)Math::Floor(pos.x / tmd.tileSize.x), (I32)Math::Floor(pos.y / tmd.tileSize.y) };
}

Vector2Int Tilemap::GetDimensions() const
{
	TilemapData& tmd = tilemapDatas[instance];

	return { (I32)tmd.width, (I32)tmd.height };
}

const Vector2& Tilemap::GetOffset() const
{
	return offset;
}

const Vector2& Tilemap::GetTileSize() const
{
	return tileSize;
}

const TileType* Tilemap::GetTiles() const
{
	return tileArray;
}

const TilemapData& Tilemap::GetData() const
{
	return tilemapDatas[instance];
}

bool Tilemap::GetDirty() const
{
	return dirty;
}

void Tilemap::Clean()
{
	dirty = false;
}