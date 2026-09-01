#include "Nihility.hpp"

#include "Resources/Scene.hpp"
#include "Rendering/UI.hpp"

class LevelOneScene : public Scene
{
public:
	void OnStart() override
	{
		LoadTilemap("level_01.tilemap");
		LoadEntities("level_01.entities");

		Entity player = SpawnPlayer({ 0.0f, 100.0f });
		SetupCamera(player);
	}

	void OnUpdate() override
	{

	}
};

class MainMenuScene : public Scene
{
public:
	void OnStart() override
	{
		SetupCamera();

		Button startButton = UI::CreateButton({ { 0_px, 0_px }, { 100_px, 30_px }, { 0.5f, 0.2f } }, "Play");

		AddToScene(startButton.entity);

		startButton.interactable.onClickName = "play_level_one";

		UI::RegisterAction(startButton.interactable.onClickName, []() {
			SceneManager::ChangeScene(std::make_shared<LevelOneScene>());
		});
	}
};

void InitGame()
{
	SceneManager::ChangeScene(std::make_shared<MainMenuScene>());
}

void ShutdownGame()
{

}

void RunGame()
{

}

int main()
{
	GameInfo info{};
	info.applicationName = L"DemoNew";
	info.versionNumber = MakeVersionNumber(0, 1, 0);
	info.InitializeGame = InitGame;
	info.ShutdownGame = ShutdownGame;
	info.RunGame = RunGame;

	Nihility::Initialize(info);
}