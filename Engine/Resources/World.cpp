#include "World.hpp"

#include "Resources.hpp"

#include "Rendering/Renderer.hpp"

#include "tracy/Tracy.hpp"

Event<Camera&, Vector<Entity>&> World::UpdateFns;
Event<CommandBuffer> World::RenderFns;
Event<> World::InitializeFns;
Event<> World::ShutdownFns;
Vector<Entity> World::entities(256, {});
Freelist World::freeEntities(256);
Camera World::camera;

bool World::Initialize()
{
	InitializeFns();

	return true;
}

void World::Shutdown()
{
	ShutdownFns();
}

void World::Update()
{
	ZoneScopedN("Scene");

	camera.Update();
	UpdateFns(camera, entities);
}

void World::Render(CommandBuffer commandBuffer)
{
	RenderFns(commandBuffer);
}

void World::SetCamera(CameraType type)
{
	camera.Create(type);
}

EntityRef World::CreateEntity(Vector2 position, Vector2 scale, Quaternion2 rotation)
{
	U32 index = freeEntities.GetFree();

	if (index == U32_MAX)
	{
		entities.Resize(entities.Size() + 1);
		entities.Resize(entities.Capacity());
		freeEntities.Resize((U32)entities.Capacity());

		index = freeEntities.GetFree();
	}

	Entity& entity = entities[index];
	entity.position = position;
	entity.scale = scale;
	entity.rotation = rotation;
	entity.prevPosition = position;
	entity.prevRotation = rotation;

	return { index };
}

Entity& World::GetEntity(U32 id)
{
	return entities[id];
}

void World::DestroyEntity(const EntityRef& ref)
{
	freeEntities.Release(ref.EntityId());
}

const Camera& World::GetCamera()
{
	return camera;
}

Vector2 World::ScreenToWorld(const Vector2& position)
{
	Matrix4 inv = camera.ViewProjection().Inverse();
	Vector4Int area = Renderer::RenderSize();

	F32 x = 2.0f * position.x / area.z - 1.0f;
	F32 y = 2.0f * position.y / area.w - 1.0f;

	return { x * inv.a.x + y * inv.a.y + inv.d.x, x * inv.b.x + y * inv.b.y + inv.d.y };
}