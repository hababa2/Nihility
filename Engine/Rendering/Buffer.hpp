#pragma once

#include "Defines.hpp"

#include "Resources/ResourceDefines.hpp"
#include "Containers/Vector.hpp"
#include "Math/Math.hpp"

struct VmaAllocation_T;
struct VkSemaphore_T;
struct VkBuffer_T;
struct VkDescriptorSet_T;

enum class NH_API BufferType
{
	Vertex,
	Index,
	Storage,
	Uniform,
	Staging,
	DrawIndirect
};

struct BufferDestructionData
{
	VkBuffer_T* vkBuffer = nullptr;
	VmaAllocation_T* bufferAllocation = nullptr;
	VkBuffer_T* vkBufferStaging = nullptr;
	VmaAllocation_T* stagingBufferAllocation = nullptr;
};

struct NH_API Buffer
{
public:
	bool Create(BufferType type, U64 size = 1024);
	void Destroy();

	bool UploadVertexData(const void* vertexData, U64 size, U64 offset = 0);
	bool UploadIndexData(const void* indexData, U64 size, U64 offset = 0);
	bool UploadStorageData(const void* storageData, U64 size, U64 offset = 0);
	bool UploadUniformData(const void* uniformData, U64 size, U64 offset = 0);
	bool UploadStagingData(const void* stagingData, U64 size, U64 offset = 0);

	void Clear();

	U64 StagingPointer() const;
	U64 Size() const;
	U64 Offset() const;

	operator VkBuffer_T*() const;

private:
	bool CheckForResize(U64 bufferSize);

	BufferType type;
	U64 bufferSize = 0;
	U64 stagingPointer = 0;
	U64 dataStart = U64_MAX;
	U64 dataEnd = 0;
	VkBuffer_T* vkBuffer = nullptr;
	VmaAllocation_T* bufferAllocation = nullptr;
	VkBuffer_T* vkBufferStaging = nullptr;
	VmaAllocation_T* stagingBufferAllocation = nullptr;

	VkDescriptorSet_T* descriptorSet = nullptr;

	friend class Renderer;
	friend class Resources;
};