#include "DataWriter.hpp"

void DataWriter::Write(const void* data, U64 size)
{
	const U8* bytes = (const U8*)&data;
	buffer.insert(buffer.end(), bytes, bytes + size);
}

void DataWriter::Append(const DataWriter& other)
{
	buffer.insert(buffer.end(), other.buffer.begin(), other.buffer.end());
}

const U8* DataWriter::Data() const
{
	return buffer.data();
}

U64 DataWriter::Size() const
{
	return buffer.size();
}