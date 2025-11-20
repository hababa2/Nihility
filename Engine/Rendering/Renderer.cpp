#include "Renderer.hpp"

#include "VulkanInclude.hpp"

#include "CommandBufferRing.hpp"
#include "UI.hpp"
#include "LineRenderer.hpp"

#include "Platform/Platform.hpp"
#include "Core/Time.hpp"
#include "Core/Logger.hpp"
#include "Math/Math.hpp"
#include "Resources/Resources.hpp"

#include "tracy/Tracy.hpp"

#define VMA_VULKAN_VERSION 1003000
#define VMA_IMPLEMENTATION

#ifdef NH_DEBUG
#define VMA_DEBUG_LOG_ENABLED 1
#define VMA_DEBUG_ALWAYS_DEDICATED_MEMORY 0
#define VMA_RECORDING_ENABLED 1
#endif

#include "vma/vk_mem_alloc.h"

VmaAllocator Renderer::vmaAllocator;
VkAllocationCallbacks* Renderer::allocationCallbacks;
VkDescriptorPool Renderer::vkDescriptorPool = VK_NULL_HANDLE;
VkDescriptorPool Renderer::vkBindlessDescriptorPool = VK_NULL_HANDLE;
DescriptorSet Renderer::descriptorSet;
Texture Renderer::colorTextures[MaxSwapchainImages];
Texture Renderer::depthTextures[MaxSwapchainImages];
Buffer Renderer::stagingBuffers[MaxSwapchainImages];
U32 Renderer::surfaceFormat;
U32 Renderer::surfaceColorSpace;
U32 Renderer::imageCount;
U32 Renderer::presentMode;
U32 Renderer::surfaceWidth;
U32 Renderer::surfaceHeight;

Instance Renderer::instance;
Device Renderer::device;
Swapchain Renderer::swapchain;
Renderpass Renderer::renderpass;

Vector<VkCommandBuffer> Renderer::commandBuffers[MaxSwapchainImages];
GlobalPushConstant Renderer::globalPushConstant;

U32 Renderer::imageIndex;
U32 Renderer::frameIndex;
U32 Renderer::previousFrame;
U32 Renderer::absoluteFrame;
VkSemaphore Renderer::imageAcquired[MaxSwapchainImages];
VkSemaphore Renderer::transferFinished[MaxSwapchainImages];
VkSemaphore Renderer::renderFinished[MaxSwapchainImages];
VkSemaphore Renderer::presentReady[MaxSwapchainImages];
U64 Renderer::renderWaitValues[MaxSwapchainImages];
U64 Renderer::transferWaitValues[MaxSwapchainImages];

Vector<SwapchainDestructionData> Renderer::swapchainsToDestroy;
Vector<TextureDestructionData> Renderer::texturesToDestroy;
Vector<BufferDestructionData> Renderer::buffersToDestroy;
Vector<PipelineDestructionData> Renderer::pipelinesToDestroy;
Vector<DescriptorSetDestructionData> Renderer::descriptorSetsToDestroy;

#ifdef NH_DEBUG
SetObjectNameFN Renderer::SetObjectName;
#endif

bool Renderer::Initialize(const StringView& name, U32 version)
{
	Logger::Trace("Initializing Renderer...");

	if (!instance.Create(name, version)) { Logger::Fatal("Failed To Create Vulkan Instance!"); return false; }
	if (!device.Create()) { Logger::Fatal("Failed To Create Vulkan Device!"); return false; }
	if (!InitializeVma()) { Logger::Fatal("Failed To Initialize Vma!"); return false; }
	if (!CreateSurfaceInfo()) { Logger::Fatal("Failed To Select Surface Format!"); return false; }
	if (!CreateColorTextures()) { Logger::Fatal("Failed To Create Color Buffers!"); return false; }
	if (!CreateDepthTextures()) { Logger::Fatal("Failed To Create Depth Buffers!"); return false; }
	if (!CommandBufferRing::Initialize()) { Logger::Fatal("Failed To Create Command Buffers!"); return false; }
	if (!CreateDescriptorPool()) { Logger::Fatal("Failed To Create Descriptor Pool!"); return false; }
	if (!CreateRenderpasses()) { Logger::Fatal("Failed To Create Renderpasses!"); return false; }
	if (!swapchain.Create()) { Logger::Fatal("Failed To Create Swapchain!"); return false; }
	if (!CreateSynchronization()) { Logger::Fatal("Failed To Create Synchronization Objects!"); return false; }
	if (!CreateStagingBuffers()) { Logger::Fatal("Failed To Create Staging Buffers!"); return false; }

#ifdef NH_DEBUG
	if (!LineRenderer::Initialize()) { Logger::Fatal("Failed To Create Line Renderer!"); return false; }
#endif

	return true;
}

void Renderer::Shutdown()
{
	Logger::Trace("Cleaning Up Renderer...");

	VkSemaphore waits[]{ renderFinished[frameIndex], transferFinished[frameIndex] };
	U64 waitValues[]{ renderWaitValues[frameIndex], transferWaitValues[frameIndex] };

	VkSemaphoreWaitInfo waitInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext = nullptr,
		.flags = 0,
		.semaphoreCount = CountOf32(waits),
		.pSemaphores = waits,
		.pValues = waitValues
	};

	vkWaitSemaphores(device, &waitInfo, U64_MAX);
	vkDeviceWaitIdle(device);

#ifdef NH_DEBUG
	LineRenderer::Shutdown();
#endif

	for (U32 i = 0; i < imageCount; ++i)
	{
		stagingBuffers[i].Destroy();
	}

	DestroyObjects();

	for (U32 i = 0; i < imageCount; ++i)
	{
		vkDestroySemaphore(device, imageAcquired[i], allocationCallbacks);
		vkDestroySemaphore(device, transferFinished[i], allocationCallbacks);
		vkDestroySemaphore(device, renderFinished[i], allocationCallbacks);
		vkDestroySemaphore(device, presentReady[i], allocationCallbacks);
	}

	renderpass.Destroy();

	vkDestroyDescriptorPool(device, vkDescriptorPool, allocationCallbacks);
	vkDestroyDescriptorPool(device, vkBindlessDescriptorPool, allocationCallbacks);

	CommandBufferRing::Shutdown();

	for (U32 i = 0; i < imageCount; ++i)
	{
		vkDestroyImageView(device, depthTextures[i].imageView, allocationCallbacks);
		vmaDestroyImage(vmaAllocator, depthTextures[i].image, depthTextures[i].allocation);
		vkDestroyImageView(device, colorTextures[i].imageView, allocationCallbacks);
		vmaDestroyImage(vmaAllocator, colorTextures[i].image, colorTextures[i].allocation);
	}

	for (VkImageView view : swapchain.imageViews)
	{
		vkDestroyImageView(device, view, allocationCallbacks);
	}

	for (VkFramebuffer framebuffer : swapchain.framebuffers)
	{
		vkDestroyFramebuffer(device, framebuffer, allocationCallbacks);
	}

	vkDestroySwapchainKHR(device, swapchain.vkSwapchain, allocationCallbacks);

#if defined(NH_DEBUG) && 0
	char* statsString = nullptr;
	vmaBuildStatsString(vmaAllocator, &statsString, VK_TRUE);
	printf("%s\n", statsString);
	vmaFreeStatsString(vmaAllocator, statsString);
#endif

	vmaDestroyAllocator(vmaAllocator);

	device.Destroy();

	instance.Destroy();
}

void Renderer::Update()
{
	ZoneScopedN("RenderMain");

	if (!Synchronize()) { return; }

	Resources::Update();
	World::Update();
#ifdef NH_DEBUG
	LineRenderer::Update();
#endif
	UI::Update();

	SubmitTransfer();

	globalPushConstant.viewProjection = World::camera.ViewProjection();
	
	CommandBuffer& commandBuffer = CommandBufferRing::GetDrawCommandBuffer(imageIndex);

	commandBuffer.Begin();
	commandBuffer.BeginRenderpass(renderpass, swapchain.framebuffers[imageIndex]);

	World::Render(commandBuffer);

#ifdef NH_DEBUG
	LineRenderer::Render(commandBuffer);
#endif

	UI::Render(commandBuffer);

	commandBuffer.EndRenderpass();
	commandBuffer.End();

	Submit();
}

bool Renderer::Synchronize()
{
	ZoneScopedN("RenderSynchronize");

	U32 i = absoluteFrame % imageCount;
	VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquired[i], VK_NULL_HANDLE, &imageIndex);
	
	VkSemaphore waits[]{ renderFinished[previousFrame], transferFinished[previousFrame] };
	U64 waitValues[]{ renderWaitValues[previousFrame], transferWaitValues[previousFrame] };
	
	VkSemaphoreWaitInfo waitInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext = nullptr,
		.flags = 0,
		.semaphoreCount = CountOf32(waits),
		.pSemaphores = waits,
		.pValues = waitValues
	};
	
	vkWaitSemaphores(device, &waitInfo, U64_MAX);

	DestroyObjects();
	
	CommandBufferRing::ResetDraw(imageIndex);
	CommandBufferRing::ResetPool(imageIndex);
	
	if (res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapchain();
		VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquired[i], VK_NULL_HANDLE, &imageIndex);
	}
	
	if (res != VK_SUBOPTIMAL_KHR) { VkValidateFR(res); }

	return true;
}

void Renderer::SubmitTransfer()
{
	ZoneScopedN("RenderTransfer");

	if (commandBuffers[imageIndex].Size())
	{
		++transferWaitValues[imageIndex];

		VkTimelineSemaphoreSubmitInfo timelineInfo{
			.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreValueCount = 0,
			.pWaitSemaphoreValues = nullptr,
			.signalSemaphoreValueCount = 1,
			.pSignalSemaphoreValues = &transferWaitValues[imageIndex]
		};

		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = &timelineInfo,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = nullptr,
			.pWaitDstStageMask = nullptr,
			.commandBufferCount = (U32)commandBuffers[imageIndex].Size(),
			.pCommandBuffers = commandBuffers[imageIndex].Data(),
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &transferFinished[imageIndex]
		};

		VkValidateF(vkQueueSubmit(device.graphicsQueue, 1, &submitInfo, nullptr));
		commandBuffers[imageIndex].Clear();
		stagingBuffers[imageIndex].stagingPointer = 0;
	}
}

void Renderer::FirstTransfer()
{
	SubmitTransfer();

	vkDeviceWaitIdle(device);
}

void Renderer::Submit()
{
	ZoneScopedN("RenderSubmit");
	CommandBuffer& commandBuffer = CommandBufferRing::GetDrawCommandBuffer(imageIndex);

	++renderWaitValues[imageIndex];
	
	VkSemaphoreSubmitInfo waitSemaphores[] = {
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext = nullptr,
		.semaphore = transferFinished[frameIndex],
		.value = transferWaitValues[frameIndex],
		.stageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR,
		.deviceIndex = 0,
	},
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext = nullptr,
		.semaphore = imageAcquired[frameIndex],
		.value = 0,
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
		.deviceIndex = 0,
	}
	};
	
	VkSemaphoreSubmitInfo signalSemaphores[] = {
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext = nullptr,
		.semaphore = renderFinished[frameIndex],
		.value = renderWaitValues[frameIndex],
		.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
		.deviceIndex = 0,
	},
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext = nullptr,
		.semaphore = presentReady[frameIndex],
		.value = 0,
		.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
		.deviceIndex = 0,
	}
	};

	VkCommandBufferSubmitInfo commandBufferInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext = nullptr,
		.commandBuffer = commandBuffer,
		.deviceMask = 0
	};

	VkSubmitInfo2 submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pNext = nullptr,
		.flags = 0,
		.waitSemaphoreInfoCount = CountOf32(waitSemaphores),
		.pWaitSemaphoreInfos = waitSemaphores,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &commandBufferInfo,
		.signalSemaphoreInfoCount = CountOf32(signalSemaphores),
		.pSignalSemaphoreInfos = signalSemaphores
	};

	VkValidateFExit(vkQueueSubmit2(device.graphicsQueue, 1u, &submitInfo, nullptr));

	VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &presentReady[imageIndex],
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &imageIndex,
		.pResults = nullptr
	};

	VkResult res = vkQueuePresentKHR(device.presentQueue, &presentInfo);
	commandBuffers[imageIndex].Clear();

	previousFrame = frameIndex;
	++frameIndex %= imageCount;
	++absoluteFrame;

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) { RecreateSwapchain(); }
	else { VkValidateF(res); }
}

U32 Renderer::ImageIndex()
{
	return imageIndex;
}

U32 Renderer::PreviousFrame()
{
	return previousFrame;
}

U32 Renderer::AbsoluteFrame()
{
	return absoluteFrame;
}

Vector4Int Renderer::RenderSize()
{
	return { 0, 0, (I32)surfaceWidth, (I32)surfaceHeight };
}

const GlobalPushConstant* Renderer::GetGlobalPushConstant()
{
	return &globalPushConstant;
}

VkSemaphore_T* Renderer::RenderFinished()
{
	return renderFinished[previousFrame];
}

const Device& Renderer::GetDevice()
{
	return device;
}

void Renderer::NameResource(VkObjectType type, void* object, const String& name)
{
#ifdef NH_DEBUG
	if (!SetObjectName) { SetObjectName = (SetObjectNameFN)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"); }

	VkDebugUtilsObjectNameInfoEXT nameInfo{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = type,
		.objectHandle = (U64)object,
		.pObjectName = name
	};

	SetObjectName(device, &nameInfo);
#endif
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
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &capabilities);

	imageCount = Math::Min(capabilities.minImageCount + 1, capabilities.maxImageCount, MaxSwapchainImages);

	if (capabilities.currentExtent.width != U32_MAX)
	{
		surfaceWidth = capabilities.currentExtent.width;
		surfaceHeight = capabilities.currentExtent.height;
	}
	else
	{
		VkExtent2D actualExtent = { 0, 0 };

		actualExtent.width = Math::Max(capabilities.minImageExtent.width, Math::Min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = Math::Max(capabilities.minImageExtent.height, Math::Min(capabilities.maxImageExtent.height, actualExtent.height));

		surfaceWidth = actualExtent.width;
		surfaceHeight = actualExtent.height;
	}

	U32 presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &presentModeCount, nullptr);
	Vector<VkPresentModeKHR> presentModes(presentModeCount, {});
	vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &presentModeCount, presentModes.Data());

	presentMode = VK_PRESENT_MODE_FIFO_KHR;

	if (imageCount >= 3)
	{
		for (const VkPresentModeKHR& mode : presentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = mode; break; }
		}
	}

	U32 formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &formatCount, nullptr);
	Vector<VkSurfaceFormatKHR> formats(formatCount, {});
	vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &formatCount, formats.Data());

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

bool Renderer::CreateColorTextures()
{
	VkExtent3D colorImageExtent{
		.width = surfaceWidth,
		.height = surfaceHeight,
		.depth = 1
	};

	VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = (VkFormat)surfaceFormat,
		.extent = colorImageExtent,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = (VkSampleCountFlagBits)device.physicalDevice.maxSampleCount, //TODO: Setting
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VmaAllocationCreateInfo allocationInfo{
		.flags = 0,
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = nullptr,
		.pUserData = nullptr,
		.priority = 0.0f
	};

	VkImageViewCreateInfo imageViewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = nullptr,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = imageCreateInfo.format,
		.components = {},
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	for (U32 i = 0; i < imageCount; ++i)
	{
		VkValidateFR(vmaCreateImage(vmaAllocator, &imageCreateInfo, &allocationInfo, &colorTextures[i].image, &colorTextures[i].allocation, nullptr));
		imageViewCreateInfo.image = colorTextures[i].image;
		VkValidateFR(vkCreateImageView(device, &imageViewCreateInfo, allocationCallbacks, &colorTextures[i].imageView));
	}

	return true;
}

bool Renderer::CreateDepthTextures()
{
	VkExtent3D depthImageExtent{
		.width = surfaceWidth,
		.height = surfaceHeight,
		.depth = 1
	};

	VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_D32_SFLOAT,
		.extent = depthImageExtent,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = (VkSampleCountFlagBits)device.physicalDevice.maxSampleCount, //TODO: Setting
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VmaAllocationCreateInfo allocationInfo{
		.flags = 0,
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = nullptr,
		.pUserData = nullptr,
		.priority = 0.0f
	};

	VkImageViewCreateInfo imageViewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = nullptr,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = imageCreateInfo.format,
		.components = {},
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	for (U32 i = 0; i < imageCount; ++i)
	{
		VkValidateFR(vmaCreateImage(vmaAllocator, &imageCreateInfo, &allocationInfo, &depthTextures[i].image, &depthTextures[i].allocation, nullptr));
		imageViewCreateInfo.image = depthTextures[i].image;
		VkValidateFR(vkCreateImageView(device, &imageViewCreateInfo, allocationCallbacks, &depthTextures[i].imageView));
	}

	return true;
}

bool Renderer::CreateDescriptorPool()
{
	VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1024 },
	};

	VkDescriptorPoolSize bindlessPoolSizes[]
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 6144,
		.poolSizeCount = CountOf32(poolSizes),
		.pPoolSizes = poolSizes
	};

	VkValidateFR(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, allocationCallbacks, &vkDescriptorPool));

	VkDescriptorPoolCreateInfo bindlessDescriptorPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
		.maxSets = 2048,
		.poolSizeCount = CountOf32(bindlessPoolSizes),
		.pPoolSizes = bindlessPoolSizes
	};

	VkValidateFR(vkCreateDescriptorPool(device, &bindlessDescriptorPoolCreateInfo, allocationCallbacks, &vkBindlessDescriptorPool));

	return true;
}

bool Renderer::CreateRenderpasses()
{
	return renderpass.Create();
}

bool Renderer::CreateSynchronization()
{
	VkSemaphoreCreateInfo semaphoreInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0
	};
	
	VkFenceCreateInfo fenceInfo{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0
	};

	for (U32 i = 0; i < imageCount; ++i)
	{
		vkCreateSemaphore(device, &semaphoreInfo, allocationCallbacks, &imageAcquired[i]);
		NameResource(VK_OBJECT_TYPE_SEMAPHORE, imageAcquired[i], { FORMAT, "Image Acquired ", i });
		vkCreateSemaphore(device, &semaphoreInfo, allocationCallbacks, &presentReady[i]);
		NameResource(VK_OBJECT_TYPE_SEMAPHORE, presentReady[i], { FORMAT, "Present Ready ", i });
	}

	VkSemaphoreTypeCreateInfo semaphoreType{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext = nullptr,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = 0
	};

	semaphoreInfo.pNext = &semaphoreType;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (U32 i = 0; i < imageCount; ++i)
	{
		vkCreateSemaphore(device, &semaphoreInfo, allocationCallbacks, &renderFinished[i]);
		NameResource(VK_OBJECT_TYPE_SEMAPHORE, renderFinished[i], { FORMAT, "Render Finished ", i });
		vkCreateSemaphore(device, &semaphoreInfo, allocationCallbacks, &transferFinished[i]);
		NameResource(VK_OBJECT_TYPE_SEMAPHORE, transferFinished[i], { FORMAT, "Transfer Finished ", i });
	}

	return true;
}

bool Renderer::CreateStagingBuffers()
{
	for (U32 i = 0; i < imageCount; ++i)
	{
		stagingBuffers[i].Create(BufferType::Staging, Gigabytes(1));
		Renderer::NameResource(VK_OBJECT_TYPE_BUFFER, stagingBuffers[i], { FORMAT, "Staging Buffer ", i});
	}

	return true;
}

void Renderer::ScheduleDestruction(Swapchain& swapchain)
{
	swapchainsToDestroy.Emplace(swapchain.vkSwapchain, Move(swapchain.imageViews), Move(swapchain.framebuffers));
}

void Renderer::ScheduleDestruction(Texture& texture)
{
	texturesToDestroy.Emplace(texture.image, texture.imageView, texture.allocation);
}

void Renderer::ScheduleDestruction(Buffer& buffer)
{
	buffersToDestroy.Emplace(buffer.vkBuffer, buffer.bufferAllocation, buffer.vkBufferStaging, buffer.stagingBufferAllocation);
}

void Renderer::ScheduleDestruction(Pipeline& pipeline)
{
	pipelinesToDestroy.Emplace(pipeline.vkPipeline);
}

void Renderer::ScheduleDestruction(DescriptorSet& descriptorSet)
{
	descriptorSetsToDestroy.Emplace(descriptorSet.vkDescriptorLayout, descriptorSet.vkDescriptorSet, descriptorSet.bindless);
}

void Renderer::DestroyObjects()
{
	//SWAPCHAIN
	for (SwapchainDestructionData& data : swapchainsToDestroy)
	{
		for (VkImageView view : data.imageViews)
		{
			vkDestroyImageView(device, view, allocationCallbacks);
		}

		for (VkFramebuffer framebuffer : data.framebuffers)
		{
			vkDestroyFramebuffer(device, framebuffer, allocationCallbacks);
		}

		vkDestroySwapchainKHR(device, data.swapchain, allocationCallbacks);
	}

	swapchainsToDestroy.Clear();

	//TEXTURE
	for (TextureDestructionData& data : texturesToDestroy)
	{
		if (data.imageView) { vkDestroyImageView(device, data.imageView, allocationCallbacks); }
		if (data.image) { vmaDestroyImage(vmaAllocator, data.image, data.allocation); }
	}

	texturesToDestroy.Clear();

	//BUFFER
	for (BufferDestructionData& data : buffersToDestroy)
	{
		if (data.vkBuffer) { vmaDestroyBuffer(vmaAllocator, data.vkBuffer, data.bufferAllocation); }
		if (data.vkBufferStaging) { vmaDestroyBuffer(vmaAllocator, data.vkBufferStaging, data.stagingBufferAllocation); }
	}

	buffersToDestroy.Clear();

	//PIPELINE
	for (PipelineDestructionData& data : pipelinesToDestroy)
	{
		if (data.vkPipeline) { vkDestroyPipeline(device, data.vkPipeline, allocationCallbacks); }
	}

	pipelinesToDestroy.Clear();

	//DESCRIPTOR SET
	for (DescriptorSetDestructionData& data : descriptorSetsToDestroy)
	{
		if (!data.bindless && data.vkDescriptorSet) { vkFreeDescriptorSets(device, vkDescriptorPool, 1, &data.vkDescriptorSet); }
		vkDestroyDescriptorSetLayout(device, data.vkDescriptorLayout, allocationCallbacks);
	}

	descriptorSetsToDestroy.Clear();
}

bool Renderer::RecreateSwapchain()
{
	VkSurfaceCapabilitiesKHR surface_properties;
	VkValidateFR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, device.vkSurface, &surface_properties));

	if (surface_properties.currentExtent.width == surfaceWidth &&
		surface_properties.currentExtent.height == surfaceHeight)
	{
		return false;
	}

	CreateSurfaceInfo(); //TODO: Might not need to do this

	for (U32 i = 0; i < imageCount; ++i)
	{
		ScheduleDestruction(depthTextures[i]);
		ScheduleDestruction(colorTextures[i]);
	}

	if (!CreateColorTextures()) { Logger::Fatal("Failed To Create Color Buffer!"); return false; }
	if (!CreateDepthTextures()) { Logger::Fatal("Failed To Create Depth Buffer!"); return false; }

	return swapchain.Create();
}

bool Renderer::UploadTexture(Resource<Texture>& texture, void* data, const Sampler& sampler)
{
	U64 offset = NextMultipleOf(stagingBuffers[imageIndex].StagingPointer(), 16);

	stagingBuffers[imageIndex].UploadStagingData(data, texture->size, offset);

	CommandBuffer& commandBuffer = CommandBufferRing::GetWriteCommandBuffer(imageIndex);
	commandBuffer.Begin();

	VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = (VkFormat)texture->format,
		.extent{
			.width = texture->width,
			.height = texture->height,
			.depth = texture->depth
		},
		.mipLevels = texture->mipmapLevels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VmaAllocationCreateInfo imageAllocInfo{
		.flags = 0,
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = 0,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = nullptr,
		.pUserData = nullptr,
		.priority = 0.0f
	};

	VkValidateR(vmaCreateImage(vmaAllocator, &imageInfo, &imageAllocInfo, &texture->image, &texture->allocation, nullptr));

	VkImageSubresourceRange stagingBufferRange{
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = texture->mipmapLevels,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	VkImageMemoryBarrier2 stagingBufferTransferBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex = 0,
		.dstQueueFamilyIndex = 0,
		.image = texture->image,
		.subresourceRange = stagingBufferRange
	};

	VkOffset3D textureOffset{
		.x = 0,
		.y = 0,
		.z = 0
	};

	VkExtent3D textureExtent{
		.width = texture->width,
		.height = texture->height,
		.depth = texture->depth
	};

	VkBufferImageCopy stagingBufferCopy{
		.bufferOffset = offset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = textureOffset,
		.imageExtent = textureExtent
	};

	VkImageMemoryBarrier2 stagingBufferShaderBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.dstStageMask = texture->mipmapLevels > 1 ? VK_PIPELINE_STAGE_2_COPY_BIT : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		.dstAccessMask = texture->mipmapLevels > 1 ? VK_ACCESS_2_TRANSFER_WRITE_BIT : VK_ACCESS_2_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = texture->mipmapLevels > 1 ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex = 0,
		.dstQueueFamilyIndex = 0,
		.image = texture->image,
		.subresourceRange = stagingBufferRange
	};

	commandBuffer.PipelineBarrier(0, 0, nullptr, 1, &stagingBufferTransferBarrier);
	commandBuffer.BufferToImage(stagingBuffers[imageIndex], texture, 1, &stagingBufferCopy);
	commandBuffer.PipelineBarrier(0, 0, nullptr, 1, &stagingBufferShaderBarrier);

	if (texture->mipmapLevels > 1)
	{
		VkImageSubresourceRange blitRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		};

		VkImageMemoryBarrier2 firstBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.srcQueueFamilyIndex = 0,
			.dstQueueFamilyIndex = 0,
			.image = texture->image,
			.subresourceRange = blitRange
		};

		VkImageMemoryBarrier2 secondBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcQueueFamilyIndex = 0,
			.dstQueueFamilyIndex = 0,
			.image = texture->image,
			.subresourceRange = blitRange
		};

		VkImageBlit mipBlit{
			.srcSubresource{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.srcOffsets{},
			.dstSubresource{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.dstOffsets{}
		};

		I32 mipWidth = texture->width;
		I32 mipHeight = texture->height;

		for (I32 i = 1; i < texture->mipmapLevels; ++i)
		{
			mipBlit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			mipBlit.srcSubresource.mipLevel = i - 1;

			mipBlit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			mipBlit.dstSubresource.mipLevel = i;

			firstBarrier.subresourceRange.baseMipLevel = i - 1;
			secondBarrier.subresourceRange.baseMipLevel = i - 1;

			commandBuffer.PipelineBarrier(0, 0, nullptr, 1, &firstBarrier);
			commandBuffer.Blit(texture, texture, VK_FILTER_LINEAR, 1, &mipBlit);
			commandBuffer.PipelineBarrier(0, 0, nullptr, 1, &secondBarrier);

			if (mipWidth > 1) { mipWidth /= 2; }
			if (mipHeight > 1) { mipHeight /= 2; }
		}

		blitRange.baseMipLevel = texture->mipmapLevels - 1;

		VkImageMemoryBarrier2 lastBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcQueueFamilyIndex = 0,
			.dstQueueFamilyIndex = 0,
			.image = texture->image,
			.subresourceRange = blitRange
		};

		commandBuffer.PipelineBarrier(0, 0, nullptr, 1, &lastBarrier);
	}

	VkValidateR(commandBuffer.End());

	commandBuffers[imageIndex].Push(commandBuffer);

	VkImageViewCreateInfo texViewInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = texture->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = (VkFormat)texture->format,
		.components{
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY
		},
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = texture->mipmapLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
	};

	VkValidateR(vkCreateImageView(device, &texViewInfo, allocationCallbacks, &texture->imageView));

	const VkBool32 anisotropyAvailable = device.physicalDevice.features.samplerAnisotropy;
	const F32 maxAnisotropy = device.physicalDevice.features.maxSamplerAnisotropy;

	VkSamplerCreateInfo texSamplerInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = (VkFilter)sampler.filterMode,
		.minFilter = (VkFilter)sampler.filterMode,
		.mipmapMode = (VkSamplerMipmapMode)sampler.mipMapSampleMode,
		.addressModeU = (VkSamplerAddressMode)sampler.edgeSampleMode,
		.addressModeV = (VkSamplerAddressMode)sampler.edgeSampleMode,
		.addressModeW = (VkSamplerAddressMode)sampler.edgeSampleMode,
		.mipLodBias = 0.0f,
		.anisotropyEnable = anisotropyAvailable,
		.maxAnisotropy = maxAnisotropy,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = (F32)texture->mipmapLevels,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE
	};

	VkValidateR(vkCreateSampler(device, &texSamplerInfo, allocationCallbacks, &texture->sampler));

	return true;
}

void Renderer::DestroyTexture(Resource<Texture>& texture)
{
	vkDestroySampler(device, texture->sampler, allocationCallbacks);
	vkDestroyImageView(device, texture->imageView, allocationCallbacks);
	vmaDestroyImage(vmaAllocator, texture->image, texture->allocation);
}