#include "Editor.hpp"

#ifdef NH_DEBUG

#include "UI.hpp"
#include "Renderer.hpp"

#include "Platform/Input.hpp"
#include "Core/Logger.hpp"

Entity Editor::viewportPanel;
Entity Editor::paletteWindow;

EditorTool Editor::activeTool = EditorTool::Brush;
TileLayer Editor::activeLayer = TileLayer::Collision;
U32 Editor::selectedTextureId = 0;

Entity Editor::cameraEntity;

Vector<std::shared_ptr<Texture>> Editor::loadedTileTextures;

void Editor::Initialize()
{
	Logger::Trace("Initializing Level Editor...");

	Entity editorBg = UI::CreatePanel({ 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.15f, 0.15f, 0.15f, 1.0f });
	editorBg.AddComponent<UIFillParent>();

	viewportPanel = UI::CreatePanel({ 0.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.15f, 0.01f }, editorBg);
	viewportPanel.AddComponent<UIResizable>();
	UIProportionalSize& prop = viewportPanel.AddComponent<UIProportionalSize>();
	prop.proportions = { 0.75f, 0.6f };

	Registry::GetComponent<UIPanel>(viewportPanel.Id()).textureId = Renderer::viewportTarget.Id();

	//paletteWindow = UI::CreateWindow("Tile Palette", { 10.0f, 10.0f }, { 200.0f, 400.0f }, true);
	//ScrollAreaEntities scrollData = UI::CreateScrollArea({ 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f }, paletteWindow);

	cameraEntity = Registry::CreateEntity({ 0.0f, 0.0f });
	cameraEntity.AddComponent<Camera>();

	// TODO: Iterate over your loaded textures and generate UI::CreatePanel buttons inside scrollData.content
}

void Editor::Shutdown()
{
	Logger::Trace("Shutting Down Level Editor...");
}

void Editor::Update()
{
	Transform2D& camTrans = Registry::GetTransform(cameraEntity.Id());

	if (Input::ButtonDragging(ButtonCode::MiddleMouse))
	{
		glm::vec2 delta = Input::MouseDelta();
		camTrans.position -= delta;
	}

	if (UI::hoveredEntity != U32_MAX && Registry::HasComponent<UIWindow>(UI::hoveredEntity)) { return; }

	if (Input::ButtonDown(ButtonCode::LeftMouse))
	{
		glm::vec2 uiMouse = UI::GetVirtualMousePosition();
		UIRect& vpRect = Registry::GetComponent<UIRect>(viewportPanel.Id());
		glm::vec2 vpPos = UI::GetAbsoluteUIPosition(viewportPanel.Id());
		glm::vec4 area = Renderer::RenderArea();

		if (uiMouse.x >= vpPos.x + area.x && uiMouse.x <= vpPos.x + vpRect.size.x - area.x &&
			uiMouse.y >= vpPos.y + area.y && uiMouse.y <= vpPos.y + vpRect.size.y - area.y)
		{
			glm::vec4 area = Renderer::RenderArea();
			glm::vec2 localViewportMouse = uiMouse - vpPos - glm::vec2{ area.x, area.y } - glm::vec2{ area.z, area.w } / 2.0f;
			glm::vec2 normalizedMouse = localViewportMouse / glm::vec2{ area.z, area.w };

			glm::vec2 worldMouse = normalizedMouse * glm::vec2{ 1920.0f, 1080.0f } + camTrans.position;

			I32 gridX = (I32)glm::floor(worldMouse.x / Tilemap::TileSize);
			I32 gridY = (I32)glm::floor(worldMouse.y / Tilemap::TileSize);

			I32 chunkX = (I32)glm::floor((F32)gridX / (F32)Tilemap::ChunkSize);
			I32 chunkY = (I32)glm::floor((F32)gridY / (F32)Tilemap::ChunkSize);

			U32 localX = (U32)(((gridX % Tilemap::ChunkSize) + Tilemap::ChunkSize) % Tilemap::ChunkSize);
			U32 localY = (U32)(((gridY % Tilemap::ChunkSize) + Tilemap::ChunkSize) % Tilemap::ChunkSize);

			Entity targetChunk = Tilemap::GetOrCreateChunk(chunkX, chunkY);
			TilemapChunk& chunkData = Registry::GetComponent<TilemapChunk>(targetChunk.Id());

			Tile& targetTile = chunkData.layers[(U32)activeLayer][localY * Tilemap::ChunkSize + localX];

			if (activeTool == EditorTool::Brush)
			{
				if (targetTile.textureId != selectedTextureId)
				{
					targetTile.textureId = selectedTextureId;
					chunkData.isDirty = true;
				}
			}
			else if (activeTool == EditorTool::Eraser)
			{
				if (targetTile.textureId != U32_MAX)
				{
					targetTile.textureId = U32_MAX;
					chunkData.isDirty = true;
				}
			}
		}
	}
}

#endif