#include "Editor.hpp"

#ifdef NH_DEBUG

#include "UI.hpp"
#include "Renderer.hpp"
#include "VulkanInclude.hpp"

#include "Resources/Scene.hpp"
#include "Platform/Input.hpp"
#include "Core/Logger.hpp"

Entity Editor::viewportPanel;
Entity Editor::tileSelectionHighlight;
Entity Editor::collisionSelectionHighlight;
Entity Editor::visualPaletteRoot;
Entity Editor::collisionPaletteRoot;

EditorTool Editor::activeTool = EditorTool::Brush;
TileLayer Editor::activeLayer = TileLayer::Midground;
U32 Editor::selectedTextureId = U32_MAX;
CollisionType Editor::activeCollisionType = CollisionType::Solid;

Entity Editor::cameraEntity;

Vector<std::shared_ptr<Texture>> Editor::loadedTileTextures;

Shader Editor::gridShader;
bool Editor::showGrid;

void Editor::Initialize()
{
	Logger::Trace("Initializing Level Editor...");

	Entity editorBg = UI::CreatePanel({ {}, { 100_vw, 100_vh } }, { 0.15f, 0.15f, 0.15f, 1.0f });

	viewportPanel = UI::CreatePanel({ { 0_px, 0_px }, { 90_vw, 60_vh }, { 0.0f, 0.0f }, 10_px, 0_px, 0_px, 150_px }, { 1.0f, 1.0f, 1.0f, 1.0f }, editorBg);
	viewportPanel.AddComponent<UIResizable>();

	Registry::GetComponent<UIPanel>(viewportPanel.Id()).textureId = Renderer::viewportTarget.Id();

	Entity toolbar = UI::CreatePanel({ {}, { 150_px, 100_vw } }, { 0.1f, 0.1f, 0.1f, 1.0f }, editorBg);

	F32 currentY = 10.0f;
	auto CreateToolButton = [&](const String& name, EditorTool tool) {
		Button btn = UI::CreateButton({ { 5.0f, currentY }, { 100.0f, 40.0f } }, name, { 0.3f, 0.3f, 0.3f, 1.0f }, toolbar);

		btn.interactable.OnClick = [tool]() {
			activeTool = tool;
		};
		currentY += 50.0f;
	};

	CreateToolButton("Brush", EditorTool::Brush);
	CreateToolButton("Eraser", EditorTool::Eraser);
	CreateToolButton("Fill", EditorTool::Fill);
	CreateToolButton("Picker", EditorTool::Select);

	currentY += 50.0f;
	auto CreateLayerButton = [&](const String& name, TileLayer layer) {
		Button btn = UI::CreateButton({ { 5.0f, currentY }, { 120.0f, 40.0f } }, name, { 0.3f, 0.3f, 0.3f, 1.0f }, toolbar);

		btn.interactable.OnClick = [layer]() {
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
		};
		currentY += 50.0f;
	};

	CreateLayerButton("Background", TileLayer::Background);
	CreateLayerButton("Midground", TileLayer::Midground);
	CreateLayerButton("Foreground", TileLayer::Foreground);
	CreateLayerButton("Collision", TileLayer::Collision);

	currentY += 50.0f;

	Button saveBtn = UI::CreateButton({ { 5.0f, currentY }, { 100.0f, 40.0f } }, "Save", { 0.3f, 0.3f, 0.3f, 1.0f }, toolbar);
	saveBtn.interactable.OnClick = []() {
		Tilemap::Save("level_01.lvl");
	};

	currentY += 50.0f;

	Button loadBtn = UI::CreateButton({ { 5.0f, currentY }, { 100.0f, 40.0f } }, "Load", { 0.3f, 0.3f, 0.3f, 1.0f }, toolbar);
	loadBtn.interactable.OnClick = []() {
		Tilemap::Load("level_01.lvl");
	};

	Vector<U32> availableTiles;

	std::shared_ptr<Texture> grass = Resources::Load<Texture>(L"try");
	std::shared_ptr<Texture> dirt = Resources::Load<Texture>(L"white");
	std::shared_ptr<Texture> stone = Resources::Load<Texture>(L"missing_texture");

	availableTiles.push_back(grass->Id());
	availableTiles.push_back(dirt->Id());
	availableTiles.push_back(stone->Id());

	BuildPaletteWindow(availableTiles);

	Entity toolbarPanel = UI::CreatePanel({ { -100_px, 10_px }, { 200_px, 40_px }, { 0.5f, 0.0f } }, { 0.1f, 0.1f, 0.1f, 1.0f }, editorBg);

	Button playBtn = UI::CreateButton({ { 50_px, 0_px }, { 30_px, 15_px } }, "Play", { 0.3f, 0.3f, 0.3f, 1.0f }, toolbarPanel);
	Button pauseBtn = UI::CreateButton({ { 100_px, 0_px }, { 30_px, 15_px } }, "Pause", { 0.3f, 0.3f, 0.3f, 1.0f }, toolbarPanel);
	Button stopBtn = UI::CreateButton({ { 150_px, 0_px }, { 30_px, 15_px } }, "Stop", { 0.3f, 0.3f, 0.3f, 1.0f }, toolbarPanel);

	// TODO: Use your layout engine margins/widths to space them out side-by-side (e.g. 33% width each)

	playBtn.interactable.OnClick = []() { Editor::Play(); };
	pauseBtn.interactable.OnClick = []() { Editor::Pause(); };
	stopBtn.interactable.OnClick = []() { Editor::Stop(); };

	cameraEntity = Registry::CreateEntity({ 0.0f, 0.0f });
	cameraEntity.AddComponent<Camera>();

	Renderer::SetActiveCamera(cameraEntity);

	gridShader.Create("grid.slang");
	showGrid = true;
}

void Editor::BuildPaletteWindow(const Vector<U32>& loadedTextureIds)
{
	Entity paletteBody = UI::CreateWindow({ { 10_vw, 10_vw }, { 300.0f, 200.0f } }, "Tile Palette", true);

	visualPaletteRoot = UI::CreateContainer({ { 0_px, 0_px }, { 100_pw, 100_ph } }, paletteBody);
	collisionPaletteRoot = UI::CreateContainer({ { 0_px, 0_px }, { 100_pw, 100_ph } }, paletteBody);
	collisionPaletteRoot.AddComponent<UIHidden>();

	ScrollAreaEntities visualScroll = UI::CreateScrollArea({ {}, { 100_pw, 100_ph } }, visualPaletteRoot);
	ScrollAreaEntities collisionScroll = UI::CreateScrollArea({ {}, { 100_pw, 100_ph } }, collisionPaletteRoot);

	constexpr F32 TileDisplaySize = 64.0f;
	constexpr F32 Padding = 10.0f;

	tileSelectionHighlight = UI::CreatePanel({ { -1000.0f, -1000.0f }, { TileDisplaySize + 4.0f, TileDisplaySize + 4.0f } }, { 1.0f, 0.8f, 0.2f, 1.0f }, visualScroll.content);
	collisionSelectionHighlight = UI::CreatePanel({ { -1000.0f, -1000.0f }, { TileDisplaySize + 4.0f, TileDisplaySize + 4.0f } }, { 1.0f, 0.8f, 0.2f, 1.0f }, collisionScroll.content);

	F32 currentX = Padding;
	F32 currentY = Padding;

	F32 maxRowWidth = 350.0f;

	for (U32 texID : loadedTextureIds)
	{
		Entity tileBtn = UI::CreatePanel({ { currentX, currentY }, { TileDisplaySize, TileDisplaySize } }, { 1.0f, 1.0f, 1.0f, 1.0f }, visualScroll.content);
		Registry::GetComponent<UIPanel>(tileBtn.Id()).textureId = texID;

		UIInteractable& interact = tileBtn.AddComponent<UIInteractable>();

		interact.OnHoverEnter = [tileBtn]() {
			Registry::GetComponent<UIPanel>(tileBtn.Id()).color = { 0.8f, 0.8f, 0.8f, 1.0f };
		};

		interact.OnHoverExit = [tileBtn]() {
			Registry::GetComponent<UIPanel>(tileBtn.Id()).color = { 1.0f, 1.0f, 1.0f, 1.0f };
		};

		interact.OnClick = [texID, currentX, currentY]() {
			selectedTextureId = texID;
			activeTool = EditorTool::Brush;

			UIRect& highlightRect = Registry::GetComponent<UIRect>(tileSelectionHighlight.Id());
			highlightRect.pos = { currentX - 2.0f, currentY - 2.0f };
		};

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

		Button collisionBtn = UI::CreateButton({ { currentX, currentY }, { TileDisplaySize, TileDisplaySize } }, label, color, collisionScroll.content);

		collisionBtn.interactable.OnClick = [type, currentX, currentY]() {
			activeCollisionType = type;
			activeTool = EditorTool::Brush;

			UIRect& highlightRect = Registry::GetComponent<UIRect>(collisionSelectionHighlight.Id());
			highlightRect.pos = { currentX - 2.0f, currentY - 2.0f };
		};

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

class TempScene : public Scene
{
public:
	void OnStart() override
	{
		LoadTilemap("temp_editor_tilemap.lvl");

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
		Tilemap::SaveSync("temp_editor_tilemap.lvl");
		// TODO: Save ECS state (Registry::Save("temp_editor_ecs.dat"))

		SceneManager::state = EngineState::Playing;

		SceneManager::ChangeScene(std::make_shared<TempScene>());
		// 3. Initialize runtime systems (e.g. wake up physics, call OnStart scripts)
		// PhysicsSystem::InitializeRuntime();
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

		Tilemap::Load("temp_editor_tilemap.lvl");
		Renderer::SetActiveCamera(cameraEntity);
		// TODO: Load ECS state (Registry::Load("temp_editor_ecs.dat"))
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

#endif