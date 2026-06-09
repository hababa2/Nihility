#include "Scene.hpp"

#include "Core/Logger.hpp"
#include "Physics/Tilemap.hpp"
#include "Physics/Physics.hpp"
#include "Rendering/Renderer.hpp"
#include "Rendering/UI.hpp"

#include "simdjson/simdjson.h"

#include "Rendering/VulkanInclude.hpp"
#include "vma/vk_mem_alloc.h"

Scene::~Scene()
{
	Unload();
}

bool Scene::LoadLDtkLevel(const String& filepath, const String& levelIdentifier)
{
	Logger::Trace("Loading Scene: ", levelIdentifier);

	Unload();

	bool foundSpawn = false;

	simdjson::ondemand::parser parser;
	simdjson::padded_string jsonStr = simdjson::padded_string::load(filepath.c_str());
	simdjson::ondemand::document doc = parser.iterate(jsonStr);

	for (simdjson::ondemand::object level : doc["levels"])
	{
		std::string_view id = level["identifier"];
		if (id != levelIdentifier) { continue; }

		tilemapEntity = Registry::CreateEntity();
		Tilemap& tilemap = tilemapEntity.AddComponent<Tilemap>();

		for (simdjson::ondemand::object layer : level["layerInstances"])
		{
			std::string_view layerType = layer["__type"];
			std::string_view layerId = layer["__identifier"];

			if (layerType == "IntGrid" && layerId == "Collisions")
			{
				tilemap.width = static_cast<U32>(uint64_t(layer["__cWid"]));
				tilemap.height = static_cast<U32>(uint64_t(layer["__cHei"]));
				tilemap.tileSize = static_cast<F32>(F64(layer["__gridSize"]));

				tilemap.grid.reserve(tilemap.width * tilemap.height);
				for (int64_t val : layer["intGridCsv"])
				{
					tilemap.grid.push_back(static_cast<TileType>(val));
				}

				struct Vertex
				{
					glm::vec2 position;
					glm::vec2 uv;
					U32 textureIndex;
				};

				Vector<Vertex> vertices;
				Vector<U32> indices;

				vertices.reserve((tilemap.width * tilemap.height / 2) * 4);
				indices.reserve((tilemap.width * tilemap.height / 2) * 6);

				U32 vertexOffset = 0;

				for (U32 y = 0; y < tilemap.height; ++y)
				{
					for (U32 x = 0; x < tilemap.width; ++x)
					{
						TileType type = tilemap.GetTile(x, y);

						if (type != TileType::Empty)
						{
							F32 posX = x * tilemap.tileSize;
							F32 posY = y * tilemap.tileSize;

							glm::vec2 uvTopLeft{ 0.0f, 0.0f };
							glm::vec2 uvBottomRight{ 1.0f, 1.0f };

							//TODO: Get texture index
							vertices.push_back({ { posX, posY }, uvTopLeft, 0 });
							vertices.push_back({ { posX + tilemap.tileSize, posY }, { uvBottomRight.x, uvTopLeft.y }, 0 });
							vertices.push_back({ { posX + tilemap.tileSize, posY + tilemap.tileSize }, uvBottomRight, 0 });
							vertices.push_back({ { posX, posY + tilemap.tileSize }, { uvTopLeft.x, uvBottomRight.y }, 0 });

							indices.push_back(vertexOffset + 0);
							indices.push_back(vertexOffset + 1);
							indices.push_back(vertexOffset + 2);
							indices.push_back(vertexOffset + 2);
							indices.push_back(vertexOffset + 3);
							indices.push_back(vertexOffset + 0);

							vertexOffset += 4;
						}
					}
				}

				TilemapRenderData& renderData = tilemapEntity.AddComponent<TilemapRenderData>();
				U64 vertexSize = vertices.size() * sizeof(Vertex);
				U64 indexSize = indices.size() * sizeof(U32);
				renderData.indexCount = (U32)indices.size();
				renderData.vertexBuffer = Renderer::CreateBuffer(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
				renderData.indexBuffer = Renderer::CreateBuffer(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
				Renderer::UploadToBuffer(renderData.vertexBuffer, vertices.data(), vertexSize);
				Renderer::UploadToBuffer(renderData.indexBuffer, indices.data(), indexSize);
			}

			if (layerType == "Entities")
			{
				for (simdjson::ondemand::object entity : layer["entityInstances"])
				{
					std::string_view entId = entity["__identifier"];

					if (entId == "Respawn")
					{
						simdjson::ondemand::array pxArray = entity["px"];
						auto it = pxArray.begin();

						F32 x = static_cast<F32>(F64(*it)); ++it;
						F32 y = static_cast<F32>(F64(*it));

						currentSpawnPos = glm::vec2(x, y);
					}
				}
			}
		}

		break;
	}

	currentLevel = levelIdentifier;

	SpawnPlayer(currentSpawnPos);
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

	auto view = Registry::View<TilemapRenderData>();

	for (U32 i = 0; i < view.Size(); ++i)
	{
		U32 id = view.GetEntity(i);
		auto [renderData] = view.Get(id);

		Renderer::DestroyBuffer(renderData.vertexBuffer);
		Renderer::DestroyBuffer(renderData.indexBuffer);
	}
}

void Scene::Update()
{

}

void Scene::SpawnPlayer(glm::vec2 spawnPosition)
{
	playerEntity = Registry::CreateEntity(spawnPosition, { 16.0f, 16.0f }, 0.0f);

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
	Entity& viewportEntity = Renderer::Viewport();
	glm::vec2 vpPos = UI::GetAbsoluteUIPosition(viewportEntity.Id());
	glm::vec2 vpSize = Registry::GetComponent<UIRect>(viewportEntity.Id()).size;

	F32 ndcX = std::clamp((mousePos.x - vpPos.x) / vpSize.x, 0.0f, 1.0f) * 2.0f - 1.0f;
	F32 ndcY = std::clamp((mousePos.y - vpPos.y) / vpSize.y, 0.0f, 1.0f) * 2.0f - 1.0f;

	F32 camHalfWidth = 640.0f;
	F32 camHalfHeight = 360.0f;

	glm::vec2 worldOffset(ndcX * camHalfWidth, ndcY * camHalfHeight);

	glm::vec2 camPos = Registry::GetComponent<Transform2D>(cameraEntity.Id()).position;

	return camPos + worldOffset;
}