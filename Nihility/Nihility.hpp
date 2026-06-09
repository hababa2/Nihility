#pragma once

#include "Defines.hpp"
#include "Core/Containers.hpp"

#include "enkiTS/TaskScheduler.h"

class NH_API Nihility
{
public:
	static void Initialize(const WStringView& applicationName);

	static enki::TaskScheduler scheduler; //TODO: Move somewhere else, maybe Platform/Mulithreading

private:
	static void Shutdown();

	friend class Renderer;
	friend class FileIO;

	STATIC_CLASS(Nihility);
};