#include "RenderTarget.hpp"

#include "VulkanInclude.hpp"
#include "Renderer.hpp"

#include "vma/vk_mem_alloc.h"

void RenderTarget::Create(U32 width, U32 height)
{
	this->width = width;
	this->height = height;

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent = { width, height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = (VkFormat)Renderer::surfaceFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	vmaCreateImage(Renderer::vmaAllocator, &imageInfo, &allocInfo, &image, &allocation, nullptr);
	Renderer::NameAllocation(allocation, "RenderTarget");

	VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = imageInfo.format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	vkCreateImageView(Renderer::device, &viewInfo, nullptr, &imageView);

	VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	vkCreateSampler(Renderer::device, &samplerInfo, Renderer::allocationCallbacks, &vkSampler);

	VkDescriptorImageInfo descriptorImageInfo{};
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptorImageInfo.imageView = imageView;
	descriptorImageInfo.sampler = vkSampler;

	id = Resources::GetTextureId();
	VkWriteDescriptorSet descriptorWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	descriptorWrite.dstSet = Renderer::globalBindlessSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = id;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &descriptorImageInfo;

	vkUpdateDescriptorSets(Renderer::device, 1, &descriptorWrite, 0, nullptr);
}

void RenderTarget::Recreate(U32 width, U32 height)
{
	this->width = width;
	this->height = height;

	vkDestroyImageView(Renderer::device, imageView, Renderer::allocationCallbacks);
	vmaDestroyImage(Renderer::vmaAllocator, image, allocation);

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent = { width, height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = (VkFormat)Renderer::surfaceFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	vmaCreateImage(Renderer::vmaAllocator, &imageInfo, &allocInfo, &image, &allocation, nullptr);
	Renderer::NameAllocation(allocation, "RenderTarget");

	VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = imageInfo.format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	vkCreateImageView(Renderer::device, &viewInfo, nullptr, &imageView);

	VkDescriptorImageInfo descriptorImageInfo{};
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptorImageInfo.imageView = imageView;
	descriptorImageInfo.sampler = vkSampler;

	VkWriteDescriptorSet descriptorWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	descriptorWrite.dstSet = Renderer::globalBindlessSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = id;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &descriptorImageInfo;

	vkUpdateDescriptorSets(Renderer::device, 1, &descriptorWrite, 0, nullptr);
}

void RenderTarget::Destroy()
{
	vkDestroySampler(Renderer::device, vkSampler, Renderer::allocationCallbacks);
	vkDestroyImageView(Renderer::device, imageView, Renderer::allocationCallbacks);

	vmaDestroyImage(Renderer::vmaAllocator, image, allocation);
}

void RenderTarget::StartRender(VkCommandBuffer_T* commandBuffer) const
{
	static constexpr F32 targetAspectRatio = 16.0f / 9.0f;

	F32 currentAspectRatio = (F32)width / (F32)height;

	F32 viewWidth = width;
	F32 viewHeight = height;
	F32 offsetX = 0.0f;
	F32 offsetY = 0.0f;

	if (currentAspectRatio > targetAspectRatio)
	{
		viewWidth = (F32)height * targetAspectRatio;
		offsetX = (width - viewWidth) * 0.5f;
	}
	else
	{
		viewHeight = (F32)width / targetAspectRatio;
		offsetY = (height - viewHeight) * 0.5f;
	}

	VkViewport viewport{};
	viewport.x = offsetX;
	viewport.y = offsetY;
	viewport.width = viewWidth;
	viewport.height = viewHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { (I32)offsetX, (I32)offsetY };
	scissor.extent = { (U32)viewWidth, (U32)viewHeight };
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	Renderer::TransitionImageLayout(commandBuffer, image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	VkRenderingAttachmentInfo viewportAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	viewportAttachment.imageView = imageView;
	viewportAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	viewportAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	viewportAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	viewportAttachment.clearValue = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
	renderingInfo.renderArea = { { 0, 0 }, { width, height } };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &viewportAttachment;
	renderingInfo.flags = 0;

	vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

void RenderTarget::EndRender(VkCommandBuffer_T* commandBuffer) const
{
	vkCmdEndRendering(commandBuffer);

	Renderer::TransitionImageLayout(commandBuffer, image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
}

U32 RenderTarget::Width() const
{
	return width;
}

U32 RenderTarget::Height() const
{
	return height;
}

U32 RenderTarget::Id() const
{
	return id;
}