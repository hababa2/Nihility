#pragma once

#include "Defines.hpp"

struct SettingsData
{
	//Engine

	F64 targetFrametime = 0.0;
	F64 targetSuspendedFrametime = 0.0;

	//Window

	U32 windowWidth = 0;
	U32 windowHeight = 0;
	U32 windowWidthSmall = 800;
	U32 windowHeightSmall = 600;
	U32 windowPositionX = 0;
	U32 windowPositionY = 0;
	U32 windowPositionXSmall = 320;
	U32 windowPositionYSmall = 180;
	bool fullscreen = false;
	bool cursorConstrained = false;
	bool cursorLocked = false;
	bool cursorShowing = true;
	U32 dpi = 0;

	//Graphics

	bool vSync = false;

	//Audio

	U16 channelCount = 2;
	F32 masterVolume = 1.0f;
	bool unfocusedAudio = false;
	bool doppler = false;
};

//TODO: custom settings
class NH_API Settings
{
public:
	static F64 TargetFrametime();
	static F64 TargetSuspendedFrametime();
	static U32 WindowWidth();
	static U32 WindowHeight();
	static U32 WindowWidthSmall();
	static U32 WindowHeightSmall();
	static U32 WindowPositionX();
	static U32 WindowPositionY();
	static U32 WindowPositionXSmall();
	static U32 WindowPositionYSmall();
	static bool Fullscreen();
	static bool CursorConstrained();
	static bool CursorLocked();
	static bool CursorShowing();
	static U32 Dpi();
	static bool VSync();
	static U16 ChannelCount();
	static F32 MasterVolume();
	static bool UnfocusedAudio();
	static bool Doppler();

private:
	static bool Initialize();
	static void Shutdown();

	static SettingsData data;

	friend class Nihility;
	friend class Platform;
	friend class Input;

	STATIC_CLASS(Settings);
};