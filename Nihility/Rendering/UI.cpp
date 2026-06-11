#include "UI.hpp"

#include "Nihility.hpp"

#include "Platform/Platform.hpp"
#include "Platform/Input.hpp"
#include "Core/Time.hpp"
#include "Core/Settings.hpp"
#include "Rendering/Renderer.hpp"
#include "Resources/Font.hpp"

#include "Rendering/VulkanInclude.hpp"
#include "vma/vk_mem_alloc.h"
#include "glm/gtc/matrix_transform.hpp"
#include "enkiTS/TaskScheduler.h"

U32 UI::currentGlobalZ = 100;

Shader UI::uiShader;
Buffer UI::uiVertexBuffers[MaxFramesInFlight];
Buffer UI::uiIndexBuffers[MaxFramesInFlight];

Shader UI::textShader;
Buffer UI::textVertexBuffers[MaxFramesInFlight];
Buffer UI::textIndexBuffers[MaxFramesInFlight];

Vector<UIDrawCmd> UI::drawCommands[MaxFramesInFlight];

U32 UI::focusedEntity = U32_MAX;
U32 UI::hoveredEntity = U32_MAX;
U32 UI::activeEntity = U32_MAX;
bool UI::cursorChanged;

bool UI::Initialize()
{
	Logger::Trace("Initializing UI System...");

	uiShader.Create("ui.slang");
	textShader.Create("text.slang");

	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		uiVertexBuffers[i] = Renderer::CreateBuffer(
			sizeof(UIVertex) * 4 * MaxPanels,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		uiIndexBuffers[i] = Renderer::CreateBuffer(
			sizeof(U32) * 6 * MaxPanels,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		textVertexBuffers[i] = Renderer::CreateBuffer(
			sizeof(TextVertex) * 4 * MaxCharacters,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		textIndexBuffers[i] = Renderer::CreateBuffer(
			sizeof(U32) * 6 * MaxCharacters,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);
	}

	Registry::RegisterComponentUpdate("UI", Update);

	return true;
}

void UI::Shutdown()
{
	Logger::Trace("Shutting Down UI System...");

	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		Renderer::DestroyBuffer(uiVertexBuffers[i]);
		Renderer::DestroyBuffer(uiIndexBuffers[i]);
		Renderer::DestroyBuffer(textVertexBuffers[i]);
		Renderer::DestroyBuffer(textIndexBuffers[i]);
	}

	uiShader.Destroy();
	textShader.Destroy();
}

void UI::Update()
{
	UpdateInput();
	UpdateLayouts();
	UpdateVisuals();
}

void UI::UpdateInput()
{
	glm::vec2 mousePos = GetVirtualMousePosition();

	cursorChanged = false;
	hoveredEntity = U32_MAX;
	auto view = Registry::View<UIRect>();

	Vector<U32> hits;
	for (U32 i = 0; i < view.Size(); ++i)
	{
		hits.push_back(view.GetEntity(i));
	}

	std::sort(hits.begin(), hits.end(), [&view](U32 a, U32 b) {
		return std::get<0>(view.Get(a)).zIndex > std::get<0>(view.Get(b)).zIndex;
	});

	for (U32 id : hits)
	{
		auto [rect] = view.Get(id);
		glm::vec2 pos = GetAbsoluteUIPosition(id);
		Scissor scissor = GetAbsoluteScissor(id);

		if (scissor.extent.x == 0 || scissor.extent.y == 0) { continue; }

		bool insideScissor = mousePos.x >= scissor.offset.x &&
			mousePos.x <= scissor.offset.x + scissor.extent.x &&
			mousePos.y >= scissor.offset.y &&
			mousePos.y <= scissor.offset.y + scissor.extent.y;

		if (!insideScissor) { continue; }

		if (mousePos.x >= pos.x && mousePos.x <= pos.x + rect.size.x &&
			mousePos.y >= pos.y && mousePos.y <= pos.y + rect.size.y)
		{
			hoveredEntity = id;
			break;
		}
	}

	if (Input::OnButtonDown(ButtonCode::LeftMouse))
	{
		activeEntity = hoveredEntity;
		focusedEntity = hoveredEntity;

		U32 searchId = activeEntity;
		while (searchId != U32_MAX)
		{
			if (Registry::HasComponent<UIWindow>(searchId))
			{
				BringWindowToFront(searchId);
				break;
			}

			if (Registry::GetSet<UIHierarchy>().Has(searchId))
			{
				searchId = Registry::GetComponent<UIHierarchy>(searchId).parent.Id();
			}
			else { break; }
		}
	}

	auto interactView = Registry::View<UIInteractable>();
	for (U32 i = 0; i < interactView.Size(); ++i) { ProcessInteractable(interactView.GetEntity(i)); }

	auto windowView = Registry::View<UIWindow>();
	for (U32 i = 0; i < windowView.Size(); ++i) { ProcessWindow(windowView.GetEntity(i)); }

	auto resizeView = Registry::View<UIResizable>();
	for (U32 i = 0; i < resizeView.Size(); ++i) { ProcessResizable(resizeView.GetEntity(i)); }

	auto scrollView = Registry::View<UIScrollArea>();
	for (U32 i = 0; i < scrollView.Size(); ++i) { ProcessScrollArea(scrollView.GetEntity(i)); }

	auto textInputView = Registry::View<UITextInput>();
	for (U32 i = 0; i < textInputView.Size(); ++i) { ProcessTextInput(textInputView.GetEntity(i)); }

	if (Input::OnButtonUp(ButtonCode::LeftMouse))
	{
		activeEntity = U32_MAX;
	}

	if (!cursorChanged) { Platform::SetCursorType(CursorType::Arrow); }
}

void UI::UpdateLayouts()
{
	auto winView = Registry::View<UIWindow, UIRect>();

	for (U32 i = 0; i < winView.Size(); ++i)
	{
		U32 id = winView.GetEntity(i);
		if (!winView.Matches(id)) { continue; }

		auto [window, rect] = winView.Get(id);

		if (window.bodyEntity != U32_MAX && Registry::HasComponent<UIRect>(window.bodyEntity))
		{
			UIRect& bodyRect = Registry::GetComponent<UIRect>(window.bodyEntity);

			bodyRect.size.x = rect.size.x - WindowBorderWidth * 2.0f;
			bodyRect.size.y = rect.size.y - window.titleBarHeight - WindowBorderWidth;
		}
	}

	auto scrollView = Registry::View<UIScrollArea, UIRect>();
	for (U32 i = 0; i < scrollView.Size(); ++i)
	{
		U32 id = scrollView.GetEntity(i);
		if (!scrollView.Matches(id)) { continue; }

		auto [scroll, maskRect] = scrollView.Get(id);

		if (scroll.contentEntity != U32_MAX && Registry::HasComponent<UIRect>(scroll.contentEntity))
		{
			UIRect& contentRect = Registry::GetComponent<UIRect>(scroll.contentEntity);

			F32 maxChildY = 0.0f;
			if (Registry::GetSet<UIHierarchy>().Has(scroll.contentEntity))
			{
				for (Entity child : Registry::GetComponent<UIHierarchy>(scroll.contentEntity).children)
				{
					if (Registry::HasComponent<UIRect>(child.Id()))
					{
						UIRect& childRect = Registry::GetComponent<UIRect>(child.Id());
						maxChildY = glm::max(maxChildY, childRect.position.y + childRect.size.y);
					}
				}
			}

			maxChildY += scroll.padding;

			Scissor scissor = GetAbsoluteScissor(id);
			F32 trueVisibleHeight = (F32)scissor.extent.y;

			F32 maxScroll = glm::max(0.0f, maxChildY - trueVisibleHeight);

			scroll.scrollOffset.y = glm::clamp(scroll.scrollOffset.y, -maxScroll, scroll.padding);
			contentRect.position.y = scroll.scrollOffset.y;
		}
	}

	auto inputView = Registry::View<UITextInput, UIRect>();
	for (U32 i = 0; i < inputView.Size(); ++i)
	{
		U32 id = inputView.GetEntity(i);
		if (!inputView.Matches(id)) { continue; }

		auto [input, rootRect] = inputView.Get(id);

		if (input.textEntity != U32_MAX && input.caretEntity != U32_MAX)
		{
			auto& textComp = Registry::GetComponent<UIText>(input.textEntity);
			auto& textRect = Registry::GetComponent<UIRect>(input.textEntity);
			auto& caretRect = Registry::GetComponent<UIRect>(input.caretEntity);
			auto& caretPanel = Registry::GetComponent<UIPanel>(input.caretEntity);
			
			input.caretIndex = glm::clamp(input.caretIndex, 0u, (U32)input.text.length());

			F32 cursorPixelOffset = GetTextWidthUpToIndex(textComp, input.caretIndex);

			F32 leftPadding = 6.0f;
			F32 rightPadding = 12.0f;
			F32 visibleWidth = rootRect.size.x - leftPadding - rightPadding;
			F32 visualCursorX = cursorPixelOffset - input.scrollOffset;

			if (visualCursorX > visibleWidth)
			{
				input.scrollOffset = cursorPixelOffset - visibleWidth;
			}
			else if (visualCursorX < 0.0f)
			{
				input.scrollOffset = cursorPixelOffset;
			}

			F32 totalTextWidth = GetTextWidthUpToIndex(textComp, (U32)textComp.text.length());
			F32 maxScroll = std::max(0.0f, totalTextWidth - visibleWidth);
			input.scrollOffset = std::clamp(input.scrollOffset, 0.0f, maxScroll);

			textRect.position.x = leftPadding - input.scrollOffset;
			caretRect.position.x = textRect.position.x + cursorPixelOffset;

			if (focusedEntity == id)
			{
				F32 blinkRate = 0.53f;
				bool showCaret = fmod(Time::AbsoluteTime(), blinkRate * 2.0) < blinkRate;
				caretPanel.color.a = showCaret ? 1.0f : 0.0f;
			}
			else
			{
				caretPanel.color.a = 0.0f;
			}
		}
	}
}

void UI::ProcessInteractable(U32 id)
{
	UIInteractable& interactable = Registry::GetComponent<UIInteractable>(id);

	bool isCurrentlyHovered = (id == hoveredEntity);
	if (isCurrentlyHovered && !interactable.isHovered)
	{
		interactable.isHovered = true;
		if (interactable.OnHoverEnter) interactable.OnHoverEnter();
	}
	else if (!isCurrentlyHovered && interactable.isHovered)
	{
		interactable.isHovered = false;
		if (interactable.OnHoverExit) interactable.OnHoverExit();
	}

	if (id == activeEntity)
	{
		interactable.isPressed = true;

		if (Input::OnButtonUp(ButtonCode::LeftMouse))
		{
			interactable.isPressed = false;

			if (isCurrentlyHovered && interactable.OnClick)
			{
				interactable.OnClick();
			}
		}
	}
	else
	{
		interactable.isPressed = false;
	}
}

void UI::ProcessScrollArea(U32 id)
{
	UIScrollArea& scroll = Registry::GetComponent<UIScrollArea>(id);

	bool isHovered = false;
	U32 searchId = hoveredEntity;
	while (searchId != U32_MAX)
	{
		if (searchId == id) { isHovered = true; break; }
		if (Registry::GetSet<UIHierarchy>().Has(searchId))
		{
			searchId = Registry::GetComponent<UIHierarchy>(searchId).parent.Id();
		}
		else { break; }
	}

	if (isHovered)
	{
		F32 wheelDelta = (F32)Input::MouseWheelDelta();
		if (wheelDelta != 0.0f)
		{
			scroll.scrollOffset.y += wheelDelta * scroll.scrollSpeed;
		}
	}
}

void UI::ProcessTextInput(U32 id)
{
	UITextInput& input = Registry::GetComponent<UITextInput>(id);
	UIText& text = Registry::GetComponent<UIText>(input.textEntity);

	if (id == hoveredEntity)
	{
		cursorChanged = true;
		Platform::SetCursorType(CursorType::IBeam);
	}

	if (Input::OnButtonDown(ButtonCode::LeftMouse))
	{
		Input::GetAndClearTextInputQueue();

		if (id == hoveredEntity)
		{
			focusedEntity = id;
			input.isFocused = true;

			if (input.textEntity != U32_MAX && Registry::HasComponent<UIText>(input.textEntity))
			{
				glm::vec2 mousePos = GetVirtualMousePosition();
				glm::vec2 textAbsPos = GetAbsoluteUIPosition(input.textEntity);
				F32 localMouseX = glm::max(0.0f, mousePos.x - textAbsPos.x);

				text.text = input.text;
				input.caretIndex = CalculateCursorIndexFromMouse(Registry::GetComponent<UIText>(input.textEntity), localMouseX);
			}
		}
		else if (id == focusedEntity)
		{
			focusedEntity = U32_MAX;
			input.isFocused = false;
		}
	}

	if (id == focusedEntity)
	{
		Vector<C> charQueue = Input::GetAndClearTextInputQueue();
		for (C c : charQueue)
		{
			if (input.text.length() < input.MaxLength)
			{
				input.text.insert(input.text.begin() + input.caretIndex, c);
				input.caretIndex++;
			}
		}

		if (Input::OnButtonDown(ButtonCode::Back) || Input::OnButtonRepeat(ButtonCode::Back))
		{
			if (input.caretIndex > 0)
			{
				input.text.erase(input.text.begin() + (input.caretIndex - 1));
				input.caretIndex--;
			}
		}

		if (Input::OnButtonDown(ButtonCode::Delete) || Input::OnButtonRepeat(ButtonCode::Delete))
		{
			if (input.caretIndex < input.text.length())
			{
				input.text.erase(input.text.begin() + input.caretIndex);
			}
		}

		if (Input::OnButtonDown(ButtonCode::Left) || Input::OnButtonRepeat(ButtonCode::Left))
		{
			if (input.caretIndex > 0) { input.caretIndex--; }
		}

		if (Input::OnButtonDown(ButtonCode::Right) || Input::OnButtonRepeat(ButtonCode::Right))
		{
			if (input.caretIndex < input.text.length()) { input.caretIndex++; }
		}

		if (input.text.empty())
		{
			text.text = input.hintText;
			text.color = { 0.3f, 0.3f, 0.3f, 1.0f };
		}
		else
		{
			text.text = input.text;
			text.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		}

		Input::ConsumeInput();
	}
}

void UI::ProcessResizable(U32 id)
{
	UIResizable& resizable = Registry::GetComponent<UIResizable>(id);
	UIRect& rect = Registry::GetComponent<UIRect>(id);
	glm::vec2 pos = GetAbsoluteUIPosition(id);
	glm::vec2 mousePos = GetVirtualMousePosition();

	bool isInside = (id == hoveredEntity || id == activeEntity);
	if (!isInside) { return; }

	bool onRightEdge = (mousePos.x > (pos.x + rect.size.x - resizable.edgeThickness)) && isInside;
	bool onLeftEdge = (mousePos.x < (pos.x + resizable.edgeThickness)) && isInside;
	bool onBottomEdge = (mousePos.y > (pos.y + rect.size.y - resizable.edgeThickness)) && isInside;
	bool onTopEdge = (mousePos.y < (pos.y + resizable.edgeThickness)) && isInside;

	if (Input::OnButtonDown(ButtonCode::LeftMouse) && (onRightEdge || onLeftEdge || onBottomEdge || onTopEdge))
	{
		resizable.isDragging = true;
		resizable.draggingRight = onRightEdge;
		resizable.draggingLeft = onLeftEdge;
		resizable.draggingBottom = onBottomEdge;
		resizable.draggingTop = onTopEdge;
	}

	HandleCursor(resizable, onRightEdge, onLeftEdge, onBottomEdge, onTopEdge);

	if (resizable.isDragging)
	{
		glm::vec2 delta = GetVirtualMouseDelta();

		if (resizable.draggingRight)
		{
			rect.size.x += delta.x;
		}
		if (resizable.draggingLeft)
		{
			F32 limitDelta = glm::min(delta.x, rect.size.x - resizable.minSize.x);
			rect.position.x += limitDelta;
			rect.size.x -= limitDelta;
		}
		if (resizable.draggingBottom)
		{
			rect.size.y += delta.y;
		}
		if (resizable.draggingTop)
		{
			F32 limitDelta = glm::min(delta.y, rect.size.y - resizable.minSize.y);
			rect.position.y += limitDelta;
			rect.size.y -= limitDelta;
		}

		rect.size = glm::clamp(rect.size, resizable.minSize, resizable.maxSize);

		if (Input::OnButtonUp(ButtonCode::LeftMouse))
		{
			resizable.isDragging = false;
		}
	}
}

void UI::ProcessWindow(U32 id)
{
	UIWindow& window = Registry::GetComponent<UIWindow>(id);
	UIRect& rect = Registry::GetComponent<UIRect>(id);
	glm::vec2 pos = GetAbsoluteUIPosition(id);
	glm::vec2 mousePos = GetVirtualMousePosition();

	F32 resizeOffset = 0.0f;
	if (Registry::HasComponent<UIResizable>(id))
	{
		resizeOffset = Registry::GetComponent<UIResizable>(id).edgeThickness;
	}

	if (Input::OnButtonDown(ButtonCode::LeftMouse) && activeEntity == id)
	{
		if (mousePos.y >= pos.y + resizeOffset && mousePos.y <= pos.y + window.titleBarHeight &&
			mousePos.x >= pos.x + resizeOffset && mousePos.x <= pos.x + rect.size.x - resizeOffset)
		{
			window.isDragging = true;
			window.dragOffset = mousePos - rect.position;
		}
	}

	if (window.isDragging)
	{
		rect.position = mousePos - window.dragOffset;

		if (Input::OnButtonUp(ButtonCode::LeftMouse))
		{
			window.isDragging = false;
		}
	}
}

void UI::HandleCursor(const UIResizable& resizable, bool onRightEdge, bool onLeftEdge, bool onBottomEdge, bool onTopEdge)
{
	cursorChanged |= resizable.isDragging || onRightEdge || onLeftEdge || onBottomEdge || onTopEdge;

	if (resizable.isDragging)
	{
		if (resizable.draggingRight)
		{
			if (resizable.draggingTop)
			{
				Platform::SetCursorType(CursorType::ResizeNESW);
			}
			else if (resizable.draggingBottom)
			{
				Platform::SetCursorType(CursorType::ResizeNWSE);
			}
			else
			{
				Platform::SetCursorType(CursorType::ResizeEW);
			}
		}
		else if (resizable.draggingLeft)
		{
			if (resizable.draggingTop)
			{
				Platform::SetCursorType(CursorType::ResizeNWSE);
			}
			else if (resizable.draggingBottom)
			{
				Platform::SetCursorType(CursorType::ResizeNESW);
			}
			else
			{
				Platform::SetCursorType(CursorType::ResizeEW);
			}
		}
		else if (resizable.draggingBottom)
		{
			Platform::SetCursorType(CursorType::ResizeNS);
		}
		else if (resizable.draggingTop)
		{
			Platform::SetCursorType(CursorType::ResizeNS);
		}
	}
	else
	{
		if (onRightEdge)
		{
			if (onTopEdge)
			{
				Platform::SetCursorType(CursorType::ResizeNESW);
			}
			else if (onBottomEdge)
			{
				Platform::SetCursorType(CursorType::ResizeNWSE);
			}
			else
			{
				Platform::SetCursorType(CursorType::ResizeEW);
			}
		}
		else if (onLeftEdge)
		{
			if (onTopEdge)
			{
				Platform::SetCursorType(CursorType::ResizeNWSE);
			}
			else if (onBottomEdge)
			{
				Platform::SetCursorType(CursorType::ResizeNESW);
			}
			else
			{
				Platform::SetCursorType(CursorType::ResizeEW);
			}
		}
		else if (onBottomEdge)
		{
			Platform::SetCursorType(CursorType::ResizeNS);
		}
		else if (onTopEdge)
		{
			Platform::SetCursorType(CursorType::ResizeNS);
		}
	}
}

void UI::UpdateVisuals()
{
	auto view = Registry::View<UIRect>();
	if (view.Size() == 0) { return; }

	Vector<U32> sortedIds;
	sortedIds.reserve(view.Size());
	for (U32 i = 0; i < view.Size(); ++i)
	{
		if (view.Matches(view.GetEntity(i))) sortedIds.push_back(view.GetEntity(i));
	}

	std::sort(sortedIds.begin(), sortedIds.end(), [&view](U32 a, U32 b) {
		return std::get<0>(view.Get(a)).zIndex < std::get<0>(view.Get(b)).zIndex;
	});

	U32 frame = Renderer::FrameIndex();
	Vector<UIDrawCmd>& commands = drawCommands[frame];
	commands.clear();

	Vector<UIVertex> panelVertices;
	Vector<U32> panelIndices;

	Vector<TextVertex> textVertices;
	Vector<U32> textIndices;

	F32 physicalAspect = (F32)Settings::WindowWidth() / (F32)Settings::WindowHeight();
	F32 virtualWidth = 1080.0f * physicalAspect;
	glm::mat4 uiProjection = glm::ortho(0.0f, 1920.0f, 0.0f, 1080.0f, -1.0f, 1.0f);

	UIDrawCmd currentCmd{};
	currentCmd.scissor = { { 0, 0 }, { 1920, 1080 } };

	auto ProcessBatch = [&](UIDrawType type, const Scissor& clipRect, U32 currentIndexOffset) {
		bool scissorChanged = clipRect.offset.x != currentCmd.scissor.offset.x || clipRect.extent.x != currentCmd.scissor.extent.x || 
			clipRect.offset.y != currentCmd.scissor.offset.y || clipRect.extent.y != currentCmd.scissor.extent.y;
		bool typeChanged = type != currentCmd.type;

		if (scissorChanged || typeChanged)
		{
			if (currentCmd.indexCount > 0) { commands.push_back(currentCmd); }
			currentCmd = {};
			currentCmd.type = type;
			currentCmd.scissor = clipRect;
			currentCmd.indexOffset = currentIndexOffset;
		}
	};

	for (U32 id : sortedIds)
	{
		Scissor clipRect = GetAbsoluteScissor(id);
		if (clipRect.extent.x == 0 || clipRect.extent.y == 0) { continue; }

		glm::vec2 absPos = GetAbsoluteUIPosition(id);
		auto& rect = Registry::GetComponent<UIRect>(id);

		if (Registry::HasComponent<UIPanel>(id))
		{
			ProcessBatch(UIDrawType::Panel, clipRect, (U32)panelIndices.size());
			auto& panel = Registry::GetComponent<UIPanel>(id);

			U32 vertexOffset = (U32)panelVertices.size();
			glm::vec2 bottomLeft = uiProjection * glm::vec4(absPos, 0.0f, 1.0f);
			glm::vec2 topRight = uiProjection * glm::vec4(absPos + rect.size, 0.0f, 1.0f);

			panelVertices.push_back({ { bottomLeft.x, bottomLeft.y }, { 0.0f, 0.0f }, panel.color, panel.textureId });
			panelVertices.push_back({ { topRight.x, bottomLeft.y }, { 1.0f, 0.0f }, panel.color, panel.textureId });
			panelVertices.push_back({ { topRight.x, topRight.y }, { 1.0f, 1.0f }, panel.color, panel.textureId });
			panelVertices.push_back({ { bottomLeft.x, topRight.y }, { 0.0f, 1.0f }, panel.color, panel.textureId });

			panelIndices.push_back(vertexOffset + 0);
			panelIndices.push_back(vertexOffset + 1);
			panelIndices.push_back(vertexOffset + 2);
			panelIndices.push_back(vertexOffset + 2);
			panelIndices.push_back(vertexOffset + 3);
			panelIndices.push_back(vertexOffset + 0);

			currentCmd.indexCount += 6;
		}

		if (Registry::HasComponent<UIText>(id))
		{
			ProcessBatch(UIDrawType::Text, clipRect, (U32)textIndices.size());
			auto& text = Registry::GetComponent<UIText>(id);

			GenerateTextData(absPos, uiProjection, text, textVertices, textIndices, currentCmd);
		}
	}

	if (currentCmd.indexCount > 0) { commands.push_back(currentCmd); }

	if (!panelVertices.empty())
	{
		uiVertexBuffers[frame].Write(panelVertices.data(), panelVertices.size() * sizeof(UIVertex));
		uiIndexBuffers[frame].Write(panelIndices.data(), panelIndices.size() * sizeof(U32));
	}

	if (!textVertices.empty())
	{
		textVertexBuffers[frame].Write(textVertices.data(), textVertices.size() * sizeof(TextVertex));
		textIndexBuffers[frame].Write(textIndices.data(), textIndices.size() * sizeof(U32));
	}
}

void UI::GenerateTextData(const glm::vec2& absPos, const glm::mat4& uiProjection, const UIText& textComp, Vector<TextVertex>& outVertices, Vector<U32>& outIndices, UIDrawCmd& command)
{
	if (textComp.text.empty() || !textComp.font) { return; }

	U32 vertexOffset = (U32)outVertices.size();

	F32 aspectCorrectionX = ((F32)Settings::WindowHeight() * 1920.0f) / ((F32)Settings::WindowWidth() * 1080.0f);
	F32 textWidth = GetTextWidth(textComp);
	F32 startX = absPos.x;

	if (textComp.alignment == TextAlignment::Center)
	{
		startX -= (textWidth * aspectCorrectionX) * 0.5f;
	}
	else if (textComp.alignment == TextAlignment::Right)
	{
		startX -= (textWidth * aspectCorrectionX);
	}

	F32 cursorX = startX;
	F32 cursorY = absPos.y;

	F32 scale = textComp.fontSize / (F32)textComp.font->GlyphSize();

	glm::vec2 textPosition = glm::vec2{ (F32)textComp.font->GlyphSize(), (F32)textComp.font->GlyphSize() } / glm::vec2{ (F32)textComp.font->GetTexture()->Width(), (F32)textComp.font->GetTexture()->Height() };
	glm::vec2 textPadding = glm::vec2{ 1.0f } / glm::vec2{ (F32)textComp.font->GetTexture()->Width(), (F32)textComp.font->GetTexture()->Height() };

	F32 halfTexelX = 0.5f / textComp.font->GetTexture()->Width();
	F32 halfTexelY = 0.5f / textComp.font->GetTexture()->Height();

	U8 prev = 255;

	for (C c : textComp.text)
	{
		U8 index = c - 32;
		const Glyph& glyph = textComp.font->GetGlyph(index);

		F32 kern = 0.0f;

		if (c == '\n')
		{
			cursorX = startX;
			cursorY += textComp.fontSize;
			prev = 255;
			continue;
		}
		else if (c != ' ')
		{
			if (prev != 255)
			{
				kern = textComp.font->glyphs[prev - 32].kerning[c - 32];
			}

			F32 left = cursorX - (glyph.x * scale * aspectCorrectionX);
			F32 right = cursorX + (textComp.fontSize * aspectCorrectionX) - (glyph.x * scale * aspectCorrectionX);
			F32 bottom = cursorY + textComp.fontSize + glyph.y * scale;
			F32 top = cursorY + glyph.y * scale;

			glm::vec2 tl = uiProjection * glm::vec4(left, top, 0.0f, 1.0f);
			glm::vec2 tr = uiProjection * glm::vec4(right, top, 0.0f, 1.0f);
			glm::vec2 br = uiProjection * glm::vec4(right, bottom, 0.0f, 1.0f);
			glm::vec2 bl = uiProjection * glm::vec4(left, bottom, 0.0f, 1.0f);

			glm::vec2 texPos = { (F32)(index % 8), (F32)(index / 8) };
			glm::vec2 uv = texPos * textPosition + (texPos + glm::vec2{ 1.0f }) * textPadding;

			outVertices.push_back({ tl, { uv.x + halfTexelX, uv.y + halfTexelY }, textComp.color, textComp.boldness, textComp.font->TextureId() });
			outVertices.push_back({ tr, { textPosition.x + uv.x - halfTexelX, uv.y + halfTexelY }, textComp.color, textComp.boldness, textComp.font->TextureId() });
			outVertices.push_back({ br, { textPosition.x + uv.x - halfTexelX, textPosition.y + uv.y - halfTexelY }, textComp.color, textComp.boldness, textComp.font->TextureId() });
			outVertices.push_back({ bl, { uv.x + halfTexelX, textPosition.y + uv.y - halfTexelY }, textComp.color, textComp.boldness, textComp.font->TextureId() });

			outIndices.push_back(vertexOffset + 0);
			outIndices.push_back(vertexOffset + 1);
			outIndices.push_back(vertexOffset + 2);
			outIndices.push_back(vertexOffset + 2);
			outIndices.push_back(vertexOffset + 3);
			outIndices.push_back(vertexOffset + 0);

			vertexOffset += 4;
			command.indexCount += 6;
		}

		cursorX += (glyph.advance + kern) * scale * aspectCorrectionX;

		prev = c;
	}
}

F32 UI::GetTextWidth(const UIText& textComp)
{
	if (textComp.text.empty() || !textComp.font) { return 0.0f; }

	F32 currentLineWidth = 0.0f;
	F32 maxLineWidth = 0.0f;
	F32 scale = textComp.fontSize / (F32)textComp.font->GlyphSize();
	U8 prev = 255;

	for (C c : textComp.text)
	{
		if (c == '\n')
		{
			maxLineWidth = glm::max(maxLineWidth, currentLineWidth);
			currentLineWidth = 0.0f;
			prev = 255;
			continue;
		}

		U8 index = c - 32;
		F32 kern = (prev != 255 && c != ' ') ? textComp.font->glyphs[prev - 32].kerning[index] : 0.0f;

		currentLineWidth += (textComp.font->GetGlyph(index).advance + kern) * scale;
		prev = c;
	}

	return glm::max(maxLineWidth, currentLineWidth);
}

F32 UI::GetTextWidthUpToIndex(const UIText& textComp, U32 stopIndex)
{
	if (textComp.text.empty() || !textComp.font || stopIndex == 0) { return 0.0f; }

	F32 aspectCorrectionX = ((F32)Settings::WindowHeight() * 1920.0f) / ((F32)Settings::WindowWidth() * 1080.0f);
	F32 scale = textComp.fontSize / (F32)textComp.font->GlyphSize();
	F32 width = 0.0f;
	U8 prev = 255;

	U32 limit = glm::min(stopIndex, (U32)textComp.text.length());

	for (U32 i = 0; i < limit; ++i)
	{
		C c = textComp.text[i];
		U8 index = c - 32;
		F32 kern = (prev != 255 && c != ' ') ? textComp.font->glyphs[prev - 32].kerning[c - 32] : 0.0f;

		width += (textComp.font->GetGlyph(index).advance + kern) * scale * aspectCorrectionX;
		prev = c;
	}

	return width;
}

U32 UI::CalculateCursorIndexFromMouse(const UIText& textComp, F32 localMouseX)
{
	if (textComp.text.empty() || !textComp.font) { return 0; }

	F32 aspectCorrectionX = ((F32)Settings::WindowHeight() * 1920.0f) / ((F32)Settings::WindowWidth() * 1080.0f);
	F32 scale = textComp.fontSize / (F32)textComp.font->GlyphSize();
	F32 currentX = 0.0f;
	U8 prev = 255;

	for (U32 i = 0; i < textComp.text.length(); ++i)
	{
		C c = textComp.text[i];
		U8 index = c - 32;
		F32 kern = (prev != 255 && c != ' ') ? textComp.font->glyphs[prev - 32].kerning[c - 32] : 0.0f;

		F32 charWidth = (textComp.font->GetGlyph(index).advance + kern) * scale * aspectCorrectionX;

		if (localMouseX <= currentX + (charWidth * 0.5f)) { return i; }

		currentX += charWidth;
		prev = c;
	}

	return (U32)textComp.text.length();
}

void UI::BringWindowToFront(U32 windowId)
{
	currentGlobalZ += 100;
	U32 baseZ = currentGlobalZ;

	auto UpdateZ = [&](auto& self, U32 id) -> void {
		if (Registry::HasComponent<UIRect>(id))
		{
			Registry::GetComponent<UIRect>(id).zIndex = baseZ++;
		}

		if (Registry::GetSet<UIHierarchy>().Has(id))
		{
			for (Entity child : Registry::GetComponent<UIHierarchy>(id).children)
			{
				self(self, child.Id());
			}
		}
	};

	UpdateZ(UpdateZ, windowId);
}

void UI::Render(VkCommandBuffer cmd)
{
	U32 frame = Renderer::FrameIndex();
	const auto& commands = drawCommands[frame];
	if (commands.empty()) { return; }

	F32 scaleX = (F32)Settings::WindowWidth() / 1920.0f;
	F32 scaleY = (F32)Settings::WindowHeight() / 1080.0f;

	auto ApplyScissor = [&](const Scissor& virtualScissor) {
		VkRect2D physicalScissor{};

		physicalScissor.offset.x = (I32)(virtualScissor.offset.x * scaleX);
		physicalScissor.offset.y = (I32)(virtualScissor.offset.y * scaleY);

		I32 right = (I32)((virtualScissor.offset.x + virtualScissor.extent.x) * scaleX);
		I32 bottom = (I32)((virtualScissor.offset.y + virtualScissor.extent.y) * scaleY);

		physicalScissor.offset.x = glm::max(0, physicalScissor.offset.x);
		physicalScissor.offset.y = glm::max(0, physicalScissor.offset.y);
		right = glm::min((I32)Settings::WindowWidth(), right);
		bottom = glm::min((I32)Settings::WindowHeight(), bottom);

		physicalScissor.extent.width = glm::max(0, right - physicalScissor.offset.x);
		physicalScissor.extent.height = glm::max(0, bottom - physicalScissor.offset.y);

		vkCmdSetScissor(cmd, 0, 1, &physicalScissor);
	};

	UIDrawType currentPipeline = UIDrawType::None;

	for (const UIDrawCmd& drawCmd : commands)
	{
		if (drawCmd.type != currentPipeline)
		{
			if (drawCmd.type == UIDrawType::Panel)
			{
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiShader.Pipeline());
				VkDescriptorSet sets[] = { Renderer::GlobalBindlessSet() };
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiShader.PipelineLayout(), 0, 1, sets, 0, nullptr);

				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(cmd, 0, 1, &uiVertexBuffers[frame].vkBuffer, offsets);
				vkCmdBindIndexBuffer(cmd, uiIndexBuffers[frame].vkBuffer, 0, VK_INDEX_TYPE_UINT32);
			}
			else if (drawCmd.type == UIDrawType::Text)
			{
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, textShader.Pipeline());
				VkDescriptorSet sets[] = { Renderer::GlobalBindlessSet() };
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, textShader.PipelineLayout(), 0, 1, sets, 0, nullptr);

				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(cmd, 0, 1, &textVertexBuffers[frame].vkBuffer, offsets);
				vkCmdBindIndexBuffer(cmd, textIndexBuffers[frame].vkBuffer, 0, VK_INDEX_TYPE_UINT32);
			}
			currentPipeline = drawCmd.type;
		}

		ApplyScissor(drawCmd.scissor);
		vkCmdDrawIndexed(cmd, drawCmd.indexCount, 1, drawCmd.indexOffset, 0, 0);
	}
}

glm::vec2 UI::GetAbsoluteUIPosition(U32 entityId)
{
	UIRect& originalRect = Registry::GetComponent<UIRect>(entityId);
	glm::vec2 absPos = originalRect.position;

	U32 currentId = entityId;
	while (Registry::GetSet<UIHierarchy>().Has(currentId))
	{
		Entity parent = Registry::GetComponent<UIHierarchy>(currentId).parent;
		if (parent.Id() == U32_MAX) { break; }

		UIRect& parentRect = Registry::GetComponent<UIRect>(parent.Id());
		UIRect& currentRect = Registry::GetComponent<UIRect>(currentId);

		glm::vec2 anchoredOffset = parentRect.size * currentRect.anchor;

		absPos += parentRect.position + anchoredOffset;

		currentId = parent.Id();
	}

	UIRect& rootRect = Registry::GetComponent<UIRect>(currentId);
	if (rootRect.anchor.x != 0.0f || rootRect.anchor.y != 0.0f)
	{
		glm::vec2 virtualScreenSize = { 1920.0f, 1080.0f };
		absPos += virtualScreenSize * rootRect.anchor;
	}

	return absPos;
}

Scissor UI::GetAbsoluteScissor(U32 entityId)
{
	F32 physicalAspect = (F32)Settings::WindowWidth() / (F32)Settings::WindowHeight();
	F32 virtualHeight = 1080.0f;
	F32 virtualWidth = virtualHeight * physicalAspect;

	F32 minX = 0.0f;
	F32 minY = 0.0f;
	F32 maxX = 1920.0f;
	F32 maxY = 1080.0f;

	U32 currentId = entityId;
	while (currentId != U32_MAX)
	{
		if (Registry::HasComponent<UIClipMask>(currentId))
		{
			glm::vec2 pos = UI::GetAbsoluteUIPosition(currentId);
			glm::vec2 size = Registry::GetComponent<UIRect>(currentId).size;

			minX = glm::max(minX, pos.x);
			minY = glm::max(minY, pos.y);
			maxX = glm::min(maxX, pos.x + size.x);
			maxY = glm::min(maxY, pos.y + size.y);
		}

		if (Registry::GetSet<UIHierarchy>().Has(currentId))
		{
			currentId = Registry::GetComponent<UIHierarchy>(currentId).parent.Id();
		}
		else { break; }
	}

	if (minX >= maxX || minY >= maxY)
	{
		return Scissor{ { 0, 0 }, { 0, 0 } };
	}

	return Scissor{
		{ (U32)(maxX - minX), (U32)(maxY - minY) },
		{ (I32)minX, (I32)minY }
	};
}

glm::vec2 UI::GetVirtualMousePosition()
{
	glm::vec2 rawMouse = Input::MousePosition();

	F32 scaleX = 1920.0f / (F32)Settings::WindowWidth();
	F32 scaleY = 1080.0f / (F32)Settings::WindowHeight();

	return { rawMouse.x * scaleX, rawMouse.y * scaleY };
}

glm::vec2 UI::GetVirtualMouseDelta()
{
	glm::vec2 rawDelta = Input::MouseDelta();

	F32 scaleX = 1920.0f / (F32)Settings::WindowWidth();
	F32 scaleY = 1080.0f / (F32)Settings::WindowHeight();

	return { rawDelta.x * scaleX, rawDelta.y * scaleY };
}

void UI::AttachToParent(Entity child, Entity parent)
{
	if (parent.Id() == U32_MAX) { return; }

	UIHierarchy& childHierarchy = child.AddComponent<UIHierarchy>();
	childHierarchy.parent = parent;

	if (!Registry::GetSet<UIHierarchy>().Has(parent.Id()))
	{
		parent.AddComponent<UIHierarchy>();
	}

	UIHierarchy& parentHierarchy = parent.GetComponent<UIHierarchy>();
	parentHierarchy.children.push_back(child);

	U32 parentZ = parent.GetComponent<UIRect>().zIndex;
	child.GetComponent<UIRect>().zIndex = parentZ + 1;
}

Entity UI::CreateContainer(glm::vec2 localPos, glm::vec2 size, glm::vec2 anchor, Entity parent)
{
	Entity entity = Registry::CreateEntity();

	UIRect& rect = entity.AddComponent<UIRect>();
	rect.position = localPos;
	rect.size = size;
	rect.anchor = anchor;

	AttachToParent(entity, parent);
	return entity;
}

Entity UI::CreatePanel(glm::vec2 localPos, glm::vec2 size, glm::vec4 color, glm::vec2 anchor, Entity parent)
{
	Entity entity = CreateContainer(localPos, size, anchor, parent);

	UIPanel& panel = entity.AddComponent<UIPanel>();
	panel.color = color;

	return entity;
}

Entity UI::CreateText(const String& text, std::shared_ptr<Font> font, glm::vec2 localPos, F32 fontSize, glm::vec4 color, glm::vec2 anchor, Entity parent)
{
	Entity entity = CreateContainer(localPos, { 0.0f, 0.0f }, anchor, parent);

	UIText& textComp = entity.AddComponent<UIText>();
	textComp.text = text;
	textComp.font = font;
	textComp.fontSize = fontSize;
	textComp.color = color;
	textComp.boldness = 0.5f;

	if (anchor.x < 0.01f) { textComp.alignment = TextAlignment::Left; }
	else if (anchor.x > 0.99f) { textComp.alignment = TextAlignment::Right; }
	else { textComp.alignment = TextAlignment::Center; }

	return entity;
}

Entity UI::CreateTextInput(std::shared_ptr<Font> font, glm::vec2 localPos, glm::vec2 size, glm::vec2 anchor, Entity parent)
{
	Entity root = CreatePanel(localPos, size, { 0.1f, 0.1f, 0.1f, 1.0f }, anchor, parent);
	root.AddComponent<UIClipMask>();
	UITextInput& inputComp = root.AddComponent<UITextInput>();

	Entity textEntity = CreateText(inputComp.hintText, font, { 6.0f, size.y * 0.1f }, size.y * 0.666f, { 0.3f, 0.3f, 0.3f, 1.0f }, { 0.0f, 0.0f }, root);
	inputComp.textEntity = textEntity.Id();

	Entity caret = CreatePanel({ 6.0f, 4.0f }, { 2.0f, size.y - 8.0f }, { 1.0f, 1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, root);
	inputComp.caretEntity = caret.Id();

	return root;
}

Entity UI::CreateButton(const String& text, std::shared_ptr<Font> font, glm::vec2 localPos, glm::vec2 size, glm::vec2 anchor, Entity parent)
{
	Entity buttonEntity = CreatePanel(localPos, size, { 0.3f, 0.3f, 0.3f, 1.0f }, anchor, parent);

	UIInteractable& interactable = buttonEntity.AddComponent<UIInteractable>();

	glm::vec2 textLocalPos = { 0.0f, size.y * 0.1f };
	Entity textEntity = CreateText(text, font, textLocalPos, 24.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.5f, 0.0f }, buttonEntity);

	interactable.OnHoverEnter = [buttonEntity]() {
		Registry::GetComponent<UIPanel>(buttonEntity.Id()).color = { 0.4f, 0.4f, 0.4f, 1.0f };
	};

	interactable.OnHoverExit = [buttonEntity]() {
		Registry::GetComponent<UIPanel>(buttonEntity.Id()).color = { 0.3f, 0.3f, 0.3f, 1.0f };
	};

	return buttonEntity;
}

Entity UI::CreateWindow(const String& title, std::shared_ptr<Font> font, glm::vec2 pos, glm::vec2 size, bool resizable)
{
	Entity windowRoot = CreatePanel(pos, size, { 0.1f, 0.1f, 0.1f, 1.0f });
	UIWindow& winComp = windowRoot.AddComponent<UIWindow>();
	windowRoot.AddComponent<UIClipMask>();
	if (resizable) { windowRoot.AddComponent<UIResizable>(); }
	winComp.titleBarHeight = 24.0f;

	CreateText(title, font, { WindowBorderWidth, 0.0f }, 16.0f, { 0.8f, 0.8f, 0.8f, 1.0f }, { 0.0f, 0.0f }, windowRoot);

	glm::vec2 bodyPos = { WindowBorderWidth, winComp.titleBarHeight };
	glm::vec2 bodySize = { size.x - WindowBorderWidth * 2.0f, size.y - winComp.titleBarHeight - WindowBorderWidth };

	Entity body = CreatePanel(bodyPos, bodySize, { 0.2f, 0.2f, 0.2f, 1.0f }, { 0.0f, 0.0f }, windowRoot);
	body.AddComponent<UIClipMask>();

	winComp.bodyEntity = body.Id();
	BringWindowToFront(windowRoot.Id());

	return body;
}

ScrollAreaEntities UI::CreateScrollArea(glm::vec2 localPos, glm::vec2 size, glm::vec2 anchor, Entity parent)
{
	Entity viewport = CreateContainer(localPos, size, anchor, parent);
	viewport.AddComponent<UIClipMask>();
	UIScrollArea& scroll = viewport.AddComponent<UIScrollArea>();
	scroll.scrollOffset.y = scroll.padding;

	Entity content = CreateContainer({ 0.0f, 0.0f }, { size.x, 0.0f }, { 0.0f, 0.0f }, viewport);
	scroll.contentEntity = content.Id();

	return { viewport, content };
}