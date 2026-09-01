#pragma once

#include "Defines.hpp"

#include "Shader.hpp"
#include "Buffer.hpp"
#include "Core/Color.hpp"
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

	constexpr UIValue& operator-() { value = -value; return *this; }

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

	void Serialize(DataWriter& writer) const
	{
		writer.Write(pos);
		writer.Write(size);
		writer.Write(marginTop);
		writer.Write(marginRight);
		writer.Write(marginBottom);
		writer.Write(marginLeft);
		writer.Write(anchor);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(pos);
		reader.Read(size);
		reader.Read(marginTop);
		reader.Read(marginRight);
		reader.Read(marginBottom);
		reader.Read(marginLeft);
		reader.Read(anchor);
	}
};

struct NH_API UIHierarchy
{
	Entity parent;
	Vector<Entity> children;

	void Serialize(DataWriter& writer) const
	{
		writer.Write(parent);
		writer.Write(children.size());
		writer.Write(children.data(), children.size() * sizeof(Entity));
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(parent);
		U64 size = 0;
		reader.Read(size);
		children.resize(size);
		reader.Read(children.data(), children.size() * sizeof(Entity));
	}
};

struct NH_API UIPanel
{
	Color color{ 0.2f, 0.2f, 0.2f, 1.0f };
	U32 textureId = U32_MAX;
	F32 cornerRadius = 0.0f;

	void Serialize(DataWriter& writer) const
	{
		writer.Write(color);
		writer.Write(textureId);
		writer.Write(cornerRadius);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(color);
		reader.Read(textureId);
		reader.Read(cornerRadius);
	}
};

struct NH_API UIInteractable
{
	bool isHovered = false;
	bool isPressed = false;

	String onClickName = "";
	String onHoverEnterName = "";
	String onHoverExitName = "";

	void Serialize(DataWriter& writer) const
	{
		writer.Write(onClickName);
		writer.Write(onHoverEnterName);
		writer.Write(onHoverExitName);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(onClickName);
		reader.Read(onHoverEnterName);
		reader.Read(onHoverExitName);
	}
};

struct NH_API Button
{
	Entity entity;
	UIInteractable& interactable;
};

struct NH_API UIIgnoreHitTest
{
	void Serialize(DataWriter& writer) const { }
	void Deserialize(DataReader& reader) { }
};

struct NH_API UIHidden
{
	void Serialize(DataWriter& writer) const {}
	void Deserialize(DataReader& reader) {}
};

struct NH_API UIWindow
{
	F32 titleBarHeight = 24.0f;
	glm::vec2 dragOffset{ 0.0f, 0.0f };
	bool isDragging = false;

	U32 bodyEntity = U32_MAX;

	void Serialize(DataWriter& writer) const
	{
		writer.Write(titleBarHeight);
		writer.Write(bodyEntity);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(titleBarHeight);
		reader.Read(bodyEntity);
	}
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

	void Serialize(DataWriter& writer) const
	{
		writer.Write(edgeThickness);
		writer.Write(minSize);
		writer.Write(maxSize);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(edgeThickness);
		reader.Read(minSize);
		reader.Read(maxSize);
	}
};

struct NH_API UIScrollArea
{
	U32 contentEntity = U32_MAX;
	glm::vec2 scrollOffset{ 0.0f, 0.0f };
	F32 scrollSpeed = 30.0f;
	F32 padding = 10.0f;

	void Serialize(DataWriter& writer) const
	{
		writer.Write(contentEntity);
		writer.Write(scrollOffset);
		writer.Write(scrollSpeed);
		writer.Write(padding);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(contentEntity);
		reader.Read(scrollOffset);
		reader.Read(scrollSpeed);
		reader.Read(padding);
	}
};

struct NH_API ScrollAreaEntities
{
	Entity viewport;
	Entity content;
};

struct NH_API UIClipMask
{
	void Serialize(DataWriter& writer) const {}
	void Deserialize(DataReader& reader) {}
};

struct NH_API UIText
{
	String text = "";
	std::shared_ptr<Font> font;
	Color color{ 1.0f, 1.0f, 1.0f, 1.0f };
	TextAlignment alignment = TextAlignment::Left;
	F32 boldness = 0.5f;

	UIValue fontSize = 1_rem;

	F32 resolvedFontSize = 16.0f;

	bool wrapText = false;
	bool autoFit = false;
	F32 minAutoFitSize = 8.0f;

	String wrappedText;

	void Serialize(DataWriter& writer) const
	{
		writer.Write(text);
		writer.Write(font->Name());
		writer.Write(color);
		writer.Write(alignment);
		writer.Write(boldness);
		writer.Write(fontSize);
		writer.Write(wrapText);
		writer.Write(autoFit);
		writer.Write(minAutoFitSize);
	}

	void Deserialize(DataReader& reader)
	{
		text = reader.ReadString();

		WString fontName = reader.ReadString<CW>();
		font = Resources::Load<Font>(fontName);

		reader.Read(color);
		reader.Read(alignment);
		reader.Read(boldness);
		reader.Read(fontSize);
		reader.Read(wrapText);
		reader.Read(autoFit);
		reader.Read(minAutoFitSize);
	}
};

struct NH_API UITextInput
{
	static constexpr U32 MaxLength = 256;

	String text = "";
	String hintText = "Enter Text";
	std::shared_ptr<Font> font;
	U32 textEntity = U32_MAX;
	U32 caretEntity = U32_MAX;
	U32 caretIndex = 0;
	F32 scrollOffset = 0.0f;
	bool isFocused = false;

	enum class InputType { Text, Integer, Float };
	InputType type = InputType::Text;

	void Serialize(DataWriter& writer) const
	{
		writer.Write(hintText);
		writer.Write(font->Name());
		writer.Write(textEntity);
		writer.Write(caretEntity);
		writer.Write(caretIndex);
		writer.Write(type);
	}

	void Deserialize(DataReader& reader)
	{
		hintText = reader.ReadString();

		WString fontName = reader.ReadString<CW>();
		font = Resources::Load<Font>(fontName);

		reader.Read(textEntity);
		reader.Read(caretEntity);
		reader.Read(caretIndex);
		reader.Read(type);
	}
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
	static Entity CreatePanel(const UIRectDef& def, const Color& color, Entity parent = {});
	static Entity CreateText(const UIRectDef& def, const String& text, UIValue fontSize, bool wrapText = false, bool autoFit = false, const Color& color = { 0.0f, 0.0f, 0.0f, 1.0f }, Entity parent = {});
	static Entity CreateTextInput(const UIRectDef& def, Entity parent = {}, bool noSerialization = false);
	static Button CreateButton(const UIRectDef& def, const String& text, const Color& color = { 0.3f, 0.3f, 0.3f, 1.0f }, Entity parent = {}, bool noSerialization = false);
	static Entity CreateWindow(const UIRectDef& def, const String& title, bool resizable = false, bool noSerialization = false);
	static ScrollAreaEntities CreateScrollArea(const UIRectDef& def, Entity parent = {}, bool noSerialization = false);

	static void DestroyChildren(Entity parent);

	static void RegisterAction(const String& name, Function<void()> callback);

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

	static void TriggerAction(const String& name);

	static void Render(VkCommandBuffer_T* cmd);

	static void AttachToParent(Entity child, Entity parent);

	static glm::vec2 WindowSize();

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

	static Hashmap<String, Function<void()>> registeredActions;

	friend class Editor;
	friend class Nihility;
	friend class Renderer;
	STATIC_CLASS(UI);
};