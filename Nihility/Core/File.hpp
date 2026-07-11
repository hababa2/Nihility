#pragma once

#include "Defines.hpp"
#include "Core/Containers.hpp"
#include "Core/Function.hpp"

#include <thread>
#include <atomic>

struct FileData
{
	C* buffer;
	U64 dataSize;
	U64 bufferSize;
	U32 threadId;
};

using FileIOCallback = Function<void(FileData&, void*)>;

constexpr U64 SectorSize = 4096;
constexpr U32 MainThread = U32_MAX;

class NH_API FileIO
{
public:
	static bool ReadFileAsync(const Path& path, FileIOCallback callback, void* userData = nullptr);
	static bool ReadPartialFileAsync(const Path& path, U64 offset, U64 readSize, FileIOCallback callback, void* userData);
	static bool WriteFileAsync(const Path& path, const void* buffer, U64 size, U64 offset = 0, FileIOCallback callback = nullptr, void* userData = nullptr);
	static FileData ReadFileSync(const Path& path);
	static FileData ReadPartialFileSync(const Path& path, U64 offset, U64 readSize);
	static bool WriteFileSync(const Path& path, const void* buffer, U64 size, U64 offset = 0);

	static void FreeData(const FileData& data);

private:
	static bool Initialize();
	static void Shutdown();

	static void WorkerLoop();

	static void* hCompletionPort;
	static std::thread workerThread;
	static std::atomic<bool> running;

	friend class Nihility;
	friend struct IORequest;

	STATIC_CLASS(FileIO);
};