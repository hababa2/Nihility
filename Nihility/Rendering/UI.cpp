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

std::shared_ptr<Font> UI::font;

U32 UI::focusedEntity = U32_MAX;
U32 UI::hoveredEntity = U32_MAX;
U32 UI::activeEntity = U32_MAX;
bool UI::cursorChanged;

bool UI::Initialize()
{
	Logger::Trace("Initializing UI System...");

	font = Resources::Load<Font>(L"arial");

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
	ResolveUnits();
	UpdateInput();
	UpdateLayouts();
	UpdateVisuals();
}

void UI::ResolveUnits()
{
	glm::vec2 viewport = WindowSize();
	F32 dpi = (F32)Settings::Dpi();

	auto ResolveEntityLayout = [&](auto& self, U32 id, glm::vec2 parentSize, F32 parentFontSize, bool parentHidden, bool parentHitTestInvisible) -> void {
		F32 currentFontSize = parentFontSize;

		bool isHidden = parentHidden || Registry::HasComponent<UIHidden>(id);
		bool isHitTestInvisible = parentHitTestInvisible || Registry::HasComponent<UIIgnoreHitTest>(id);

		if (Registry::HasComponent<UIText>(id))
		{
			UIText& text = Registry::GetComponent<UIText>(id);
			text.resolvedFontSize = UI::ResolveUIValue(text.fontSize, parentSize.y, parentSize, viewport, dpi, parentFontSize);

			currentFontSize = text.resolvedFontSize;
		}

		glm::vec2 containerSize = { 0.0f, 0.0f };
		if (Registry::HasComponent<UIRect>(id))
		{
			UIRect& rect = Registry::GetComponent<UIRect>(id);

			rect.cascadedHidden = isHidden;
			rect.cascadedHitTestInvisible = isHitTestInvisible;

			F32 marginTop = ResolveUIValue(rect.marginTop, parentSize.x, parentSize, viewport, dpi, currentFontSize);
			F32 marginRight = ResolveUIValue(rect.marginRight, parentSize.x, parentSize, viewport, dpi, currentFontSize);
			F32 marginBottom = ResolveUIValue(rect.marginBottom, parentSize.x, parentSize, viewport, dpi, currentFontSize);
			F32 marginLeft = ResolveUIValue(rect.marginLeft, parentSize.x, parentSize, viewport, dpi, currentFontSize);

			glm::vec2 effectiveParentSize = {
				glm::max(0.0f, parentSize.x - (marginLeft + marginRight)),
				glm::max(0.0f, parentSize.y - (marginTop + marginBottom))
			};

			rect.resolvedSize.x = ResolveUIValue(rect.size.x, effectiveParentSize.x, parentSize, viewport, dpi, currentFontSize) - marginLeft - marginRight;
			rect.resolvedSize.y = ResolveUIValue(rect.size.y, effectiveParentSize.y, parentSize, viewport, dpi, currentFontSize) - marginTop - marginBottom;

			rect.resolvedPos.x = ResolveUIValue(rect.pos.x, effectiveParentSize.x, parentSize, viewport, dpi, currentFontSize) + marginLeft;
			rect.resolvedPos.y = ResolveUIValue(rect.pos.y, effectiveParentSize.y, parentSize, viewport, dpi, currentFontSize) + marginTop;

			containerSize = rect.resolvedSize;
			parentSize = rect.resolvedSize;
		}

		if (Registry::HasComponent<UIText>(id))
		{
			UIText& text = Registry::GetComponent<UIText>(id);
			std::shared_ptr<Font> font = UI::GetFont();

			text.wrappedText = text.text;

			if (text.autoFit && containerSize.x > 0.0f)
			{
				while (text.resolvedFontSize > text.minAutoFitSize)
				{
					F32 currentWidth = MeasureTextWidth(font, text.text, text.resolvedFontSize);
					if (currentWidth <= containerSize.x) { break; }
					text.resolvedFontSize -= 1.0f;
				}

				currentFontSize = text.resolvedFontSize;
			}

			if (text.wrapText && containerSize.x > 0.0f)
			{
				text.wrappedText = CalculateWrappedText(font, text.text, text.resolvedFontSize, containerSize.x);
			}
		}

		if (Registry::GetSet<UIHierarchy>().Has(id))
		{
			for (Entity child : Registry::GetComponent<UIHierarchy>(id).children)
			{
				self(self, child.Id(), parentSize, currentFontSize, isHidden, isHitTestInvisible);
			}
		}
	};

	auto view = Registry::View<UIRect>();
	for (U32 i = 0; i < view.Size(); ++i)
	{
		U32 id = view.GetEntity(i);

		bool isRoot = true;
		if (Registry::GetSet<UIHierarchy>().Has(id))
		{
			if (Registry::GetComponent<UIHierarchy>(id).parent.Id() != U32_MAX)
			{
				isRoot = false;
			}
		}

		if (isRoot)
		{
			ResolveEntityLayout(ResolveEntityLayout, id, viewport, RootFontSize, false, false);
		}
	}
}

void UI::UpdateInput()
{
	glm::vec2 mousePos = Input::MousePosition();

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

		if (rect.cascadedHidden || rect.cascadedHitTestInvisible) { continue; }

		glm::vec2 pos = GetAbsoluteUIPosition(id);
		Scissor scissor = GetAbsoluteScissor(id);

		if (scissor.extent.x == 0 || scissor.extent.y == 0) { continue; }

		bool insideScissor = mousePos.x >= scissor.offset.x &&
			mousePos.x <= scissor.offset.x + scissor.extent.x &&
			mousePos.y >= scissor.offset.y &&
			mousePos.y <= scissor.offset.y + scissor.extent.y;

		if (!insideScissor) { continue; }

		if (mousePos.x >= pos.x && mousePos.x <= pos.x + rect.resolvedSize.x &&
			mousePos.y >= pos.y && mousePos.y <= pos.y + rect.resolvedSize.y)
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

			bodyRect.resolvedSize.x = rect.resolvedSize.x - WindowBorderWidth * 2.0f;
			bodyRect.resolvedSize.y = rect.resolvedSize.y - window.titleBarHeight - WindowBorderWidth;
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
						maxChildY = glm::max(maxChildY, childRect.resolvedPos.y + childRect.resolvedSize.y);
					}
				}
			}

			maxChildY += scroll.padding;

			Scissor scissor = GetAbsoluteScissor(id);
			F32 trueVisibleHeight = (F32)scissor.extent.y;

			F32 maxScroll = glm::max(0.0f, maxChildY - trueVisibleHeight);

			scroll.scrollOffset.y = glm::clamp(scroll.scrollOffset.y, -maxScroll, scroll.padding);
			contentRect.resolvedPos.y = scroll.scrollOffset.y;

			glm::vec2 pSize = GetParentSize(id);
			glm::vec2 vp = WindowSize();
			F32 dpi = (F32)Settings::Dpi();

			SetUIValueFromPixels(contentRect.pos.y, contentRect.resolvedPos.y, pSize.x, pSize, vp, dpi, RootFontSize);
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
			F32 visibleWidth = rootRect.resolvedSize.x - leftPadding - rightPadding;
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

			textRect.resolvedPos.x = leftPadding - input.scrollOffset;
			caretRect.resolvedPos.x = textRect.resolvedPos.x + cursorPixelOffset;

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
		if (interactable.OnHoverEnter) { interactable.OnHoverEnter(); }
	}
	else if (!isCurrentlyHovered && interactable.isHovered)
	{
		interactable.isHovered = false;
		if (interactable.OnHoverExit) { interactable.OnHoverExit(); }
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
				glm::vec2 mousePos = Input::MousePosition();
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
	glm::vec2 mousePos = Input::MousePosition();

	bool isInside = (id == hoveredEntity || id == activeEntity);
	if (!isInside) { return; }

	bool onRightEdge = (mousePos.x > (pos.x + rect.resolvedSize.x - resizable.edgeThickness)) && isInside;
	bool onLeftEdge = (mousePos.x < (pos.x + resizable.edgeThickness)) && isInside;
	bool onBottomEdge = (mousePos.y > (pos.y + rect.resolvedSize.y - resizable.edgeThickness)) && isInside;
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
		glm::vec2 delta = Input::MouseDelta();

		if (resizable.draggingRight)
		{
			rect.resolvedSize.x += delta.x;
		}

		if (resizable.draggingLeft)
		{
			F32 limitDelta = glm::min(delta.x, rect.resolvedSize.x - resizable.minSize.x);
			rect.resolvedPos.x += limitDelta;
			rect.resolvedSize.x -= limitDelta;
		}

		if (resizable.draggingBottom)
		{
			rect.resolvedSize.y += delta.y;
		}

		if (resizable.draggingTop)
		{
			F32 limitDelta = glm::min(delta.y, rect.resolvedSize.y - resizable.minSize.y);
			rect.resolvedPos.y += limitDelta;
			rect.resolvedSize.y -= limitDelta;
		}

		rect.resolvedSize = glm::clamp(rect.resolvedSize, resizable.minSize, resizable.maxSize);

		glm::vec2 pSize = GetParentSize(id);
		glm::vec2 vp = WindowSize();
		F32 dpi = (F32)Settings::Dpi();

		F32 mT = UI::ResolveUIValue(rect.marginTop, pSize.y, pSize, vp, dpi, RootFontSize);
		F32 mR = UI::ResolveUIValue(rect.marginRight, pSize.x, pSize, vp, dpi, RootFontSize);
		F32 mB = UI::ResolveUIValue(rect.marginBottom, pSize.y, pSize, vp, dpi, RootFontSize);
		F32 mL = UI::ResolveUIValue(rect.marginLeft, pSize.x, pSize, vp, dpi, RootFontSize);

		glm::vec2 effectivePSize = {
			std::max(0.0f, pSize.x - (mL + UI::ResolveUIValue(rect.marginRight, pSize.x, pSize, vp, dpi, RootFontSize))),
			std::max(0.0f, pSize.y - (mT + UI::ResolveUIValue(rect.marginBottom, pSize.y, pSize, vp, dpi, RootFontSize)))
		};

		SetUIValueFromPixels(rect.size.x, rect.resolvedSize.x + mR + mL, pSize.x, pSize, vp, dpi, RootFontSize);
		SetUIValueFromPixels(rect.size.y, rect.resolvedSize.y + mT + mB, pSize.y, pSize, vp, dpi, RootFontSize);
		SetUIValueFromPixels(rect.pos.x, rect.resolvedPos.x - mL, pSize.x, pSize, vp, dpi, RootFontSize);
		SetUIValueFromPixels(rect.pos.y, rect.resolvedPos.y - mT, pSize.y, pSize, vp, dpi, RootFontSize);

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
	glm::vec2 mousePos = Input::MousePosition();

	F32 resizeOffset = 0.0f;
	if (Registry::HasComponent<UIResizable>(id))
	{
		resizeOffset = Registry::GetComponent<UIResizable>(id).edgeThickness;
	}

	if (Input::OnButtonDown(ButtonCode::LeftMouse) && activeEntity == id)
	{
		if (mousePos.y >= pos.y + resizeOffset && mousePos.y <= pos.y + window.titleBarHeight &&
			mousePos.x >= pos.x + resizeOffset && mousePos.x <= pos.x + rect.resolvedSize.x - resizeOffset)
		{
			window.isDragging = true;
			window.dragOffset = mousePos - rect.resolvedPos;
		}
	}

	if (window.isDragging)
	{
		glm::vec2 newPixelPos = mousePos - window.dragOffset;

		glm::vec2 pSize = GetParentSize(id);
		glm::vec2 vp = WindowSize();
		F32 dpi = (F32)Settings::Dpi();

		F32 mL = UI::ResolveUIValue(rect.marginLeft, pSize.x, pSize, vp, dpi, RootFontSize);
		F32 mT = UI::ResolveUIValue(rect.marginTop, pSize.y, pSize, vp, dpi, RootFontSize);

		glm::vec2 effectivePSize = {
			std::max(0.0f, pSize.x - (mL + UI::ResolveUIValue(rect.marginRight, pSize.x, pSize, vp, dpi, RootFontSize))),
			std::max(0.0f, pSize.y - (mT + UI::ResolveUIValue(rect.marginBottom, pSize.y, pSize, vp, dpi, RootFontSize)))
		};

		SetUIValueFromPixels(rect.pos.x, newPixelPos.x - mL, pSize.x, pSize, vp, dpi, RootFontSize);
		SetUIValueFromPixels(rect.pos.y, newPixelPos.y - mT, pSize.y, pSize, vp, dpi, RootFontSize);

		rect.resolvedPos = newPixelPos;

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

	glm::vec vp = WindowSize();
	F32 physicalAspect = vp.x / vp.y;
	F32 virtualWidth = 1080.0f * physicalAspect;
	glm::mat4 uiProjection = glm::ortho(0.0f, vp.x, 0.0f, vp.y, -1.0f, 1.0f);

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
		UIRect& rect = Registry::GetComponent<UIRect>(id);

		if (rect.cascadedHidden) { continue; }

		Scissor clipRect = GetAbsoluteScissor(id);
		if (clipRect.extent.x == 0 || clipRect.extent.y == 0) { continue; }

		glm::vec2 absPos = GetAbsoluteUIPosition(id);

		if (Registry::HasComponent<UIPanel>(id))
		{
			ProcessBatch(UIDrawType::Panel, clipRect, (U32)panelIndices.size());
			auto& panel = Registry::GetComponent<UIPanel>(id);

			U32 vertexOffset = (U32)panelVertices.size();

			glm::vec2 bl = uiProjection * glm::vec4(absPos, 0.0f, 1.0f);
			glm::vec2 tr = uiProjection * glm::vec4(absPos + rect.resolvedSize, 0.0f, 1.0f);

			panelVertices.push_back({ { bl.x, bl.y }, { 0.0f, 0.0f }, panel.color, panel.textureId });
			panelVertices.push_back({ { tr.x, bl.y }, { 1.0f, 0.0f }, panel.color, panel.textureId });
			panelVertices.push_back({ { tr.x, tr.y }, { 1.0f, 1.0f }, panel.color, panel.textureId });
			panelVertices.push_back({ { bl.x, tr.y }, { 0.0f, 1.0f }, panel.color, panel.textureId });

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

	F32 scale = textComp.resolvedFontSize / (F32)textComp.font->GlyphSize();

	//F32 aspectCorrectionX = (vp.y * 1920.0f) / (vp.x * 1080.0f);
	F32 textWidth = GetTextWidth(textComp);
	F32 startX = absPos.x;

	if (textComp.alignment == TextAlignment::Center)
	{
		startX -= (textWidth) * 0.5f;
	}
	else if (textComp.alignment == TextAlignment::Right)
	{
		startX -= (textWidth);
	}

	F32 cursorX = startX;
	F32 cursorY = absPos.y;

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
			cursorY += textComp.resolvedFontSize;
			prev = 255;
			continue;
		}
		else if (c != ' ')
		{
			if (prev != 255)
			{
				kern = textComp.font->glyphs[prev - 32].kerning[c - 32];
			}

			F32 left = cursorX - (glyph.x * scale);
			F32 right = cursorX + (textComp.resolvedFontSize) - (glyph.x * scale);
			F32 bottom = cursorY + textComp.resolvedFontSize + glyph.y * scale;
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

		cursorX += (glyph.advance + kern) * scale;

		prev = c;
	}
}

F32 UI::GetTextWidth(const UIText& textComp)
{
	if (textComp.text.empty() || !textComp.font) { return 0.0f; }

	F32 currentLineWidth = 0.0f;
	F32 maxLineWidth = 0.0f;
	F32 scale = textComp.resolvedFontSize / (F32)textComp.font->GlyphSize();
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

	glm::vec2 vp = WindowSize();
	F32 aspectCorrectionX = (vp.y * 1920.0f) / (vp.x * 1080.0f);
	F32 scale = textComp.resolvedFontSize / (F32)textComp.font->GlyphSize();
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

	glm::vec2 vp = WindowSize();
	F32 aspectCorrectionX = (vp.y * 1920.0f) / (vp.x * 1080.0f);
	F32 scale = textComp.resolvedFontSize / (F32)textComp.font->GlyphSize();
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

F32 UI::MeasureTextWidth(std::shared_ptr<Font> font, const String& text, F32 fontSize)
{
	if (text.empty() || !font) { return 0.0f; }

	F32 currentLineWidth = 0.0f;
	F32 maxLineWidth = 0.0f;
	F32 scale = fontSize / (F32)font->GlyphSize();
	U8 prev = 255;

	for (C c : text)
	{
		if (c == '\n')
		{
			maxLineWidth = glm::max(maxLineWidth, currentLineWidth);
			currentLineWidth = 0.0f;
			prev = 255;
			continue;
		}

		U8 index = c - 32;
		F32 kern = (prev != 255 && c != ' ') ? font->glyphs[prev - 32].kerning[index] : 0.0f;

		currentLineWidth += (font->GetGlyph(index).advance + kern) * scale;
		prev = c;
	}

	return glm::max(maxLineWidth, currentLineWidth);
}

String UI::CalculateWrappedText(std::shared_ptr<Font> font, const String& text, F32 fontSize, F32 maxWidth)
{
	std::istringstream words(text);
	String word;
	String currentLine = "";
	String finalResult = "";

	while (words >> word)
	{
		String testLine = currentLine.empty() ? word : currentLine + " " + word;
		F32 testWidth = MeasureTextWidth(font, testLine, fontSize);

		if (testWidth > maxWidth && !currentLine.empty())
		{
			finalResult += currentLine + "\n";
			currentLine = word;
		}
		else
		{
			currentLine = testLine;
		}
	}

	finalResult += currentLine;
	return finalResult;
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

	auto ApplyScissor = [&](const Scissor& virtualScissor) {
		VkRect2D physicalScissor{};

		physicalScissor.offset.x = glm::max(0, (I32)virtualScissor.offset.x);
		physicalScissor.offset.y = glm::max(0, (I32)virtualScissor.offset.y);

		glm::vec2 vp = WindowSize();
		I32 right = glm::min((I32)vp.x, (I32)(virtualScissor.offset.x + virtualScissor.extent.x));
		I32 bottom = glm::min((I32)vp.y, (I32)(virtualScissor.offset.y + virtualScissor.extent.y));

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
	UIRect& currentRect = Registry::GetComponent<UIRect>(entityId);

	bool isRoot = true;
	Entity parent = {};

	if (Registry::GetSet<UIHierarchy>().Has(entityId))
	{
		parent = Registry::GetComponent<UIHierarchy>(entityId).parent;
		if (parent.Id() != U32_MAX)
		{
			isRoot = false;
		}
	}

	if (isRoot)
	{
		glm::vec2 windowSize = WindowSize();
		glm::vec2 anchorPixelOffset = windowSize * currentRect.anchor;

		return anchorPixelOffset + currentRect.resolvedPos;
	}

	glm::vec2 parentAbsPos = GetAbsoluteUIPosition(parent.Id());
	UIRect& parentRect = Registry::GetComponent<UIRect>(parent.Id());

	glm::vec2 anchorPixelOffset = parentRect.resolvedSize * currentRect.anchor;

	return parentAbsPos + anchorPixelOffset + currentRect.resolvedPos;
}

Scissor UI::GetAbsoluteScissor(U32 entityId)
{
	glm::vec2 vp = WindowSize();
	F32 physicalAspect = vp.x / vp.y;
	F32 virtualHeight = 1080.0f;
	F32 virtualWidth = virtualHeight * physicalAspect;

	F32 minX = 0.0f;
	F32 minY = 0.0f;
	F32 maxX = vp.x;
	F32 maxY = vp.y;

	U32 currentId = entityId;
	while (currentId != U32_MAX)
	{
		if (Registry::HasComponent<UIClipMask>(currentId))
		{
			UIRect& rect = Registry::GetComponent<UIRect>(currentId);
			glm::vec2 pos = UI::GetAbsoluteUIPosition(currentId);
			glm::vec2 size = rect.resolvedSize;

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

Entity UI::CreateContainer(const UIRectDef& def, Entity parent)
{
	Entity entity = Registry::CreateEntity();

	UIRect& rect = entity.AddComponent<UIRect>();
	rect.pos = def.pos;
	rect.size = def.size;
	rect.marginTop = def.marginTop;
	rect.marginBottom = def.marginBottom;
	rect.marginLeft = def.marginLeft;
	rect.marginRight = def.marginRight;
	rect.anchor = def.anchor;

	AttachToParent(entity, parent);
	return entity;
}

Entity UI::CreatePanel(const UIRectDef& def, const Color& color, Entity parent)
{
	Entity entity = CreateContainer(def, parent);

	UIPanel& panel = entity.AddComponent<UIPanel>();
	panel.color = color;

	return entity;
}

Entity UI::CreateText(const UIRectDef& def, const String& text, UIValue fontSize, bool wrapText, bool autoFit, const Color& color, Entity parent)
{
	Entity entity = CreateContainer(def, parent);

	UIText& textComp = entity.AddComponent<UIText>();
	textComp.text = text;
	textComp.font = font;
	textComp.fontSize = fontSize;
	textComp.wrapText = wrapText;
	textComp.autoFit = autoFit;
	textComp.color = color;
	textComp.boldness = 0.5f;

	if (def.anchor.x < 0.01f) { textComp.alignment = TextAlignment::Left; }
	else if (def.anchor.x > 0.99f) { textComp.alignment = TextAlignment::Right; }
	else { textComp.alignment = TextAlignment::Center; }

	return entity;
}

Entity UI::CreateTextInput(const UIRectDef& def, Entity parent)
{
	Entity root = CreatePanel(def, { 0.1f, 0.1f, 0.1f, 1.0f }, parent);
	root.AddComponent<UIClipMask>();
	UITextInput& inputComp = root.AddComponent<UITextInput>();

	Entity textEntity = CreateText({ { 6_px, 10_ph }, def.size }, inputComp.hintText, 24.0_px, false, false, { 0.3f, 0.3f, 0.3f, 1.0f }, root);
	textEntity.AddComponent<UIIgnoreHitTest>();
	inputComp.textEntity = textEntity.Id();

	Entity caret = CreatePanel({ { 6_px, 4_px }, { 2_px, 80_ph }, { 0.0f, 0.0f } }, { 1.0f, 1.0f, 1.0f, 0.0f }, root);
	inputComp.caretEntity = caret.Id();

	return root;
}

Button UI::CreateButton(const UIRectDef& def, const String& text, const Color& color, Entity parent)
{
	Entity buttonEntity = CreatePanel(def, color, parent);

	UIInteractable& interactable = buttonEntity.AddComponent<UIInteractable>();

	Entity textEntity = CreateText({ { 0_px, 10_ph }, def.size, { 0.5f, 0.0f } }, text, 24_px, false, true, color.GetContrastTextColor(), buttonEntity);
	textEntity.AddComponent<UIIgnoreHitTest>();

	interactable.OnHoverEnter = [buttonEntity, color, textEntity]() {
		Color newColor = color.MultiplySaturation(0.7f).MultiplyValue(0.8f);
		Registry::GetComponent<UIPanel>(buttonEntity.Id()).color = newColor;
		Registry::GetComponent<UIText>(textEntity.Id()).color = newColor.GetContrastTextColor();
	};

	interactable.OnHoverExit = [buttonEntity, color, textEntity]() {
		Registry::GetComponent<UIPanel>(buttonEntity.Id()).color = color;
		Registry::GetComponent<UIText>(textEntity.Id()).color = color.GetContrastTextColor();
	};

	return { buttonEntity, interactable };
}

Entity UI::CreateWindow(const UIRectDef& def, const String& title, bool resizable)
{
	Entity windowRoot = CreatePanel(def, { 0.1f, 0.1f, 0.1f, 1.0f });
	UIWindow& winComp = windowRoot.AddComponent<UIWindow>();
	windowRoot.AddComponent<UIClipMask>();
	if (resizable) { windowRoot.AddComponent<UIResizable>(); }
	winComp.titleBarHeight = 24.0f;

	Entity textEntity = CreateText({ { WindowBorderWidth, 0_px }, { 100_pw, winComp.titleBarHeight } }, title, 16_px, false, false, { 0.8f, 0.8f, 0.8f, 1.0f }, windowRoot);
	textEntity.AddComponent<UIIgnoreHitTest>();

	Entity body = CreatePanel({ { 0_px, 0_px }, { 100_pw, 100_ph }, { 0.0f, 0.0f }, winComp.titleBarHeight, WindowBorderWidth, WindowBorderWidth, WindowBorderWidth }, { 0.2f, 0.2f, 0.2f, 1.0f }, windowRoot);
	body.AddComponent<UIClipMask>();

	winComp.bodyEntity = body.Id();
	BringWindowToFront(windowRoot.Id());

	return body;
}

ScrollAreaEntities UI::CreateScrollArea(const UIRectDef& def, Entity parent)
{
	Entity viewport = CreateContainer(def, parent);
	viewport.AddComponent<UIClipMask>();
	UIScrollArea& scroll = viewport.AddComponent<UIScrollArea>();
	scroll.scrollOffset.y = scroll.padding;

	Entity content = CreateContainer({ {}, { 100_pw, 0_px } }, viewport);
	scroll.contentEntity = content.Id();

	return { viewport, content };
}

std::shared_ptr<Font> UI::GetFont()
{
	return font;
}

glm::vec2 UI::GetParentSize(U32 id)
{
	if (Registry::GetSet<UIHierarchy>().Has(id))
	{
		Entity parent = Registry::GetComponent<UIHierarchy>(id).parent;
		if (parent.Id() != U32_MAX && Registry::HasComponent<UIRect>(parent.Id()))
		{
			return Registry::GetComponent<UIRect>(parent.Id()).resolvedSize;
		}
	}

	return WindowSize();
};

F32 UI::ResolveUIValue(const UIValue& val, F32 parentAxis, glm::vec2 parentSize, glm::vec2 viewportSize, F32 dpi, F32 parentFontSize)
{
	static constexpr F32 InchToCm = 0.3937007874f;
	static constexpr F32 InchToMm = 0.03937007874f;

	switch (val.unit)
	{
	case UIUnit::Pixel: { return val.value; }
	case UIUnit::Percent: { return val.value * 0.01f * parentAxis; }
	case UIUnit::ParentWidth: { return val.value * 0.01f * parentSize.x; }
	case UIUnit::ParentHeight: { return val.value * 0.01f * parentSize.y; }
	case UIUnit::ParentMin: { return val.value * 0.01f * (parentSize.x < parentSize.y ? parentSize.x : parentSize.y); }
	case UIUnit::ParentMax: { return val.value * 0.01f * (parentSize.x > parentSize.y ? parentSize.x : parentSize.y); }
	case UIUnit::ViewportWidth: { return val.value * 0.01f * viewportSize.x; }
	case UIUnit::ViewportHeight: { return val.value * 0.01f * viewportSize.y; }
	case UIUnit::ViewportMin: { return val.value * 0.01f * (viewportSize.x < viewportSize.y ? viewportSize.x : viewportSize.y); }
	case UIUnit::ViewportMax: { return val.value * 0.01f * (viewportSize.x > viewportSize.y ? viewportSize.x : viewportSize.y); }
	case UIUnit::Inch: { return val.value * dpi; }
	case UIUnit::Centimeters: { return val.value * dpi * InchToCm; }
	case UIUnit::Millimeters: { return val.value * dpi * InchToMm; }
	case UIUnit::Em: { return val.value * parentFontSize; }
	case UIUnit::Rem: { return val.value * RootFontSize; }
	}

	return val.value;
}

void UI::SetUIValueFromPixels(UIValue& val, F32 targetPixels, F32 parentAxis, glm::vec2 parentSize, glm::vec2 viewportSize, F32 dpi, F32 parentFontSize)
{
	static constexpr F32 CmToInch = 2.54f;
	static constexpr F32 MmToInch = 25.4f;

	switch (val.unit)
	{
	case UIUnit::Pixel: { val.value = targetPixels; } break;
	case UIUnit::Percent: { val.value = parentAxis > 0.0f ? targetPixels / parentAxis * 100.0f : 0.0f; } break;
	case UIUnit::ParentWidth: { val.value = parentSize.x > 0.0f ? targetPixels / parentSize.x * 100.0f : 0.0f; } break;
	case UIUnit::ParentHeight: { val.value = parentSize.y > 0.0f ? targetPixels / parentSize.y * 100.0f : 0.0f; } break;
	case UIUnit::ParentMin: {
		F32 axis = (parentSize.x < parentSize.y ? parentSize.x : parentSize.y);
		val.value = axis > 0.0f ? targetPixels / axis * 100.0f : 0.0f;
	} break;
	case UIUnit::ParentMax: {
		F32 axis = (parentSize.x > parentSize.y ? parentSize.x : parentSize.y);
		val.value = axis > 0.0f ? targetPixels / axis * 100.0f : 0.0f;
	} break;
	case UIUnit::ViewportWidth: { val.value = viewportSize.x > 0.0f ? targetPixels / viewportSize.x * 100.0f : 0.0f; } break;
	case UIUnit::ViewportHeight: { val.value = viewportSize.y > 0.0f ? targetPixels / viewportSize.y * 100.0f : 0.0f; } break;
	case UIUnit::ViewportMin: {
		F32 axis = (viewportSize.x < viewportSize.y ? viewportSize.x : viewportSize.y);
		val.value = axis > 0.0f ? targetPixels / axis * 100.0f : 0.0f;
	} break;
	case UIUnit::ViewportMax: {
		F32 axis = (viewportSize.x > viewportSize.y ? viewportSize.x : viewportSize.y);
		val.value = axis > 0.0f ? targetPixels / axis * 100.0f : 0.0f;
	} break;
	case UIUnit::Inch: { val.value = dpi > 0.0f ? targetPixels / dpi : 0.0f; } break;
	case UIUnit::Centimeters: { val.value = dpi > 0.0f ? targetPixels / dpi * CmToInch : 0.0f; } break;
	case UIUnit::Millimeters: { val.value = dpi > 0.0f ? targetPixels / dpi * MmToInch : 0.0f; } break;
	case UIUnit::Em: { val.value = parentFontSize > 0.0f ? targetPixels / parentFontSize : 0.0f; } break;
	case UIUnit::Rem: { val.value = RootFontSize > 0.0f ? targetPixels / RootFontSize : 0.0f; } break;
	}
}

glm::vec2 UI::WindowSize()
{
#ifdef NH_DEBUG
	return { (F32)Settings::WindowWidth(), (F32)Settings::WindowHeight() };
#else
	glm::vec4 area = Renderer::RenderArea();

	return { area.z, area.w };
#endif
}