#include "Physics.hpp"

#include "Core/Time.hpp"
#include "Core/Logger.hpp"
#include "Platform/Memory.hpp"
#include "Resources/Settings.hpp"
#include "Resources/TilemapComponent.hpp"
#include "Resources/TilemapColliderComponent.hpp"

#include "tracy/Tracy.hpp"

Vector<AABB> Physics::colliders;
Vector<GridCollider> Physics::tilemapColliders;

I32 AssertFcn(const C8* condition, const C8* fileName, I32 lineNumber)
{
	Logger::Fatal("Assertion: '", condition, "' In File: '", fileName, "' On Line: ", lineNumber);
	return 1;
}

bool Physics::Initialize()
{
	return true;
}

void Physics::Shutdown()
{

}

void Physics::Update()
{
	ZoneScopedN("Physics");
}

U32 Physics::AddCollider(const AABB& collider)
{
	U32 index = (U32)colliders.Size();
	colliders.Push(collider);

	return index;
}

U32 Physics::AddTilemapCollider(const ComponentRef<TilemapCollider>& tilemapCollider)
{
	GridCollider collider{};
	collider.dimensions = tilemapCollider->dimensions;
	collider.tileSize = tilemapCollider->tileSize;
	collider.offset = tilemapCollider->offset;
	collider.tileArray = tilemapCollider->tiles;

	U32 index = (U32)tilemapColliders.Size();
	tilemapColliders.Push(collider);

	return index;
}

void Physics::RemoveCollider(U32 index)
{

}

void Physics::RemoveTilemapCollider(U32 index)
{

}


Collision Physics::CheckCollision(const AABB& collider)
{
	Vector2 center = collider.lowerBound + (collider.upperBound - collider.lowerBound) * 0.5f;
	center.y = -center.y;

	for (const GridCollider& col : tilemapColliders)
	{
		Vector2 tilemapPosition = (center - Vector2{ col.offset.x, -col.offset.y }) / col.tileSize;

		Vector2Int xy = { (I32)Math::Floor(tilemapPosition.x), (I32)Math::Floor(tilemapPosition.y) };

		U32 width = (U32)Math::Ceiling((collider.upperBound.x - collider.lowerBound.x) / col.tileSize.x);
		U32 height = (U32)Math::Ceiling((collider.upperBound.y - collider.lowerBound.y) / col.tileSize.y);

		for (I32 x = (I32)(tilemapPosition.x - width); x < (I32)(tilemapPosition.x + width); ++x)
		{
			for (I32 y = (I32)(tilemapPosition.y - height); y < (I32)(tilemapPosition.y + height); ++y)
			{
				//TODO: switch statement with a different check for each type
				if (x < col.dimensions.x && x >= 0 && y < col.dimensions.y && y >= 0 && col.tileArray[x + y * col.dimensions.x] == TileType::Full)
				{
					AABB aabb{};

					aabb.upperBound = { x * col.tileSize.x + col.offset.x + col.tileSize.x, -y * col.tileSize.y + col.offset.y + col.tileSize.y };
					aabb.lowerBound = { x * col.tileSize.x + col.offset.x, -y * col.tileSize.y + col.offset.y };

					if (collider.lowerBound.x < aabb.upperBound.x && collider.upperBound.x > aabb.lowerBound.x &&
						collider.lowerBound.y < aabb.upperBound.y && collider.upperBound.y > aabb.lowerBound.y)
					{
						return { aabb, true };
					}
				}
			}
		}
	}

	for (const AABB& col : colliders)
	{
		if (collider.lowerBound.x < col.upperBound.x && collider.upperBound.x > col.lowerBound.x &&
			collider.lowerBound.y < col.upperBound.y && collider.upperBound.y > col.lowerBound.y)
		{
			return { col, true };
		}
	}

	return { {}, false };
}