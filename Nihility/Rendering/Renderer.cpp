#include "Renderer.hpp"

#include "VulkanInclude.hpp"
#include "Nihility.hpp"
#include "Shader.hpp"
#include "Editor.hpp"
#include "UI.hpp"

#include "Core/Settings.hpp"
#include "Core/Time.hpp"
#include "Platform/Platform.hpp"
#include "Components/Registry.hpp"
#include "Physics/Tilemap.hpp"

#define VMA_VULKAN_VERSION 1003000
#define VMA_IMPLEMENTATION

#ifdef NH_DEBUG
#define VMA_DEBUG_LOG_ENABLED 1
#define VMA_DEBUG_ALWAYS_DEDICATED_MEMORY 0
#define VMA_RECORDING_ENABLED 1
#endif

#include "vma/vk_mem_alloc.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "enkiTS/TaskScheduler.h"

#include <algorithm>

void WorkerCommandContext::Create(Device& device, const VkAllocationCallbacks* allocator, U32 graphicsQueueIndex, U32 transferQueueIndex, U32 preallocateCount)
{
	VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolInfo.queueFamilyIndex = graphicsQueueIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		vkCreateCommandPool(device, &poolInfo, allocator, &graphicsPools[i]);

		commandBuffers[i].resize(preallocateCount);
		VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocInfo.commandPool = graphicsPools[i];
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		allocInfo.commandBufferCount = preallocateCount;

		vkAllocateCommandBuffers(device, &allocInfo, commandBuffers[i].data());
	}

	poolInfo.queueFamilyIndex = transferQueueIndex;

	vkCreateCommandPool(device, &poolInfo, allocator, &transferPool);
}

void WorkerCommandContext::Destroy(Device& device, const VkAllocationCallbacks* allocator)
{
	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		vkDestroyCommandPool(device, graphicsPools[i], allocator);
	}
	vkDestroyCommandPool(device, transferPool, allocator);
}

void WorkerCommandContext::ResetFrame(Device& device, U32 frameIndex)
{
	vkResetCommandPool(device, graphicsPools[frameIndex], 0);
	currentBufferIndex = 0;
}

VkCommandBuffer_T* WorkerCommandContext::GetNextCommandBuffer(U32 frameIndex)
{
	if (currentBufferIndex >= commandBuffers[frameIndex].size()) { BreakPoint; }
	return commandBuffers[frameIndex][currentBufferIndex++];
}

Instance Renderer::instance;
Device Renderer::device;
Swapchain Renderer::swapchain;
Shader Renderer::spriteShader;

VkDescriptorSetLayout Renderer::globalBindlessSetLayout;
VkDescriptorPool Renderer::globalDescriptorPool;
VkDescriptorSet_T* Renderer::globalBindlessSet;

VkDescriptorSetLayout Renderer::ssboSetLayout;
VkDescriptorSet Renderer::ssboDescriptorSets[MaxFramesInFlight];

VkBuffer Renderer::spriteBuffers[MaxFramesInFlight];
VmaAllocation Renderer::spriteAllocations[MaxFramesInFlight];
void* Renderer::spriteMappedData[MaxFramesInFlight];

VkCommandPool_T* Renderer::primaryPools[MaxFramesInFlight];
VkCommandBuffer_T* Renderer::primaryBuffers[MaxFramesInFlight];

U32 Renderer::surfaceFormat;
U32 Renderer::surfaceColorSpace;
U32 Renderer::imageCount;
U32 Renderer::presentMode;
U32 Renderer::surfaceWidth;
U32 Renderer::surfaceHeight;

U32 Renderer::imageIndex = 0;
U64 Renderer::currentFrameNumber = 1;
VkSemaphore Renderer::frameTimelineSemaphore;
VkSemaphore Renderer::imageAcquiredSemaphores[MaxFramesInFlight];
VkSemaphore Renderer::renderFinishedSemaphores[MaxSwapchainImages];

U32 Renderer::workerThreadCount;
Vector<WorkerCommandContext> Renderer::workerContexts;
RenderExtractor Renderer::extractor;

VmaAllocator Renderer::vmaAllocator;
VkAllocationCallbacks* Renderer::allocationCallbacks;

Vector<Function<void()>> Renderer::pendingDeletions;
Vector<Function<void()>> Renderer::deletionQueues[MaxFramesInFlight];

#ifdef NH_DEBUG
RenderTarget Renderer::viewportTarget;
#endif

bool Renderer::Initialize(const StringView& name, U32 version)
{
	Logger::Trace("Initializing Renderer...");

	workerThreadCount = Nihility::scheduler.GetNumTaskThreads();

	if (!instance.Create(name, version)) { Logger::Fatal("Failed To Create Vulkan Instance!"); return false; }
	if (!device.Create()) { Logger::Fatal("Failed To Create Vulkan Device!"); return false; }
	if (!InitializeVma()) { Logger::Fatal("Failed To Initialize Vma!"); return false; }
	if (!CreateSurfaceInfo()) { Logger::Fatal("Failed To Create Surface Info!"); return false; }
	if (!swapchain.Create()) { Logger::Fatal("Failed To Create Swapchain!"); return false; }
	if (!Shader::Initialize()) { Logger::Fatal("Failed To Initialize Shader System!"); return false; }
	if (!CreateDescriptorSet()) { Logger::Fatal("Failed To Create Pipeline Layout!"); return false; }
	if (!CreateShaders()) { Logger::Fatal("Failed To Create Shaders!"); return false; }
	if (!CreateStorageBuffers()) { Logger::Fatal("Failed To Create Storage Buffers!"); return false; }
	if (!CreateSynchronization()) { Logger::Fatal("Failed To Create Synchronization Objects!"); return false; }
	if (!CreateCommandBuffers()) { Logger::Fatal("Failed To Create Command Buffers!"); return false; }

#ifdef NH_DEBUG
	viewportTarget.Create(1920, 1080);
#endif

	return true;
}

void Renderer::Stop()
{
	vkDeviceWaitIdle(device);
}

void Renderer::Shutdown()
{
	Logger::Trace("Shutting Down Renderer...");

	for (WorkerCommandContext& ctx : workerContexts)
	{
		ctx.Destroy(device, allocationCallbacks);
	}

	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		vkDestroyCommandPool(device, primaryPools[i], allocationCallbacks);
	}

	for (auto& func : pendingDeletions) { func(); }
	pendingDeletions.clear();

	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		for (auto& func : deletionQueues[i]) { func(); }
		deletionQueues[i].clear();
	}

#ifdef NH_DEBUG
	viewportTarget.Destroy();
#endif

	vkDestroySemaphore(device, frameTimelineSemaphore, allocationCallbacks);

	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		vkDestroySemaphore(device, imageAcquiredSemaphores[i], allocationCallbacks);
	}

	for (U32 i = 0; i < MaxSwapchainImages; ++i)
	{
		vkDestroySemaphore(device, renderFinishedSemaphores[i], allocationCallbacks);
	}

	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		if (spriteBuffers[i] != nullptr)
		{
			vmaDestroyBuffer(vmaAllocator, spriteBuffers[i], spriteAllocations[i]);
		}
	}

	spriteShader.Destroy();

	vkDestroyDescriptorPool(device, globalDescriptorPool, allocationCallbacks);
	vkDestroyDescriptorSetLayout(device, globalBindlessSetLayout, allocationCallbacks);

	Shader::Shutdown();

	swapchain.Destroy();

#if defined(NH_DEBUG) && 0
	C* statsString;
	vmaBuildStatsString(vmaAllocator, &statsString, VK_TRUE);
	Logger::Trace("VMA Memory Leak Dump:\n{}", statsString);
	vmaFreeStatsString(vmaAllocator, statsString);
#endif

	vmaDestroyAllocator(vmaAllocator);

	device.Destroy();

	instance.Destroy();
}

bool Renderer::BeginFrame()
{
	U64 frameIndex = currentFrameNumber % MaxFramesInFlight;

	if (Platform::Resized())
	{
		Platform::resized = false;
		RecreateSwapchain();
		return false;
	}

	if (currentFrameNumber > MaxFramesInFlight)
	{
		U64 waitValue = currentFrameNumber - MaxFramesInFlight;
		VkSemaphoreWaitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
		waitInfo.semaphoreCount = 1;
		waitInfo.pSemaphores = &frameTimelineSemaphore;
		waitInfo.pValues = &waitValue;

		vkWaitSemaphores(device, &waitInfo, UINT64_MAX);
	}

	FlushDeletionQueue();

	VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquiredSemaphores[frameIndex], nullptr, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapchain();
		return false;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		Logger::Error("Failed To Acquire Swapchain Image!");
		return false;
	}

	VkValidateFR(result);

	vkResetCommandPool(device, primaryPools[frameIndex], 0);

	//TODO: Maybe dispatch this
	for (WorkerCommandContext& ctx : workerContexts)
	{
		ctx.ResetFrame(device, (U32)frameIndex);
	}

	extractor.BeginExtraction();

	return true;
}

void Renderer::EndFrame()
{
	U64 frameIndex = currentFrameNumber % MaxFramesInFlight;
	VkCommandBuffer primaryCmd = primaryBuffers[frameIndex];
	extractor.FinishExtraction();
	const RenderPacket& renderData = extractor.GetReadPacket();
	std::span<const SpriteData> activeSprites = renderData.GetActiveSprites();
	U32 spriteCount = (U32)activeSprites.size();

	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(primaryCmd, &beginInfo);

#ifdef NH_DEBUG
	UIRect& rect = Registry::GetComponent<UIRect>(Editor::viewportPanel.Id());

	U32 width = glm::max(1u, (U32)rect.resolvedSize.x);
	U32 height = glm::max(1u, (U32)rect.resolvedSize.y);

	if (width != viewportTarget.Width() || height != viewportTarget.Height())
	{
		vkDeviceWaitIdle(device);
		viewportTarget.Recreate(width, height);
	}

	viewportTarget.StartRender(primaryCmd);
#else
	glm::vec4 area = Renderer::RenderArea();

	VkViewport viewport{};
	viewport.x = area.x;
	viewport.y = area.y;
	viewport.width = area.z;
	viewport.height = area.w;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(primaryCmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { (I32)area.x, (I32)area.y };
	scissor.extent = { (U32)area.z, (U32)area.w };
	vkCmdSetScissor(primaryCmd, 0, 1, &scissor);

	Renderer::TransitionImageLayout(primaryCmd, swapchain.images[imageIndex],
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	VkRenderingAttachmentInfo viewportAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	viewportAttachment.imageView = swapchain.imageViews[imageIndex];
	viewportAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	viewportAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	viewportAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	viewportAttachment.clearValue = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
	renderingInfo.renderArea = { { 0, 0 }, { surfaceWidth, surfaceHeight } };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &viewportAttachment;
	renderingInfo.flags = 0;

	vkCmdBeginRendering(primaryCmd, &renderingInfo);
#endif
	Tilemap::RenderTilemaps(primaryCmd);

	if (spriteCount > 0)
	{
		memcpy(spriteMappedData[frameIndex], activeSprites.data(), spriteCount * sizeof(SpriteData));
		vmaFlushAllocation(vmaAllocator, spriteAllocations[frameIndex], 0, spriteCount * sizeof(SpriteData));
		vkCmdBindPipeline(primaryCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spriteShader.vkPipeline);

		VkDescriptorSet sets[] = { globalBindlessSet, spriteShader.GetDescriptorSet(1, (U32)frameIndex) };
		vkCmdBindDescriptorSets(primaryCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spriteShader.vkPipelineLayout, 0, CountOf32(sets), sets, 0, nullptr);
		vkCmdDraw(primaryCmd, 4, spriteCount, 0, 0);
	}
#ifdef NH_DEBUG
	Editor::RenderGrid(primaryCmd);
	viewportTarget.EndRender(primaryCmd);

	TransitionImageLayout(primaryCmd, swapchain.images[imageIndex],
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<F32>(surfaceWidth);
	viewport.height = static_cast<F32>(surfaceHeight);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(primaryCmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { surfaceWidth, surfaceHeight };
	vkCmdSetScissor(primaryCmd, 0, 1, &scissor);

	VkRenderingAttachmentInfo swapchainAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	swapchainAttachment.imageView = swapchain.imageViews[imageIndex];
	swapchainAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	swapchainAttachment.clearValue = { { 0.2f, 0.2f, 0.25f, 1.0f } };

	VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
	renderingInfo.renderArea = { 0, 0, surfaceWidth, surfaceHeight };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &swapchainAttachment;
	renderingInfo.flags = 0;

	vkCmdBeginRendering(primaryCmd, &renderingInfo);

	UI::Render(primaryCmd);

	vkCmdEndRendering(primaryCmd);

	TransitionImageLayout(primaryCmd, swapchain.images[imageIndex],
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);
#else
	UI::Render(primaryCmd);

	vkCmdEndRendering(primaryCmd);

	TransitionImageLayout(primaryCmd, swapchain.images[imageIndex],
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);
#endif
	vkEndCommandBuffer(primaryCmd);

	VkCommandBufferSubmitInfo cmdSubmitInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	cmdSubmitInfo.commandBuffer = primaryCmd;

	VkSemaphoreSubmitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	waitInfo.semaphore = imageAcquiredSemaphores[frameIndex];
	waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo signalPresentInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	signalPresentInfo.semaphore = renderFinishedSemaphores[imageIndex];
	signalPresentInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	VkSemaphoreSubmitInfo signalTimelineInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	signalTimelineInfo.semaphore = frameTimelineSemaphore;
	signalTimelineInfo.value = currentFrameNumber;
	signalTimelineInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	VkSemaphoreSubmitInfo signalInfos[] = { signalPresentInfo, signalTimelineInfo };

	VkSubmitInfo2 submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.pWaitSemaphoreInfos = &waitInfo;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdSubmitInfo;
	submitInfo.signalSemaphoreInfoCount = 2;
	submitInfo.pSignalSemaphoreInfos = signalInfos;

	vkQueueSubmit2(device.graphicsQueue, 1, &submitInfo, nullptr);

	VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	VkResult presentResult = vkQueuePresentKHR(device.graphicsQueue, &presentInfo);

	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || Platform::Resized())
	{
		Platform::resized = false;
		RecreateSwapchain();
	}

	VkValidateF(presentResult);

	for (auto& func : pendingDeletions)
	{
		deletionQueues[frameIndex].push_back(Move(func));
	}
	pendingDeletions.clear();

	++currentFrameNumber;
}

bool Renderer::InitializeVma()
{
	VmaAllocatorCreateInfo allocatorInfo{
		.flags = 0,
		.physicalDevice = device.physicalDevice,
		.device = device,
		.preferredLargeHeapBlockSize = 0,
		.pAllocationCallbacks = allocationCallbacks,
		.pDeviceMemoryCallbacks = nullptr,
		.pHeapSizeLimit = nullptr,
		.pVulkanFunctions = nullptr,
		.instance = instance,
		.vulkanApiVersion = VK_API_VERSION_1_3,
		.pTypeExternalMemoryHandleTypes = nullptr
	};

	VkValidateFR(vmaCreateAllocator(&allocatorInfo, &vmaAllocator));

	return true;
}

bool Renderer::CreateSurfaceInfo()
{
	VkSurfaceCapabilitiesKHR capabilities;
	VkValidateFR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &capabilities));

	imageCount = glm::min(capabilities.minImageCount + 1, glm::min(capabilities.maxImageCount, MaxSwapchainImages));

	if (capabilities.currentExtent.width != U32_MAX)
	{
		surfaceWidth = capabilities.currentExtent.width;
		surfaceHeight = capabilities.currentExtent.height;
	}
	else
	{
		VkExtent2D actualExtent = { 0, 0 };

		actualExtent.width = glm::max(capabilities.minImageExtent.width, glm::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = glm::max(capabilities.minImageExtent.height, glm::min(capabilities.maxImageExtent.height, actualExtent.height));

		surfaceWidth = actualExtent.width;
		surfaceHeight = actualExtent.height;
	}

	U32 presentModeCount;
	VkValidateFR(vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &presentModeCount, nullptr));
	Vector<VkPresentModeKHR> presentModes(presentModeCount, {});
	VkValidateFR(vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &presentModeCount, presentModes.data()));

	presentMode = VK_PRESENT_MODE_FIFO_KHR;

	if (imageCount >= 3)
	{
		for (const VkPresentModeKHR& mode : presentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = mode; break; }
		}
	}

	U32 formatCount;
	VkValidateFR(vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &formatCount, nullptr));
	Vector<VkSurfaceFormatKHR> formats(formatCount, {});
	VkValidateFR(vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &formatCount, formats.data()));

	Vector<VkSurfaceFormatKHR> desiredFormats = {
		{ VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
		{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }
	};

	for (const VkSurfaceFormatKHR& desiredFormat : desiredFormats)
	{
		for (const VkSurfaceFormatKHR& availableFormat : formats)
		{
			if (desiredFormat.format == availableFormat.format && desiredFormat.colorSpace == availableFormat.colorSpace)
			{
				surfaceFormat = desiredFormat.format;
				surfaceColorSpace = desiredFormat.colorSpace;
				return true;
			}
		}
	}

	surfaceFormat = formats[0].format;
	surfaceColorSpace = formats[0].colorSpace;
	return true;
}

bool Renderer::CreateDescriptorSet()
{
	VkDescriptorSetLayoutBinding textureBind{};
	textureBind.binding = 0;
	textureBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureBind.descriptorCount = 100000;
	textureBind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	textureBind.pImmutableSamplers = nullptr;

	VkDescriptorBindingFlags bindlessFlags =
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{};
	extendedInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	extendedInfo.bindingCount = 1;
	extendedInfo.pBindingFlags = &bindlessFlags;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &textureBind;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutInfo.pNext = &extendedInfo;

	VkValidateFR(vkCreateDescriptorSetLayout(device, &layoutInfo, allocationCallbacks, &globalBindlessSetLayout));

	VkDescriptorPoolSize poolSizes[] = {
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = CountOf32(poolSizes);
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = 100;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

	VkValidateFR(vkCreateDescriptorPool(device, &poolInfo, allocationCallbacks, &globalDescriptorPool));

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = globalDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &globalBindlessSetLayout;

	VkValidateFR(vkAllocateDescriptorSets(device, &allocInfo, &globalBindlessSet));

	return true;
}

bool Renderer::CreateShaders()
{
	spriteShader.Create("sprite.slang");

	return true;
}

bool Renderer::CreateStorageBuffers()
{
	VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = sizeof(SpriteData) * 50000;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	VmaAllocationCreateInfo allocCreateInfo{};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		VmaAllocationInfo vmaInfo;
		vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocCreateInfo, &spriteBuffers[i], &spriteAllocations[i], &vmaInfo);
		NameAllocation(spriteAllocations[i], "SpriteBuffer");
		spriteMappedData[i] = vmaInfo.pMappedData;

		spriteShader.UpdateStorageBuffer(1, 0, i, spriteBuffers[i], sizeof(SpriteData) * 50000);
	}

	return true;
}

bool Renderer::CreateSynchronization()
{
	VkSemaphoreTypeCreateInfo typeCreateInfo{};
	typeCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	typeCreateInfo.initialValue = 0;

	VkSemaphoreCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	createInfo.pNext = &typeCreateInfo;

	VkValidateFR(vkCreateSemaphore(device, &createInfo, allocationCallbacks, &frameTimelineSemaphore));

	typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_BINARY;

	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		VkValidateFR(vkCreateSemaphore(device, &createInfo, allocationCallbacks, &imageAcquiredSemaphores[i]));
	}

	for (U32 i = 0; i < MaxSwapchainImages; ++i)
	{
		VkValidateFR(vkCreateSemaphore(device, &createInfo, allocationCallbacks, &renderFinishedSemaphores[i]));
	}

	return true;
}

bool Renderer::CreateCommandBuffers()
{
	for (U32 i = 0; i < MaxFramesInFlight; ++i)
	{
		VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolInfo.queueFamilyIndex = device.physicalDevice.graphicsQueueIndex;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		vkCreateCommandPool(device, &poolInfo, allocationCallbacks, &primaryPools[i]);

		VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocInfo.commandPool = primaryPools[i];
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		vkAllocateCommandBuffers(device, &allocInfo, &primaryBuffers[i]);
	}

	workerContexts.resize(workerThreadCount);
	for (WorkerCommandContext& ctx : workerContexts)
	{
		ctx.Create(device, allocationCallbacks, device.physicalDevice.graphicsQueueIndex, device.physicalDevice.transferQueueIndex, 50); //TODO: Don't hardcode count
	}

	return true;
}

void Renderer::DeferDestruction(Function<void()>&& function)
{
	pendingDeletions.push_back(Move(function));
}

void Renderer::FlushDeletionQueue()
{
	U32 frameIndex = FrameIndex();
	for (auto& func : deletionQueues[frameIndex]) { func(); }

	deletionQueues[frameIndex].clear();
}

void Renderer::DestroyTexture(Texture& texture)
{
	vkDestroySampler(device, texture.sampler, allocationCallbacks);
	vkDestroyImageView(device, texture.imageView, allocationCallbacks);

	vmaDestroyImage(vmaAllocator, texture.image, texture.allocation);
}

VkCommandBuffer Renderer::RecordSecondaryCommandBuffer(U32 threadId, U32 frameIndex, std::span<const SpriteData> sprites)
{
	WorkerCommandContext& context = workerContexts[threadId];
	VkCommandBuffer cmd = workerContexts[threadId].GetNextCommandBuffer(frameIndex);

	VkCommandBufferInheritanceRenderingInfo inheritanceRenderingInfo{};
	inheritanceRenderingInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
	inheritanceRenderingInfo.colorAttachmentCount = 1;
	inheritanceRenderingInfo.pColorAttachmentFormats = (VkFormat*)&surfaceFormat;
	inheritanceRenderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkCommandBufferInheritanceInfo inheritanceInfo{};
	inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.pNext = &inheritanceRenderingInfo;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	beginInfo.pInheritanceInfo = &inheritanceInfo;

	vkBeginCommandBuffer(cmd, &beginInfo);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spriteShader.vkPipelineLayout,
		0, 1, &globalBindlessSet, 0, nullptr);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spriteShader.vkPipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<F32>(Settings::WindowWidth());
	viewport.height = static_cast<F32>(Settings::WindowHeight());
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { Settings::WindowWidth(), Settings::WindowHeight() };
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	for (const SpriteData& sprite : sprites)
	{
		vkCmdPushConstants(cmd, spriteShader.vkPipelineLayout,
			VK_SHADER_STAGE_ALL_GRAPHICS,
			0, sizeof(SpriteData), &sprite);

		vkCmdDraw(cmd, 4, 1, 0, 0);
	}

	vkEndCommandBuffer(cmd);
	return cmd;
}

void Renderer::RecreateSwapchain()
{
	vkDeviceWaitIdle(device);

	CreateSurfaceInfo();
	swapchain.Recreate();
}

void Renderer::TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
	U64 srcStageMask, U64 srcAccessMask, U64 dstStageMask, U64 dstAccessMask)
{
	VkImageMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcStageMask = srcStageMask;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstStageMask = dstStageMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.image = image;

	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &barrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

VkCommandBuffer Renderer::BeginSingleTimeCommands(VkCommandPool commandPool)
{
	//TODO: Potentially use ring buffer for pre-allocated transer command buffers
	VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void Renderer::EndSingleTimeCommands(VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkCommandBufferSubmitInfo cmdSubmitInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	cmdSubmitInfo.commandBuffer = commandBuffer;

	VkSubmitInfo2 submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdSubmitInfo;

	VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence fence;
	vkCreateFence(device, &fenceInfo, allocationCallbacks, &fence);

	vkQueueSubmit2(device.transferQueue, 1, &submitInfo, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

	vkDestroyFence(device, fence, allocationCallbacks);
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

bool Renderer::UploadTexture(std::shared_ptr<Texture> texture, void* data, U32 threadId)
{
	WorkerCommandContext& context = workerContexts[threadId];

	VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = texture->size;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBuffer stagingBuffer = nullptr;
	VmaAllocation stagingAllocation = nullptr;
	VmaAllocationInfo stagingAllocInfo{};
	vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocInfo, &stagingBuffer, &stagingAllocation, &stagingAllocInfo);
	NameAllocation(stagingAllocation, "StagingBuffer");

	memcpy(stagingAllocInfo.pMappedData, data, texture->size);

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = texture->width;
	imageInfo.extent.height = texture->height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = (VkFormat)texture->format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	U32 queueFamilies[] = { device.physicalDevice.graphicsQueueIndex, device.physicalDevice.transferQueueIndex };
	if (queueFamilies[0] != queueFamilies[1])
	{
		imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		imageInfo.queueFamilyIndexCount = 2;
		imageInfo.pQueueFamilyIndices = queueFamilies;
	}
	else
	{
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	vmaCreateImage(vmaAllocator, &imageInfo, &vmaAllocInfo, &texture->image, &texture->allocation, nullptr);
	NameAllocation(texture->allocation, texture->name);

	VkCommandBuffer cmd = BeginSingleTimeCommands(context.transferPool);

	TransitionImageLayout(cmd, texture->image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_2_NONE, 0,
		VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { texture->width, texture->height, 1 };

	vkCmdCopyBufferToImage(cmd, stagingBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	TransitionImageLayout(cmd, texture->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);

	EndSingleTimeCommands(context.transferPool, cmd);

	VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = texture->image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = (VkFormat)texture->format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkValidateFR(vkCreateImageView(device, &viewInfo, allocationCallbacks, &texture->imageView));

	VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.magFilter = (VkFilter)texture->sampler.filterMode;
	samplerInfo.minFilter = (VkFilter)texture->sampler.filterMode;
	samplerInfo.addressModeU = (VkSamplerAddressMode)texture->sampler.edgeSampleMode;
	samplerInfo.addressModeV = (VkSamplerAddressMode)texture->sampler.edgeSampleMode;
	samplerInfo.addressModeW = (VkSamplerAddressMode)texture->sampler.edgeSampleMode;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = (VkBorderColor)texture->sampler.borderColor;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = (VkSamplerMipmapMode)texture->sampler.mipMapSampleMode;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	VkValidateFR(vkCreateSampler(device, &samplerInfo, allocationCallbacks, &texture->sampler));

	VkDescriptorImageInfo descriptorImageInfo{};
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptorImageInfo.imageView = texture->imageView;
	descriptorImageInfo.sampler = texture->sampler;

	VkWriteDescriptorSet descriptorWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	descriptorWrite.dstSet = globalBindlessSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = texture->id;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &descriptorImageInfo;

	vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

	vmaDestroyBuffer(vmaAllocator, stagingBuffer, stagingAllocation);

	return true;
}

Buffer Renderer::CreateBuffer(U64 size, U64 usage, VmaMemoryUsage memoryUsage)
{
	Buffer buffer{};
	buffer.size = size;

	VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = size;
	bufferInfo.usage = (VkBufferUsageFlags)usage;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = memoryUsage;
	
	if (memoryUsage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST || memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU)
	{
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	}

	VmaAllocationInfo vmaInfo{};
	vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocInfo, &buffer.vkBuffer, &buffer.allocation, &vmaInfo);
	NameAllocation(buffer.allocation, "Buffer");

	buffer.mappedData = vmaInfo.pMappedData;

	return buffer;
}

void Renderer::DestroyBuffer(Buffer& buffer)
{
	if (buffer.vkBuffer == nullptr) { return; }

	VkBuffer vkBuf = buffer.vkBuffer;
	VmaAllocation alloc = buffer.allocation;

	DeferDestruction([vkBuf, alloc]()
	{
		vmaDestroyBuffer(vmaAllocator, vkBuf, alloc);
	});

	buffer.vkBuffer = nullptr;
	buffer.allocation = nullptr;
}

void Renderer::UploadToBuffer(Buffer& dstBuffer, const void* data, U64 size, U32 threadId)
{
	WorkerCommandContext& context = workerContexts[threadId];

	Buffer stagingBuffer = CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	stagingBuffer.Write(data, size);

	VkCommandBuffer cmd = BeginSingleTimeCommands(context.transferPool);

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(cmd, stagingBuffer.vkBuffer, dstBuffer.vkBuffer, 1, &copyRegion);

	EndSingleTimeCommands(context.transferPool, cmd);

	DestroyBuffer(stagingBuffer);
}

VkDescriptorSet_T* Renderer::GlobalBindlessSet()
{
	return globalBindlessSet;
}

U32 Renderer::FrameIndex()
{
	return currentFrameNumber % MaxFramesInFlight;
}

void Renderer::SubmitSprite(const SpriteData& sprite)
{
	extractor.SubmitSprite(sprite);
}

glm::mat4 Renderer::GetViewProjectionMatrix()
{
	auto cameraView = Registry::View<Camera>();
	glm::mat4 viewMatrix = glm::mat4(1.0f);

	F32 width = 1920.0f;
	F32 height = 1080.0f;

	if (cameraView.Size() > 0)
	{
		U32 activeCamId = cameraView.GetEntity(0);
		Transform2D& camTrans = Registry::GetTransform(activeCamId);

		glm::vec2 halfScreen = glm::vec2(width, height) / 2.0f;
#ifdef NH_DEBUG
		viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-camTrans.position, 0.0f));
#else
		viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-camTrans.position + halfScreen, 0.0f));
#endif
	}

	glm::mat4 projection = glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
	return projection * viewMatrix;
}

glm::vec4 Renderer::RenderArea()
{
	static constexpr F32 targetAspectRatio = 16.0f / 9.0f;
#ifdef NH_DEBUG
	UIRect& vpRect = Registry::GetComponent<UIRect>(Editor::viewportPanel.Id());

	F32 width = vpRect.resolvedSize.x;
	F32 height = vpRect.resolvedSize.y;
#else
	F32 width = (F32)surfaceWidth;
	F32 height = (F32)surfaceHeight;
#endif
	F32 currentAspectRatio = width / height;

	F32 viewWidth = width;
	F32 viewHeight = height;
	F32 offsetX = 0.0f;
	F32 offsetY = 0.0f;

	if (currentAspectRatio > targetAspectRatio)
	{
		viewWidth = height * targetAspectRatio;
		offsetX = (width - viewWidth) * 0.5f;
	}
	else
	{
		viewHeight = width / targetAspectRatio;
		offsetY = (height - viewHeight) * 0.5f;
	}

	return { offsetX, offsetY, viewWidth, viewHeight };
}

void Renderer::NameAllocation(VmaAllocation_T* allocation, const WString& name)
{
#ifdef NH_DEBUG
	vmaSetAllocationName(vmaAllocator, allocation, ConvertToView<C>(name).data());
#endif
}

void Renderer::NameAllocation(VmaAllocation_T* allocation, const String& name)
{
#ifdef NH_DEBUG
	vmaSetAllocationName(vmaAllocator, allocation, name.data());
#endif
}