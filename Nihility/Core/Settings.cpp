#include "Settings.hpp"

#include "Core/Logger.hpp"
#include "Core/Containers.hpp"
#include "Core/File.hpp"

SettingsData Settings::data = {};

bool Settings::Initialize()
{
	Logger::Trace("Initializing Settings...");

	FileData fileData = FileIO::ReadFileSync("Settings.cfg");

	if (fileData.dataSize)
	{
		memcpy(&data, fileData.buffer, sizeof(SettingsData));
	}

	return true;
}

void Settings::Shutdown()
{
	Logger::Trace("Shutting Down Settings...");

	FileIO::WriteFileSync("Settings.cfg", (C*)&data, sizeof(SettingsData));
}

F64 Settings::TargetFrametime()
{
	return data.targetFrametime;
}

F64 Settings::TargetSuspendedFrametime()
{
	return data.targetSuspendedFrametime;
}

U32 Settings::WindowWidth()
{
	return data.windowWidth;
}

U32 Settings::WindowHeight()
{
	return data.windowHeight;
}

U32 Settings::WindowWidthSmall()
{
	return data.windowWidthSmall;
}

U32 Settings::WindowHeightSmall()
{
	return data.windowHeightSmall;
}

U32 Settings::WindowPositionX()
{
	return data.windowPositionX;
}

U32 Settings::WindowPositionY()
{
	return data.windowPositionY;
}

U32 Settings::WindowPositionXSmall()
{
	return data.windowPositionXSmall;
}

U32 Settings::WindowPositionYSmall()
{
	return data.windowPositionYSmall;
}

bool Settings::Fullscreen()
{
	return data.fullscreen;
}

bool Settings::CursorConstrained()
{
	return data.cursorConstrained;
}

bool Settings::CursorLocked()
{
	return data.cursorLocked;
}

bool Settings::CursorShowing()
{
	return data.cursorShowing;
}

U32 Settings::Dpi()
{
	return data.dpi;
}

bool Settings::VSync()
{
	return data.vSync;
}

U16 Settings::ChannelCount()
{
	return data.channelCount;
}

F32 Settings::MasterVolume()
{
	return data.masterVolume;
}

bool Settings::UnfocusedAudio()
{
	return data.unfocusedAudio;
}

bool Settings::Doppler()
{
	return data.doppler;
}