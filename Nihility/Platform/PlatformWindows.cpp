#include "Platform.hpp"

#include "Input.hpp"
#include "Core/Logger.hpp"
#include "Core/Settings.hpp"

//TODO: Temp
#include "Resources/Resources.hpp"

#ifdef NH_PLATFORM_WINDOWS

#include "WindowsInclude.hpp"
#include <ole2.h>
#include <shellapi.h>

std::thread Platform::windowThread;
std::mutex Platform::initMutex;
std::condition_variable Platform::initCondition;
bool Platform::windowInitialized = false;
bool Platform::windowCreationSuccess = false;

U32 Platform::screenWidth;
U32 Platform::screenHeight;
U32 Platform::virtualScreenWidth;
U32 Platform::virtualScreenHeight;
U32 Platform::refreshRate;
UL32 Platform::style;
UL32 Platform::styleEx;
std::atomic<bool> Platform::resized = false;
std::atomic<bool> Platform::running = false;
bool Platform::resizing;
bool Platform::focused;
bool Platform::minimised;

Rect Platform::border;
HINSTANCE Platform::instance;
HWND Platform::hWnd;

HICON Platform::currentCursor;
HICON Platform::arrow;
HICON Platform::hand;
HICON Platform::IBeam;
HICON Platform::crosshair;
HICON Platform::sizeNS;
HICON Platform::sizeWE;
HICON Platform::sizeNESW;
HICON Platform::sizeNWSE;

bool Platform::Initialize(const WStringView& applicationName)
{
	Logger::Trace("Initializing Platform Layer...");

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	instance = GetModuleHandleW(nullptr);

	arrow = LoadCursorW(nullptr, IDC_ARROW);
	hand = LoadCursorW(nullptr, IDC_HAND);
	IBeam = LoadCursorW(nullptr, IDC_IBEAM);
	crosshair = LoadCursorW(nullptr, IDC_CROSS);
	sizeNS = LoadCursorW(nullptr, IDC_SIZENS);
	sizeWE = LoadCursorW(nullptr, IDC_SIZEWE);
	sizeNESW = LoadCursorW(nullptr, IDC_SIZENESW);
	sizeNWSE = LoadCursorW(nullptr, IDC_SIZENWSE);
	currentCursor = arrow;

	style = WS_POPUP | WS_BORDER | WS_VISIBLE;
	styleEx = WS_EX_ACCEPTFILES;
	U32 dpi = GetDpiForSystem();
	U32 lastDpi = Settings::Dpi();
	Settings::data.dpi = dpi;

	screenWidth = GetSystemMetricsForDpi(SM_CXSCREEN, dpi);
	screenHeight = GetSystemMetricsForDpi(SM_CYSCREEN, dpi);
	virtualScreenWidth = GetSystemMetricsForDpi(SM_CXVIRTUALSCREEN, dpi);
	virtualScreenHeight = GetSystemMetricsForDpi(SM_CYVIRTUALSCREEN, dpi);

	if (lastDpi)
	{
		Settings::data.windowPositionXSmall = MulDiv(Settings::WindowPositionXSmall(), dpi, lastDpi);
		Settings::data.windowPositionYSmall = MulDiv(Settings::WindowPositionYSmall(), dpi, lastDpi);
		Settings::data.windowWidthSmall = MulDiv(Settings::WindowWidthSmall(), dpi, lastDpi);
		Settings::data.windowHeightSmall = MulDiv(Settings::WindowHeightSmall(), dpi, lastDpi);
	}

	if (Settings::Fullscreen())
	{
		Settings::data.windowPositionX = 0;
		Settings::data.windowPositionY = 0;
		Settings::data.windowWidth = 0;
		Settings::data.windowHeight = 0;
	}
	else
	{
		Settings::data.windowPositionX = Settings::WindowPositionXSmall();
		Settings::data.windowPositionY = Settings::WindowPositionYSmall();
		Settings::data.windowWidth = Settings::WindowWidthSmall();
		Settings::data.windowHeight = Settings::WindowHeightSmall();
	}

	AdjustWindowRectExForDpi((RECT*)&border, style, 0, styleEx, dpi);

	DEVMODE monitorInfo{};
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &monitorInfo);
	F64 framerate = Settings::TargetFrametime();

	if (framerate == 0.0) { Settings::data.targetFrametime = 1.0 / monitorInfo.dmDisplayFrequency; }
	refreshRate = monitorInfo.dmDisplayFrequency;

	running = true;

	windowThread = std::thread(WindowThreadLoop, applicationName);
	std::unique_lock<std::mutex> lock(initMutex);
	initCondition.wait(lock, [] { return windowInitialized; });

	if (!windowCreationSuccess)
	{
		running = false;
		if (windowThread.joinable()) windowThread.join();
		return false;
	}

	RegisterClipboardFormatW(L"NihilityClipboard");

	return true;
}

void Platform::WindowThreadLoop(const WStringView& applicationName)
{
	OleInitialize(nullptr);

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowsMessageProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = instance;
	wc.hIcon = LoadIconW(nullptr, L""); //MAKEINTRESOURCEA(IDI_ICON)
	wc.hCursor = nullptr;
	wc.hbrBackground = nullptr;
	wc.lpszMenuName = MenuName.data();
	wc.lpszClassName = ClassName.data();
	wc.hIconSm = LoadIconW(nullptr, L""); //MAKEINTRESOURCEA(IDI_ICON)

	RegisterClassExW(&wc);

	hWnd = CreateWindowExW(styleEx, ClassName.data(), applicationName.data(), style,
		Settings::WindowPositionX() + border.left, Settings::WindowPositionY() + border.top,
		Settings::WindowWidth() + border.right - border.left, Settings::WindowHeight() + border.bottom - border.top,
		nullptr, nullptr, instance, nullptr);

	if (hWnd)
	{
		DragAcceptFiles(hWnd, TRUE);

		ShowWindow(hWnd, Settings::Fullscreen() ? SW_SHOWMAXIMIZED : SW_SHOW);
		UpdateWindow(hWnd);
	}

	{
		std::lock_guard<std::mutex> lock(initMutex);
		windowCreationSuccess = (hWnd != nullptr);
		windowInitialized = true;
	}
	initCondition.notify_one();

	if (!windowCreationSuccess) { return; }

	Input::RegisterDevices();

	MSG msg;
	int result;

	while (running && (result = GetMessageW(&msg, nullptr, 0, 0)) != 0)
	{
		if (result == -1) { break; }

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	running = false;
	OleUninitialize();
}

void Platform::Shutdown()
{
	Logger::Trace("Shutting Down Platform Layer...");

	if (hWnd && running)
	{
		PostMessageW(hWnd, WM_CLOSE, 0, 0);
	}

	if (windowThread.joinable())
	{
		windowThread.join();
	}

	UnregisterClassW(ClassName.data(), instance);
	hWnd = nullptr;
}

bool Platform::Update()
{
	return running;
}

void Platform::Close()
{
	if (hWnd && running)
	{
		PostMessageW(hWnd, WM_CLOSE, 0, 0);
	}
}

void Platform::SetFullscreen(bool fullscreen)
{
	Settings::data.fullscreen = fullscreen;

	if (fullscreen)
	{
		SetWindowPos(hWnd, nullptr, border.left, border.top,
			screenWidth + border.right - border.left, screenHeight + border.bottom - border.top, SWP_SHOWWINDOW);
	}
	else
	{
		SetWindowPos(hWnd, nullptr, Settings::WindowPositionXSmall() + border.left, Settings::WindowPositionYSmall() + border.top,
			Settings::WindowWidthSmall() + border.right - border.left, Settings::WindowHeightSmall() + border.bottom - border.top, SWP_SHOWWINDOW);
	}
}

void Platform::SetWindowSize(U32 width, U32 height)
{
	if (!Settings::Fullscreen())
	{
		SetWindowPos(hWnd, nullptr, Settings::WindowPositionX() + border.left, Settings::WindowPositionY() + border.top,
			width + border.right - border.left, height + border.bottom - border.top, SWP_SHOWWINDOW);
	}
}

void Platform::SetWindowPosition(I32 x, I32 y)
{
	if (!Settings::Fullscreen())
	{
		SetWindowPos(hWnd, nullptr, x + border.left, y + border.top,
			Settings::WindowWidth() + border.right - border.left, Settings::WindowHeight() + border.bottom - border.top, SWP_SHOWWINDOW);
	}
}

void Platform::SetConsoleWindowTitle(const WStringView& name)
{
	SetConsoleTitleW(name.data());
}

U32 Platform::ScreenWidth()
{
	return screenWidth;
}

U32 Platform::ScreenHeight()
{
	return screenHeight;
}

U32 Platform::VirtualScreenWidth()
{
	return virtualScreenWidth;
}

U32 Platform::VirtualScreenHeight()
{
	return virtualScreenHeight;
}

bool Platform::Focused()
{
	return focused;
}

bool Platform::Minimised()
{
	return minimised;
}

bool Platform::Resized()
{
	return resized;
}

bool Platform::MouseConstrained()
{
	return Settings::CursorConstrained();
}

void Platform::ConstrainMouse(bool b)
{
	Settings::data.cursorConstrained = b;
}

void Platform::SetCursorType(CursorType type)
{
	switch (type)
	{
	case CursorType::Arrow: { currentCursor = arrow; } break;
	case CursorType::Hand: { currentCursor = hand; } break;
	case CursorType::ResizeNS: { currentCursor = sizeNS; } break;
	case CursorType::ResizeEW: { currentCursor = sizeWE; } break;
	case CursorType::ResizeNESW: { currentCursor = sizeNESW; } break;
	case CursorType::ResizeNWSE: { currentCursor = sizeNWSE; } break;
	}

	PostMessageW(hWnd, WM_SETCURSOR, (U64)hWnd, MAKELPARAM(HTCLIENT, 0));
}

I64 __stdcall Platform::WindowsMessageProc(HWND__* hWnd, U32 msg, U64 wParam, I64 lParam)
{
	switch (msg)
	{
	case WM_CREATE: {} return 0;
	case WM_SETFOCUS: {
		focused = true;
	} return 0;
	case WM_KILLFOCUS: {
		focused = false;
	} return 0;
	case WM_QUIT: {
		focused = false;
		running = false;
	} return 0;
	case WM_CLOSE: {
		focused = false;
		running = false;
	} return 0;
	case WM_DESTROY: {
		focused = false;
		running = false;
		PostQuitMessage(0);
	} return 0;
	case WM_ERASEBKGND: {} return 1;
	case WM_DPICHANGED: {
		Settings::data.dpi = HIWORD(wParam);
		AdjustWindowRectExForDpi((RECT*)&border, style, 0, styleEx, Settings::Dpi());
		RECT* rect = (RECT*)lParam;

		screenWidth = GetSystemMetricsForDpi(SM_CXSCREEN, Settings::Dpi());
		screenHeight = GetSystemMetricsForDpi(SM_CYSCREEN, Settings::Dpi());
		virtualScreenWidth = GetSystemMetricsForDpi(SM_CXVIRTUALSCREEN, Settings::Dpi());
		virtualScreenHeight = GetSystemMetricsForDpi(SM_CYVIRTUALSCREEN, Settings::Dpi());

		Settings::data.windowPositionXSmall = rect->left - border.left;
		Settings::data.windowPositionYSmall = rect->top - border.top;
		Settings::data.windowWidthSmall = rect->right - rect->left - border.right + border.left;
		Settings::data.windowHeightSmall = rect->bottom - rect->top - border.bottom + border.top;

		if (!Settings::Fullscreen())
		{
			Settings::data.windowPositionX = Settings::WindowPositionXSmall();
			Settings::data.windowPositionY = Settings::WindowPositionYSmall();
			Settings::data.windowWidth = Settings::WindowWidthSmall();
			Settings::data.windowHeight = Settings::WindowHeightSmall();

			SetWindowPos(hWnd, nullptr, Settings::WindowPositionX() + border.left, Settings::WindowPositionY() + border.top,
				Settings::WindowWidth() + border.right - border.left, Settings::WindowHeight() + border.bottom - border.top, SWP_NOZORDER | SWP_NOACTIVATE);
		}

		resized = true;
	} return 0;
	case WM_SIZE: {
		switch (wParam)
		{
		case SIZE_MINIMIZED: {
			focused = false;
			minimised = true;
		} break;
		case SIZE_RESTORED: {
			focused = true;
			minimised = false;
		} break;
		}

		RECT rect{};
		GetWindowRect(hWnd, &rect);

		Settings::data.windowPositionX = rect.left - border.left;
		Settings::data.windowPositionY = rect.top - border.top;
		Settings::data.windowWidth = rect.right - rect.left - border.right + border.left;
		Settings::data.windowHeight = rect.bottom - rect.top - border.bottom + border.top;

		if (!Settings::Fullscreen())
		{
			Settings::data.windowPositionXSmall = Settings::WindowPositionX();
			Settings::data.windowPositionYSmall = Settings::WindowPositionY();
			Settings::data.windowWidthSmall = Settings::WindowWidth();
			Settings::data.windowHeightSmall = Settings::WindowHeight();
		}

		screenWidth = GetSystemMetricsForDpi(SM_CXSCREEN, Settings::Dpi());
		screenHeight = GetSystemMetricsForDpi(SM_CYSCREEN, Settings::Dpi());
		virtualScreenWidth = GetSystemMetricsForDpi(SM_CXVIRTUALSCREEN, Settings::Dpi());
		virtualScreenHeight = GetSystemMetricsForDpi(SM_CYVIRTUALSCREEN, Settings::Dpi());

		resized = true;
	} return 0;
	case WM_SIZING: {
	} return 1;
	case WM_MOVE: {
		Settings::data.windowPositionX = LOWORD(lParam);
		Settings::data.windowPositionY = HIWORD(lParam);

		if (!Settings::Fullscreen())
		{
			Settings::data.windowPositionXSmall = Settings::WindowPositionX();
			Settings::data.windowPositionYSmall = Settings::WindowPositionY();
		}

		screenWidth = GetSystemMetricsForDpi(SM_CXSCREEN, Settings::Dpi());
		screenHeight = GetSystemMetricsForDpi(SM_CYSCREEN, Settings::Dpi());
		virtualScreenWidth = GetSystemMetricsForDpi(SM_CXVIRTUALSCREEN, Settings::Dpi());
		virtualScreenHeight = GetSystemMetricsForDpi(SM_CYVIRTUALSCREEN, Settings::Dpi());
	} return 0;
	case WM_SETCURSOR: {
		switch (LOWORD(lParam))
		{
		case HTCLIENT: { SetCursor(currentCursor); } return 1; //Client Area
		//case HTCAPTION: { SetCursor(arrow); } return 1; //Title Bar
		//case HTSYSMENU: { SetCursor(hand); } return 1; //Window Menu
		//case HTGROWBOX: { SetCursor(arrow); } return 1; //Size Box?
		//case HTMENU: { SetCursor(hand); } return 1; //Menu
		//case HTHSCROLL: { SetCursor(hand); } return 1; //Horizontal Scroll Bar
		//case HTVSCROLL: { SetCursor(hand); } return 1; //Vertical Scroll Bar
		//case HTMINBUTTON: { SetCursor(hand); } return 1; //Minimize Button
		//case HTMAXBUTTON: { SetCursor(hand); } return 1; //Maximize Button
		//case HTLEFT: { SetCursor(sizeWE); } return 1; //Border Left
		//case HTRIGHT: { SetCursor(sizeWE); } return 1; //Border Right
		//case HTTOP: { SetCursor(sizeNS); } return 1; //Border Top
		//case HTTOPLEFT: { SetCursor(sizeNWSE); } return 1; //Border Top Left
		//case HTTOPRIGHT: { SetCursor(sizeNESW); } return 1; //Border Top Right
		//case HTBOTTOM: { SetCursor(sizeNS); } return 1; //Border Bottom
		//case HTBOTTOMLEFT: { SetCursor(sizeNESW); } return 1; //Border Bottom Left
		//case HTBOTTOMRIGHT: { SetCursor(sizeNWSE); } return 1; //Border Bottom Right
		//case HTBORDER: { SetCursor(arrow); } return 1; //Any Border (Not Resizable)
		//case HTOBJECT: { SetCursor(); } return 1;
		//case HTCLOSE: { SetCursor(); } return 1;
		//case HTHELP: { SetCursor(); } return 1;
		default: break;
		}
	} break;
	case WM_DROPFILES: {
		static CW path[1024]{};

		HDROP dropInfo = (HDROP)wParam;
		U32 count = DragQueryFileW(dropInfo, 0xFFFFFFFF, nullptr, 0);

		U32 pathSize;

		for (U32 i = 0; i < count; ++i)
		{
			pathSize = DragQueryFileW(dropInfo, i, path, 1024);

			Resources::UploadResource(path); //TODO: Event system
		}

		DragFinish(dropInfo);
	} return 0;
	case WM_INPUT: {
		Input::UpdateRawInput(lParam);
	} return 0;
	case WM_INPUT_DEVICE_CHANGE: {
		Input::UpdateConnectionStatus((HANDLE)lParam, wParam);
	} return 0;
	}

	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

#endif