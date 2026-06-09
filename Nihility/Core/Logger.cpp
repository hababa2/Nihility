#include "Logger.hpp"

#include "File.hpp"

#include "Platform/WindowsInclude.hpp"
#include "Platform/Memory.hpp"

void* Logger::hConsole{ nullptr };
C* Logger::buffer{ nullptr };
U64 Logger::bufferOffset{ 0 };
U64 Logger::fileOffset{ 0 };

SpinLock Logger::lock;

bool Logger::Initialize()
{
	buffer = (C*)Memory::AllocateAligned(SectorSize, SectorSize);
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	return true;
}

void Logger::Shutdown()
{
	Flush();
}

void Logger::Flush()
{
	if (bufferOffset == 0) { return; }

	memset(buffer + bufferOffset, 0, SectorSize - bufferOffset);

	FileIO::WriteFileAsync("Log.txt", buffer, bufferOffset, fileOffset, nullptr,
		[](const FileData& data, void*) {
		Memory::FreeAligned(data.buffer);
	});

	fileOffset += bufferOffset;

	buffer = (C*)Memory::AllocateAligned(SectorSize, SectorSize);
	bufferOffset = 0;
}

void Logger::Log(const String& msg)
{
	std::lock_guard<SpinLock> guard(lock);

	UL32 written;
	WriteConsoleA(hConsole, msg.c_str(), (UL32)msg.size(), &written, NULL);

	//if (bufferOffset + msg.size() > SectorSize) { Flush(); }
	//
	//memcpy(buffer + bufferOffset, msg.c_str(), msg.size());
	//bufferOffset += msg.size();
}