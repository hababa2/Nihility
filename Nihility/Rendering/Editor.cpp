#include "Editor.hpp"

#ifdef NH_DEBUG

#include "UI.hpp"
#include "Renderer.hpp"
#include "VulkanInclude.hpp"

#include "Resources/Scene.hpp"
#include "Platform/Input.hpp"
#include "Core/Logger.hpp"
#include "Core/File.hpp"

Entity Editor::viewportPanel;
Entity Editor::tileSelectionHighlight;
Entity Editor::collisionSelectionHighlight;
Entity Editor::visualPaletteRoot;
Entity Editor::collisionPaletteRoot;
Entity Editor::saveMenuRoot;
Entity Editor::loadMenuRoot;

EditorTool Editor::activeTool = EditorTool::Brush;
TileLayer Editor::activeLayer = TileLayer::Midground;
U32 Editor::selectedTextureId = U32_MAX;
CollisionType Editor::activeCollisionType = CollisionType::Solid;
String Editor::currentLevel;

Entity Editor::cameraEntity;

Vector<std::shared_ptr<Texture>> Editor::loadedTileTextures;

Shader Editor::gridShader;
bool Editor::showGrid;

void Editor::Initialize()
{
	Logger::Trace("Initializing Level Editor...");

	Entity editorBg = UI::CreatePanel({ {}, { 100_vw, 100_vh } }, { 0.15f, 0.15f, 0.15f, 1.0f });
	editorBg.AddComponent<NoSerialization>();

	viewportPanel = UI::CreatePanel({ { 0_px, 0_px }, { 90_vw, 60_vh }, { 0.0f, 0.0f }, 10_px, 0_px, 0_px, 150_px }, { 1.0f, 1.0f, 1.0f, 1.0f }, editorBg);
	viewportPanel.AddComponent<UIResizable>();
	viewportPanel.AddComponent<NoSerialization>();

	Registry::GetComponent<UIPanel>(viewportPanel.Id()).textureId = Renderer::viewportTarget.Id();

	Entity toolbar = UI::CreatePanel({ {}, { 150_px, 100_vw } }, { 0.1f, 0.1f, 0.1f, 1.0f }, editorBg);
	toolbar.AddComponent<NoSerialization>();

	F32 currentY = 10.0f;
	auto CreateToolButton = [&](const String& name, EditorTool tool) {
		Button btn = UI::CreateButton({ { 5.0f, currentY }, { 100.0f, 40.0f } }, name, { 0.3f, 0.3f, 0.3f, 1.0f }, toolbar, true);
		btn.interactable.onClickName = name;

		UI::RegisterAction(name, [tool]() {
			activeTool = tool;
		});

		currentY += 50.0f;
	};

	CreateToolButton("Brush", EditorTool::Brush);
	CreateToolButton("Eraser", EditorTool::Eraser);
	CreateToolButton("Fill", EditorTool::Fill);
	CreateToolButton("Picker", EditorTool::Select);

	currentY += 50.0f;
	auto CreateLayerButton = [&](const String& name, TileLayer layer) {
		Button btn = UI::CreateButton({ { 5.0f, currentY }, { 120.0f, 40.0f } }, name, { 0.3f, 0.3f, 0.3f, 1.0f }, toolbar, true);
		btn.interactable.onClickName = name;

		UI::RegisterAction(name, [layer]() {
			activeLayer = layer;

			if (activeLayer == TileLayer::Collision)
			{
				if (!Registry::HasComponent<UIHidden>(visualPaletteRoot.Id())) { visualPaletteRoot.AddComponent<UIHidden>(); }
				if (Registry::HasComponent<UIHidden>(collisionPaletteRoot.Id())) { Registry::RemoveComponent<UIHidden>(collisionPaletteRoot.Id()); }

				Tilemap::showCollision = true;
			}
			else
			{
				if (!Registry::HasComponent<UIHidden>(collisionPaletteRoot.Id())) { collisionPaletteRoot.AddComponent<UIHidden>(); }
				if (Registry::HasComponent<UIHidden>(visualPaletteRoot.Id())) { Registry::RemoveComponent<UIHidden>(visualPaletteRoot.Id()); }

				Tilemap::showCollision = false;
			}
		});

		currentY += 50.0f;
	};

	CreateLayerButton("Background", TileLayer::Background);
	CreateLayerButton("Midground", TileLayer::Midground);
	CreateLayerButton("Foreground", TileLayer::Foreground);
	CreateLayerButton("Collision", TileLayer::Collision);

	saveMenuRoot = UI::CreatePanel({ { 0_px, 0_px }, { 150_px, 300_px }, { 0.5f, 0.5f } }, { 0.1f, 0.1f, 0.1f, 1.0f }, editorBg);
	saveMenuRoot.AddComponent<NoSerialization>();
	saveMenuRoot.AddComponent<UIHidden>();

	auto BuildSaveMenu = [&]() {
		UI::DestroyChildren(saveMenuRoot);

		Entity levelName = UI::CreateTextInput({ { 10_px, 0_px }, { 60_px, 40_px } }, saveMenuRoot, true);

		Button saveBtn = UI::CreateButton({ { 10_px, 50_px }, { 30_px, 20_px } }, "Save", { 0.3f, 0.3f, 0.3f, 1.0f }, saveMenuRoot, true);

		saveBtn.interactable.onClickName = "SaveLevel";

		UI::RegisterAction(saveBtn.interactable.onClickName, [levelName]() mutable {
			Editor::SaveLevel(levelName.GetComponent<UITextInput>().text);
			saveMenuRoot.AddComponent<UIHidden>();
		});

		Button cancelBtn = UI::CreateButton({ { 10_px, 70_px }, { 30_px, 20_px } }, "Cancel", { 0.3f, 0.3f, 0.3f, 1.0f }, saveMenuRoot, true);

		cancelBtn.interactable.onClickName = "CancelSave";

		UI::RegisterAction(cancelBtn.interactable.onClickName, []() {
			saveMenuRoot.AddComponent<UIHidden>();
		});
	};

	currentY += 50.0f;

	Button saveBtn = UI::CreateButton({ { 5.0f, currentY }, { 100.0f, 40.0f } }, "Save As", { 0.3f, 0.3f, 0.3f, 1.0f }, toolbar, true);
	saveBtn.interactable.onClickName = "Save As";

	UI::RegisterAction("Save As", [&]() {
		if (saveMenuRoot.HasComponent<UIHidden>())
		{
			BuildSaveMenu();
			Registry::RemoveComponent<UIHidden>(saveMenuRoot.Id());
		}
	});

	loadMenuRoot = UI::CreatePanel({ { 0_px, 0_px }, { 150_px, 300_px }, { 0.5f, 0.5f } }, { 0.1f, 0.1f, 0.1f, 1.0f }, editorBg);
	loadMenuRoot.AddComponent<NoSerialization>();
	loadMenuRoot.AddComponent<UIHidden>();

	auto BuildLoadMenu = [&]() {
		UI::DestroyChildren(loadMenuRoot);

		Vector<String> levels = FileIO::GetSavedLevels();

		F32 curY = 0.0f;

		for (const String& levelName : levels)
		{
			Button btn = UI::CreateButton({ { 10_px, curY }, { 30_px, 20_px } }, levelName, { 0.3f, 0.3f, 0.3f, 1.0f }, loadMenuRoot, true);
			curY += 40.0f;

			btn.interactable.onClickName = levelName;

			UI::RegisterAction(levelName, [levelName]() {
				Editor::LoadLevel(levelName);
				loadMenuRoot.AddComponent<UIHidden>();
			});
		}

		Button cancelBtn = UI::CreateButton({ { 10_px, curY }, { 30_px, 20_px } }, "Cancel", { 0.3f, 0.3f, 0.3f, 1.0f }, loadMenuRoot, true);

		cancelBtn.interactable.onClickName = "CancelLoad";

		UI::RegisterAction(cancelBtn.interactable.onClickName, []() {
			loadMenuRoot.AddComponent<UIHidden>();
		});
	};

	currentY += 50.0f;

	Button loadBtn = UI::CreateButton({ { 5.0f, currentY }, { 100.0f, 40.0f } }, "Load", { 0.3f, 0.3f, 0.3f, 1.0f }, toolbar, true);
	loadBtn.interactable.onClickName = "Load";

	UI::RegisterAction("Load", [&]() {
		if (loadMenuRoot.HasComponent<UIHidden>())
		{
			BuildLoadMenu();
			Registry::RemoveComponent<UIHidden>(loadMenuRoot.Id());
		}
	});

	Vector<U32> availableTiles;

	std::shared_ptr<Texture> grass = Resources::Load<Texture>(L"try");
	std::shared_ptr<Texture> dirt = Resources::Load<Texture>(L"white");
	std::shared_ptr<Texture> stone = Resources::Load<Texture>(L"missing_texture");

	availableTiles.push_back(grass->Id());
	availableTiles.push_back(dirt->Id());
	availableTiles.push_back(stone->Id());

	BuildPaletteWindow(availableTiles);

	Entity toolbarPanel = UI::CreatePanel({ { -100_px, 10_px }, { 200_px, 40_px }, { 0.5f, 0.0f } }, { 0.1f, 0.1f, 0.1f, 1.0f }, editorBg);
	toolbarPanel.AddComponent<NoSerialization>();

	Button playBtn = UI::CreateButton({ { 50_px, 0_px }, { 30_px, 15_px } }, "Play", { 0.3f, 0.3f, 0.3f, 1.0f }, toolbarPanel, true);
	playBtn.interactable.onClickName = "Play";

	UI::RegisterAction("Play", []() {
		Editor::Play();
	});

	Button pauseBtn = UI::CreateButton({ { 100_px, 0_px }, { 30_px, 15_px } }, "Pause", { 0.3f, 0.3f, 0.3f, 1.0f }, toolbarPanel, true);
	pauseBtn.interactable.onClickName = "Pause";

	UI::RegisterAction("Pause", []() {
		Editor::Pause();
	});

	Button stopBtn = UI::CreateButton({ { 150_px, 0_px }, { 30_px, 15_px } }, "Stop", { 0.3f, 0.3f, 0.3f, 1.0f }, toolbarPanel, true);
	stopBtn.interactable.onClickName = "Stop";

	UI::RegisterAction("Stop", []() {
		Editor::Stop();
	});

	cameraEntity = Registry::CreateEntity({ 0.0f, 0.0f });
	cameraEntity.AddComponent<Camera>();
	cameraEntity.AddComponent<NoSerialization>();

	Renderer::SetActiveCamera(cameraEntity);

	gridShader.Create("grid.slang");
	showGrid = true;

	Input::BindAxis("Horizontal", ButtonCode::A, -1.0f);
	Input::BindAxis("Horizontal", ButtonCode::D, 1.0f);
	Input::BindAction("Jump", ButtonCode::Space);
}

void Editor::BuildPaletteWindow(const Vector<U32>& loadedTextureIds)
{
	Entity paletteBody = UI::CreateWindow({ { 10_vw, 10_vw }, { 300.0f, 200.0f } }, "Tile Palette", true, true);

	visualPaletteRoot = UI::CreateContainer({ { 0_px, 0_px }, { 100_pw, 100_ph } }, paletteBody);
	visualPaletteRoot.AddComponent<NoSerialization>();

	collisionPaletteRoot = UI::CreateContainer({ { 0_px, 0_px }, { 100_pw, 100_ph } }, paletteBody);
	collisionPaletteRoot.AddComponent<UIHidden>();
	collisionPaletteRoot.AddComponent<NoSerialization>();

	ScrollAreaEntities visualScroll = UI::CreateScrollArea({ {}, { 100_pw, 100_ph } }, visualPaletteRoot, true);
	ScrollAreaEntities collisionScroll = UI::CreateScrollArea({ {}, { 100_pw, 100_ph } }, collisionPaletteRoot, true);

	constexpr F32 TileDisplaySize = 64.0f;
	constexpr F32 Padding = 10.0f;

	tileSelectionHighlight = UI::CreatePanel({ { -1000.0f, -1000.0f }, { TileDisplaySize + 4.0f, TileDisplaySize + 4.0f } }, { 1.0f, 0.8f, 0.2f, 1.0f }, visualScroll.content);
	tileSelectionHighlight.AddComponent<NoSerialization>();
	collisionSelectionHighlight = UI::CreatePanel({ { -1000.0f, -1000.0f }, { TileDisplaySize + 4.0f, TileDisplaySize + 4.0f } }, { 1.0f, 0.8f, 0.2f, 1.0f }, collisionScroll.content);
	collisionSelectionHighlight.AddComponent<NoSerialization>();

	F32 currentX = Padding;
	F32 currentY = Padding;

	F32 maxRowWidth = 350.0f;

	for (U32 texID : loadedTextureIds)
	{
		Button tileBtn = UI::CreateButton({ { currentX, currentY }, { TileDisplaySize, TileDisplaySize } }, "", { 1.0f, 1.0f, 1.0f, 1.0f }, visualScroll.content, true);
		Registry::GetComponent<UIPanel>(tileBtn.entity.Id()).textureId = texID;

		tileBtn.interactable.onClickName = "tile_click" + std::to_string(texID);
		tileBtn.interactable.onHoverEnterName = "tile_hover" + std::to_string(texID);
		tileBtn.interactable.onHoverExitName = "tile_exit" + std::to_string(texID);

		UI::RegisterAction(tileBtn.interactable.onClickName, [texID, currentX, currentY]() {
			selectedTextureId = texID;
			activeTool = EditorTool::Brush;

			UIRect& highlightRect = Registry::GetComponent<UIRect>(tileSelectionHighlight.Id());
			highlightRect.pos = { currentX - 2.0f, currentY - 2.0f };
		});

		UI::RegisterAction(tileBtn.interactable.onHoverEnterName, [tileBtn]() {
			Registry::GetComponent<UIPanel>(tileBtn.entity.Id()).color = { 0.8f, 0.8f, 0.8f, 1.0f };
		});

		UI::RegisterAction(tileBtn.interactable.onHoverExitName, [tileBtn]() {
			Registry::GetComponent<UIPanel>(tileBtn.entity.Id()).color = { 1.0f, 1.0f, 1.0f, 1.0f };
		});

		currentX += TileDisplaySize + Padding;

		if (currentX + TileDisplaySize > maxRowWidth)
		{
			currentX = Padding;
			currentY += TileDisplaySize + Padding;
		}
	}

	currentX = Padding;
	currentY = Padding;

	auto CreateColButton = [&](Entity parent, CollisionType type, const Color& color, const String& label) {

		Button collisionBtn = UI::CreateButton({ { currentX, currentY }, { TileDisplaySize, TileDisplaySize } }, label, color, collisionScroll.content, true);
		collisionBtn.interactable.onClickName = label;

		UI::RegisterAction(label, [type, currentX, currentY]() {
			activeCollisionType = type;
			activeTool = EditorTool::Brush;

			UIRect& highlightRect = Registry::GetComponent<UIRect>(collisionSelectionHighlight.Id());
			highlightRect.pos = { currentX - 2.0f, currentY - 2.0f };
		});

		currentX += TileDisplaySize + Padding;

		if (currentX + TileDisplaySize > maxRowWidth)
		{
			currentX = Padding;
			currentY += TileDisplaySize + Padding;
		}
	};

	CreateColButton(collisionPaletteRoot, CollisionType::Solid, { 0.0f, 1.0f, 0.0f, 1.0f }, "Solid");
	CreateColButton(collisionPaletteRoot, CollisionType::OneWay, { 1.0f, 1.0f, 0.0f, 1.0f }, "One-Way");
	CreateColButton(collisionPaletteRoot, CollisionType::Climbable, { 0.0f, 0.0f, 1.0f, 1.0f }, "Climbable");
	CreateColButton(collisionPaletteRoot, CollisionType::Fluid, { 0.0f, 1.0f, 1.0f, 1.0f }, "Fluid");
	CreateColButton(collisionPaletteRoot, CollisionType::Slippery, { 1.0f, 1.0f, 1.0f, 1.0f }, "Slippery");
	CreateColButton(collisionPaletteRoot, CollisionType::Hazard, { 1.0f, 0.0f, 0.0f, 1.0f }, "Hazard");
	CreateColButton(collisionPaletteRoot, CollisionType::Trigger, { 1.0f, 0.0f, 1.0f, 1.0f }, "Trigger");
}

void Editor::Shutdown()
{
	Logger::Trace("Shutting Down Level Editor...");

	gridShader.Destroy();
}

void Editor::Update()
{
	Transform2D& camTrans = Registry::GetTransform(cameraEntity.Id());

	if (UI::hoveredEntity != viewportPanel.Id() || (UI::activeEntity != U32_MAX && UI::activeEntity != viewportPanel.Id()))
	{
		return;
	}

	if (Input::ButtonDragging(ButtonCode::MiddleMouse))
	{
		glm::vec4 area = Renderer::RenderArea();
		glm::vec2 delta = Input::MouseDelta();
		camTrans.position -= delta / glm::vec2{ area.z, area.w } * glm::vec2{ 1920.0f, 1080.0f };
	}

	if (UI::hoveredEntity != U32_MAX && Registry::HasComponent<UIWindow>(UI::hoveredEntity)) { return; }

	if (Input::ButtonDown(ButtonCode::LeftMouse))
	{
		glm::vec2 uiMouse = Input::MousePosition();
		UIRect& vpRect = Registry::GetComponent<UIRect>(viewportPanel.Id());
		glm::vec2 vpPos = vpRect.resolvedPos;
		glm::vec2 vpSize = vpRect.resolvedSize;
		glm::vec4 area = Renderer::RenderArea();

		if (uiMouse.x >= vpPos.x + area.x && uiMouse.x <= vpPos.x + vpSize.x - area.x &&
			uiMouse.y >= vpPos.y + area.y && uiMouse.y <= vpPos.y + vpSize.y - area.y)
		{
			glm::vec4 area = Renderer::RenderArea();
			glm::vec2 localViewportMouse = uiMouse - vpPos - glm::vec2{ area.x, area.y } - glm::vec2{ area.z, area.w } / 2.0f;
			glm::vec2 normalizedMouse = localViewportMouse / glm::vec2{ area.z, area.w };

			glm::vec2 worldMouse = normalizedMouse * glm::vec2{ 1920.0f, 1080.0f } + glm::vec2{ 960.0f, 540.0f } + camTrans.position;

			I32 gridX = (I32)glm::floor(worldMouse.x / TileSize);
			I32 gridY = (I32)glm::floor(worldMouse.y / TileSize);

			U32* data = (activeLayer == TileLayer::Collision) ? (U32*)&activeCollisionType : &selectedTextureId;

			if (Input::OnButtonDown(ButtonCode::LeftMouse))
			{
				if (activeTool == EditorTool::Select)
				{
					Tile& tile = Tilemap::GetTileAtGlobal(gridX, gridY, (U32)activeLayer, false);

					if (tile.data != U32_MAX)
					{
						*data = tile.data;
						activeTool = EditorTool::Brush;
					}
				}
				else if (activeTool == EditorTool::Fill && *data != U32_MAX)
				{
					Tile& clickedTile = Tilemap::GetTileAtGlobal(gridX, gridY, (U32)activeLayer, false);
					U32 targetData = clickedTile.data;

					if (targetData == *data) { return; }

					Vector<glm::ivec2> queue;
					queue.push_back({ gridX, gridY });

					U32 iterations = 0;
					constexpr U32 MaxFillLimit = 4096;

					while (!queue.empty() && iterations < MaxFillLimit)
					{
						glm::ivec2 curr = queue.back();
						queue.pop_back();

						Tile& currentTile = Tilemap::GetTileAtGlobal(curr.x, curr.y, (U32)activeLayer, false);

						if (currentTile.data == targetData)
						{
							Tile& writeTile = Tilemap::GetTileAtGlobal(curr.x, curr.y, (U32)activeLayer, true);
							writeTile.data = *data;

							queue.push_back({ curr.x + 1, curr.y });
							queue.push_back({ curr.x - 1, curr.y });
							queue.push_back({ curr.x, curr.y + 1 });
							queue.push_back({ curr.x, curr.y - 1 });

							iterations++;
						}
					}
				}
			}
			else if (Input::ButtonDown(ButtonCode::LeftMouse))
			{
				if (activeTool == EditorTool::Brush && *data != U32_MAX)
				{
					Tile& tile = Tilemap::GetTileAtGlobal(gridX, gridY, (U32)activeLayer, true);
					tile.data = *data;
				}
				else if (activeTool == EditorTool::Eraser)
				{
					Tile& tile = Tilemap::GetTileAtGlobal(gridX, gridY, (U32)activeLayer, true);
					tile.data = U32_MAX;
				}
			}
		}
	}
}

struct TempScene : public Scene
{
public:
	void OnStart() override
	{
		Editor::LoadLevel("temp_editor");

		Entity player = SpawnPlayer({ 0.0f, 100.0f });
		Renderer::SetActiveCamera(SetupCamera(player));
	}

	void OnUpdate() override
	{

	}
};

void Editor::Play()
{
	if (SceneManager::state == EngineState::Editor)
	{
		SaveLevel("temp_editor");

		SceneManager::state = EngineState::Playing;
		SceneManager::ChangeScene(std::make_shared<TempScene>());
	}
	else if (SceneManager::state == EngineState::Paused)
	{
		SceneManager::state = EngineState::Playing;
	}
}

void Editor::Pause()
{
	if (SceneManager::state == EngineState::Playing)
	{
		SceneManager::state = EngineState::Paused;
	}
}

void Editor::Stop()
{
	if (SceneManager::state != EngineState::Editor)
	{
		SceneManager::state = EngineState::Editor;

		SceneManager::GetActiveScene()->ClearScene();

		DeleteLevel("temp_editor");
		LoadLevel(currentLevel);
		Renderer::SetActiveCamera(cameraEntity);
	}
}

void Editor::RenderGrid(VkCommandBuffer cmd)
{
	if (!showGrid || SceneManager::state != EngineState::Editor) { return; }

	Transform2D& camTrans = Registry::GetTransform(cameraEntity.Id());
	UIRect& vpRect = Registry::GetComponent<UIRect>(viewportPanel.Id());

	glm::vec4 area = Renderer::RenderArea();

	GridPushConstants pc{};
	pc.camPos = camTrans.position;
	pc.camZoom = camTrans.scale;
	pc.offset = glm::vec2(area.x, area.y);
	pc.scale = 1920.0f / area.z;
	pc.tileSize = (F32)TileSize;
	pc.chunkMult = (F32)ChunkSize;

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridShader.Pipeline());

	vkCmdPushConstants(cmd, gridShader.PipelineLayout(), VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(GridPushConstants), &pc);

	vkCmdDraw(cmd, 6, 1, 0, 0);
}

void Editor::SaveLevel(const String& levelName)
{
	String tilemapPath = "levels/" + levelName + ".tilemap";
	String entitiesPath = "levels/" + levelName + ".entities";

	Tilemap::Save(tilemapPath);
	Registry::SaveState(entitiesPath);
}

void Editor::LoadLevel(const String& levelName)
{
	if (levelName != "temp_editor") { currentLevel = levelName; }

	String tilemapPath = "levels/" + levelName + ".tilemap";
	String entitiesPath = "levels/" + levelName + ".entities";

	Tilemap::Load(tilemapPath);
	Registry::LoadState(entitiesPath);
}

void Editor::DeleteLevel(const String& levelName)
{
	String tilemapPath = "levels/" + levelName + ".tilemap";
	String entitiesPath = "levels/" + levelName + ".entities";

	FileIO::DeleteFile(tilemapPath);
	FileIO::DeleteFile(entitiesPath);
}

#endif