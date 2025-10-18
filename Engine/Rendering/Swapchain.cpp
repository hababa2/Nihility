#include "Swapchain.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"

#include "Math/Math.hpp"

bool Swapchain::Create()
{
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Renderer::device.physicalDevice, Renderer::device.vkSurface, &capabilities);

	VkExtent2D extent = {
		Renderer::surfaceWidth,
		Renderer::surfaceHeight
	};

	bool sameQueue = Renderer::device.physicalDevice.graphicsQueueIndex == Renderer::device.physicalDevice.presentQueueIndex;
	U32 queueFamilyIndices[]{ Renderer::device.physicalDevice.graphicsQueueIndex, Renderer::device.physicalDevice.presentQueueIndex };

	Renderer::ScheduleDestruction(*this);

	VkSwapchainCreateInfoKHR swapchainCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = Renderer::device.vkSurface,
		.minImageCount = Renderer::imageCount,
		.imageFormat = (VkFormat)Renderer::surfaceFormat,
		.imageColorSpace = (VkColorSpaceKHR)Renderer::surfaceColorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = sameQueue ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
		.queueFamilyIndexCount = sameQueue ? 0 : CountOf32(queueFamilyIndices),
		.pQueueFamilyIndices = sameQueue ? nullptr : queueFamilyIndices,
		.preTransform = capabilities.currentTransform,
#if defined(NH_PLATFORM_ANDROID)
		.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
#else
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
#endif
		.presentMode = (VkPresentModeKHR)Renderer::presentMode,
		.clipped = true,
		.oldSwapchain = vkSwapchain
	};

	VkValidateFR(vkCreateSwapchainKHR(Renderer::device, &swapchainCreateInfo, Renderer::allocationCallbacks, &vkSwapchain));

	U32 imageCount;
	VkValidate(vkGetSwapchainImagesKHR(Renderer::device, vkSwapchain, &imageCount, nullptr));
	images.Resize(imageCount, nullptr);
	VkValidate(vkGetSwapchainImagesKHR(Renderer::device, vkSwapchain, &imageCount, images.Data()));

	imageViews.Resize(images.Size(), nullptr);
	framebuffers.Resize(images.Size(), nullptr);

	VkImageViewUsageCreateInfo usage = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
	usage.pNext = nullptr;
	usage.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	for (U64 i = 0; i < images.Size(); ++i)
	{
		VkImageViewCreateInfo imageViewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = &usage,
			.flags = 0,
			.image = images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = (VkFormat)Renderer::surfaceFormat,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_R,
				.g = VK_COMPONENT_SWIZZLE_G,
				.b = VK_COMPONENT_SWIZZLE_B,
				.a = VK_COMPONENT_SWIZZLE_A
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		VkValidateFR(vkCreateImageView(Renderer::device, &imageViewCreateInfo, Renderer::allocationCallbacks, &imageViews[i]));

		VkImageView attachments[] = { Renderer::colorTextures[i].imageView, Renderer::depthTextures[i].imageView, imageViews[i]};

		VkFramebufferCreateInfo framebufferCreateInfo{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = Renderer::renderpass,
			.attachmentCount = CountOf32(attachments),
			.pAttachments = attachments,
			.width = Renderer::surfaceWidth,
			.height = Renderer::surfaceHeight,
			.layers = 1
		};

		VkValidateFR(vkCreateFramebuffer(Renderer::device, &framebufferCreateInfo, Renderer::allocationCallbacks, &framebuffers[i]));
	}

	return true;
}

void Destroy()
{

}

Swapchain::operator VkSwapchainKHR_T* () const
{
	return vkSwapchain;
}

VkSwapchainKHR_T* const* Swapchain::operator&() const
{
	return &vkSwapchain;
}