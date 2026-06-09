#pragma once

#include "Defines.hpp"

#include <cstddef>
#include <memory>
#include <atomic>

struct FrameArena
{
public:
	explicit FrameArena(U64 capacity) : capacity(capacity), offset(0)
	{
		memory = std::make_unique<U8[]>(capacity);
	}

	void* Allocate(U64 size, U64 alignment = alignof(std::max_align_t))
	{
		U64 currentOffset = offset.load(std::memory_order_relaxed);
		U64 padding = 0;
		U64 nextOffset = 0;

		do
		{
			padding = (alignment - (currentOffset % alignment)) % alignment;
			nextOffset = currentOffset + padding + size;
		} while (!offset.compare_exchange_weak(currentOffset, nextOffset,
			std::memory_order_acquire,
			std::memory_order_relaxed));

		return memory.get() + currentOffset + padding;
	}

	void Reset()
	{
		offset.store(0, std::memory_order_release);
	}

private:
	std::unique_ptr<U8[]> memory;
	U64 capacity;
	std::atomic<U64> offset;
};