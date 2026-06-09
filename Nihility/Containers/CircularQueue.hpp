#pragma once

#include "Defines.hpp"

#include <atomic>

template<class T>
struct CircularQueue
{
private:
	struct Cell
	{
		std::atomic<U64> sequence;
		T data;
	};

public:
	CircularQueue() = default;

	void Create(U64 capacity)
	{
		bufferMask = capacity - 1;

		buffer = (Cell*)_aligned_malloc(capacity * sizeof(Cell), CacheLineSize);

		for (U64 i = 0; i < capacity; ++i)
		{
			buffer[i].sequence.store(i, std::memory_order_relaxed);
		}
	}

	void Destroy()
	{
		if (buffer)
		{
			_aligned_free(buffer);
			buffer = nullptr;
		}
	}

	bool Push(const T& data)
	{
		Cell* cell;
		U64 pos = enqueuePos.load(std::memory_order_relaxed);

		while (true)
		{
			cell = &buffer[pos & bufferMask];
			U64 seq = cell->sequence.load(std::memory_order_acquire);
			I64 dif = (I64)seq - (I64)pos;

			if (dif == 0)
			{
				if (enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) { break; }
			}
			else if (dif < 0) { return false; }
			else { pos = enqueuePos.load(std::memory_order_relaxed); }
		}

		cell->data = data;
		cell->sequence.store(pos + 1, std::memory_order_release);
		return true;
	}

	bool Pop(T& data)
	{
		Cell* cell;
		U64 pos = dequeuePos.load(std::memory_order_relaxed);

		while (true)
		{
			cell = &buffer[pos & bufferMask];
			U64 seq = cell->sequence.load(std::memory_order_acquire);
			I64 dif = (I64)seq - (I64)(pos + 1);

			if (dif == 0)
			{
				if (dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) { break; }
			}
			else if (dif < 0) { return false; }
			else { pos = dequeuePos.load(std::memory_order_relaxed); }
		}

		data = cell->data;
		cell->sequence.store(pos + bufferMask + 1, std::memory_order_release);
		return true;
	}

private:
	static constexpr U64 CacheLineSize = 64;

	typedef char CacheLinePad[CacheLineSize];

	CacheLinePad pad0{ 0 };
	U64 bufferMask = 0;
	Cell* buffer = nullptr;

	CacheLinePad pad1{ 0 };
	std::atomic<U64> enqueuePos{ 0 };

	CacheLinePad pad2{ 0 };
	std::atomic<U64> dequeuePos{ 0 };

	CacheLinePad pad3{ 0 };
};