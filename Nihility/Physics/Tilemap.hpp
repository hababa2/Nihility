#pragma once

#include "Defines.hpp"

#include "Core/Containers.hpp"
#include "Components/Components.hpp"
#include "Rendering/Buffer.hpp"

enum class TileType : U32
{
	Empty = 0,
	Solid = 1,
	OneWayPlatform = 2,
	Hazard = 3
};

struct NH_API TilemapRenderData
{
	Buffer vertexBuffer;
	Buffer indexBuffer;
	U32 indexCount = 0;

	U32 textureAtlasId = 0;
};

struct ColliderAABB;

struct NH_API Tilemap
{
	Vector<TileType> grid;
	U32 width = 0;
	U32 height = 0;
	F32 tileSize = 16.0f;

	TileType GetTile(I32 x, I32 y) const
	{
		if (x < 0 || x >= (I32)width || y < 0 || y >= (I32)height) { return TileType::Solid; }

		return grid[y * width + x];
	}

	bool SweepX(Transform2D& transform, const ColliderAABB& aabb, glm::vec2& velocity) const;
	bool SweepY(Transform2D& transform, const ColliderAABB& aabb, glm::vec2& velocity) const;
};
