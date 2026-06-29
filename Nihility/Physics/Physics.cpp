#include "Physics.hpp"

#include "Nihility.hpp"
#include "Tilemap.hpp"

#include "Core/Time.hpp"
#include "Components/Registry.hpp"

#include "enkiTS/TaskScheduler.h"

bool Physics::Initialize()
{
	Registry::RegisterComponentUpdate("Physics", Update, { "PlayerController" });

	return true;
}

void Physics::Shutdown()
{

}

void Physics::Update()
{
	auto attackers = Registry::View<Hitbox>();
	auto defenders = Registry::View<Hurtbox, Rigidbody2D>();

	for (U32 i = 0; i < attackers.Size(); ++i)
	{
		U32 atkId = attackers.GetEntity(i);
		auto [hitbox] = attackers.Get(atkId);

		if (!hitbox.isActive) { continue; }

		Transform2D& atkTrans = Registry::GetTransform(atkId);
		glm::vec2 hitboxPos = atkTrans.position + hitbox.offset;

		for (U32 j = 0; j < defenders.Size(); ++j)
		{
			U32 defId = defenders.GetEntity(j);
			if (atkId == defId || !defenders.Matches(defId)) { continue; }

			auto [hurtbox, body] = defenders.Get(defId);

			if (hurtbox.isInvincible || hitbox.teamId == hurtbox.teamId) { continue; }

			Transform2D& defTrans = Registry::GetTransform(defId);
			glm::vec2 hurtboxPos = defTrans.position + hurtbox.offset;

			if (IntersectAABBAABB(hitboxPos, hitbox.halfExtents, hurtboxPos, hurtbox.halfExtents))
			{
				//TODO: Hit callback

				if (glm::length(hitbox.knockback) > 0.0f)
				{
					F32 dir = (hurtboxPos.x > hitboxPos.x) ? 1.0f : -1.0f;

					body.velocity.x = hitbox.knockback.x * dir;
					body.velocity.y = hitbox.knockback.y;

					//TODO: Stun player/enemy?
				}

				hurtbox.isInvincible = true; //TODO: I-frame counter
			}
		}
	}

	auto view = Registry::View<Rigidbody2D, ColliderAABB>();
	std::span<const U32> entities = Registry::GetSet<Rigidbody2D>().GetDenseEntities();

	if (entities.empty()) { return; }

	enki::TaskSet physicsTask((U32)entities.size(), [&](enki::TaskSetPartition range, U32 threadnum)
	{
		for (U32 i = range.start; i < range.end; ++i)
		{
			U32 id = entities[i];
			if (!view.Matches(id)) { continue; }
			auto [body, collider] = view.Get(id);
			Transform2D& transform = Registry::GetTransform(id);

			body.isGrounded = false;
			body.isAgainstWall = false;

			if (Tilemap::SweepX(transform, collider, body.velocity))
			{
				body.isAgainstWall = true;
			}

			if (Tilemap::SweepY(transform, collider, body.velocity))
			{
				if (body.velocity.y <= 0.0f) { body.isGrounded = true; }
			}

			I32 tileX = (I32)std::floor(transform.position.x / Tilemap::TileSize);
			I32 tileY = (I32)std::floor(transform.position.y / Tilemap::TileSize);

			if (Tilemap::GetTileCollision(tileX, tileY) == CollisionFlags::Hazard)
			{
				if (Registry::GetSet<PlayerController>().Has(id))
				{
					PlayerController& pc = Registry::GetComponent<PlayerController>(id);

					//TODO: Respawn routine
					transform.position = pc.spawnPosition;
					body.velocity = { 0.0f, 0.0f };
				}
				else
				{
					// If an enemy or physics object falls into spikes, you likely want 
					// to queue them for destruction instead of respawning them.
					// e.g., DeferredDestroyQueue.push_back(id);
				}
			}
		}
	});

	Nihility::scheduler.AddTaskSetToPipe(&physicsTask);
	Nihility::scheduler.WaitforTask(&physicsTask);
}

bool Physics::IntersectCircleCircle(const glm::vec2& posA, F32 radA, const glm::vec2& posB, F32 radB)
{
	glm::vec2 d = posB - posA;
	F32 distSquared = glm::dot(d, d);
	F32 radiusSum = radA + radB;
	return distSquared < (radiusSum * radiusSum);
}

bool Physics::IntersectAABBAABB(const glm::vec2& posA, const glm::vec2& extA, const glm::vec2& posB, const glm::vec2& extB)
{
	return std::abs(posA.x - posB.x) <= (extA.x + extB.x) &&
		std::abs(posA.y - posB.y) <= (extA.y + extB.y);
}

bool Physics::IntersectCircleAABB(const glm::vec2& circlePos, F32 radius, const glm::vec2& aabbPos, const glm::vec2& aabbExtents)
{
	glm::vec2 closestPoint = glm::clamp(circlePos, aabbPos - aabbExtents, aabbPos + aabbExtents);

	glm::vec2 d = circlePos - closestPoint;
	F32 distSquared = glm::dot(d, d);

	return distSquared < (radius * radius);
}