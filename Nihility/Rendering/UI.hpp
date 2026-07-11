#pragma once

#include "Defines.hpp"

#include "Shader.hpp"
#include "Buffer.hpp"
#include "Core/Function.hpp"
#include "Core/Containers.hpp"
#include "Components/Registry.hpp"
#include "Resources/Resources.hpp"

#include <glm/glm.hpp>

#undef CreateWindow

enum class NH_API TextAlignment
{
	Left,
	Center,
	Right
};

enum class UIDrawType
{
	None,
	Panel,
	Text
};

struct NH_API UIRect
{
	glm::vec2 position{ 0.0f, 0.0f };
	glm::vec2 size{ 100.0f, 100.0f };
	glm::vec2 anchor{ 0.0f, 0.0f };
	U32 zIndex = 0;
};

struct NH_API UIHierarchy
{
	Entity parent;
	Vector<Entity> children;
};

struct NH_API UIPanel
{
	glm::vec4 color{ 0.2f, 0.2f, 0.2f, 1.0f };
	U32 textureId = U32_MAX;
	F32 cornerRadius = 0.0f;
};

struct NH_API UIInteractable
{
	bool isHovered = false;
	bool isPressed = false;

	Function<void()> OnClick = nullptr;
	Function<void()> OnHoverEnter = nullptr;
	Function<void()> OnHoverExit = nullptr;
};

struct NH_API UIWindow
{
	F32 titleBarHeight = 24.0f;
	glm::vec2 dragOffset{ 0.0f, 0.0f };
	bool isDragging = false;

	U32 bodyEntity = U32_MAX;
};

struct NH_API UIResizable
{
	F32 edgeThickness = 6.0f;
	glm::vec2 minSize{ 50.0f, 50.0f };
	glm::vec2 maxSize{ FLT_MAX, FLT_MAX };

	bool isDragging = false;
	bool draggingRight = false;
	bool draggingBottom = false;
	bool draggingLeft = false;
	bool draggingTop = false;
};

struct NH_API UIScrollArea
{
	U32 contentEntity = U32_MAX;
	glm::vec2 scrollOffset{ 0.0f, 0.0f };
	F32 scrollSpeed = 30.0f;
	F32 padding = 10.0f;
};

struct NH_API ScrollAreaEntities
{
	Entity viewport;
	Entity content;
};

struct NH_API UIClipMask {};

struct NH_API UIFillParent
{
	F32 padding = 0.0f;
};

struct NH_API UIProportionalSize
{
	glm::vec2 proportions{ 1.0f, 1.0f };
};

struct NH_API UIText
{
	String text;
	std::shared_ptr<Font> font;
	glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	F32 fontSize = 24.0f;
	F32 boldness = 0.5f;

	TextAlignment alignment = TextAlignment::Left;
};

struct NH_API UITextInput
{
	static constexpr U32 MaxLength = 256;

	String text;
	String hintText = "Enter Text";
	U32 textEntity = U32_MAX;
	U32 caretEntity = U32_MAX;
	U32 caretIndex = 0;
	F32 scrollOffset = 0.0f;
	bool isFocused = false;

	enum class InputType { Text, Integer, Float };
	InputType type = InputType::Text;
};

struct Scissor
{
	glm::uvec2 extent;
	glm::ivec2 offset;
};

struct UIDrawCmd
{
	UIDrawType type = UIDrawType::None;
	U32 indexCount = 0;
	U32 indexOffset = 0;
	Scissor scissor;
};

struct UIVertex
{
	glm::vec2 position;
	glm::vec2 uv;
	glm::vec4 color;
	U32 textureIndex = U32_MAX;
};

struct TextVertex
{
	glm::vec2 position;
	glm::vec2 uv;
	glm::vec4 color;
	F32 boldness;
	U32 textureIndex;
};

struct VkCommandBuffer_T;

class NH_API UI
{
public:
	static Entity CreateContainer(glm::vec2 localPos, glm::vec2 size, glm::vec2 anchor = { 0.0f, 0.0f }, Entity parent = {});
	static Entity CreatePanel(glm::vec2 localPos, glm::vec2 size, glm::vec4 color, glm::vec2 anchor = { 0.0f, 0.0f }, Entity parent = {});
	static Entity CreateText(const String& text, glm::vec2 localPos, F32 fontSize, glm::vec4 color, glm::vec2 anchor = { 0.0f, 0.0f }, Entity parent = {});
	static Entity CreateTextInput(glm::vec2 localPos, glm::vec2 size, glm::vec2 anchor = { 0.0f, 0.0f }, Entity parent = {});
	static Entity CreateButton(const String& text, glm::vec2 localPos, glm::vec2 size, glm::vec2 anchor = { 0.0f, 0.0f }, Entity parent = {});
	static Entity CreateWindow(const String& title, glm::vec2 pos, glm::vec2 size, bool resizable = false);
	static ScrollAreaEntities CreateScrollArea(glm::vec2 localPos, glm::vec2 size, glm::vec2 anchor = { 0.0f, 0.0f }, Entity parent = {});

	static glm::vec2 GetAbsoluteUIPosition(U32 entityId);
	static Scissor GetAbsoluteScissor(U32 entityId);

	static glm::vec2 GetVirtualMousePosition();
	static glm::vec2 GetVirtualMouseDelta();

	static std::shared_ptr<Font> GetFont();

private:
	static bool Initialize();
	static void Shutdown();
	static void Update();

	static void UpdateInput();
	static void UpdateLayouts();
	static void UpdateVisuals();

	static void ProcessInteractable(U32 id);
	static void ProcessScrollArea(U32 id);
	static void ProcessTextInput(U32 id);
	static void ProcessResizable(U32 id);
	static void ProcessWindow(U32 id);

	static void HandleCursor(const UIResizable& resizable, bool onRightEdge, bool onLeftEdge, bool onBottomEdge, bool onTopEdge);
	static void GenerateTextData(const glm::vec2& absPos, const glm::mat4& uiProjection, const UIText& textComp, Vector<TextVertex>& outVertices, Vector<U32>& outIndices, UIDrawCmd& command);
	static F32 GetTextWidth(const UIText& textComp);
	static F32 GetTextWidthUpToIndex(const UIText& textComp, U32 stopIndex);
	static U32 CalculateCursorIndexFromMouse(const UIText& textComp, F32 localMouseX);
	static void BringWindowToFront(U32 windowId);

	static void Render(VkCommandBuffer_T* cmd);

	static void AttachToParent(Entity child, Entity parent);

	static constexpr U32 MaxPanels = 1000;
	static constexpr U32 MaxCharacters = 1000;
	static constexpr F32 WindowBorderWidth = 2.0f;

	static U32 currentGlobalZ;

	static Shader uiShader;
	static Buffer uiVertexBuffers[MaxFramesInFlight];
	static Buffer uiIndexBuffers[MaxFramesInFlight];

	static Shader textShader;
	static Buffer textVertexBuffers[MaxFramesInFlight];
	static Buffer textIndexBuffers[MaxFramesInFlight];

	static Vector<UIDrawCmd> drawCommands[MaxFramesInFlight];

	static std::shared_ptr<Font> font;

	static U32 focusedEntity;
	static U32 hoveredEntity;
	static U32 activeEntity;
	static bool cursorChanged;

	friend class Editor;
	friend class Nihility;
	friend class Renderer;
	STATIC_CLASS(UI);
};