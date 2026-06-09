#include "Tilemap.hpp"

#include "Physics.hpp"

#include "Core/Time.hpp"

bool Tilemap::SweepX(Transform2D& transform, const ColliderAABB& aabb, glm::vec2& velocity) const
{
	F32 amount = velocity.x * (F32)Time::DeltaTime();
	if (amount == 0.0f) { return false; }

	bool movingRight = amount > 0.0f;
	F32 newX = transform.position.x + amount;

	static constexpr F32 Inset = 0.01f;
	F32 topY = transform.position.y + aabb.offset.y + aabb.halfExtents.y - Inset;
	F32 bottomY = transform.position.y + aabb.offset.y - aabb.halfExtents.y + Inset;

	I32 startTileY = (I32)std::floor(bottomY / tileSize);
	I32 endTileY = (I32)std::floor(topY / tileSize);

	F32 currentEdgeX = movingRight ?
		(transform.position.x + aabb.offset.x + aabb.halfExtents.x) :
		(transform.position.x + aabb.offset.x - aabb.halfExtents.x);

	F32 leadingEdgeX = currentEdgeX + amount;

	I32 startTileX = (I32)std::floor(currentEdgeX / tileSize);
	I32 endTileX = (I32)std::floor(leadingEdgeX / tileSize);

	I32 stepX = movingRight ? 1 : -1;

	for (I32 x = startTileX; x != endTileX + stepX; x += stepX)
	{
		for (I32 y = startTileY; y <= endTileY; ++y)
		{
			if (GetTile(x, y) == TileType::Solid)
			{
				if (movingRight) { transform.position.x = (x * tileSize) - aabb.halfExtents.x - aabb.offset.x; }
				else { transform.position.x = ((x + 1) * tileSize) + aabb.halfExtents.x - aabb.offset.x; }

				velocity.x = 0.0f;
				return true;
			}
		}
	}

	transform.position.x = newX;
	return false;
}

bool Tilemap::SweepY(Transform2D& transform, const ColliderAABB& aabb, glm::vec2& velocity) const
{
	F32 amount = velocity.y * (F32)Time::DeltaTime();
	if (amount == 0.0f) { return false; }

	bool movingDown = amount > 0.0f;
	F32 newY = transform.position.y + amount;

	static constexpr F32 Inset = 0.01f;
	F32 rightX = transform.position.x + aabb.offset.x + aabb.halfExtents.x - Inset;
	F32 leftX = transform.position.x + aabb.offset.x - aabb.halfExtents.x + Inset;

	I32 startTileX = (I32)std::floor(leftX / tileSize);
	I32 endTileX = (I32)std::floor(rightX / tileSize);

	F32 currentEdgeY = movingDown ?
		(transform.position.y + aabb.offset.y + aabb.halfExtents.y) :
		(transform.position.y + aabb.offset.y - aabb.halfExtents.y);

	F32 leadingEdgeY = currentEdgeY + amount;

	I32 startTileY = (I32)std::floor(currentEdgeY / tileSize);
	I32 endTileY = (I32)std::floor(leadingEdgeY / tileSize);

	I32 stepY = movingDown ? 1 : -1;

	for (I32 y = startTileY; y != endTileY + stepY; y += stepY)
	{
		for (I32 x = startTileX; x <= endTileX; ++x)
		{
			TileType type = GetTile(x, y);

			if (type == TileType::Solid)
			{
				if (!movingDown)
				{
					static constexpr F32 CornerTolerance = 4.0f;

					F32 tileLeft = x * tileSize;
					F32 tileRight = tileLeft + tileSize;

					if (leftX < tileRight && leftX > tileRight - CornerTolerance && GetTile(x + 1, y) == TileType::Empty)
					{
						transform.position.x = tileRight + aabb.halfExtents.x + 0.01f;
						continue;
					}

					if (rightX > tileLeft && rightX < tileLeft + CornerTolerance && GetTile(x - 1, y) == TileType::Empty)
					{
						transform.position.x = tileLeft - aabb.halfExtents.x - 0.01f;
						continue;
					}
				}

				if (movingDown) { transform.position.y = (y * tileSize) - aabb.halfExtents.y - aabb.offset.y; }
				else { transform.position.y = ((y + 1) * tileSize) + aabb.halfExtents.y - aabb.offset.y; }

				velocity.y = 0.0f;
				return true;
			}
			else if (type == TileType::OneWayPlatform && movingDown)
			{
				F32 previousBottomY = transform.position.y + aabb.offset.y + aabb.halfExtents.y;
				F32 platformTopY = y * tileSize;

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