#pragma once

#include "Defines.hpp"

#include "Containers.hpp"

struct FileData;

struct DataReader
{
	DataReader(void* buffer, U64 size);
	DataReader(FileData& data);

	template<class Type> U64 Read(Type& value);
	U64 Read(void* buffer, U64 size);

	template<typename Ch = C>
	Containers::StringBase<Ch> ReadString();

	template<typename Ch = C>
	Containers::StringBase<Ch> ReadLine();

	void Reset();
	void Seek(I64 offset);
	void SeekFromStart(I64 offset);

	U8* Data();
	U8* Data() const;
	U8* Pointer() const;
	I64 Size();

private:
	U8* data;
	U8* pointer;
	U64 size;
};

template<class Type>
inline U64 DataReader::Read(Type& value)
{
	return Read(&value, sizeof(Type));
}

template<typename Ch>
inline Containers::StringBase<Ch> DataReader::ReadString()
{
	Containers::StringBase<Ch> str((Ch*)pointer);
	pointer += (str.size() + 1) * sizeof(Ch);

	return Move(str);
}

template<typename Ch>
inline Containers::StringBase<Ch> DataReader::ReadLine()
{
	Ch* it = (Ch*)pointer;

	while ((it - data) * sizeof(Ch) < size && *it != '\n' && *it != '\r') { ++it; }

	String str((Ch*)pointer, it - (Ch*)pointer);
	pointer = (U8*)(it + 1);

	return Move(str);
}