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

enum class UIUnit : U8
{
	Pixel,			// Absolute physical pixels
	Percent,		// 1 percent of the parent's matching axis
	ParentWidth,	// 1 percent of the parent's width
	ParentHeight,	// 1 percent of the parent's height
	ParentMin,		// 1 percent of the parent's smaller dimension
	ParentMax,		// 1 percent of the parent's larger dimension
	ViewportWidth,	// 1 percent of the OS window's width
	ViewportHeight,	// 1 percent of the OS window's height
	ViewportMin,	// 1 percent of the OS window's smaller dimension
	ViewportMax,	// 1 percent of the OS window's larger dimension
	Inch,			// Physical screen inches
	Centimeters,	// Physical screen centimeters
	Millimeters,	// Physical screen millimeters
	Em,				// Relative to the parent element's font size
	Rem,			// Relative to the root OS window's font size
};

struct UIValue
{
	constexpr UIValue() : value(0.0f), unit(UIUnit::Pixel) {}
	constexpr UIValue(F32 value) : value(value), unit(UIUnit::Pixel) {}
	constexpr UIValue(F32 value, UIUnit unit) : value(value), unit(unit) {}

	F32 value = 0.0f;
	UIUnit unit = UIUnit::Pixel;
};

constexpr UIValue operator"" _px(LF64 value) { return { (F32)value, UIUnit::Pixel }; } // Absolute physical pixels
constexpr UIValue operator"" _per(LF64 value) { return { (F32)value, UIUnit::Percent }; } // 1 percent of the parent's matching axis
constexpr UIValue operator"" _pw(LF64 value) { return { (F32)value, UIUnit::ParentWidth }; } // 1 percent of the parent's width
constexpr UIValue operator"" _ph(LF64 value) { return { (F32)value, UIUnit::ParentHeight }; } // 1 percent of the parent's height
constexpr UIValue operator"" _pmin(LF64 value) { return { (F32)value, UIUnit::ParentMin }; } // 1 percent of the parent's smaller dimension
constexpr UIValue operator"" _pmax(LF64 value) { return { (F32)value, UIUnit::ParentMax }; } // 1 percent of the parent's larger dimension
constexpr UIValue operator"" _vw(LF64 value) { return { (F32)value, UIUnit::ViewportWidth }; } // 1 percent of the OS window's width
constexpr UIValue operator"" _vh(LF64 value) { return { (F32)value, UIUnit::ViewportHeight }; } // 1 percent of the OS window's height
constexpr UIValue operator"" _vmin(LF64 value) { return { (F32)value, UIUnit::ViewportMin }; } // 1 percent of the OS window's smaller dimension
constexpr UIValue operator"" _vmax(LF64 value) { return { (F32)value, UIUnit::ViewportMax }; } // 1 percent of the OS window's larger dimension
constexpr UIValue operator"" _in(LF64 value) { return { (F32)value, UIUnit::Inch }; } // Physical screen inches
constexpr UIValue operator"" _cm(LF64 value) { return { (F32)value, UIUnit::Centimeters }; } // Physical screen centimeters
constexpr UIValue operator"" _mm(LF64 value) { return { (F32)value, UIUnit::Millimeters }; } // Physical screen millimeters
constexpr UIValue operator"" _em(LF64 value) { return { (F32)value, UIUnit::Em }; } // Relative to the parent element's font size
constexpr UIValue operator"" _rem(LF64 value) { return { (F32)value, UIUnit::Rem }; } // Relative to the root OS window's font size

constexpr UIValue operator"" _px(U64 value) { return { (F32)value, UIUnit::Pixel }; } // Absolute physical pixels
constexpr UIValue operator"" _per(U64 value) { return { (F32)value, UIUnit::Percent }; } // 1 percent of the parent's matching axis
constexpr UIValue operator"" _pw(U64 value) { return { (F32)value, UIUnit::ParentWidth }; } // 1 percent of the parent's width
constexpr UIValue operator"" _ph(U64 value) { return { (F32)value, UIUnit::ParentHeight }; } // 1 percent of the parent's height
constexpr UIValue operator"" _pmin(U64 value) { return { (F32)value, UIUnit::ParentMin }; } // 1 percent of the parent's smaller dimension
constexpr UIValue operator"" _pmax(U64 value) { return { (F32)value, UIUnit::ParentMax }; } // 1 percent of the parent's larger dimension
constexpr UIValue operator"" _vw(U64 value) { return { (F32)value, UIUnit::ViewportWidth }; } // 1 percent of the OS window's width
constexpr UIValue operator"" _vh(U64 value) { return { (F32)value, UIUnit::ViewportHeight }; } // 1 percent of the OS window's height
constexpr UIValue operator"" _vmin(U64 value) { return { (F32)value, UIUnit::ViewportMin }; } // 1 percent of the OS window's smaller dimension
constexpr UIValue operator"" _vmax(U64 value) { return { (F32)value, UIUnit::ViewportMax }; } // 1 percent of the OS window's larger dimension
constexpr UIValue operator"" _in(U64 value) { return { (F32)value, UIUnit::Inch }; } // Physical screen inches
constexpr UIValue operator"" _cm(U64 value) { return { (F32)value, UIUnit::Centimeters }; } // Physical screen centimeters
constexpr UIValue operator"" _mm(U64 value) { return { (F32)value, UIUnit::Millimeters }; } // Physical screen millimeters
constexpr UIValue operator"" _em(U64 value) { return { (F32)value, UIUnit::Em }; } // Relative to the parent element's font size
constexpr UIValue operator"" _rem(U64 value) { return { (F32)value, UIUnit::Rem }; } // Relative to the root OS window's font size

struct NH_API UIVector
{
	UIValue x = 0_px;
	UIValue y = 0_px;
};

struct NH_API UIRectDef
{
	UIVector pos = { 0_px, 0_px };
	UIVector size = { 100_px, 100_px };
	glm::vec2 anchor{ 0.0f, 0.0f };

	UIValue marginTop = 0_px;
	UIValue marginRight = 0_px;
	UIValue marginBottom = 0_px;
	UIValue marginLeft = 0_px;
};

struct NH_API UIRect
{
	UIVector pos = { 0_px, 0_px };
	UIVector size = { 100_px, 100_px };

	UIValue marginTop = 0_px;
	UIValue marginRight = 0_px;
	UIValue marginBottom = 0_px;
	UIValue marginLeft = 0_px;

	glm::vec2 anchor{ 0.0f, 0.0f };

	glm::vec2 resolvedPos = { 0.0f, 0.0f };
	glm::vec2 resolvedSize = { 0.0f, 0.0f };

	bool cascadedHidden = false;
	bool cascadedHitTestInvisible = false;

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

struct NH_API UIIgnoreHitTest {};

struct NH_API UIHidden {};

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

struct NH_API UIText
{
	String text;
	std::shared_ptr<Font> font;
	glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	TextAlignment alignment = TextAlignment::Left;
	F32 boldness = 0.5f;

	UIValue fontSize = 1_rem;

	F32 resolvedFontSize = 16.0f;

	bool wrapText = false;
	bool autoFit = false;
	F32 minAutoFitSize = 8.0f;

	String wrappedText;
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
	static Entity CreateContainer(const UIRectDef& def, Entity parent = {});
	static Entity CreatePanel(const UIRectDef& def, const glm::vec4& color, Entity parent = {});
	static Entity CreateText(const UIRectDef& def, const String& text, UIValue fontSize, bool wrapText = false, bool autoFit = false, const glm::vec4& color = { 0.0f, 0.0f, 0.0f, 1.0f }, Entity parent = {});
	static Entity CreateTextInput(const UIRectDef& def, Entity parent = {});
	static Entity CreateButton(const UIRectDef& def, const String& text, const glm::vec4& color = { 0.3f, 0.3f, 0.3f, 1.0f }, Entity parent = {});
	static Entity CreateWindow(const UIRectDef& def, const String& title, bool resizable = false);
	static ScrollAreaEntities CreateScrollArea(const UIRectDef& def, Entity parent = {});

	static glm::vec2 GetAbsoluteUIPosition(U32 entityId);
	static Scissor GetAbsoluteScissor(U32 entityId);

	static std::shared_ptr<Font> GetFont();

private:
	static bool Initialize();
	static void Shutdown();
	static void Update();

	static void ResolveUnits();
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
	static F32 MeasureTextWidth(std::shared_ptr<Font> font, const String& text, F32 fontSize);
	static String CalculateWrappedText(std::shared_ptr<Font> font, const String& text, F32 fontSize, F32 maxWidth);
	static void BringWindowToFront(U32 windowId);
	static glm::vec2 GetParentSize(U32 id);
	static F32 ResolveUIValue(const UIValue& val, F32 parentAxis, glm::vec2 parentSize, glm::vec2 viewportSize, F32 dpi, F32 parentFontSize);
	static void SetUIValueFromPixels(UIValue& val, F32 targetPixels, F32 parentAxis, glm::vec2 parentSize, glm::vec2 viewportSize, F32 dpi, F32 parentFontSize);

	static void Render(VkCommandBuffer_T* cmd);

	static void AttachToParent(Entity child, Entity parent);

	static constexpr U32 MaxPanels = 1000;
	static constexpr U32 MaxCharacters = 1000;
	static constexpr F32 WindowBorderWidth = 2.0f;
	static constexpr F32 RootFontSize = 16.0f;

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