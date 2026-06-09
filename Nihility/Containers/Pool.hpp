#pragma once

#include "Defines.hpp"

#include <atomic>
#include <array>
#include <new>
#include <cstddef>

template <typename T, U64 PoolSize>
struct LockFreePool
{
private:
	struct Slot {
		alignas(std::hardware_destructive_interference_size) std::atomic<bool> inUse{ false };
		alignas(T) U8 data[sizeof(T)];
	};

	std::array<Slot, PoolSize> slots;

public:
	LockFreePool() = default;
	~LockFreePool() = default;

	template<typename... Args>
	T* Allocate(Args&&... args)
	{
		for (U64 i = 0; i < PoolSize; ++i)
		{
			bool expected = false;
			if (slots[i].inUse.compare_exchange_strong(expected, true,
				std::memory_order_acquire,
				std::memory_order_relaxed))
			{
				return new (&slots[i].data) T(Forward<Args>(args)...);
			}
		}

		BreakPoint;
		return nullptr;
	}

	void Free(T* ptr)
	{
		if (!ptr) { return; }

		U64 offset = reinterpret_cast<U8*>(ptr) - reinterpret_cast<U8*>(slots.data());
		U64 index = offset / sizeof(Slot);

		ptr->~T();

		slots[index].inUse.store(false, std::memory_order_release);
	}
};