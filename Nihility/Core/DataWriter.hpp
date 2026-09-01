#pragma once

#include "Defines.hpp"

#include "Core/Containers.hpp"

struct NH_API DataWriter
{
public:
	template<typename T>
	void Write(const T& data);
	template<typename Ch>
	void Write(const Containers::StringBase<Ch>& str);
	void Write(const void* data, U64 size);
	void Append(const DataWriter& other);

	const U8* Data() const;
	U64 Size() const;

private:
	Vector<U8> buffer;
};

template<typename T>
inline void DataWriter::Write(const T& data)
{
	U8* bytes = (U8*)&data;
	buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
}

template<typename Ch>
inline void DataWriter::Write(const Containers::StringBase<Ch>& str)
{
	const U8* bytes = (const U8*)str.data();
	U64 length = (str.size() + 1) * sizeof(Ch);
	buffer.insert(buffer.end(), bytes, bytes + length);
}