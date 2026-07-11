#pragma once

#include "Defines.hpp"

#ifdef NH_DEBUG

#include "Shader.hpp"

#include "Physics/Tilemap.hpp"
#include "Components/Registry.hpp"

enum class EditorTool { Brush, Eraser, Fill, Select };

struct VkCommandBuffer_T;

class NH_API Editor
{
public:


private:
	static void Initialize();
	static void Shutdown();
	static void Update();

	static void BuildPaletteWindow(const Vector<U32>& loadedTextureIds);
	static void RenderGrid(VkCommandBuffer_T* cmd);

	static Entity viewportPanel;
	static Entity selectionHighlight;

	static EditorTool activeTool;
	static TileLayer activeLayer;
	static U32 selectedTextureId;
	
	static Entity cameraEntity;

	static Vector<std::shared_ptr<Texture>> loadedTileTextures;

	static Shader gridShader;
	static bool showGrid;

	friend class Nihility;
	friend class Renderer;

	STATIC_CLASS(Editor);
};

#endif