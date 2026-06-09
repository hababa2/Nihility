#pragma once

#include "Defines.hpp"

#include <glm/glm.hpp>

struct NH_API Rigidbody2D
{
	glm::vec2 velocity{ 0.0f, 0.0f };

	bool isGrounded = false;
	bool isAgainstWall = false;
};

struct NH_API ColliderAABB
{
	glm::vec2 halfExtents{ 0.5f, 0.5f };
	glm::vec2 offset{ 0.0f, 0.0f };
};

struct NH_API Hurtbox
{
	glm::vec2 halfExtents{ 0.5f, 0.5f };
	glm::vec2 offset{ 0.0f, 0.0f };
	U32 teamId = 0;
	bool isInvincible = false;
};

struct NH_API Hitbox
{
	glm::vec2 halfExtents{ 0.5f, 0.5f };
	glm::vec2 offset{ 0.0f, 0.0f };
	U32 teamId = 1;
	U32 damage = 1;

	glm::vec2 knockback{ 0.0f, 0.0f };

	bool isActive = false;
};

struct CollisionManifold
{
	U32 entityA;
	U32 entityB;
	glm::vec2 normal{ 0.0f, 0.0f };
	F32 penetrationDepth = 0.0f;
	bool isHit = false;
};

class NH_API Physics
{
public:
	static bool IntersectCircleCircle(const glm::vec2& posA, F32 radA, const glm::vec2& posB, F32 radB);
	static bool IntersectAABBAABB(const glm::vec2& posA, const glm::vec2& extA, const glm::vec2& posB, const glm::vec2& extB);
	static bool IntersectCircleAABB(const glm::vec2& circlePos, F32 radius, const glm::vec2& aabbPos, const glm::vec2& aabbExtents);

private:
	static bool Initialize();
	static void Shutdown();

	static void Update();

	friend class Nihility;

	STATIC_CLASS(Physics);
};