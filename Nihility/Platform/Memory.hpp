#pragma once

#include "Defines.hpp"
#include "Multithreading.hpp"
#include "Containers/CircularQueue.hpp"

#include <cstringt.h>

template<class Type, class... Parameters>
inline Type& Construct(Type* dst, Parameters&&... parameters) noexcept
{
	return *(new (dst) Type(Forward<Parameters>(parameters)...));
}

template<class Type, class... Parameters>
inline Type& Assign(Type* dst, Parameters&&... parameters) noexcept
{
	return dst->operator=(Forward<Parameters>(parameters)...);
}

template<class Type>
inline Type* CopyData(Type* dst, const Type* src, U64 count)
{
	constexpr U64 size = IsVoid<Type> ? 1 : sizeof(Type);

	if (dst == src) { return dst; }

	Type* dest = dst;

	if (dst > src && dst < src + count)
	{
		dst += count - 1;
		src += count - 1;

		if constexpr (IsNonPrimitive<Type>)
		{
			while (count--) { new (dst--) Type(*src--); }

			return dest;
		}
		else
		{
			return (Type*)memmove(dst, src, count * size);
		}
	}
	else
	{
		if constexpr (IsNonPrimitive<Type>)
		{
			while (count--) { new (dst++) Type(*src++); }

			return dest;
		}
		else
		{
			return (Type*)memcpy(dst, src, count * size);
		}
	}
}

template<class Type>
inline Type* MoveData(Type* dst, Type* src, U64 count)
{
	constexpr U64 size = IsVoid<Type> ? 1 : sizeof(Type);

	if (dst == src) { return dst; }

	Type* dest = dst;

	if (dst > src && dst < src + count)
	{
		dst += count - 1;
		src += count - 1;

		if constexpr (IsNonPrimitive<Type> && IsMoveConstructible<Type>)
		{
			while (count--)
			{
				new (dst--) Type(Move(*src));
				if (!std::is_move_constructible_v<Type> && std::is_destructible_v<Type>) { src->~Type(); }
				--src;
			}

			return dest;
		}
		else
		{
			return (Type*)memmove(dst, src, count * size);
		}
	}
	else
	{
		if constexpr (IsNonPrimitive<Type> && IsMoveConstructible<Type>)
		{
			while (count--)
			{
				new (dst++) Type(Move(*src));
				if (!std::is_move_constructible_v<Type> && std::is_destructible_v<Type>) { src->~Type(); }
				++src;
			}

			return dest;
		}
		else
		{
			return (Type*)memcpy(dst, src, sizeof(Type) * count);
		}
	}
}

NH_API constexpr U64 Kilobytes(U64 n) { return n * 1024Ui64; }
NH_API constexpr U64 Megabytes(U64 n) { return n * 1024Ui64 * 1024Ui64; }
NH_API constexpr U64 Gigabytes(U64 n) { return n * 1024Ui64 * 1024Ui64 * 1024Ui64; }

#ifndef MEMORY_SIZE
static constexpr inline U64 DynamicMemorySize = Gigabytes(1);
#else
static constexpr inline U64 DynamicMemorySize = MEMORY_SIZE;
#endif

enum class RegionSize : U64
{
	KB1 = Kilobytes(1),
	KB16 = Kilobytes(16),
	KB256 = Kilobytes(256),
	MB4 = Megabytes(4),
};

struct Region1kb { U8 memory[*RegionSize::KB1]; };
struct Region16kb { U8 memory[*RegionSize::KB16]; };
struct Region256kb { U8 memory[*RegionSize::KB256]; };
struct Region4mb { U8 memory[*RegionSize::MB4]; };

struct alignas(64) MemoryRegion
{
private:
	void Create(U8* pointer, U32 regionSize, U32 capacity);
	void Destroy();

	bool Allocate(void** pointer);
	bool Reallocate(void** pointer);
	void Free(void** pointer);
	bool WithinRegion(void* pointer);

	CircularQueue<void*> freelist{};

	U32 regionSize = 0;

	U8* region = nullptr;
	U8* endRegion = nullptr;

	friend class Memory;
};

class NH_API Memory
{
public:
	template<Pointer Type> static void Allocate(Type* pointer);
	template<Pointer Type> static U64 Allocate(Type* pointer, U64 count);
	template<Pointer Type> static U64 Reallocate(Type* pointer, U64 count);
	template<Pointer Type> static void Free(Type* pointer);

	static bool IsAllocated(void* pointer);

	static void* AllocateAligned(U64 size, U64 alignment);
	static void FreeAligned(void* pointer);

private:
	static bool Initialize();
	static void Shutdown();

	static U64 AllocateInternal(void** pointer, U64 size, U64 typeSize);
	static U64 ReallocateInternal(void** pointer, U64 size, U64 typeSize);
	static void FreeInternal(void** pointer);

	static U32 allocations;
	static U8* memory;

	static MemoryRegion region1kb;
	static MemoryRegion region16kb;
	static MemoryRegion region256kb;
	static MemoryRegion region4mb;

	static std::atomic<I32> initStatus;

	friend class Nihility;
	friend struct MemoryRegion;

	STATIC_CLASS(Memory);
};

template<Pointer Type>
inline void Memory::Allocate(Type* pointer)
{
	static bool b = Initialize();

	if (IsAllocated(*pointer)) { return; }

	AllocateInternal((void**)pointer, sizeof(RemovePointer<Type>), sizeof(RemovePointer<Type>));
}

template<Pointer Type>
inline U64 Memory::Allocate(Type* pointer, U64 count)
{
	static bool b = Initialize();

	if (IsAllocated(*pointer)) { return Reallocate<Type>(pointer, count); }

	return AllocateInternal((void**)pointer, sizeof(RemovePointer<Type>) * count, sizeof(RemovePointer<Type>));
}

template<Pointer Type>
inline U64 Memory::Reallocate(Type* pointer, U64 count)
{
	static bool b = Initialize();

	if (!IsAllocated(*pointer)) { return Allocate<Type>(pointer, count); }

	U64 size = sizeof(RemovePointer<Type>) * count;

	return ReallocateInternal((void**)pointer, sizeof(RemovePointer<Type>) * count, sizeof(RemovePointer<Type>));
}

template<Pointer Type>
inline void Memory::Free(Type* pointer)
{
	FreeInternal((void**)pointer);
}

enum class Align : U64 {};

NH_NODISCARD __declspec(allocator) void* operator new(U64 size);
NH_NODISCARD __declspec(allocator) void* operator new[](U64 size);
NH_NODISCARD __declspec(allocator) void* operator new(U64 size, Align alignment);
NH_NODISCARD __declspec(allocator) void* operator new[](U64 size, Align alignment);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete(void* ptr, Align alignment) noexcept;
void operator delete[](void* ptr, Align alignment) noexcept;
void operator delete(void* ptr, U64 size) noexcept;
void operator delete[](void* ptr, U64 size) noexcept;
void operator delete(void* ptr, U64 size, Align alignment) noexcept;
void operator delete[](void* ptr, U64 size, Align alignment) noexcept;

template<typename T>
struct Allocator
{
public:
	using value_type = T;

	Allocator() = default;

	template<typename U>
	Allocator(const Allocator<U>& other) noexcept {}

	T* allocate(U64 n)
	{
		if (n == 0) { return nullptr; }

		T* p = static_cast<T*>(::operator new(n * sizeof(T)));
		return p;
	}

	void deallocate(T* p, U64 n) noexcept
	{
		::operator delete(p);
	}

	bool operator==(const Allocator& other) const noexcept { return true; }
	bool operator!=(const Allocator& other) const noexcept { return false; }
};

template <typename T, typename U>
bool operator==(const Allocator<T>& lhs, const Allocator<U>& rhs) noexcept { return true; }

template <typename T, typename U>
bool operator!=(const Allocator<T>& lhs, const Allocator<U>& rhs) noexcept { return false; }
