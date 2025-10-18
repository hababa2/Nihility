#include "UI.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"

#include "Resources/Resources.hpp"
#include "Platform/Input.hpp"
#include "Math/Random.hpp"

Material UI::uiMaterial;
Shader UI::uiVertexShader;
Shader UI::uiFragmentShader;
Vector<UI::UIInstance> UI::instances;
Vector<Element> UI::elements;

Material UI::textMaterial;
Shader UI::textVertexShader;
Shader UI::textFragmentShader;
Vector<UI::TextInstance> UI::textInstances;
ResourceRef<Font> UI::font;

F32 UI::textWidth;
F32 UI::textHeight;
Vector2 UI::textPosition;
Vector2 UI::textPadding;

#ifdef NH_DEBUG
Text UI::fpsText;
#endif

Element::Element(U32 index) : index(index) {}

Element::~Element()
{
	Destroy();
}

void Element::Destroy()
{
	OnHover.Destroy();
	OnExit.Destroy();
	OnClick.Destroy();
	OnDrag.Destroy();
	OnRelease.Destroy();
	OnScroll.Destroy();
}

void Element::SetPosition(const Vector2& position)
{
	UI::instances[index].position = position * 2.0f - Vector2::One;
}

void Element::SetRotation(const Quaternion2& rotation)
{
	UI::instances[index].rotation = rotation;
}

void Element::SetScale(const Vector2& scale)
{
	UI::instances[index].scale = scale * 2.0f;
}

void Element::SetColor(const Vector4& color)
{
	UI::instances[index].instColor = color;
}

ElementRef::ElementRef() {}

ElementRef::ElementRef(NullPointer) {}

ElementRef::ElementRef(U32 elementId) : elementId(elementId) {}

void ElementRef::Destroy()
{
	elementId = U32_MAX;
}

ElementRef::ElementRef::ElementRef(const ElementRef& other) : elementId(other.elementId) {}

ElementRef::ElementRef::ElementRef(ElementRef&& other) noexcept : elementId(other.elementId)
{
	other.elementId = U32_MAX;
}

ElementRef& ElementRef::operator=(NullPointer)
{
	elementId = U32_MAX;

	return *this;
}

ElementRef& ElementRef::operator=(const ElementRef& other)
{
	elementId = other.elementId;

	return *this;
}

ElementRef& ElementRef::operator=(ElementRef&& other) noexcept
{
	elementId = other.elementId;
	other.elementId = U32_MAX;

	return *this;
}

ElementRef::~ElementRef()
{
	elementId = U32_MAX;
}

Element* ElementRef::Get()
{
	return &UI::elements[elementId];
}

const Element* ElementRef::Get() const
{
	return &UI::elements[elementId];
}

Element* ElementRef::operator->()
{
	return &UI::elements[elementId];
}

const Element* ElementRef::operator->() const
{
	return &UI::elements[elementId];
}

Element& ElementRef::operator*()
{
	return UI::elements[elementId];
}

const Element& ElementRef::operator*() const
{
	return UI::elements[elementId];
}

ElementRef::operator Element* ()
{
	return &UI::elements[elementId];
}

ElementRef::operator const Element* () const
{
	return &UI::elements[elementId];
}

bool ElementRef::operator==(const ElementRef& other) const
{
	return elementId == other.elementId;
}

bool ElementRef::Valid() const
{
	return &UI::elements[elementId];
}

ElementRef::operator bool() const
{
	return &UI::elements[elementId];
}

bool UI::Initialize()
{
	PipelineLayout uiPipelineLayout;

	uiPipelineLayout.Create({ Resources::DummyDescriptorSet(), Resources::BindlessTexturesDescriptorSet() });

	uiVertexShader.Create("shaders/ui.vert.spv", ShaderStage::Vertex);
	uiFragmentShader.Create("shaders/ui.frag.spv", ShaderStage::Fragment);

	Vector<VkVertexInputBindingDescription> inputs = {
		{ 0, sizeof(UIVertex), VK_VERTEX_INPUT_RATE_VERTEX },
		{ 1, sizeof(UIInstance), VK_VERTEX_INPUT_RATE_INSTANCE}
	};

	Vector<VkVertexInputAttributeDescription> attributes = {
		{ 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(UIVertex, position) },
		{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(UIVertex, texcoord) },

		{ 2, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(UIInstance, position) },
		{ 3, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(UIInstance, scale) },
		{ 4, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(UIInstance, rotation) },
		{ 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(UIInstance, instColor) },
		{ 6, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(UIInstance, instTexcoord) },
		{ 7, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(UIInstance, instTexcoordScale) },
		{ 8, 1, VK_FORMAT_R32_UINT, offsetof(UIInstance, textureIndex) },
	};

	Pipeline uiPipeline;
	uiPipeline.Create(uiPipelineLayout, { PolygonMode::Fill }, { uiVertexShader, uiFragmentShader }, inputs, attributes);
	uiMaterial.Create(uiPipelineLayout, uiPipeline, { Resources::DummyDescriptorSet(), Resources::BindlessTexturesDescriptorSet() });

	UIVertex vertices[4] = {
		{ { 0.0f, 1.0f }, { 0.0f, 1.0f } },
		{ { 0.0f, 0.0f }, { 0.0f, 0.0f } },
		{ { 1.0f, 1.0f }, { 1.0f, 1.0f } },
		{ { 1.0f, 0.0f }, { 1.0f, 0.0f } }
	};

	U32 indices[6] = { 0, 1, 2, 2, 1, 3 };

	uiMaterial.UploadVertices(vertices, sizeof(UIVertex) * 4, 0);
	uiMaterial.UploadIndices(indices, sizeof(U32) * 6, 0);

	font = Resources::LoadFont("fonts/arial.nhf");
	
	PipelineLayout textPipelineLayout;
	
	textPipelineLayout.Create({ Resources::DummyDescriptorSet(), Resources::BindlessTexturesDescriptorSet() });
	
	textVertexShader.Create("shaders/text.vert.spv", ShaderStage::Vertex);
	textFragmentShader.Create("shaders/text.frag.spv", ShaderStage::Fragment);
	
	Vector<VkVertexInputBindingDescription> textInputs = {
		{ 0, sizeof(TextVertex), VK_VERTEX_INPUT_RATE_VERTEX },
		{ 1, sizeof(TextInstance), VK_VERTEX_INPUT_RATE_INSTANCE}
	};
	
	Vector<VkVertexInputAttributeDescription> textAttributes = {
		{ 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TextVertex, position) },
		{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TextVertex, texcoord) },
	
		{ 2, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(TextInstance, position) },
		{ 3, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(TextInstance, texcoord) },
		{ 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TextInstance, fgColor) },
		{ 5, 1, VK_FORMAT_R32_SFLOAT, offsetof(TextInstance, scale) },
		{ 6, 1, VK_FORMAT_R32_UINT, offsetof(TextInstance, textureIndex) },
	};
	
	Pipeline textPipeline;
	textPipeline.Create(textPipelineLayout, { PolygonMode::Fill, TopologyMode::TriangleList, BindPoint::Graphics, false }, { textVertexShader, textFragmentShader }, textInputs, textAttributes);
	textMaterial.Create(textPipelineLayout, textPipeline, { Resources::DummyDescriptorSet(), Resources::BindlessTexturesDescriptorSet() });
	
	textWidth = font->glyphSize / 1920.0f;
	textHeight = font->glyphSize / 1080.0f;
	
	textPosition = Vector2{ (F32)font->glyphSize, (F32)font->glyphSize } / Vector2{ (F32)font->texture->Width(), (F32)font->texture->Height() };
	textPadding = Vector2::One / Vector2{ (F32)font->texture->Width(), (F32)font->texture->Height() };
	
	TextVertex textVertices[4] = {
		{ { 0.0f,		textHeight },	{ 0.0f,				textPosition.y } },
		{ { 0.0f,		0.0f },			{ 0.0f,				0.0f } },
		{ { textWidth,	textHeight },	{ textPosition.x,	textPosition.y } },
		{ { textWidth,	0.0f },			{ textPosition.x,	0.0f } }
	};
	
	textMaterial.UploadVertices(textVertices, sizeof(TextVertex) * 4, 0);
	textMaterial.UploadIndices(indices, sizeof(U32) * 6, 0);

#ifdef NH_DEBUG
	ElementInfo info{};
	info.area = { 0.9775f, 0.005f, 1.0f, 1.0f };
	info.color = { 0.0f, 1.0f, 0.0f, 1.0f };

	fpsText = CreateText(info, "000", 3.0f);
#endif

	return true;
}

void UI::Shutdown()
{
	uiVertexShader.Destroy();
	uiFragmentShader.Destroy();
	uiMaterial.Destroy();

	textVertexShader.Destroy();
	textFragmentShader.Destroy();
	textMaterial.Destroy();

	elements.Destroy();
	instances.Destroy();
	textInstances.Destroy();
}

void UI::Update()
{
#ifdef NH_DEBUG
	ChangeText(fpsText, { FORMAT, Time::FrameRate() });
#endif

	Vector2 mousePosition = Input::MousePosition() / (Vector2)Renderer::RenderSize() * 2.0f - Vector2::One;

	for (Element& element : elements)
	{
		UIInstance& instance = instances[element.index];

		if (mousePosition.x > instance.position.x && mousePosition.x < instance.position.x + instance.scale.x &&
			mousePosition.y > instance.position.y && mousePosition.y < instance.position.y + instance.scale.y)
		{
			if (!element.hovered && element.OnHover) { element.OnHover(element); }

			element.hovered = true;

			if ((Input::OnButtonDown(ButtonCode::LeftMouse) || Input::OnButtonDown(ButtonCode::RightMouse)) && element.OnClick)
			{
				element.OnClick(element);
			}
		}
		else
		{
			if (element.hovered && element.OnExit) { element.OnExit(element); }

			element.hovered = false;
		}
	}

	if (instances.Size())
	{
		uiMaterial.UploadInstances(instances.Data(), (U32)(instances.Size() * sizeof(UIInstance)), 0);
	}

	if (textInstances.Size())
	{
		textMaterial.UploadInstances(textInstances.Data(), (U32)(textInstances.Size() * sizeof(TextInstance)), 0);
	}
}

void UI::Render(CommandBuffer commandBuffer)
{
	if (instances.Size())
	{
		uiMaterial.Bind(commandBuffer);
	}

	if (textInstances.Size())
	{
		textMaterial.Bind(commandBuffer);
	}
}

ElementRef UI::CreateElement(const ElementInfo& info)
{
	U32 index = (U32)instances.Size();

	UIInstance instance{};
	instance.position = info.area.xy() * 2.0f - Vector2::One;
	instance.scale = (info.area.zw() - info.area.xy()) * 2.0f;
	instance.rotation = info.rotation;
	instance.instColor = info.color;
	instance.instTexcoord = info.textureArea.xy();
	instance.instTexcoordScale = info.textureArea.zw() - info.textureArea.xy();
	instance.textureIndex = info.texture.Handle();

	instances.Push(instance);

	U32 elementId = (U32)elements.Size();
	elements.Push(index);

	return elementId;
}

Text UI::CreateText(const ElementInfo& info, const String& text, F32 scale)
{
	U32 instanceIndex = (U32)textInstances.Size();
	U32 count = 0;

	scale *= 16.0f / (F32)font->glyphSize;

	Vector2 startPosition = info.area.xy() * 2.0f - Vector2::One;
	Vector2 position = startPosition;

	F32 yOffset = textHeight * scale / 4.0f;

	U8 prev = 255;

	for (C8 c : text)
	{
		Glyph& glyph = font->glyphs[c - 32];

		if (c == '\n')
		{
			position.x = startPosition.x;
			position.y += (font->ascent + font->lineGap) * textHeight * scale;
		}
		else if (c != ' ')
		{
			U8 index = c - 32;

			Vector2 texPos = { (F32)(index % 8), (F32)(index / 8) };

			TextInstance instance{};
			instance.position = position - Vector2{ glyph.x * textWidth * scale, -glyph.y * textHeight * scale + yOffset };
			if (prev == 255 || prev == '\n') { instance.position.x -= glyph.leftBearing * textWidth * scale; }
			instance.texcoord = texPos * textPosition + (texPos + Vector2::One) * textPadding;
			instance.fgColor = info.color;
			instance.scale = scale;
			instance.textureIndex = font->texture.Handle();

			textInstances.Push(instance);
			++count;
		}

		position.x += glyph.advance * textWidth * scale;

		if (prev != 255)
		{
			position.x += font->glyphs[prev - 32].kerning[c - 32] * textWidth * scale;
		}

		prev = c;
	}

	return { startPosition, instanceIndex, count };
}

bool UI::ChangeText(const Text& text, const String& newText)
{
	if (newText.Size() > text.count) { return false; }

	Vector2 startPosition = text.startPosition;
	Vector2 position = startPosition;

	F32 scale = textInstances[0].scale;
	F32 yOffset = textHeight * scale / 4.0f;

	U8 prev = 255;
	U32 i = 0;

	for (C8 c : newText)
	{
		Glyph& glyph = font->glyphs[c - 32];

		if (c == '\n')
		{
			position.x = startPosition.x;
			position.y += (font->ascent + font->lineGap) * textHeight * scale;
		}
		else if (c != ' ')
		{
			U8 index = c - 32;

			Vector2 texPos = { (F32)(index % 8), (F32)(index / 8) };

			TextInstance& instance = textInstances[text.index + i];

			instance.position = position - Vector2{ glyph.x * textWidth * scale, -glyph.y * textHeight * scale + yOffset };
			if (prev == 255 || prev == '\n') { instance.position.x -= glyph.leftBearing * textWidth * scale; }
			instance.texcoord = texPos * textPosition + (texPos + Vector2::One) * textPadding;
		}

		position.x += glyph.advance * textWidth * scale;

		if (prev != 255)
		{
			position.x += font->glyphs[prev - 32].kerning[c - 32] * textWidth * scale;
		}

		prev = c;
		++i;
	}

	return true;
}

void UI::DestroyText(const Text& text)
{
	textInstances.Erase(text.index, text.index + text.count);
}