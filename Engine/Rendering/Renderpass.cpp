#include "Renderpass.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"

bool Renderpass::Create()
{
	VkAttachmentDescription2 colorAttachment{
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.format = (VkFormat)Renderer::surfaceFormat,
		.samples = (VkSampleCountFlagBits)Renderer::device.physicalDevice.maxSampleCount,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkAttachmentReference2 colorAttachmentReference{
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	};

	VkAttachmentDescription2 depthAttachment{
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.format = VK_FORMAT_D32_SFLOAT,
		.samples = (VkSampleCountFlagBits)Renderer::device.physicalDevice.maxSampleCount,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	VkAttachmentReference2 depthAttachmentReference{
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
	};

	VkAttachmentDescription2 colorAttachmentResolve{
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.format = (VkFormat)Renderer::surfaceFormat,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkAttachmentReference2 colorAttachmentResolveReference{
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 2,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	};

	VkMemoryBarrier2 renderBarrier{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
	};

	VkSubpassDescription2 subpass{
		.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.viewMask = 0,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentReference,
		.pResolveAttachments = &colorAttachmentResolveReference,
		.pDepthStencilAttachment = &depthAttachmentReference,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr
	};

	VkSubpassDependency2 depthDependancy{
		.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
		.pNext = nullptr,
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR,
		.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR,
		.srcAccessMask = VK_PIPELINE_STAGE_2_NONE,
		.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.dependencyFlags = 0,
		.viewOffset = 0
	};

	VkSubpassDependency2 renderDependancy{
		.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
		.pNext = nullptr,
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_PIPELINE_STAGE_2_NONE,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dependencyFlags = 0,
		.viewOffset = 0
	};

	VkSubpassDependency2 dependencies[] = { depthDependancy, renderDependancy };
	VkAttachmentDescription2 attachments[] = { colorAttachment, depthAttachment, colorAttachmentResolve };

	VkRenderPassCreateInfo2 renderPassInfo{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = CountOf32(attachments),
		.pAttachments = attachments,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = CountOf32(dependencies),
		.pDependencies = dependencies,
		.correlatedViewMaskCount = 0,
		.pCorrelatedViewMasks = nullptr
	};

	VkValidateFR(vkCreateRenderPass2(Renderer::device, &renderPassInfo, Renderer::allocationCallbacks, &vkRenderpass));

	return true;
}

void Renderpass::Destroy()
{
	if (vkRenderpass)
	{
		vkDestroyRenderPass(Renderer::device, vkRenderpass, Renderer::allocationCallbacks);
	}
}

Renderpass::operator VkRenderPass_T* () const
{
	return vkRenderpass;
}