#include "Swapchain.hpp"

#include "VulkanInclude.hpp"
#include "Renderer.hpp"

struct SwapchainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	Vector<VkSurfaceFormatKHR> formats;
	Vector<VkPresentModeKHR> presentModes;
};

bool Swapchain::Create()
{
	SwapchainSupportDetails swapchainSupport = QuerySwapChainSupport(Renderer::device.physicalDevice, Renderer::device.vkSurface);

	U32 imageCount = swapchainSupport.capabilities.minImageCount + 1;
	if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount)
	{
		imageCount = swapchainSupport.capabilities.maxImageCount;
		Renderer::imageCount = imageCount;
	}

	bool sameQueue = Renderer::device.physicalDevice.graphicsQueueIndex == Renderer::device.physicalDevice.presentQueueIndex;
	U32 queueFamilyIndices[]{ Renderer::device.physicalDevice.graphicsQueueIndex, Renderer::device.physicalDevice.presentQueueIndex };

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = Renderer::device.vkSurface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = (VkFormat)Renderer::surfaceFormat;
	createInfo.imageColorSpace = (VkColorSpaceKHR)Renderer::surfaceColorSpace;
	createInfo.imageExtent = swapchainSupport.capabilities.currentExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = sameQueue ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
	createInfo.queueFamilyIndexCount = sameQueue ? 0 : CountOf32(queueFamilyIndices);
	createInfo.pQueueFamilyIndices = sameQueue ? nullptr : queueFamilyIndices;
	createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
#ifdef NH_PLATFORM_ANDROID
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
#else
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
#endif
	createInfo.presentMode = (VkPresentModeKHR)Renderer::presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = vkSwapchain;

	vkCreateSwapchainKHR(Renderer::device, &createInfo, Renderer::allocationCallbacks, &vkSwapchain);

	vkGetSwapchainImagesKHR(Renderer::device, vkSwapchain, &imageCount, nullptr);
	images.resize(imageCount, {});
	vkGetSwapchainImagesKHR(Renderer::device, vkSwapchain, &imageCount, images.data());

	imageViews.resize(imageCount, {});

	for (U64 i = 0; i < images.size(); ++i)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = images[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = (VkFormat)Renderer::surfaceFormat;

		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		vkCreateImageView(Renderer::device, &viewInfo, Renderer::allocationCallbacks, &imageViews[i]);
	}

	return true;
}

void Swapchain::Recreate()
{
	Renderer::ScheduleDestruction(*this);
	Create();
}

void Swapchain::Destroy()
{
	for (VkImageView view : imageViews)
	{
		vkDestroyImageView(Renderer::device, view, Renderer::allocationCallbacks);
	}

	vkDestroySwapchainKHR(Renderer::device, vkSwapchain, Renderer::allocationCallbacks);
}

SwapchainSupportDetails Swapchain::QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	SwapchainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

	U32 formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
	}

	U32 presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

Swapchain::operator VkSwapchainKHR_T* () const
{
	return vkSwapchain;
}

VkSwapchainKHR_T* const* Swapchain::operator&() const
{
	return &vkSwapchain;
}