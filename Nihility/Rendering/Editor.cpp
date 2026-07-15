#include "Editor.hpp"

#ifdef NH_DEBUG

#include "UI.hpp"
#include "Renderer.hpp"
#include "VulkanInclude.hpp"

#include "Platform/Input.hpp"
#include "Core/Logger.hpp"

Entity Editor::viewportPanel;
Entity Editor::selectionHighlight;

EditorTool Editor::activeTool = EditorTool::Brush;
TileLayer Editor::activeLayer = TileLayer::Collision;
U32 Editor::selectedTextureId = U32_MAX;

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
		Entity btn = UI::CreateButton({ { 5.0f, currentY }, { 100.0f, 40.0f } }, name, toolbar);
		UIInteractable& interact = Registry::GetComponent<UIInteractable>(btn.Id());

		interact.OnClick = [tool]() {
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
		Entity btn = UI::CreateButton({ { 5.0f, currentY }, { 120.0f, 40.0f } }, name, toolbar);
		UIInteractable& interact = Registry::GetComponent<UIInteractable>(btn.Id());

		interact.OnClick = [layer]() {
			activeLayer = layer;
		};
		currentY += 50.0f;
	};

	CreateLayerButton("Background", TileLayer::Background);
	CreateLayerButton("Midground", TileLayer::Midground);
	CreateLayerButton("Collision", TileLayer::Collision);
	CreateLayerButton("Foreground", TileLayer::Foreground);
	CreateLayerButton("Logic", TileLayer::Logic);

	currentY += 50.0f;

	Entity saveBtn = UI::CreateButton({ { 5.0f, currentY }, { 100.0f, 40.0f } }, "Save", toolbar);
	Registry::GetComponent<UIInteractable>(saveBtn.Id()).OnClick = []() {
		Tilemap::Save("level_01.lvl");
	};

	currentY += 50.0f;

	Entity loadBtn = UI::CreateButton({ { 5.0f, currentY }, { 100.0f, 40.0f } }, "Load", toolbar);
	Registry::GetComponent<UIInteractable>(loadBtn.Id()).OnClick = []() {
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

	cameraEntity = Registry::CreateEntity({ 0.0f, 0.0f });
	cameraEntity.AddComponent<Camera>();

	gridShader.Create("grid.slang");
	showGrid = true;
}

void Editor::BuildPaletteWindow(const Vector<U32>& loadedTextureIds)
{
	Entity paletteBody = UI::CreateWindow({ { 10_vw, 10_vw }, { 300.0f, 200.0f } }, "Tile Palette", true);

	ScrollAreaEntities scroll = UI::CreateScrollArea({ {}, { 100_pw, 100_ph } }, paletteBody);

	constexpr F32 TileDisplaySize = 64.0f;
	constexpr F32 Padding = 10.0f;

	selectionHighlight = UI::CreatePanel({ { -1000.0f, -1000.0f }, { TileDisplaySize + 4.0f, TileDisplaySize + 4.0f } }, { 1.0f, 0.8f, 0.2f, 1.0f }, scroll.content);

	F32 currentX = Padding;
	F32 currentY = Padding;

	F32 maxRowWidth = 350.0f;

	for (U32 texID : loadedTextureIds)
	{
		Entity tileBtn = UI::CreatePanel({ { currentX, currentY }, { TileDisplaySize, TileDisplaySize } }, { 1.0f, 1.0f, 1.0f, 1.0f }, scroll.content);
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

			UIRect& highlightRect = Registry::GetComponent<UIRect>(selectionHighlight.Id());
			highlightRect.pos = { currentX - 2.0f, currentY - 2.0f };
		};

		currentX += TileDisplaySize + Padding;

		if (currentX + TileDisplaySize > maxRowWidth)
		{
			currentX = Padding;
			currentY += TileDisplaySize + Padding;
		}
	}
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

			if (Input::OnButtonDown(ButtonCode::LeftMouse))
			{
				if (activeTool == EditorTool::Select)
				{
					Tile& tile = Tilemap::GetTileAtGlobal(gridX, gridY, (U32)activeLayer, false);
					if (tile.textureId != U32_MAX)
					{
						selectedTextureId = tile.textureId;
						activeTool = EditorTool::Brush;
					}
				}
				else if (activeTool == EditorTool::Fill && selectedTextureId != U32_MAX)
				{
					Tile& clickedTile = Tilemap::GetTileAtGlobal(gridX, gridY, (U32)activeLayer, false);
					U32 targetTextureId = clickedTile.textureId;

					if (targetTextureId == selectedTextureId) { return; }

					Vector<glm::ivec2> queue;
					queue.push_back({ gridX, gridY });

					U32 iterations = 0;
					constexpr U32 MaxFillLimit = 4096;

					while (!queue.empty() && iterations < MaxFillLimit)
					{
						glm::ivec2 curr = queue.back();
						queue.pop_back();

						Tile& currentTile = Tilemap::GetTileAtGlobal(curr.x, curr.y, (U32)activeLayer, false);

						if (currentTile.textureId == targetTextureId)
						{
							Tile& writeTile = Tilemap::GetTileAtGlobal(curr.x, curr.y, (U32)activeLayer, true);
							writeTile.textureId = selectedTextureId;

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
				if (activeTool == EditorTool::Brush && selectedTextureId != U32_MAX)
				{
					Tile& tile = Tilemap::GetTileAtGlobal(gridX, gridY, (U32)activeLayer, true);
					tile.textureId = selectedTextureId;
				}
				else if (activeTool == EditorTool::Eraser)
				{
					Tile& tile = Tilemap::GetTileAtGlobal(gridX, gridY, (U32)activeLayer, true);
					tile.textureId = U32_MAX;
				}
			}
		}
	}
}

void Editor::RenderGrid(VkCommandBuffer cmd)
{
	if (!showGrid) { return; }

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