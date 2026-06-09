#include "DataReader.hpp"

#include "File.hpp"

DataReader::DataReader(void* buffer, U64 size) : data((U8*)buffer), pointer(data), size(size) {}

DataReader::DataReader(FileData& data) : data((U8*)data.buffer), pointer(this->data), size(data.dataSize) {}

U64 DataReader::Read(void* buffer, U64 size)
{
	U64 t = pointer - data;
	U64 t2 = t + size;

	if ((pointer - data) + size > this->size) { return 0; }

	memcpy(buffer, pointer, size);
	pointer += size;

	return size;
}

void DataReader::Reset()
{
	pointer = data;
}

void DataReader::Seek(I64 offset)
{
	pointer += offset;
}

void DataReader::SeekFromStart(I64 offset)
{
	pointer = data + offset;
}

U8* DataReader::Data()
{
	return data;
}

U8* DataReader::Data() const
{
	return data;
}

U8* DataReader::Pointer() const
{
	return pointer;
}

I64 DataReader::Size()
{
	return size;
}
