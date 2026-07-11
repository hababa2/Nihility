#include "Registry.hpp"

#include "Nihility.hpp"

#include "Core/Containers.hpp"
#include "Core/Logger.hpp"
#include "Core/Time.hpp"
#include "Core/Settings.hpp"
#include "Rendering/Renderer.hpp"
#include "Platform/Input.hpp"
#include "Platform/Audio.hpp"
#include "Physics/Physics.hpp"

#include "enkiTS/TaskScheduler.h"

#undef max
#undef min
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/random.hpp"

#include <unordered_map>
#include <queue>

void UpdateSprites()
{
	glm::mat4 viewProj = Renderer::GetViewProjectionMatrix();

	SparseSet<SpriteComponent>& spriteSet = Registry::GetSet<SpriteComponent>();
	std::span<SpriteComponent> sprites = spriteSet.GetDenseData();

	if (!sprites.empty())
	{
		std::span<const U32> entities = spriteSet.GetDenseEntities();

		enki::TaskSet updateTask((U32)sprites.size(), [&](enki::TaskSetPartition range, U32 threadnum)
		{
			for (U32 i = range.start; i < range.end; ++i)
			{
				U32 id = entities[i];

				Transform2D& transform = Registry::GetTransform(id);
				SpriteComponent& sprite = sprites[i];

				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, glm::vec3(transform.position, 0.0f));

				if (transform.rotation != 0.0f)
				{
					model = glm::rotate(model, transform.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
				}

				model = glm::scale(model, glm::vec3(transform.scale, 1.0f));

				SpriteData data{};
				data.transform = viewProj * model;
				data.color = sprite.color;
				data.textureIndex = sprite.textureId;
				data.zIndex = sprite.zIndex;

				Renderer::SubmitSprite(data);
			}
		});

		Nihility::scheduler.AddTaskSetToPipe(&updateTask);
		Nihility::scheduler.WaitforTask(&updateTask);
	}
}

void UpdatePlayer()
{
	F32 dt = (F32)Time::DeltaTime();
	auto view = Registry::View<PlayerController, Rigidbody2D>();

	for (U32 i = 0; i < view.Size(); ++i)
	{
		U32 id = view.GetEntity(i);
		if (!view.Matches(id)) { continue; }
		auto [controller, body] = view.Get(id);

		body.velocity.x = Input::GetAxis("Horizontal") * controller.moveSpeed;
		
		F32 gravity = 900.0f;

		if (glm::abs(body.velocity.y) < 40.0f) { gravity *= 0.5f; }

		body.velocity.y += gravity * dt;

		if (body.isGrounded) { controller.coyoteTimer = controller.coyoteTime; }
		else { controller.coyoteTimer -= dt; }

		if (Input::GetAction("Jump", InputType::Press)) { controller.jumpBufferTimer = controller.jumpBufferTime; }
		else { controller.jumpBufferTimer -= dt; }

		if (controller.jumpBufferTimer > 0.0f && controller.coyoteTimer > 0.0f)
		{
			body.velocity.y = controller.jumpForce;

			controller.jumpBufferTimer = 0.0f;
			controller.coyoteTimer = 0.0f;
		}

		if ((Input::GetAction("Jump", InputType::Unpress) || Input::GetAction("Jump", InputType::Release)) && body.velocity.y < 0.0f)
		{
			body.velocity.y *= 0.5f;
		}
	}
}

void UpdateCameras()
{
	auto view = Registry::View<Camera, CameraTarget>();
	F32 dt = (F32)Time::DeltaTime();

	for (U32 i = 0; i < view.Size(); ++i)
	{
		U32 id = view.GetEntity(i);
		if (!view.Matches(id)) { continue; }
		auto [camera, follower] = view.Get(id);
		Transform2D& camTrans = Registry::GetTransform(id);

		if (follower.targetEntity != U32_MAX)
		{
			Transform2D& targetTrans = Registry::GetTransform(follower.targetEntity);

			glm::vec2 targetPos = targetTrans.position + follower.offset;

			F32 t = 1.0f - std::exp(-follower.smoothSpeed * dt);
			camTrans.position = glm::mix(camTrans.position, targetPos, t);
		}

		Audio::SetListener({ camTrans.position, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f });
	}
}

void UpdateAudioEmitters()
{
	auto view = Registry::View<AudioEmitter>();

	for (U32 i = 0; i < view.Size(); ++i)
	{
		U32 id = view.GetEntity(i);
		auto [emitter] = view.Get(id);

		Transform2D& transform = Registry::GetTransform(id);

		Audio::SetClipPosition(emitter.handle, { transform.position, 0.0f }, { transform.position - emitter.prevPosition, 0.0f });

		emitter.prevPosition = transform.position;
	}
}

Transform2D& Entity::Transform()
{
	return Registry::GetTransform(id);
}

Vector<Transform2D> Registry::transforms;
Vector<U32> Registry::freeEntities;

Vector<ISparseSet*> Registry::componentPools;

Vector<ComponentNode> Registry::registeredComponentUpdates;
Vector<ComponentUpdateFn> Registry::executionOrder;

bool Registry::Initialize()
{
	Logger::Trace("Initializing Component Registry...");

	transforms.reserve(10000);
	freeEntities.reserve(10000);

	RegisterComponentUpdate("PlayerController", UpdatePlayer, { "UI" });
	RegisterComponentUpdate("Camera", UpdateCameras, { "Physics", "PlayerController" });
	RegisterComponentUpdate("AudioEmitter", UpdateAudioEmitters, { "Physics", "PlayerController" });
	RegisterComponentUpdate("Sprite", UpdateSprites, { "Physics", "PlayerController" });

	return true;
}

void Registry::Shutdown()
{
	Logger::Trace("Shutting Down Component Registry...");

	transforms.clear();
	freeEntities.clear();
}

void Registry::Update()
{
	for (ComponentUpdateFn update : executionOrder)
	{
		update();
	}
}

Entity Registry::CreateEntity(const Transform2D& transform)
{
	Entity entity;

	if (!freeEntities.empty())
	{
		entity.id = freeEntities.back();
		freeEntities.pop_back();

		transforms[entity.id] = transform;
	}
	else
	{
		entity.id = (U32)transforms.size();
		transforms.push_back(transform);
	}

	return entity;
}

Entity Registry::CreateEntity(glm::vec2 position, glm::vec2 scale, F32 rotation)
{
	Transform2D t{};
	t.position = position;
	t.scale = scale;
	t.rotation = rotation;

	return CreateEntity(t);
}

void Registry::DestroyEntity(Entity& entity)
{
	if (entity.id == U32_MAX) { return; }

	for (ISparseSet* pool : componentPools)
	{
		if (pool->Has(entity.id))
		{
			pool->Remove(entity.id);
		}
	}

	freeEntities.push_back(entity.id);

	entity.id = U32_MAX;
}

Transform2D& Registry::GetTransform(U32 id)
{
	return transforms[id];
}

void Registry::RegisterComponentUpdate(const String& name, ComponentUpdateFn func, const Vector<String>& dependencies)
{
	registeredComponentUpdates.push_back({ name, func, dependencies });
}

bool Registry::CompileComponentGraph()
{
	Logger::Trace("Compiling Component Graph...");

	executionOrder.clear();

	Hashmap<String, U32> inDegree;
	Hashmap<String, Vector<String>> adjacencyList;
	Hashmap<String, ComponentUpdateFn> funcMap;

	for (const ComponentNode& node : registeredComponentUpdates)
	{
		inDegree[node.name] = 0;
		funcMap[node.name] = node.updateFunc;
	}

	for (const ComponentNode& node : registeredComponentUpdates)
	{
		for (const String& dep : node.dependencies)
		{
			if (funcMap.find(dep) == funcMap.end())
			{
				Logger::Error("System '", node.name, "' depends on unregistered system '", dep, "'!");
				return false;
			}

			adjacencyList[dep].push_back(node.name);
			inDegree[node.name]++;
		}
	}

	Queue<String> zeroInDegreeQueue;
	for (const std::pair<const String, U32>& pair : inDegree)
	{
		if (pair.second == 0)
		{
			zeroInDegreeQueue.push(pair.first);
		}
	}

	U32 processedCount = 0;
	while (!zeroInDegreeQueue.empty())
	{
		String current = zeroInDegreeQueue.front();
		zeroInDegreeQueue.pop();

		executionOrder.push_back(funcMap[current]);
		processedCount++;

		for (const String& dependent : adjacencyList[current])
		{
			inDegree[dependent]--;
			if (inDegree[dependent] == 0)
			{
				zeroInDegreeQueue.push(dependent);
			}
		}
	}

	if (processedCount != registeredComponentUpdates.size())
	{
		Logger::Fatal("Circular dependency detected in System graph!");
		return false;
	}

	return true;
}