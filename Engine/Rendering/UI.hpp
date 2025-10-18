#pragma once

#include "Defines.hpp"

#include "Resources/Material.hpp"
#include "Resources/Font.hpp"
#include "Math/Math.hpp"
#include "Core/Events.hpp"

struct NH_API Element
{
	Event<Element&> OnHover;
	Event<Element&> OnExit;
	Event<Element&> OnClick;
	Event<Element&> OnDrag;
	Event<Element&> OnRelease;
	Event<Element&> OnScroll;

	void SetPosition(const Vector2& position);
	void SetRotation(const Quaternion2& rotation);
	void SetScale(const Vector2& scale);
	void SetColor(const Vector4& color);

private:
	Element(U32 index);
	~Element();
	void Destroy();

	U32 index = U32_MAX;
	bool hovered = false;

	friend class UI;
};

struct NH_API ElementRef
{
	ElementRef();
	ElementRef(NullPointer);
	ElementRef(U32 elementId);
	void Destroy();

	ElementRef(const ElementRef& other);
	ElementRef(ElementRef&& other) noexcept;
	ElementRef& operator=(NullPointer);
	ElementRef& operator=(const ElementRef& other);
	ElementRef& operator=(ElementRef&& other) noexcept;
	~ElementRef();

	Element* Get();
	const Element* Get() const;
	Element* operator->();
	const Element* operator->() const;
	Element& operator*();
	const Element& operator*() const;
	operator Element* ();
	operator const Element* () const;

	bool operator==(const ElementRef& other) const;

	bool Valid() const;
	operator bool() const;

private:
	U32 elementId = U32_MAX;
};

struct NH_API ElementInfo
{
	Vector4 area = { 0.0f, 0.0f, 1.0f, 1.0f };
	Quaternion2 rotation = Quaternion2::Identity;
	Vector4 color = Vector4::One;
	ResourceRef<Texture> texture = nullptr;
	Vector4 textureArea = { 0.0f, 0.0f, 1.0f, 1.0f };

	ElementRef parent = nullptr;

	bool ignore = false;
	bool enabled = true;
};

struct NH_API Text
{
private:
	Text(const Vector2& startPosition, U32 index, U32 count) : startPosition(startPosition), index(index), count(count) {};
	Text() {};

	Vector2 startPosition;
	U32 index;
	U32 count;

	friend class UI;
};

struct CommandBuffer;

class NH_API UI
{
	struct UIVertex
	{
		Vector2 position = Vector2::Zero;
		Vector2 texcoord = Vector2::Zero;
	};

	struct UIInstance
	{
		Vector2 position = Vector2::Zero;
		Vector2 scale = Vector2::One;
		Quaternion2 rotation = Quaternion2::Identity;
		Vector4 instColor = Vector4::One;
		Vector2 instTexcoord = Vector2::Zero;
		Vector2 instTexcoordScale = Vector2::One;
		U32 textureIndex = 0;
	};

	struct TextVertex
	{
		Vector2 position = Vector2::Zero;
		Vector2 texcoord = Vector2::Zero;
	};

	struct TextInstance
	{
		Vector2 position = Vector2::Zero;
		Vector2 texcoord = Vector2::Zero;
		Vector4 fgColor = Vector4::One;
		F32 scale = 0.0f;
		U32 textureIndex = 0;
	};

public:
	static ElementRef CreateElement(const ElementInfo& info);
	static Text CreateText(const ElementInfo& info, const String& text, F32 scale);
	static bool ChangeText(const Text& text, const String& newText);
	static void DestroyText(const Text& text);

private:
	static bool Initialize();
	static void Shutdown();

	static void Update();
	static void Render(CommandBuffer commandBuffer);

	static Material uiMaterial;
	static Shader uiVertexShader;
	static Shader uiFragmentShader;
	static Vector<UIInstance> instances;
	static Vector<Element> elements;

	static Material textMaterial;
	static Shader textVertexShader;
	static Shader textFragmentShader;
	static Vector<TextInstance> textInstances;
	static ResourceRef<Font> font;

	static F32 textWidth;
	static F32 textHeight;
	static Vector2 textPosition;
	static Vector2 textPadding;

#ifdef NH_DEBUG
	static Text fpsText;
#endif

	STATIC_CLASS(UI);
	friend class Engine;
	friend class Renderer;
	friend struct Element;
	friend struct ElementRef;
};