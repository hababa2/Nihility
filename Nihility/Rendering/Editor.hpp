#pragma once

#include "Defines.hpp"

#ifdef NH_DEBUG

#include "Physics/Tilemap.hpp"
#include "Components/Registry.hpp"

enum class EditorTool { Brush, Eraser, Fill, Select };

class NH_API Editor
{
public:


private:
	static void Initialize();
	static void Shutdown();
	static void Update();

	static Entity viewportPanel;
	static Entity paletteWindow;

	static EditorTool activeTool;
	static TileLayer activeLayer;
	static U32 selectedTextureId;
	
	static Entity cameraEntity;

	static Vector<std::shared_ptr<Texture>> loadedTileTextures;

	friend class Nihility;
	friend class Renderer;

	STATIC_CLASS(Editor);
};

#endif