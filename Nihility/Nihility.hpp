#pragma once

#include "Defines.hpp"
#include "Core/Containers.hpp"
#include "Core/Function.hpp"

#include "enkiTS/TaskScheduler.h"

struct NH_API GameInfo
{
	WStringView applicationName;
	U32 versionNumber;
	Function<void()> InitializeGame;
	Function<void()> ShutdownGame;
	Function<void()> RunGame;

	GameInfo() = default;

	GameInfo(GameInfo&& other) noexcept : applicationName(std::move(other.applicationName)), InitializeGame(std::move(other.InitializeGame)), ShutdownGame(std::move(other.ShutdownGame)), RunGame(std::move(other.RunGame)) {}

	GameInfo& operator=(GameInfo&& other) noexcept
	{
		applicationName = std::move(other.applicationName);
		InitializeGame = std::move(other.InitializeGame);
		ShutdownGame = std::move(other.ShutdownGame);
		RunGame = std::move(other.RunGame);

		return *this;
	}
};

class NH_API Nihility
{
public:
	static void Initialize(GameInfo& gameInfo);

	static enki::TaskScheduler scheduler; //TODO: Move somewhere else, maybe Platform/Mulithreading

private:
	static void Shutdown();

	static GameInfo info;

	friend class Renderer;
	friend class FileIO;

	STATIC_CLASS(Nihility);
};