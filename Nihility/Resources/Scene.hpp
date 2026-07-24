#pragma once

#include "Defines.hpp"

#include "Components/Registry.hpp"

struct NH_API Scene
{
	struct TilesetInfo
	{
		U32 engineTextureId = 0;
		F32 width = 0.0f;
		F32 height = 0.0f;
	};

public:
	Scene() = default;
	~Scene();

	bool LoadLevel(const String& filepath);
	void Unload();

	void Update();

private:
	void SpawnPlayer(glm::vec2 spawnPosition);
	void SetupCamera();

	Entity playerEntity;
	Entity cameraEntity;
	Entity tilemapEntity;

	String currentLevel = "";
	glm::vec2 currentSpawnPos{ 0.0f, 0.0f };

	Hashmap<I64, TilesetInfo> tilesetMap;
};