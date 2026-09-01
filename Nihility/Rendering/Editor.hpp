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
	static void Play();
	static void Pause();
	static void Stop();

private:
	static void Initialize();
	static void Shutdown();
	static void Update();

	static void BuildPaletteWindow(const Vector<U32>& loadedTextureIds);
	static void RenderGrid(VkCommandBuffer_T* cmd);

	static void SaveLevel(const String& levelName);
	static void LoadLevel(const String& levelName);
	static void DeleteLevel(const String& levelName);

	static Entity viewportPanel;
	static Entity tileSelectionHighlight;
	static Entity collisionSelectionHighlight;
	static Entity visualPaletteRoot;
	static Entity collisionPaletteRoot;
	static Entity saveMenuRoot;
	static Entity loadMenuRoot;

	static EditorTool activeTool;
	static TileLayer activeLayer;
	static U32 selectedTextureId;
	static CollisionType activeCollisionType;
	static String currentLevel;

	static Entity cameraEntity;

	static Vector<std::shared_ptr<Texture>> loadedTileTextures;

	static Shader gridShader;
	static bool showGrid;

	friend class Nihility;
	friend class Renderer;
	friend struct TempScene;

	STATIC_CLASS(Editor);
};

#endif