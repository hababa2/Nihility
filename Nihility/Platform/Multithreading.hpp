#pragma once

#include "Defines.hpp"

#include <atomic>
#include <mutex>

struct SpinLock {
	std::atomic_flag locked = ATOMIC_FLAG_INIT;
public:
	void lock() { while (locked.test_and_set(std::memory_order_acquire)) { _mm_pause(); } }
	void unlock() { locked.clear(std::memory_order_release); }
};