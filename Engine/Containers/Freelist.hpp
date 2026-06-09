#pragma once

#include "Defines.hpp"

#include "Platform/Memory.hpp"
#include "Multithreading/ThreadSafety.hpp"

struct NH_API Freelist
{
public:
	Freelist();
	Freelist(U32 count);

	Freelist& operator()(U32 count);

	~Freelist();
	void Destroy();

	void Reset();

	U32 GetFree();
	void Release(U32 index);

	bool Full() const;
	U32 Size() const;
	U32 Capacity() const;
	U32 Last() const;

	void Resize(U32 count);

private:
	U32 capacity = 0;
	U32 used = 0;

	U32 freeCount = 0;
	U32* freeIndices = nullptr;
	U32 lastFree = 0;
};

inline Freelist::Freelist() {}

inline Freelist::Freelist(U32 count) : capacity(count)
{
	Memory::Allocate(&freeIndices, count);
}

inline Freelist& Freelist::operator()(U32 count)
{
	if (freeIndices)
	{
		Resize(count);

		return *this;
	}

	freeCount = 0;
	lastFree = 0;
	capacity = count;
	Memory::Allocate(&freeIndices, count);

	return *this;
}

inline Freelist::~Freelist()
{
	Destroy();
}

inline void Freelist::Destroy()
{
	capacity = 0;
	freeCount = 0;
	lastFree = 0;

	Memory::Free(&freeIndices);
}

inline void Freelist::Reset()
{
	freeCount = 0;
	lastFree = 0;
	used = 0;
	memset(freeIndices, 0, sizeof(U32) * capacity);
}

inline U32 Freelist::GetFree()
{
	if (Full()) { return U32_MAX; }

	U32 index = SafeDecrement(&freeCount);

	if (index < capacity) { ++used; return freeIndices[index]; }

	++used;
	++freeCount;
	if (freeCount > 10000) { BreakPoint; }
	return SafeIncrement(&lastFree) - 1;
}

inline void Freelist::Release(U32 index)
{
	--used;
	freeIndices[SafeIncrement(&freeCount) - 1] = index;
}

inline bool Freelist::Full() const
{
	return lastFree >= capacity && freeCount == 0;
}

inline U32 Freelist::Size() const
{
	return used;
}

inline U32 Freelist::Capacity() const
{
	return capacity;
}

inline U32 Freelist::Last() const
{
	return lastFree;
}

inline void Freelist::Resize(U32 count)
{
	//TODO: Make thread safe

	if (count <= capacity) { return; }

	Memory::Reallocate(&freeIndices, count);

	capacity = count;
}