#include "Scene.hpp"

#include "Core/Logger.hpp"
#include "Physics/Tilemap.hpp"
#include "Physics/Physics.hpp"
#include "Rendering/Renderer.hpp"
#include "Rendering/UI.hpp"

#include "Rendering/VulkanInclude.hpp"
#include "vma/vk_mem_alloc.h"

Scene::~Scene()
{
	Unload();
}

bool Scene::LoadLDtkLevel(const String& filepath, const String& levelIdentifier)
{
	Logger::Trace("Loading Scene: ", levelIdentifier);

	currentLevel = levelIdentifier;

	SpawnPlayer({ 0.0f, 0.0f });
	SetupCamera();

	return true;
}

void Scene::Unload()
{
	if (currentLevel.empty()) { return; }

	// In a complete implementation, you would need a Registry::Clear() or 
	// Registry::DestroyEntity() function to actually free these IDs and component data.
	// For now, we just reset our local trackers.

	currentLevel = "";
	// playerEntity.Destroy(); 
	// cameraEntity.Destroy();
	// tilemapEntity.Destroy();
}

void Scene::Update()
{

}

void Scene::SpawnPlayer(glm::vec2 spawnPosition)
{
	playerEntity = Registry::CreateEntity(spawnPosition, { 32.0f, 32.0f }, 0.0f);

	PlayerController& pc = playerEntity.AddComponent<PlayerController>();
	pc.moveSpeed = 150.0f;
	pc.spawnPosition = spawnPosition;

	Rigidbody2D& body = playerEntity.AddComponent<Rigidbody2D>();
	body.velocity = { 0.0f, 0.0f };

	ColliderAABB& aabb = playerEntity.AddComponent<ColliderAABB>();
	aabb.halfExtents = { 8.0f, 8.0f };
	aabb.offset = { 0.0f, 0.0f };

	Hurtbox& hurtbox = playerEntity.AddComponent<Hurtbox>();
	hurtbox.halfExtents = { 6.0f, 6.0f };
	hurtbox.teamId = 0;

	SpriteComponent& sprite = playerEntity.AddComponent<SpriteComponent>();
	sprite.color = { 0.0f, 1.0f, 0.0f, 1.0f };
	sprite.zIndex = 10;
}

void Scene::SetupCamera()
{
	cameraEntity = Registry::CreateEntity(currentSpawnPos);

	cameraEntity.AddComponent<Camera>();

	CameraTarget& target = cameraEntity.AddComponent<CameraTarget>();
	target.targetEntity = playerEntity.Id();
	target.smoothSpeed = 12.0f;
}

glm::vec2 Scene::ScreenToWorldSpace(glm::vec2 mousePos)
{
	glm::ivec4 viewportArea = Renderer::RenderArea();

	F32 ndcX = std::clamp((mousePos.x - viewportArea.x) / viewportArea.z, 0.0f, 1.0f) * 2.0f - 1.0f;
	F32 ndcY = std::clamp((mousePos.y - viewportArea.y) / viewportArea.w, 0.0f, 1.0f) * 2.0f - 1.0f;

	F32 camHalfWidth = 640.0f;
	F32 camHalfHeight = 360.0f;

	glm::vec2 worldOffset(ndcX * camHalfWidth, ndcY * camHalfHeight);

	glm::vec2 camPos = Registry::GetComponent<Transform2D>(cameraEntity.Id()).position;

	return camPos + worldOffset;
}