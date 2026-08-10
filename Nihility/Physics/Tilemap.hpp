#pragma once

#include "Defines.hpp"

#include "Core/Containers.hpp"
#include "Core/Function.hpp"
#include "Components/Registry.hpp"
#include "Components/Components.hpp"
#include "Rendering/Buffer.hpp"
#include "Rendering/Shader.hpp"

#include <mutex>

static constexpr U32 ChunkSize = 32;
static constexpr F32 TileSize = 32.0f;

enum class TileLayer
{
	Background,
	Midground,
	Foreground,
	Collision,

	Count
};

enum class CollisionType : U32
{
	None = U32_MAX,
	Solid = 1,
	OneWay = 2,
	Climbable = 3,
	Fluid = 4,
	Slippery = 5,
	Hazard = 6,
	Trigger = 7,

	Count
};

struct Tile
{
	U32 data = U32_MAX;
};

struct NH_API TilemapChunk
{
	TilemapChunk();

	glm::ivec2 gridPosition{ 0, 0 };

	Array<Array<Tile, ChunkSize* ChunkSize>, (U32)TileLayer::Count> layers{};

	bool isDirty = true;
};

struct TileVertex
{
	glm::vec3 position;
	glm::vec2 uv;
	glm::vec4 color;
	U32 textureId;
};

struct DebugVertex
{
	glm::vec3 position;
	U32 typeData;
};

struct NH_API TilemapRenderData
{
	Buffer vertexBuffer;
	Buffer indexBuffer;
	U32 indexCount = 0;
	U32 vertexCount = 0;
	U32 maxVertexCount = 0;
#ifdef NH_DEBUG
	Buffer debugVertexBuffer;
	Buffer debugIndexBuffer;
	U32 debugIndexCount = 0;
	U32 debugMaxVertexCount = 0;
#endif
	bool isInitialized = false;
};

struct LoadedChunkData
{
	I32 chunkX;
	I32 chunkY;
	Array<Array<Tile, ChunkSize* ChunkSize>, (U32)TileLayer::Count> layers;
};

struct ColliderAABB;
struct VkCommandBuffer_T;

class NH_API Tilemap
{
public:
	static U32 GetTileCollision(I32 x, I32 y);
	static Entity GetOrCreateChunk(I32 chunkX, I32 chunkY);
	static Entity GetChunk(I32 chunkX, I32 chunkY);
	static Tile& GetTileAtGlobal(I32 gridX, I32 gridY, U32 layer, bool write = false);

	static bool SweepX(Transform2D& transform, const ColliderAABB& aabb, glm::vec2& velocity);
	static bool SweepY(Transform2D& transform, const ColliderAABB& aabb, glm::vec2& velocity);

	static void Save(const String& filepath);
	static void SaveSync(const String& filepath);
	static void Load(const String& filepath);

	static void Unload();

private:
	static bool Initialize();
	static void Shutdown();
	static void Update();

	static void RenderTilemaps(VkCommandBuffer_T* cmd);

	static void EnsureRenderDataExists(U32 entityId, U32 requiredVertices, U32 requiredIndices, U32 debugVertices, U32 debugIndices);

	static Shader tilemapShader;
#ifdef NH_DEBUG
	static Shader debugShader;
	static bool showCollision;
#endif

	static std::mutex taskMutex;
	static Vector<Function<void()>> mainThreadTasks;

	static Hashmap<U64, Entity> chunkMap;

	friend class Editor;
	friend class Nihility;
	friend class Renderer;

	STATIC_CLASS(Tilemap);
};
