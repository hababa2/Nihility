#pragma once

#include "Defines.hpp"

#include "Core/Containers.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>

enum class NH_API CursorType
{
	Arrow,
	Hand,
	IBeam,
	Crosshair,
	ResizeEW,
	ResizeNS,
	ResizeNESW,
	ResizeNWSE,
};

#ifdef NH_PLATFORM_WINDOWS
struct HINSTANCE__;
struct HWND__;
struct HICON__;

struct Rect
{
	L32 left;
	L32 top;
	L32 right;
	L32 bottom;
};
#endif

class Platform
{
public:
	static void SetFullscreen(bool fullscreen);
	static void SetWindowSize(U32 width, U32 height);
	static void SetWindowPosition(I32 x, I32 y);
	static void SetConsoleWindowTitle(const WStringView& name);
	static U32 ScreenWidth();
	static U32 ScreenHeight();
	static U32 VirtualScreenWidth();
	static U32 VirtualScreenHeight();
	static bool Focused();
	static bool Minimised();
	static bool Resized();
	static bool MouseConstrained();
	static void ConstrainMouse(bool b);
	static void SetCursorType(CursorType type);

	static void Close();

private:
	static bool Initialize(const WStringView& applicationName);
	static void WindowThreadLoop(const WStringView& applicationName);
	static void Shutdown();

	static bool Update();

	static std::thread windowThread;
	static std::mutex initMutex;
	static std::condition_variable initCondition;
	static bool windowInitialized;
	static bool windowCreationSuccess;

	static U32 screenWidth;
	static U32 screenHeight;
	static U32 virtualScreenWidth;
	static U32 virtualScreenHeight;
	static U32 refreshRate;
	static UL32 style;
	static UL32 styleEx;
	static std::atomic<bool> resized;
	static std::atomic<bool> running;
	static bool resizing;
	static bool focused;
	static bool minimised;

#ifdef NH_PLATFORM_WINDOWS
	static I64 __stdcall WindowsMessageProc(HWND__* hWnd, U32 msg, U64 wParam, I64 lParam);

	static Rect border;
	static HINSTANCE__* instance;
	static HWND__* hWnd;

	static HICON__* currentCursor;
	static HICON__* arrow;
	static HICON__* hand;
	static HICON__* IBeam;
	static HICON__* crosshair;
	static HICON__* sizeNS;
	static HICON__* sizeWE;
	static HICON__* sizeNESW;
	static HICON__* sizeNWSE;

	static constexpr inline WStringView MenuName = L"Nihility Menu";
	static constexpr inline WStringView ClassName = L"Nihility Class";
#endif

	friend class Nihility;
	friend class Renderer;
	friend class Input;
	friend struct Device;

	STATIC_CLASS(Platform);
};