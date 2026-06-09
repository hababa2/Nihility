#include "File.hpp"

#include "Nihility.hpp"
#include "Platform/WindowsInclude.hpp"
#include "Core/Logger.hpp"

#include "enkiTS/TaskScheduler.h"

HANDLE FileIO::hCompletionPort;
std::thread FileIO::workerThread;
std::atomic<bool> FileIO::running{ true };

enum class IOOperation { READ, WRITE };

struct IORequest : public OVERLAPPED, public enki::ITaskSet {
	IOOperation opType{ IOOperation::READ };
	C* buffer{ nullptr };
	U64 dataSize{ 0 };
	U64 bytesToTransfer{ 0 };
	U64 bytesTransferred{ 0 };
	void* userData{ nullptr };
	HANDLE fileHandle{ nullptr };

	FileIOCallback onComplete{ nullptr };

	IORequest()
	{
		Internal = 0;
		InternalHigh = 0;
		Offset = 0;
		OffsetHigh = 0;
		hEvent = 0;
		m_SetSize = 1;
	}

	void ExecuteRange(enki::TaskSetPartition range, U32 threadnum) override;
};

static LockFreePool<IORequest, 256> requestPool;

void IORequest::ExecuteRange(enki::TaskSetPartition range, U32 threadnum)
{
	if (opType == IOOperation::WRITE)
	{
		LARGE_INTEGER ptr;
		ptr.QuadPart = Offset + dataSize;

		if (SetFilePointerEx(fileHandle, ptr, nullptr, FILE_BEGIN))
		{
			SetEndOfFile(fileHandle);
		}
	}

	if (onComplete)
	{
		FileData data;
		data.buffer = buffer;
		data.dataSize = dataSize;
		data.bufferSize = bytesToTransfer;
		data.threadId = threadnum;

		onComplete(data, userData);
	}
	else
	{
		Memory::FreeAligned(buffer);
	}

	CloseHandle(fileHandle);
	PostQueuedCompletionStatus(FileIO::hCompletionPort, 0, (ULONG_PTR)this, (LPOVERLAPPED)1);
}

bool FileIO::Initialize()
{
	Logger::Trace("Initializing File System...");

	hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	workerThread = std::thread(WorkerLoop);

	return true;
}

void FileIO::Shutdown()
{
	Logger::Trace("Shutting Down File System...");

	running = false;
	PostQueuedCompletionStatus(hCompletionPort, 0, 0, nullptr);
	if (workerThread.joinable()) { workerThread.join(); }
	CloseHandle(hCompletionPort);
}

bool FileIO::ReadFileAsync(const Path& path, FileIOCallback callback, void* userData)
{
	HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, nullptr);

	if (hFile == INVALID_HANDLE_VALUE) { return false; }

	LARGE_INTEGER size;
	if (!GetFileSizeEx(hFile, &size))
	{
		CloseHandle(hFile);
		return false;
	}

	U64 fileSize = (U64)size.QuadPart;
	U64 alignedSize = (fileSize + (SectorSize - 1)) & ~(SectorSize - 1);
	C* buffer = (C*)Memory::AllocateAligned(alignedSize, SectorSize);

	CreateIoCompletionPort(hFile, hCompletionPort, (U64)hFile, 0);

	IORequest* req = requestPool.Allocate();
	req->opType = IOOperation::READ;
	req->buffer = buffer;
	req->dataSize = fileSize;
	req->bytesToTransfer = alignedSize;
	req->userData = userData;
	req->fileHandle = hFile;
	req->onComplete = callback;
	req->Offset = 0;
	req->OffsetHigh = 0;

	I32 result = ReadFile(hFile, buffer, (UL32)alignedSize, nullptr, req);

	UL32 err = GetLastError();
	if (!result && err != ERROR_IO_PENDING)
	{
		CloseHandle(hFile);
		Memory::FreeAligned(buffer);
		requestPool.Free(req);
		return false;
	}

	return true;
}

bool FileIO::ReadPartialFileAsync(const Path& path, U64 offset, U64 readSize, FileIOCallback callback, void* userData)
{
	HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, nullptr);

	if (hFile == INVALID_HANDLE_VALUE) { return false; }

	U64 alignedOffset = offset & ~(SectorSize - 1);
	U64 offsetDiff = offset - alignedOffset;
	U64 alignedSize = (readSize + offsetDiff + (SectorSize - 1)) & ~(SectorSize - 1);

	C* buffer = (C*)Memory::AllocateAligned(alignedSize, SectorSize);

	CreateIoCompletionPort(hFile, hCompletionPort, (U64)hFile, 0);

	IORequest* req = requestPool.Allocate();
	req->opType = IOOperation::READ;
	req->buffer = buffer;
	req->dataSize = readSize;
	req->bytesToTransfer = alignedSize;
	req->userData = userData;
	req->onComplete = callback;

	req->Offset = (UL32)alignedOffset;
	req->OffsetHigh = (UL32)(alignedOffset >> 32);
	req->fileHandle = hFile;

	I32 result = ReadFile(hFile, buffer, (UL32)alignedSize, nullptr, req);

	UL32 err = GetLastError();
	if (!result && err != ERROR_IO_PENDING)
	{
		CloseHandle(hFile);
		Memory::FreeAligned(buffer);
		requestPool.Free(req);
		return false;
	}

	return true;
}

bool FileIO::WriteFileAsync(const Path& path, const void* data, U64 size, U64 offset, FileIOCallback callback, void* userData)
{
	if (size == 0) { return false; }

	U64 alignedSize = (size + (SectorSize - 1)) & ~(SectorSize - 1);
	U64 alignedOffset = (offset + (SectorSize - 1)) & ~(SectorSize - 1);
	C* buffer = (C*)Memory::AllocateAligned(alignedSize, SectorSize);
	memcpy(buffer, data, size);

	if (alignedSize > size)
	{
		memset(buffer + size, 0, alignedSize - size);
	}

	HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, nullptr);

	if (hFile == INVALID_HANDLE_VALUE) { Memory::FreeAligned(buffer); return false; }

	CreateIoCompletionPort(hFile, hCompletionPort, (ULONG_PTR)hFile, 0);

	IORequest* req = requestPool.Allocate();
	req->opType = IOOperation::WRITE;
	req->buffer = buffer;
	req->dataSize = size;
	req->bytesToTransfer = alignedSize;
	req->userData = userData;
	req->onComplete = callback;
	req->fileHandle = hFile;
	req->Offset = (UL32)alignedOffset;
	req->OffsetHigh = (UL32)(alignedOffset >> 32);

	I32 result = WriteFile(hFile, buffer, (UL32)alignedSize, nullptr, req);

	UL32 err = GetLastError();
	if (!result && err != ERROR_IO_PENDING)
	{
		CloseHandle(hFile);
		Memory::FreeAligned(buffer);
		requestPool.Free(req);
		return false;
	}

	return true;
}

FileData FileIO::ReadFileSync(const Path& path)
{
	HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

	if (hFile == INVALID_HANDLE_VALUE) { return {}; }

	LARGE_INTEGER sizeLi;
	if (!GetFileSizeEx(hFile, &sizeLi))
	{
		CloseHandle(hFile);
		return {};
	}

	U64 fileSize = (U64)sizeLi.QuadPart;
	U64 alignedSize = (fileSize + (SectorSize - 1)) & ~(SectorSize - 1);
	C* buffer = (C*)Memory::AllocateAligned(alignedSize, SectorSize);

	if (!buffer)
	{
		CloseHandle(hFile);
		return {};
	}

	UL32 bytesRead = 0;
	I32 success = ReadFile(hFile, buffer, (UL32)alignedSize, &bytesRead, nullptr);

	CloseHandle(hFile);

	if (!success)
	{
		Memory::FreeAligned(buffer);
		return {};
	}

	if (alignedSize > fileSize)
	{
		memset(buffer + fileSize, 0, alignedSize - fileSize);
	}

	return { buffer, fileSize, alignedSize, MainThread };
}

bool FileIO::WriteFileSync(const Path& path, const void* data, U64 size, U64 offset)
{
	if (size == 0) { return false; }

	U64 alignedSize = (size + (SectorSize - 1)) & ~(SectorSize - 1);
	U64 alignedOffset = (offset + (SectorSize - 1)) & ~(SectorSize - 1);
	C* buffer = (C*)Memory::AllocateAligned(alignedSize, SectorSize);
	memcpy(buffer, data, size);

	if (alignedSize > size)
	{
		memset(buffer + size, 0, alignedSize - size);
	}

	HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
		FILE_FLAG_NO_BUFFERING, nullptr);

	if (hFile == INVALID_HANDLE_VALUE) { Memory::FreeAligned(buffer); return false; }

	LARGE_INTEGER liOffset;
	liOffset.QuadPart = alignedOffset;
	if (!SetFilePointerEx(hFile, liOffset, nullptr, FILE_BEGIN))
	{
		CloseHandle(hFile);
		Memory::FreeAligned(buffer);
		return false;
	}

	UL32 bytesWritten = 0;
	I32 result = WriteFile(
		hFile,
		buffer,
		(UL32)alignedSize,
		&bytesWritten,
		nullptr
	);

	if (!result)
	{
		UL32 error = GetLastError();
		BreakPoint;
	}

	CloseHandle(hFile);
	Memory::FreeAligned(buffer);

	return result && bytesWritten == (UL32)alignedSize;
}

void FileIO::FreeData(const FileData& data)
{
	Memory::FreeAligned(data.buffer);
}

void FileIO::WorkerLoop()
{
	UL32 bytesTransferred;
	U64 completionKey;
	LPOVERLAPPED pOverlapped;

	while (running)
	{
		I32 result = GetQueuedCompletionStatus(
			hCompletionPort,
			&bytesTransferred,
			&completionKey,
			&pOverlapped,
			INFINITE
		);

		if (pOverlapped == (LPOVERLAPPED)1)
		{
			IORequest* reqToFree = (IORequest*)completionKey;
			requestPool.Free(reqToFree);
			continue;
		}

		if (!pOverlapped)
		{
			if (!running) { break; }
			continue;
		}

		IORequest* req = static_cast<IORequest*>(pOverlapped);
		req->bytesTransferred = bytesTransferred;

		if (result)
		{
			Nihility::scheduler.AddTaskSetToPipe(req);
		}
		else
		{
			CloseHandle(req->fileHandle);
			Memory::FreeAligned(req->buffer);
			requestPool.Free(req);
		}
	}
}