#pragma once

#include "RenderingDefines.hpp"

#include "RenderTarget.hpp"
#include "FrameArena.hpp"
#include "Instance.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "Shader.hpp"
#include "Buffer.hpp"

#include "Core/Containers.hpp"
#include "Core/Function.hpp"
#include "Resources/Texture.hpp"
#include "Components/Registry.hpp"

#include "ShaderIncludes/ShaderTypes.h"

struct RenderPacket
{
	FrameArena arena;

	SpriteData* sprites;
	std::atomic<U64> spriteCount;
	U64 maxSprites;

	RenderPacket(U64 arenaSize, U64 maxSpriteCapacity) : arena(arenaSize), spriteCount(0), maxSprites(maxSpriteCapacity)
	{
		sprites = static_cast<SpriteData*>(arena.Allocate(sizeof(SpriteData) * maxSprites, alignof(SpriteData)));
	}

	void PushSprite(const SpriteData& instance)
	{
		U64 index = spriteCount.fetch_add(1, std::memory_order_relaxed);
		if (index < maxSprites) { sprites[index] = instance; }
	}

	void Clear()
	{
		spriteCount.store(0, std::memory_order_relaxed);
	}

	std::span<const SpriteData> GetActiveSprites() const
	{
		return { sprites, spriteCount.load(std::memory_order_relaxed) };
	}
};

struct RenderExtractor
{
public:
	RenderExtractor() : bufferedPackets{ RenderPacket(Megabytes(16), 50000), RenderPacket(Megabytes(16), 50000) } {}

	void BeginExtraction()
	{
		std::swap(writeIndex, readIndex);

		GetWritePacket().Clear();
	}

	void SubmitSprite(const SpriteData& sprite)
	{
		GetWritePacket().PushSprite(sprite);
	}

	void FinishExtraction()
	{
		U64 currentCount = GetWritePacket().spriteCount.load(std::memory_order_relaxed);
		SpriteData* currentSprites = GetWritePacket().sprites;

		std::sort(currentSprites, currentSprites + currentCount,
			[](const SpriteData& a, const SpriteData& b)
		{
			return a.zIndex < b.zIndex;
		});
	}

	RenderPacket& GetWritePacket() { return bufferedPackets[writeIndex]; }
	const RenderPacket& GetReadPacket() const { return bufferedPackets[readIndex]; }

private:
	Array<RenderPacket, 2> bufferedPackets;

	U32 writeIndex = 0;
	U32 readIndex = 1;
};

struct VkCommandPool_T;
struct VkCommandBuffer_T;
struct VkAllocationCallbacks;

struct WorkerCommandContext
{
	VkCommandPool_T* graphicsPools[MaxFramesInFlight];
	Vector<VkCommandBuffer_T*> commandBuffers[MaxFramesInFlight];

	VkCommandPool_T* transferPool;

	U32 currentBufferIndex = 0;

	void Create(Device& device, const VkAllocationCallbacks* allocator, U32 graphicsQueueIndex, U32 transferQueueIndex, U32 preallocateCount);
	void Destroy(Device& device, const VkAllocationCallbacks* allocator);

	void ResetFrame(Device& device, U32 frameIndex);

	VkCommandBuffer_T* GetNextCommandBuffer(U32 frameIndex);
};

enum VkFormat;
enum VkImageLayout;
enum VmaMemoryUsage;
struct VmaAllocation_T;
struct VmaAllocator_T;
struct VkImage_T;
struct VkBuffer_T;
struct VkPipeline_T;
struct VkSemaphore_T;
struct VkDescriptorSet_T;
struct VkPipelineLayout_T;
struct VkDescriptorPool_T;
struct VkDescriptorSetLayout_T;

class NH_API Renderer
{
public:
	static Buffer CreateBuffer(U64 size, U64 usage, VmaMemoryUsage memoryUsage);
	static void DestroyBuffer(Buffer& buffer);
	static void UploadToBuffer(Buffer& dstBuffer, const void* data, U64 size, U32 threadId = 0);

	static VkDescriptorSet_T* GlobalBindlessSet();
	static U32 FrameIndex();

	static void SubmitSprite(const SpriteData& sprite);

	static void SetActiveCamera(Entity camera);
	static glm::mat4 GetViewProjectionMatrix();
	static glm::vec4 RenderArea();

	static void NameAllocation(VmaAllocation_T* allocation, const WString& name);
	static void NameAllocation(VmaAllocation_T* allocation, const String& name);

private:
	static bool Initialize(const StringView& name, U32 version);
	static void Stop();
	static void Shutdown();

	static bool BeginFrame();
	static void EndFrame();

	static bool InitializeVma();
	static bool CreateSurfaceInfo();
	static bool CreateDescriptorSet();
	static bool CreateShaders();
	static bool CreateStorageBuffers();
	static bool CreateSynchronization();
	static bool CreateCommandBuffers();

	static void DestroyTexture(Texture& texture);

	static void DeferDestruction(Function<void()>&& function);
	static void FlushDeletionQueue();

	static void RecreateSwapchain();
	static void TransitionImageLayout(VkCommandBuffer_T* cmd, VkImage_T* image, VkImageLayout oldLayout, VkImageLayout newLayout,
		U64 srcStageMask, U64 srcAccessMask, U64 dstStageMask, U64 dstAccessMask);
	static VkCommandBuffer_T* RecordSecondaryCommandBuffer(U32 threadId, U32 frameIndex, std::span<const SpriteData> sprites);
	static VkCommandBuffer_T* BeginSingleTimeCommands(VkCommandPool_T* commandPool);
	static void EndSingleTimeCommands(VkCommandPool_T* commandPool, VkCommandBuffer_T* commandBuffer);
	static bool UploadTexture(std::shared_ptr<Texture> texture, void* data, U32 threadId);

	static Instance instance;
	static Device device;
	static Swapchain swapchain;
	static Shader spriteShader;

	static VkDescriptorSetLayout_T* globalBindlessSetLayout;
	static VkDescriptorPool_T* globalDescriptorPool;
	static VkDescriptorSet_T* globalBindlessSet;

	static VkDescriptorSetLayout_T* ssboSetLayout;
	static VkDescriptorSet_T* ssboDescriptorSets[MaxFramesInFlight];

	static VkBuffer_T* spriteBuffers[MaxFramesInFlight];
	static VmaAllocation_T* spriteAllocations[MaxFramesInFlight];
	static void* spriteMappedData[MaxFramesInFlight];

	static VkCommandPool_T* primaryPools[MaxFramesInFlight];
	static VkCommandBuffer_T* primaryBuffers[MaxFramesInFlight];

	static U32 surfaceFormat;
	static U32 surfaceColorSpace;
	static U32 imageCount;
	static U32 presentMode;
	static U32 surfaceWidth;
	static U32 surfaceHeight;

	static U32 imageIndex;
	static U64 currentFrameNumber;
	static VkSemaphore_T* frameTimelineSemaphore;
	static VkSemaphore_T* imageAcquiredSemaphores[MaxFramesInFlight];
	static VkSemaphore_T* renderFinishedSemaphores[MaxSwapchainImages];

	static U32 workerThreadCount;
	static Vector<WorkerCommandContext> workerContexts;
	static RenderExtractor extractor;

	static VmaAllocator_T* vmaAllocator;
	static VkAllocationCallbacks* allocationCallbacks;

	static Vector<Function<void()>> pendingDeletions;
	static Vector<Function<void()>> deletionQueues[MaxFramesInFlight];

	static Entity activeCamera;

#ifdef NH_DEBUG
	static RenderTarget viewportTarget;
#endif

	friend class Nihility;
	friend class Resources;
	friend class Tilemap;
	friend class Editor;
	friend struct RenderTarget;
	friend struct Swapchain;
	friend struct Instance;
	friend struct Device;
	friend struct Shader;

	STATIC_CLASS(Renderer);
};