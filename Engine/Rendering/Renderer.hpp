#pragma once

#include "VulkanDefines.hpp"

#include "Instance.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "CommandBuffer.hpp"
#include "Buffer.hpp"
#include "Renderpass.hpp"
#include "PipelineLayout.hpp"
#include "Pipeline.hpp"
#include "DescriptorSet.hpp"
#include "Shader.hpp"
#include "Camera.hpp"

#include "Resources/ResourceDefines.hpp"
#include "Resources/Texture.hpp"
#include "Resources/World.hpp"
#include "Containers/String.hpp"
#include "Containers/Deque.hpp"

enum VkResult;
enum VkObjectType;
struct VmaAllocator_T;
struct VmaAllocation_T;
struct VkDescriptorPool_T;
struct VkCommandBuffer_T;
struct VkSemaphore_T;
struct VkImage_T;
struct VkImageView_T;
struct VkFramebuffer_T;
struct VkFence_T;
struct VkCommandPool_T;
struct VkSwapchainKHR_T;
struct VkAllocationCallbacks;
struct VkDebugUtilsObjectNameInfoEXT;

using SetObjectNameFN = VkResult(__stdcall*)(VkDevice_T* device, const VkDebugUtilsObjectNameInfoEXT* nameInfo);

class NH_API Renderer
{
public:
	static U32 ImageIndex();
	static U32 PreviousFrame();
	static U32 AbsoluteFrame();

	static Vector4Int RenderSize();

	static const GlobalPushConstant* GetGlobalPushConstant();
	static VkSemaphore_T* RenderFinished();
	static const Device& GetDevice();
	static void NameResource(VkObjectType type, void* object, const String& name);

private:
	static bool Initialize(const StringView& name, U32 version);
	static void Shutdown();

	static void Update();
	static bool Synchronize();
	static void FirstTransfer();
	static void SubmitTransfer();
	static void Submit();

	static bool InitializeVma();
	static bool CreateSurfaceInfo();
	static bool CreateColorTextures();
	static bool CreateDepthTextures();
	static bool CreateDescriptorPool();
	static bool CreateRenderpasses();
	static bool CreateSynchronization();
	static bool CreateStagingBuffers();

	static void ScheduleDestruction(Swapchain& swapchain);
	static void ScheduleDestruction(Texture& texture);
	static void ScheduleDestruction(Buffer& buffer);
	static void ScheduleDestruction(Pipeline& pipeline);
	static void ScheduleDestruction(DescriptorSet& descriptorSet);
	static void DestroyObjects();

	static bool RecreateSwapchain();

	static bool UploadTexture(Resource<Texture>& texture, void* data, const Sampler& sampler);
	static void DestroyTexture(Resource<Texture>& texture);

	//Resources
	static VmaAllocator_T* vmaAllocator;
	static VkAllocationCallbacks* allocationCallbacks;
	static VkDescriptorPool_T* vkDescriptorPool;
	static VkDescriptorPool_T* vkBindlessDescriptorPool;
	static DescriptorSet descriptorSet;
	static Texture colorTextures[MaxSwapchainImages];
	static Texture depthTextures[MaxSwapchainImages];
	static Buffer stagingBuffers[MaxSwapchainImages];
	static U32 surfaceFormat;
	static U32 surfaceColorSpace;
	static U32 imageCount;
	static U32 presentMode;
	static U32 surfaceWidth;
	static U32 surfaceHeight;

	//Vulkan Objects
	static Instance instance;
	static Device device;
	static Swapchain swapchain;
	static Renderpass renderpass;

	//Recording
	static Vector<VkCommandBuffer_T*> commandBuffers[MaxSwapchainImages];
	static GlobalPushConstant globalPushConstant;

	//Synchronization
	static U32 imageIndex;
	static U32 frameIndex;
	static U32 previousFrame;
	static U32 absoluteFrame;
	static VkSemaphore_T* imageAcquired[MaxSwapchainImages];
	static VkSemaphore_T* transferFinished[MaxSwapchainImages];
	static VkSemaphore_T* renderFinished[MaxSwapchainImages];
	static VkSemaphore_T* presentReady[MaxSwapchainImages];
	static U64 renderWaitValues[MaxSwapchainImages];
	static U64 transferWaitValues[MaxSwapchainImages];

	static Vector<SwapchainDestructionData> swapchainsToDestroy;
	static Vector<TextureDestructionData> texturesToDestroy;
	static Vector<BufferDestructionData> buffersToDestroy;
	static Vector<PipelineDestructionData> pipelinesToDestroy;
	static Vector<DescriptorSetDestructionData> descriptorSetsToDestroy;

	//Debug
#ifdef NH_DEBUG
	static SetObjectNameFN SetObjectName;
#endif

	friend class Engine;
	friend class Resources;
	friend class CommandBufferRing;
	friend class World;
	friend struct Instance;
	friend struct PhysicalDevice;
	friend struct Device;
	friend struct Swapchain;
	friend struct CommandBuffer;
	friend struct Buffer;
	friend struct Renderpass;
	friend struct PipelineLayout;
	friend struct Pipeline;
	friend struct Shader;
	friend struct FrameBuffer;
	friend struct DescriptorSet;
	friend struct Material;

	STATIC_CLASS(Renderer);
};