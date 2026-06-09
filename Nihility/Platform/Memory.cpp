#include "Memory.hpp"

void MemoryRegion::Create(U8* pointer, U32 size, U32 capacity)
{
	region = pointer;
	regionSize = size;
	endRegion = region + (size * capacity);

	U32 queueCapacity = BitFloor(capacity);

	freelist.Create(queueCapacity);

	for (U64 i = 0; i < capacity; ++i)
	{
		freelist.Push(region + (i * regionSize));
	}
}

void MemoryRegion::Destroy()
{
	freelist.Destroy();
}

bool MemoryRegion::Allocate(void** pointer)
{
	return freelist.Pop(*pointer);
}

bool MemoryRegion::Reallocate(void** pointer)
{
	if (Memory::region1kb.WithinRegion(*pointer))
	{
		if (this == &Memory::region1kb) { return false; }

		void* temp = *pointer;
		Allocate(pointer);
		memmove(*pointer, temp, sizeof(Region1kb));
		Memory::region1kb.Free(&temp);
	}
	else if (Memory::region16kb.WithinRegion(*pointer))
	{
		if (this == &Memory::region16kb) { return false; }

		void* temp = *pointer;
		Allocate(pointer);
		memmove(*pointer, temp, sizeof(Region16kb));
		Memory::region16kb.Free(&temp);
	}
	else if (Memory::region256kb.WithinRegion(*pointer))
	{
		if (this == &Memory::region256kb) { return false; }

		void* temp = *pointer;
		Allocate(pointer);
		memmove(*pointer, temp, sizeof(Region256kb));
		Memory::region256kb.Free(&temp);
	}
	else if (Memory::region4mb.WithinRegion(*pointer))
	{
		if (this == &Memory::region4mb) { return false; }

		void* temp = *pointer;
		Allocate(pointer);
		memmove(*pointer, temp, sizeof(Region4mb));
		Memory::region4mb.Free(&temp);
	}

	return true;
}

void MemoryRegion::Free(void** pointer)
{
	freelist.Push(*pointer);
}

bool MemoryRegion::WithinRegion(void* pointer)
{
	return pointer >= region && pointer < endRegion;
}

U32 Memory::allocations = 0;
U8* Memory::memory = nullptr;

MemoryRegion Memory::region1kb{};
MemoryRegion Memory::region16kb{};
MemoryRegion Memory::region256kb{};
MemoryRegion Memory::region4mb{};

std::atomic<I32> Memory::initStatus{ 0 };

bool Memory::Initialize()
{
	I32 expected = 0;

	if (initStatus.compare_exchange_strong(expected, 1))
	{
		U32 maxKilobytes = DynamicMemorySize / 1024;

		U32 region4mbCount = U32(maxKilobytes / 81920);
		U32 region256kbCount = U32(maxKilobytes * 0.15) / 256;
		U32 region16kbCount = U32(maxKilobytes * 0.3) / 16;

		U64 usedBytes = (U64)region4mbCount * 4096 * 1024 +
			(U64)region256kbCount * 256 * 1024 +
			(U64)region16kbCount * 16 * 1024;

		U32 region1kbCount = 0;
		if (DynamicMemorySize > usedBytes)
		{
			region1kbCount = U32((DynamicMemorySize - usedBytes) / 1024);
		}

		memory = (U8*)_aligned_malloc(DynamicMemorySize, 64);
		if (!memory)
		{
			initStatus.store(0);
			return false;
		}

		region1kb.Create(memory, sizeof(Region1kb), region1kbCount);
		region16kb.Create(region1kb.region + region1kbCount * sizeof(Region1kb), sizeof(Region16kb), region16kbCount);
		region256kb.Create(region16kb.region + region16kbCount * sizeof(Region16kb), sizeof(Region256kb), region256kbCount);
		region4mb.Create(region256kb.region + region256kbCount * sizeof(Region256kb), sizeof(Region4mb), region4mbCount);

		initStatus.store(2, std::memory_order_release);
		return true;
	}

	while (initStatus.load(std::memory_order_acquire) != 2) { _mm_pause(); }
	return true;
}

void Memory::Shutdown()
{
	initStatus.store(0);

	//TODO: investigate unfreed data structures
	//region1kb.Destroy();
	//region16kb.Destroy();
	//region256kb.Destroy();
	//region4mb.Destroy();
}

U64 Memory::AllocateInternal(void** pointer, U64 size, U64 typeSize)
{
	if (size > sizeof(Region4mb))
	{
		*pointer = malloc(size);
		return size;
	}

	if (size <= sizeof(Region1kb))
	{
		if (region1kb.Allocate((void**)pointer)) { return sizeof(Region1kb) / typeSize; }
	}

	if (size <= sizeof(Region16kb))
	{
		if (region16kb.Allocate((void**)pointer)) { return sizeof(Region16kb) / typeSize; }
	}

	if (size <= sizeof(Region256kb))
	{
		if (region256kb.Allocate((void**)pointer)) { return sizeof(Region256kb) / typeSize; }
	}

	if (size <= sizeof(Region4mb))
	{
		if (region4mb.Allocate((void**)pointer)) { return sizeof(Region4mb) / typeSize; }
	}

	*pointer = malloc(size);
	return size;
}

U64 Memory::ReallocateInternal(void** pointer, U64 size, U64 typeSize)
{
	if (size <= sizeof(Region1kb)) { return sizeof(Region1kb) / typeSize; }
	else if (size <= sizeof(Region16kb)) { region16kb.Reallocate(pointer); return sizeof(Region16kb) / typeSize; }
	else if (size <= sizeof(Region256kb)) { region256kb.Reallocate(pointer); return sizeof(Region256kb) / typeSize; }
	else if (size <= sizeof(Region4mb)) { region4mb.Reallocate(pointer); return sizeof(Region4mb) / typeSize; }
	else { *pointer = realloc(*pointer, size); return size / typeSize; }

	return 0;
}

void Memory::FreeInternal(void** pointer)
{
	if (IsAllocated(*pointer))
	{
		if (region1kb.WithinRegion(*pointer)) { region1kb.Free(pointer); }
		else if (region16kb.WithinRegion(*pointer)) { region16kb.Free(pointer); }
		else if (region256kb.WithinRegion(*pointer)) { region256kb.Free(pointer); }
		else if (region4mb.WithinRegion(*pointer)) { region4mb.Free(pointer); }
	}
	else { free(*pointer); }
}

bool Memory::IsAllocated(void* pointer)
{
	return pointer != nullptr && pointer >= memory && pointer < memory + DynamicMemorySize;
}

void* Memory::AllocateAligned(U64 size, U64 alignment)
{
	return _aligned_malloc(size, alignment);
}

void Memory::FreeAligned(void* pointer)
{
	_aligned_free(pointer);
}

NH_NODISCARD __declspec(allocator) void* operator new(U64 size) { if (size == 0) { return nullptr; } U8* ptr = nullptr; Memory::Allocate(&ptr, size); return ptr; }
NH_NODISCARD __declspec(allocator) void* operator new[](U64 size) { if (size == 0) { return nullptr; } U8* ptr = nullptr; Memory::Allocate(&ptr, size); return ptr; }
NH_NODISCARD __declspec(allocator) void* operator new(U64 size, Align alignment) { if (size == 0) { return nullptr; } return Memory::AllocateAligned(size, *alignment); }
NH_NODISCARD __declspec(allocator) void* operator new[](U64 size, Align alignment) { if (size == 0) { return nullptr; } return Memory::AllocateAligned(size, *alignment); }
void operator delete(void* ptr) noexcept { Memory::Free(&ptr); }
void operator delete[](void* ptr) noexcept { Memory::Free(&ptr); }
void operator delete(void* ptr, Align alignment) noexcept { Memory::FreeAligned(ptr); }
void operator delete[](void* ptr, Align alignment) noexcept { Memory::FreeAligned(ptr); }
void operator delete(void* ptr, U64 size) noexcept { Memory::Free(&ptr); }
void operator delete[](void* ptr, U64 size) noexcept { Memory::Free(&ptr); }
void operator delete(void* ptr, U64 size, Align alignment) noexcept { Memory::FreeAligned(ptr); }
void operator delete[](void* ptr, U64 size, Align alignment) noexcept { Memory::FreeAligned(ptr); }