#pragma once

#include "VulkanDefines.hpp"

#include "CommandBuffer.hpp"

#include "Containers/Freelist.hpp"

struct VkCommandPool_T;

class CommandBufferRing
{
private:
	static bool Initialize();
	static void Shutdown();

	static void ResetDrawPool();
	static void ResetDraw(U32 frameIndex);
	static void ResetPool(U32 frameIndex);

	static CommandBuffer& GetDrawCommandBuffer(U32 frameIndex);
	static CommandBuffer& GetWriteCommandBuffer(U32 frameIndex);

	static constexpr inline U8 MaxPools = MaxSwapchainImages;
	static constexpr inline U8 BuffersPerPool = 128;
	static constexpr inline U16 MaxBuffers = BuffersPerPool * MaxPools;

	static VkCommandPool_T* drawCommandPool;
	static VkCommandPool_T* commandPools[MaxPools];
	static CommandBuffer drawCommandBuffers[MaxPools];
	static CommandBuffer commandBuffers[MaxBuffers];
	static Freelist freeCommandBuffers[MaxPools];

	STATIC_CLASS(CommandBufferRing);

	friend class Renderer;
	friend struct Buffer;
};