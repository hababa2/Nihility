#include "Buffer.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"

#include "CommandBufferRing.hpp"
#include "Platform/Memory.hpp"

#include "vma/vk_mem_alloc.h"

bool Buffer::Create(BufferType type, U64 size)
{
	this->type = type;

	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = size,
		.usage = 0,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
	};

	VmaAllocationCreateInfo allocationCreateInfo{
		.flags = 0,
		.usage = VMA_MEMORY_USAGE_UNKNOWN,
		.requiredFlags = 0,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = nullptr,
		.pUserData = nullptr,
		.priority = 0.0f
	};

	switch (type)
	{
	case BufferType::Vertex: {
		bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		VkValidateR(vmaCreateBuffer(Renderer::vmaAllocator, &bufferCreateInfo, &allocationCreateInfo, &vkBuffer, &bufferAllocation, nullptr));

		bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		VkValidateR(vmaCreateBuffer(Renderer::vmaAllocator, &bufferCreateInfo, &allocationCreateInfo, &vkBufferStaging, &stagingBufferAllocation, nullptr));

		bufferSize = size;
	} return true;
	case BufferType::Index: {
		bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		VkValidateR(vmaCreateBuffer(Renderer::vmaAllocator, &bufferCreateInfo, &allocationCreateInfo, &vkBuffer, &bufferAllocation, nullptr));

		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		VkValidateR(vmaCreateBuffer(Renderer::vmaAllocator, &bufferCreateInfo, &allocationCreateInfo, &vkBufferStaging, &stagingBufferAllocation, nullptr));

		bufferSize = size;
	} return true;
	case BufferType::Storage: {
		bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		VkValidateR(vmaCreateBuffer(Renderer::vmaAllocator, &bufferCreateInfo, &allocationCreateInfo, &vkBuffer, &bufferAllocation, nullptr));

		bufferSize = size;
	} return true;
	case BufferType::Uniform: {
		bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		VkValidateR(vmaCreateBuffer(Renderer::vmaAllocator, &bufferCreateInfo, &allocationCreateInfo, &vkBuffer, &bufferAllocation, nullptr));

		bufferSize = size;
	} return true;
	case BufferType::Staging: {
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		VkValidateR(vmaCreateBuffer(Renderer::vmaAllocator, &bufferCreateInfo, &allocationCreateInfo, &vkBuffer, &bufferAllocation, nullptr));
	} return true;
	}

	return false;
}

void Buffer::Destroy()
{
	Renderer::ScheduleDestruction(*this);

	bufferSize = 0;
	vkBuffer = VK_NULL_HANDLE;
	bufferAllocation = nullptr;
	vkBufferStaging = VK_NULL_HANDLE;
	stagingBufferAllocation = nullptr;
}

bool Buffer::UploadVertexData(const void* vertexData, U64 size, U64 offset)
{
	dataStart = Math::Min(dataStart, offset);
	dataEnd = Math::Max(dataEnd, size + offset);

	if (bufferSize < size)
	{
		Destroy();

		if (!Create(type, size)) { return false; }

		bufferSize = size;
	}

	void* data;
	VkValidateR(vmaMapMemory(Renderer::vmaAllocator, stagingBufferAllocation, &data));

	memcpy((U8*)data + offset, vertexData, size);
	vmaUnmapMemory(Renderer::vmaAllocator, stagingBufferAllocation);
	vmaFlushAllocation(Renderer::vmaAllocator, stagingBufferAllocation, 0, size);

	VkBufferMemoryBarrier2 readBarrier{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
		.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
		.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = vkBufferStaging,
		.offset = 0,
		.size = bufferSize
	};

	VkBufferCopy stagingBufferCopy{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = bufferSize
	};

	CommandBuffer& commandBuffer = CommandBufferRing::GetWriteCommandBuffer(Renderer::imageIndex);

	commandBuffer.Begin();
	commandBuffer.BufferToBuffer(vkBufferStaging, vkBuffer, 1, &stagingBufferCopy);
	commandBuffer.PipelineBarrier(0, 1, &readBarrier, 0, nullptr);
	commandBuffer.End();
	Renderer::commandBuffers[Renderer::imageIndex].Push(commandBuffer);

	return true;
}

bool Buffer::UploadIndexData(const void* indexData, U64 size, U64 offset)
{
	dataStart = Math::Min(dataStart, offset);
	dataEnd = Math::Max(dataEnd, size + offset);

	if (bufferSize < size)
	{
		Destroy();
		if (!Create(type, size)) { return false; }

		bufferSize = size;
	}

	void* data;
	VkValidateR(vmaMapMemory(Renderer::vmaAllocator, stagingBufferAllocation, &data));

	memcpy((U8*)data + offset, indexData, size);
	vmaUnmapMemory(Renderer::vmaAllocator, stagingBufferAllocation);
	vmaFlushAllocation(Renderer::vmaAllocator, stagingBufferAllocation, 0, size);

	VkBufferMemoryBarrier2 indexBufferBarrier{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
		.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
		.dstAccessMask = VK_ACCESS_2_INDEX_READ_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = vkBufferStaging,
		.offset = 0,
		.size = bufferSize
	};

	VkBufferCopy stagingBufferCopy{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = bufferSize
	};

	CommandBuffer& commandBuffer = CommandBufferRing::GetWriteCommandBuffer(Renderer::imageIndex);

	commandBuffer.Begin();
	commandBuffer.BufferToBuffer(vkBufferStaging, vkBuffer, 1, &stagingBufferCopy);
	commandBuffer.PipelineBarrier(0, 1, &indexBufferBarrier, 0, nullptr);
	commandBuffer.End();
	Renderer::commandBuffers[Renderer::imageIndex].Push(commandBuffer);

	return true;
}

bool Buffer::UploadStorageData(const void* storageData, U64 size, U64 offset)
{
	dataStart = Math::Min(dataStart, offset);
	dataEnd = Math::Max(dataEnd, size + offset);

	bool bufferResized = false;
	if (size > bufferSize)
	{
		Destroy();
		Create(type, size);
		bufferResized = true;
	}

	void* data;
	VkValidateR(vmaMapMemory(Renderer::vmaAllocator, bufferAllocation, &data));

	memcpy((U8*)data + offset, storageData, size);
	vmaUnmapMemory(Renderer::vmaAllocator, bufferAllocation);
	vmaFlushAllocation(Renderer::vmaAllocator, bufferAllocation, 0, bufferSize);

	return bufferResized;
}

bool Buffer::CheckForResize(U64 size)
{
	if (size > bufferSize)
	{
		Destroy();

		return Create(type, size);
	}

	return false;
}

bool Buffer::UploadUniformData(const void* uniformData, U64 size, U64 offset)
{
	dataStart = Math::Min(dataStart, offset);
	dataEnd = Math::Max(dataEnd, size + offset);

	void* data;
	VkValidateR(vmaMapMemory(Renderer::vmaAllocator, bufferAllocation, &data));

	memcpy((U8*)data + offset, uniformData, size);
	vmaUnmapMemory(Renderer::vmaAllocator, bufferAllocation);
	vmaFlushAllocation(Renderer::vmaAllocator, bufferAllocation, 0, bufferSize);

	return true;
}

bool Buffer::UploadStagingData(const void* stagingData, U64 size, U64 offset)
{
	dataStart = Math::Min(dataStart, offset);
	dataEnd = Math::Max(dataEnd, size + offset);

	stagingPointer = Math::Max(offset + size, stagingPointer);

	void* data;
	VkValidateR(vmaMapMemory(Renderer::vmaAllocator, bufferAllocation, &data));

	memcpy((U8*)data + offset, stagingData, size);
	vmaUnmapMemory(Renderer::vmaAllocator, bufferAllocation);
	vmaFlushAllocation(Renderer::vmaAllocator, bufferAllocation, 0, bufferSize);

	return true;
}

void Buffer::Clear()
{
	dataStart = U64_MAX;
	dataEnd = 0;
}

U64 Buffer::StagingPointer() const
{
	return stagingPointer;
}

U64 Buffer::Size() const
{
	return dataEnd - dataStart;
}

U64 Buffer::Offset() const
{
	return dataStart;
}

Buffer::operator VkBuffer_T* () const
{
	return vkBuffer;
}