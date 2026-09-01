#include "Nihility.hpp"

#include "Platform/Platform.hpp"
#include "Platform/Input.hpp"
#include "Platform/Audio.hpp"
#include "Core/Time.hpp"
#include "Core/File.hpp"
#include "Core/Logger.hpp"
#include "Core/Settings.hpp"
#include "Physics/Physics.hpp"
#include "Physics/Tilemap.hpp"
#include "Resources/Resources.hpp"
#include "Resources/Texture.hpp"
#include "Resources/Scene.hpp"
#include "Rendering/UI.hpp"
#include "Rendering/Editor.hpp"
#include "Rendering/Renderer.hpp"
#include "Components/Registry.hpp"
#include "Components/Components.hpp"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/random.hpp"

#include <clocale>

enki::TaskScheduler Nihility::scheduler;

GameInfo Nihility::info;

void Nihility::Initialize(GameInfo& gameInfo)
{
	std::setlocale(LC_ALL, ".UTF8");

	info = std::move(gameInfo);

	scheduler.Initialize();
	bool render = true;

	if (!Time::Initialize()) { return; }
	if (!Logger::Initialize()) { return; }
	if (!FileIO::Initialize()) { return; }
	if (!Settings::Initialize()) { return; }
	if (!Input::Initialize()) { return; }
	if (!Platform::Initialize(info.applicationName)) { return; }
	if (!Audio::Initialize()) { return; }
	if (!Renderer::Initialize(ConvertView<C>(info.applicationName), info.versionNumber)) { return; }
	if (!Resources::Initialize()) { return; }
	if (!Registry::Initialize()) { return; }
	if (!UI::Initialize()) { return; }
	if (!Physics::Initialize()) { return; }
	if (!Tilemap::Initialize()) { return; }

#ifdef NH_DEBUG
	SceneManager::state = EngineState::Editor;
	Editor::Initialize();
#else
	info.InitializeGame();
#endif

	if (!Registry::CompileComponentGraph()) { return; }

	Entity FPSEntity = UI::CreateText({ {}, {}, { 1.0f, 0.0f } }, "0", 25.0f, false, false, { 0.0f, 1.0f, 0.0f, 1.0f });
	FPSEntity.AddComponent<NoSerialization>();

	while (Platform::running)
	{
		render = true;

		Time::Update();
		FPSEntity.GetComponent<UIText>().text = std::to_string(Time::FrameRate());
		Platform::Update();
		Input::Update();

		if (Input::OnButtonDown(ButtonCode::Escape))
		{
			Platform::Close();
			break;
		}

		if (Input::OnButtonDown(ButtonCode::F11))
		{
			Platform::SetFullscreen(!Settings::Fullscreen());
		}

		if (!Platform::minimised)
		{
			render = Renderer::BeginFrame();
		}

#ifdef NH_DEBUG
		if (SceneManager::state == EngineState::Editor)
		{
			Editor::Update();
		}
		else if (SceneManager::state == EngineState::Playing)
		{
			SceneManager::Update();
		}
		else if (SceneManager::state == EngineState::Paused)
		{

		}
#else
		info.RunGame();
		SceneManager::Update();
#endif
		Registry::Update();
		Audio::Update();

		if (!Platform::minimised && render)
		{
			Renderer::EndFrame();
		}

		F64 remainingFrameTime = Settings::TargetFrametime() - Time::FrameUpTime();
		I64 remainingUS = (I64)(remainingFrameTime * 1000000.0);

		while (remainingUS > 0)
		{
			_mm_pause();

			remainingFrameTime = Settings::TargetFrametime() - Time::FrameUpTime();
			remainingUS = (I64)(remainingFrameTime * 1000000.0);
		}
	}

	Shutdown();
}

void Nihility::Shutdown()
{
	Renderer::Stop();

#ifdef NH_DEBUG
	Editor::Shutdown();
#else
	info.ShutdownGame();
#endif

	Tilemap::Shutdown();
	Physics::Shutdown();
	UI::Shutdown();
	Registry::Shutdown();
	Resources::Shutdown();
	Audio::Shutdown();
	Renderer::Shutdown();
	Platform::Shutdown();
	Input::Shutdown();
	Settings::Shutdown();
	FileIO::Shutdown();
	Logger::Shutdown();
	Memory::Shutdown();
	Time::Shutdown();

	scheduler.WaitforAllAndShutdown();
}