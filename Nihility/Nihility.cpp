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

std::shared_ptr<Texture> sprites[4];

std::shared_ptr<AudioClip> music;
PlaybackHandle musicHandle;
std::shared_ptr<AudioClip> sfx1;
PlaybackHandle sfx1Handle;
std::shared_ptr<AudioClip> sfx2;
PlaybackHandle sfx2Handle;

ChannelHandle sfxChannel;

Scene scene{};

void InitGame()
{
	sprites[0] = Resources::Load<Texture>(L"white");
	sprites[1] = Resources::Load<Texture>(L"missing_texture");
	sprites[2] = Resources::Load<Texture>(L"iconoclast");
	sprites[3] = Resources::Load<Texture>(L"try");

	music = Resources::Load<AudioClip>(L"Electric Zoo");
	sfx1 = Resources::Load<AudioClip>(L"GMF...DAMN");
	sfx2 = Resources::Load<AudioClip>(L"GMFD");

	Input::BindAxis("Horizontal", ButtonCode::D, 1.0f);
	Input::BindAxis("Horizontal", ButtonCode::A, -1.0f);
	Input::BindAction("Jump", ButtonCode::Space);

	Input::BindAxis("Horizontal", AxisCode::LeftJoystickX, 1.0f);
	Input::BindAction("Jump", ButtonCode::GamepadA);

	sfxChannel = Audio::CreateChannel("sfx");

	scene.LoadLDtkLevel("test_level.ldtk", "Level_0");
}

void ShutdownGame()
{
	scene.Unload();
}

void RunGame()
{

}

void Nihility::Initialize(const WStringView& applicationName)
{
	std::setlocale(LC_ALL, ".UTF8");

	scheduler.Initialize();
	bool render = true;

	if (!Time::Initialize()) { return; }
	if (!Logger::Initialize()) { return; }
	if (!FileIO::Initialize()) { return; }
	if (!Settings::Initialize()) { return; }
	if (!Input::Initialize()) { return; }
	if (!Platform::Initialize(applicationName)) { return; }
	if (!Audio::Initialize()) { return; }
	if (!Renderer::Initialize(ConvertView<C>(applicationName), MakeVersionNumber(0, 1, 0))) { return; }
	if (!Resources::Initialize()) { return; }
	if (!Registry::Initialize()) { return; }
	if (!UI::Initialize()) { return; }
	if (!Physics::Initialize()) { return; }
	if (!Tilemap::Initialize()) { return; }

#ifdef NH_DEBUG
	Editor::Initialize();
#else
	InitGame();
#endif

	if (!Registry::CompileComponentGraph()) { return; }

	Entity FPSEntity = UI::CreateText("0", { 0.0f, 0.0f }, 25.0f, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
	UIText& FPS = FPSEntity.GetComponent<UIText>();

	while (Platform::running)
	{
		render = true;

		Time::Update();
		FPS.text = std::to_string(Time::FrameRate());
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
		Editor::Update();
#else
		RunGame();
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
	ShutdownGame();
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