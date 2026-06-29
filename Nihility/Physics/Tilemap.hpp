#pragma once

#include "Defines.hpp"

#include "Core/Containers.hpp"
#include "Components/Registry.hpp"
#include "Components/Components.hpp"
#include "Rendering/Buffer.hpp"

enum class TileLayer
{
	Background,
	Midground,
	Collision,
	Foreground,
	Logic,

	Count
};

enum CollisionFlags : U32
{
	None = 0,
	Solid = 1 << 0,
	OneWayPlatform = 1 << 1,
	Hazard = 1 << 2
};

struct Tile
{
	U32 textureId = U32_MAX;
	U32 collisionFlags = 0;
};

struct NH_API TilemapChunk
{
	TilemapChunk();

	glm::ivec2 gridPosition{ 0, 0 };

	Array<Vector<Tile>, (U32)TileLayer::Count> layers;

	bool isDirty = true;
};

struct TileVertex
{
	glm::vec3 position;
	glm::vec2 uv;
	glm::vec4 color;
	U32 textureId;
};

struct NH_API TilemapRenderData
{
	Buffer vertexBuffer;
	Buffer indexBuffer;
	U32 indexCount = 0;
	U32 vertexCount = 0;
	U32 maxVertexCount = 0;

	bool isInitialized = false;
};

struct ColliderAABB;

class NH_API Tilemap
{
public:
	static U32 GetTileCollision(I32 x, I32 y);
	static Entity GetOrCreateChunk(I32 chunkX, I32 chunkY);

	static bool SweepX(Transform2D& transform, const ColliderAABB& aabb, glm::vec2& velocity);
	static bool SweepY(Transform2D& transform, const ColliderAABB& aabb, glm::vec2& velocity);

	static constexpr U32 ChunkSize = 32;
	static constexpr F32 TileSize = 32.0f;

private:
	static bool Initialize();
	static void Shutdown();
	static void Update();

	static void EnsureRenderDataExists(U32 entityId, U32 requiredVertices, U32 requiredIndices);

	static Hashmap<U64, Entity> chunkMap;

	friend class Editor;
	friend class Nihility;

	STATIC_CLASS(Tilemap);
};
