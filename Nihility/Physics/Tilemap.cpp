#include "Tilemap.hpp"

#include "Physics.hpp"

#include "Core/Time.hpp"
#include "Rendering/VulkanInclude.hpp"
#include "Rendering/Renderer.hpp"

#include "vma/vk_mem_alloc.h"

TilemapChunk::TilemapChunk()
{
	for (U32 i = 0; i < (U32)TileLayer::Count; ++i)
	{
		layers[i].resize(Tilemap::ChunkSize * Tilemap::ChunkSize);
	}
}

Hashmap<U64, Entity> Tilemap::chunkMap;

bool Tilemap::Initialize()
{
	Logger::Trace("Initializing Tilemap...");

	Registry::RegisterComponentUpdate("Tilemap", Update);

	return true;
}

void Tilemap::Shutdown()
{
	Logger::Trace("Shutting Down Tilemap...");

	auto view = Registry::View<TilemapRenderData>();

	for (U32 i = 0; i < view.Size(); ++i)
	{
		U32 id = view.GetEntity(i);
		auto [renderData] = view.Get(id);

		Renderer::DestroyBuffer(renderData.vertexBuffer);
		Renderer::DestroyBuffer(renderData.indexBuffer);
	}
}

void Tilemap::Update()
{
	auto view = Registry::View<TilemapChunk>();
	if (view.Size() == 0) { return; }

	static Vector<TileVertex> vertices;
	static Vector<U32> indices;

	for (U32 i = 0; i < view.Size(); ++i)
	{
		U32 entityId = view.GetEntity(i);
		if (!view.Matches(entityId)) { continue; }

		auto [chunk] = view.Get(entityId);

		if (!chunk.isDirty) { continue; }

		vertices.clear();
		indices.clear();
		U32 vertexOffset = 0;

		glm::vec2 chunkOrigin = {
			(F32)(chunk.gridPosition.x * ChunkSize) * TileSize,
			(F32)(chunk.gridPosition.y * ChunkSize) * TileSize
		};

		for (U32 layerIdx = 0; layerIdx < (U32)TileLayer::Count; ++layerIdx)
		{
#ifndef NH_DEBUG
			if ((TileLayer)layerIdx == TileLayer::Logic) { continue; }
#endif
			const auto& layerData = chunk.layers[layerIdx];

			F32 zDepth = -0.5f + (layerIdx * 0.25f);

			for (U32 y = 0; y < ChunkSize; ++y)
			{
				for (U32 x = 0; x < ChunkSize; ++x)
				{
					const Tile& tile = layerData[y * ChunkSize + x];

					if (tile.textureId == U32_MAX) { continue; }

					glm::vec3 pos = {
						chunkOrigin.x + (x * TileSize),
						chunkOrigin.y + (y * TileSize),
						zDepth
					};

					glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

					vertices.push_back({ { pos.x, pos.y, pos.z }, { 0.0f, 0.0f }, color, tile.textureId });
					vertices.push_back({ { pos.x + TileSize, pos.y, pos.z }, { 1.0f, 0.0f }, color, tile.textureId });
					vertices.push_back({ { pos.x + TileSize, pos.y + TileSize, pos.z }, { 1.0f, 1.0f }, color, tile.textureId });
					vertices.push_back({ { pos.x, pos.y + TileSize, pos.z }, { 0.0f, 1.0f }, color, tile.textureId });

					indices.push_back(vertexOffset + 0);
					indices.push_back(vertexOffset + 1);
					indices.push_back(vertexOffset + 2);
					indices.push_back(vertexOffset + 2);
					indices.push_back(vertexOffset + 3);
					indices.push_back(vertexOffset + 0);

					vertexOffset += 4;
				}
			}
		}

		EnsureRenderDataExists(entityId, (U32)vertices.size(), (U32)indices.size());

		TilemapRenderData& renderData = Registry::GetComponent<TilemapRenderData>(entityId);
		renderData.indexCount = (U32)indices.size();
		renderData.vertexCount = (U32)vertices.size();

		if (renderData.indexCount > 0)
		{
			renderData.vertexBuffer.Write(vertices.data(), vertices.size() * sizeof(TileVertex));
			renderData.indexBuffer.Write(indices.data(), indices.size() * sizeof(U32));
		}

		chunk.isDirty = false;
	}
}

void Tilemap::EnsureRenderDataExists(U32 entityId, U32 requiredVertices, U32 requiredIndices)
{
	if (!Registry::HasComponent<TilemapRenderData>(entityId))
	{
		Registry::AddComponent<TilemapRenderData>(entityId);
	}

	TilemapRenderData& renderData = Registry::GetComponent<TilemapRenderData>(entityId);

	if (requiredVertices == 0)
	{
		if (renderData.isInitialized)
		{
			Renderer::DestroyBuffer(renderData.vertexBuffer);
			Renderer::DestroyBuffer(renderData.indexBuffer);
			renderData.isInitialized = false;
		}
		return;
	}

	if (!renderData.isInitialized || requiredVertices > renderData.maxVertexCount)
	{
		if (renderData.isInitialized)
		{
			Renderer::DestroyBuffer(renderData.vertexBuffer);
			Renderer::DestroyBuffer(renderData.indexBuffer);
		}

		U32 paddedVertices = requiredVertices + 128;
		U32 paddedIndices = requiredIndices + 192;

		renderData.maxVertexCount = paddedVertices;

		renderData.vertexBuffer = Renderer::CreateBuffer(
			sizeof(TileVertex) * paddedVertices,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		renderData.indexBuffer = Renderer::CreateBuffer(
			sizeof(U32) * paddedIndices,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		renderData.isInitialized = true;
	}
}

U32 Tilemap::GetTileCollision(I32 x, I32 y)
{
	I32 chunkX = (I32)std::floor((F32)x / (F32)ChunkSize);
	I32 chunkY = (I32)std::floor((F32)y / (F32)ChunkSize);

	U64 key = ((U64)(U32)chunkX << 32) | (U32)chunkY;
	auto it = chunkMap.find(key);

	if (it == chunkMap.end()) { return CollisionFlags::None; }

	TilemapChunk& chunkData = Registry::GetComponent<TilemapChunk>(it->second.Id());

	U32 localX = (U32)(((x % ChunkSize) + ChunkSize) % ChunkSize);
	U32 localY = (U32)(((y % ChunkSize) + ChunkSize) % ChunkSize);

	return chunkData.layers[(U32)TileLayer::Collision][localY * ChunkSize + localX].collisionFlags;
}

Entity Tilemap::GetOrCreateChunk(I32 chunkX, I32 chunkY)
{
	U64 key = ((U64)(U32)chunkX << 32) | (U32)chunkY;

	auto it = chunkMap.find(key);
	if (it != chunkMap.end())
	{
		return it->second;
	}

	Entity chunkEntity = Registry::CreateEntity();
	TilemapChunk& chunkData = chunkEntity.AddComponent<TilemapChunk>();

	chunkData.gridPosition = { chunkX, chunkY };
	chunkData.isDirty = true;

	chunkMap[key] = chunkEntity;
	return chunkEntity;
}

bool Tilemap::SweepX(Transform2D& transform, const ColliderAABB& aabb, glm::vec2& velocity)
{
	F32 amount = velocity.x * (F32)Time::DeltaTime();
	if (amount == 0.0f) { return false; }

	bool movingRight = amount > 0.0f;
	F32 newX = transform.position.x + amount;

	static constexpr F32 Inset = 0.01f;
	F32 topY = transform.position.y + aabb.offset.y + aabb.halfExtents.y - Inset;
	F32 bottomY = transform.position.y + aabb.offset.y - aabb.halfExtents.y + Inset;

	I32 startTileY = (I32)std::floor(bottomY / TileSize);
	I32 endTileY = (I32)std::floor(topY / TileSize);

	F32 currentEdgeX = movingRight ?
		(transform.position.x + aabb.offset.x + aabb.halfExtents.x) :
		(transform.position.x + aabb.offset.x - aabb.halfExtents.x);

	F32 leadingEdgeX = currentEdgeX + amount;

	I32 startTileX = (I32)std::floor(currentEdgeX / TileSize);
	I32 endTileX = (I32)std::floor(leadingEdgeX / TileSize);

	I32 stepX = movingRight ? 1 : -1;

	for (I32 x = startTileX; x != endTileX + stepX; x += stepX)
	{
		for (I32 y = startTileY; y <= endTileY; ++y)
		{
			U32 flags = GetTileCollision(x, y);

			if (flags & CollisionFlags::Solid)
			{
				if (movingRight) { transform.position.x = (x * TileSize) - aabb.halfExtents.x - aabb.offset.x; }
				else { transform.position.x = ((x + 1) * TileSize) + aabb.halfExtents.x - aabb.offset.x; }

				velocity.x = 0.0f;
				return true;
			}
		}
	}

	transform.position.x = newX;
	return false;
}

bool Tilemap::SweepY(Transform2D& transform, const ColliderAABB& aabb, glm::vec2& velocity)
{
	F32 amount = velocity.y * (F32)Time::DeltaTime();
	if (amount == 0.0f) { return false; }

	bool movingDown = amount > 0.0f;
	F32 newY = transform.position.y + amount;

	static constexpr F32 Inset = 0.01f;
	F32 rightX = transform.position.x + aabb.offset.x + aabb.halfExtents.x - Inset;
	F32 leftX = transform.position.x + aabb.offset.x - aabb.halfExtents.x + Inset;

	I32 startTileX = (I32)std::floor(leftX / TileSize);
	I32 endTileX = (I32)std::floor(rightX / TileSize);

	F32 currentEdgeY = movingDown ?
		(transform.position.y + aabb.offset.y + aabb.halfExtents.y) :
		(transform.position.y + aabb.offset.y - aabb.halfExtents.y);

	F32 leadingEdgeY = currentEdgeY + amount;

	I32 startTileY = (I32)std::floor(currentEdgeY / TileSize);
	I32 endTileY = (I32)std::floor(leadingEdgeY / TileSize);

	I32 stepY = movingDown ? 1 : -1;

	for (I32 y = startTileY; y != endTileY + stepY; y += stepY)
	{
		for (I32 x = startTileX; x <= endTileX; ++x)
		{
			U32 flags = GetTileCollision(x, y);

			if (flags & CollisionFlags::Solid)
			{
				if (!movingDown)
				{
					static constexpr F32 CornerTolerance = 4.0f;

					F32 tileLeft = x * TileSize;
					F32 tileRight = tileLeft + TileSize;

					if (leftX < tileRight && leftX > tileRight - CornerTolerance && !(GetTileCollision(x + 1, y) & CollisionFlags::Solid))
					{
						transform.position.x = tileRight + aabb.halfExtents.x + 0.01f;
						continue;
					}

					if (rightX > tileLeft && rightX < tileLeft + CornerTolerance && !(GetTileCollision(x - 1, y) & CollisionFlags::Solid))
					{
						transform.position.x = tileLeft - aabb.halfExtents.x - 0.01f;
						continue;
					}
				}

				if (movingDown) { transform.position.y = (y * TileSize) - aabb.halfExtents.y - aabb.offset.y; }
				else { transform.position.y = ((y + 1) * TileSize) + aabb.halfExtents.y - aabb.offset.y; }

				velocity.y = 0.0f;
				return true;
			}
			else if ((flags & CollisionFlags::OneWayPlatform) && movingDown)
			{
				F32 previousBottomY = transform.position.y + aabb.offset.y + aabb.halfExtents.y;
				F32 platformTopY = y * TileSize;

				if (previousBottomY <= platformTopY + Inset)
				{
					transform.position.y = platformTopY - aabb.halfExtents.y - aabb.offset.y;
					velocity.y = 0.0f;
					return true;
				}
			}
		}
	}

	transform.position.y = newY;
	return false;
}