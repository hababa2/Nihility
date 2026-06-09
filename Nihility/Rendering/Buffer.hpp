#pragma once

#include "Defines.hpp"

#include <string.h>

struct VkBuffer_T;
struct VmaAllocation_T;

struct NH_API Buffer
{
	VkBuffer_T* vkBuffer = nullptr;
	VmaAllocation_T* allocation = nullptr;
	void* mappedData = nullptr;
	U64 size = 0;

	void Write(const void* data, U64 dataSize, U64 offset = 0)
	{
		if (mappedData)
		{
			memcpy(static_cast<U8*>(mappedData) + offset, data, dataSize);
		}
	}
};