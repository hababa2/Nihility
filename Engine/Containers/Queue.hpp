#pragma once

#include "Defines.hpp"

#include "Platform\Memory.hpp"

template<class Type>
struct Queue
{
public:
	Queue();
	Queue(U64 capacity);
	Queue(const Queue& other);
	Queue(Queue&& other);

	Queue& operator=(const Queue& other);
	Queue& operator=(Queue&& other);

	~Queue();
	void Destroy();
	void Clear();

	void Push(const Type& value);
	void Push(Type&& value) noexcept;
	bool Pop(Type& value);

	void Reserve(U64 capacity);

	U64 Capacity() const;
	U64 Size() const;
	bool Empty() const;
	bool Full() const;

private:
	U64 capacity = 0;
	U64 capacityMask = 0;
	U64 front = 0;
	U64 back = 0;
	Type* array = nullptr;
};

template<class Type>
inline Queue<Type>::Queue()
{
	capacity = BitFloor(Memory::Allocate(&array, capacity));
	capacityMask = capacity - 1;
}

template<class Type>
inline Queue<Type>::Queue(U64 cap)
{
	capacity = BitFloor(Memory::Allocate(&array, cap));
	capacityMask = capacity - 1;
}

template<class Type>
inline Queue<Type>::Queue(const Queue<Type>& other) : capacity(other.capacity), capacityMask(other.capacity), front(other.front), back(other.back)
{
	Memory::Allocate(&array, capacity);
	CopyData(array, other.array, capacity);
}

template<class Type>
inline Queue<Type>::Queue(Queue<Type>&& other) : capacity(other.capacity), capacityMask(other.capacity), front(other.front), back(other.back), array(other.array)
{
	other.array = nullptr;
	other.Destroy();
}

template<class Type>
inline Queue<Type>& Queue<Type>::operator=(const Queue<Type>& other)
{
	if (array) { Memory::Free(&array); }
	front = other.front;
	back = other.back;
	capacity = other.capacity;
	capacityMask = other.capacityMask;
	Memory::Allocate(&array, capacity);

	CopyData(array, other.array, capacity);

	return *this;
}

template<class Type>
inline Queue<Type>& Queue<Type>::operator=(Queue<Type>&& other)
{
	if (array) { Memory::Free(&array); }
	front = other.front;
	back = other.back;
	capacity = other.capacity;
	capacityMask = other.capacityMask;
	array = other.array;

	other.array = nullptr;
	other.Destroy();

	return *this;
}

template<class Type>
inline Queue<Type>::~Queue() { Destroy(); }

template<class Type>
inline void Queue<Type>::Destroy()
{
	front = 0;
	back = 0;
	capacity = 0;
	capacityMask = 0;
	if (array) { Memory::Free(&array); }
}

template<class Type>
inline void Queue<Type>::Clear()
{
	front = 0;
	back = 0;
}

template<class Type>
inline void Queue<Type>::Push(const Type& value)
{
	if (Full()) { Reserve(capacity + 1); }

	Construct<Type>(array + front++, value);
}

template<class Type>
inline void Queue<Type>::Push(Type&& value) noexcept
{
	if (Full()) { Reserve(capacity + 1); }

	Construct<Type>(array + front++, Move(value));
}

template<class Type>
inline bool Queue<Type>::Pop(Type& value)
{
	if (!Empty()) { Construct<Type>(&value, Move(array[back++ & capacityMask])); return true; }

	return false;
}

template<class Type>
inline void Queue<Type>::Reserve(U64 cap)
{
	capacity = BitFloor(Memory::Reallocate(&array, cap));
	capacityMask = capacity - 1;
}

template<class Type>
inline U64 Queue<Type>::Capacity() const { return capacity; }

template<class Type>
inline U64 Queue<Type>::Size() const { return front - back; }

template<class Type>
inline bool Queue<Type>::Empty() const { return front == back; }

template<class Type>
inline bool Queue<Type>::Full() const { return front == back + capacity; }