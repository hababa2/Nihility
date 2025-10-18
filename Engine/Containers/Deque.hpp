#pragma once

#include "Defines.hpp"

#include "Platform\Memory.hpp"

template<class Type>
struct Deque
{
public:
	Deque();
	Deque(U64 capacity);
	Deque(const Deque& other);
	Deque(Deque&& other);

	Deque& operator=(const Deque& other);
	Deque& operator=(Deque&& other);

	~Deque();
	void Destroy();
	void Clear();

	void PushFront(const Type& value);
	void PushFront(Type&& value) noexcept;
	void PushBack(const Type& value);
	void PushBack(Type&& value) noexcept;
	bool PopFront(Type& value);
	bool PopBack(Type& value);
	void PopFront();
	void PopBack();

	const Type& operator[](U64 i) const;
	Type& operator[](U64 i);
	const Type& Front() const;
	Type& Front();
	const Type& Back() const;
	Type& Back();

	U64 Capacity() const;
	U64 Size() const;
	bool Empty() const;
	bool Full() const;

private:
	U64 capacity = 0;
	U64 size = 0;
	U64 front = U64_MAX;
	U64 back = U64_MAX;
	Type* array = nullptr;
};

template<class Type>
inline Deque<Type>::Deque()
{
	capacity = BitFloor(Memory::Allocate(&array, capacity));
}

template<class Type>
inline Deque<Type>::Deque(U64 cap)
{
	capacity = BitFloor(Memory::Allocate(&array, cap));
}

template<class Type>
inline Deque<Type>::Deque(const Deque<Type>& other) : capacity(other.capacity), size(other.size), front(other.front), back(other.back)
{
	Memory::Allocate(&array, capacity);
	CopyData(array, other.array, capacity);
}

template<class Type>
inline Deque<Type>::Deque(Deque<Type>&& other) : capacity(other.capacity), size(other.size), front(other.front), back(other.back), array(other.array)
{
	other.array = nullptr;
	other.Destroy();
}

template<class Type>
inline Deque<Type>& Deque<Type>::operator=(const Deque<Type>& other)
{
	if (array) { Memory::Free(&array); }
	front = other.front;
	back = other.back;
	size = other.size;
	capacity = other.capacity;
	Memory::Allocate(&array, capacity);

	CopyData(array, other.array, capacity);

	return *this;
}

template<class Type>
inline Deque<Type>& Deque<Type>::operator=(Deque<Type>&& other)
{
	if (array) { Memory::Free(&array); }
	front = other.front;
	back = other.back;
	size = other.size;
	capacity = other.capacity;
	array = other.array;

	other.array = nullptr;
	other.Destroy();

	return *this;
}

template<class Type>
inline Deque<Type>::~Deque() { Destroy(); }

template<class Type>
inline void Deque<Type>::Destroy()
{
	front = U64_MAX;
	back = U64_MAX;
	size = 0;
	capacity = 0;
	if (array) { Memory::Free(&array); }
}

template<class Type>
inline void Deque<Type>::Clear()
{
	front = U64_MAX;
	back = U64_MAX;
}

template<class Type>
inline void Deque<Type>::PushFront(const Type& value)
{
	if (Full()) { return; }

	++size;
	if (front == U64_MAX)
	{
		front = 0;
		back = 0;
		Construct<Type>(array + front, value);
	}
	else
	{
		if (front == 0) { front = capacity - 1; }
		else { --front; }

		Construct<Type>(array + front, value);
	}
}

template<class Type>
inline void Deque<Type>::PushFront(Type&& value) noexcept
{
	if (Full()) { return; }

	++size;
	if (front == U64_MAX)
	{
		front = 0;
		back = 0;
		Construct<Type>(array + front, Move(value));
	}
	else
	{
		if (front == 0) { front = capacity - 1; }
		else { --front; }

		Construct<Type>(array + front, Move(value));
	}
}

template<class Type>
inline void Deque<Type>::PushBack(const Type& value)
{
	if (Full()) { return; }

	++size;
	if (back == U64_MAX)
	{
		front = back = 0;
		Construct<Type>(array + back, value);
	}
	else
	{
		if (back == capacity - 1) { back = 0; }
		else { ++back; }

		Construct<Type>(array + back, value);
	}
}

template<class Type>
inline void Deque<Type>::PushBack(Type&& value) noexcept
{
	if (Full()) { return; }

	++size;
	if (back == U64_MAX)
	{
		front = back = 0;
		Construct<Type>(array + back, Move(value));
	}
	else
	{
		if (back == capacity - 1) { back = 0; }
		else { ++back; }

		Construct<Type>(array + back, Move(value));
	}
}

template<class Type>
inline bool Deque<Type>::PopFront(Type& value)
{
	if (Empty()) { return false; }

	Construct<Type>(&value, Move(array[front]));

	--size;
	if (front == back)
	{
		front = U64_MAX;
		back = U64_MAX;
	}
	else
	{
		if (front == capacity - 1) { front = 0; }
		else { ++front; }
	}

	return true;
}

template<class Type>
inline bool Deque<Type>::PopBack(Type& value)
{
	if (Empty()) { return false; }

	Construct<Type>(&value, Move(array[back]));

	--size;
	if (front == back)
	{
		front = U64_MAX;
		back = U64_MAX;
	}
	else
	{
		if (back == 0) { back = capacity - 1; }
		else { --back; }
	}

	return true;
}

template<class Type>
inline void Deque<Type>::PopFront()
{
	if (Empty()) { return; }

	--size;
	if (front == back)
	{
		front = U64_MAX;
		back = U64_MAX;
	}
	else
	{
		if (front == capacity - 1) { front = 0; }
		else { ++front; }
	}
}

template<class Type>
inline void Deque<Type>::PopBack()
{
	if (Empty()) { return; }

	--size;
	if (front == back)
	{
		front = U64_MAX;
		back = U64_MAX;
	}
	else
	{
		if (back == 0) { back = capacity - 1; }
		else { --back; }
	}
}

template<class Type>
inline const Type& Deque<Type>::operator[](U64 i) const { return array[i]; }

template<class Type>
inline Type& Deque<Type>::operator[](U64 i) { return array[i]; }

template<class Type>
inline const Type& Deque<Type>::Front() const
{
	return array[front];
}

template<class Type>
inline Type& Deque<Type>::Front()
{
	return array[front];
}

template<class Type>
inline const Type& Deque<Type>::Back() const
{
	return array[back];
}

template<class Type>
inline Type& Deque<Type>::Back()
{
	return array[back];
}

template<class Type>
inline U64 Deque<Type>::Capacity() const { return capacity; }

template<class Type>
inline U64 Deque<Type>::Size() const { return size; }

template<class Type>
inline bool Deque<Type>::Empty() const
{
	return front == U64_MAX;
}

template<class Type>
inline bool Deque<Type>::Full() const
{
	return (back + 1) % capacity == front;
}