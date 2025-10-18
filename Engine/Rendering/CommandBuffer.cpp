#include "CommandBuffer.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"

CommandBuffer::CommandBuffer() {}

CommandBuffer::CommandBuffer(VkCommandBuffer_T* vkCommandBuffer) : vkCommandBuffer(vkCommandBuffer)
{

}

U32 CommandBuffer::Begin()
{
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.pNext = nullptr;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	return vkBeginCommandBuffer(vkCommandBuffer, &beginInfo);
}

U32 CommandBuffer::End()
{
	return vkEndCommandBuffer(vkCommandBuffer);
}

void CommandBuffer::ClearAttachments(U32 attachmentCount, VkClearAttachment* attachments, U32 rectCount, VkClearRect* rects)
{
	vkCmdClearAttachments(vkCommandBuffer, attachmentCount, attachments, rectCount, rects);
}

void CommandBuffer::BeginRenderpass(const Renderpass& renderpass, VkFramebuffer_T* frameBuffer)
{
	VkClearValue colorClearValue;
	colorClearValue.color = { { 0.25f, 0.25f, 0.25f, 1.0f } };

	VkClearValue depthValue;
	depthValue.depthStencil.depth = 1.0f;

	Vector<VkClearValue> clearValues = { colorClearValue, depthValue };

	VkRenderPassBeginInfo renderPassBegin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderPassBegin.pNext = nullptr;
	renderPassBegin.renderPass = renderpass;
	renderPassBegin.framebuffer = frameBuffer;
	renderPassBegin.renderArea.offset.x = 0;
	renderPassBegin.renderArea.offset.y = 0;
	renderPassBegin.renderArea.extent.width = Renderer::surfaceWidth;
	renderPassBegin.renderArea.extent.height = Renderer::surfaceHeight;
	renderPassBegin.clearValueCount = (U32)clearValues.Size();
	renderPassBegin.pClearValues = clearValues.Data();

	vkCmdBeginRenderPass(vkCommandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (F32)Renderer::surfaceWidth;
	viewport.height = (F32)Renderer::surfaceHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent.width = Renderer::surfaceWidth;
	scissor.extent.height = Renderer::surfaceHeight;

	vkCmdSetViewport(vkCommandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(vkCommandBuffer, 0, 1, &scissor);
}

void CommandBuffer::NextSubpass()
{
	vkCmdNextSubpass(vkCommandBuffer, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBuffer::EndRenderpass()
{
	vkCmdEndRenderPass(vkCommandBuffer);
}

void CommandBuffer::BindPipeline(const Pipeline& pipeline)
{
	vkCmdBindPipeline(vkCommandBuffer, (VkPipelineBindPoint)pipeline.bindPoint, pipeline);
}

void CommandBuffer::BindIndexBuffer(const Buffer& buffer, U32 offset)
{
	vkCmdBindIndexBuffer(vkCommandBuffer, buffer, offset, VK_INDEX_TYPE_UINT32); //TODO: Accept different index types
}

void CommandBuffer::BindVertexBuffers(U32 count, const VkBuffer* buffers, U64* offsets)
{
	vkCmdBindVertexBuffers(vkCommandBuffer, 0, count, buffers, offsets);
}

void CommandBuffer::BindDescriptorSets(BindPoint bindPoint, const PipelineLayout& pipelineLayout, U32 setOffset, U32 setCount, const VkDescriptorSet* sets)
{
	if (setCount)
	{
		vkCmdBindDescriptorSets(vkCommandBuffer, (VkPipelineBindPoint)bindPoint, pipelineLayout, setOffset, setCount, sets, 0, nullptr);
	}
}

void CommandBuffer::PushConstants(const PipelineLayout& pipelineLayout, U32 stages, U32 offset, U32 size, const void* data)
{
	vkCmdPushConstants(vkCommandBuffer, pipelineLayout, stages, offset, size, data);
}

void CommandBuffer::Draw(U32 firstVertex, U32 vertexCount, U32 firstInstance, U32 instanceCount)
{
	vkCmdDraw(vkCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::DrawIndexed(U32 indexCount, U32 instanceCount, U32 firstIndex, I32 vertexOffset, U32 firstInstance)
{
	vkCmdDrawIndexed(vkCommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer::DrawIndexedIndirect(const Buffer& buffer, U32 count, U32 offset)
{
	//TODO: Take into account physicalDeviceProperties.limits.maxDrawIndirectCount and physicalDeviceFeatures.drawIndirectFirstInstance;

	if (Renderer::device.physicalDevice.features.multiDrawIndirect)
	{
		vkCmdDrawIndexedIndirect(vkCommandBuffer, buffer, offset, count, sizeof(VkDrawIndexedIndirectCommand));
	}
	else
	{
		for (U32 i = 0; i < count; ++i)
		{
			vkCmdDrawIndexedIndirect(vkCommandBuffer, buffer, sizeof(VkDrawIndexedIndirectCommand) * i + offset, 1, sizeof(VkDrawIndexedIndirectCommand));
		}
	}
}

void CommandBuffer::DrawIndexedIndirectCount(const Buffer& drawBuffer, const Buffer& countBuffer, U32 drawOffset, U32 countOffset)
{
	vkCmdDrawIndexedIndirectCount(vkCommandBuffer, drawBuffer, drawOffset, countBuffer, countOffset, 4096u, sizeof(VkDrawIndexedIndirectCommand));
}

void CommandBuffer::DrawIndirectCount(const Buffer& drawBuffer, const Buffer& countBuffer, U32 drawOffset, U32 countOffset)
{
	vkCmdDrawIndirectCount(vkCommandBuffer, drawBuffer, drawOffset, countBuffer, countOffset, 4096u, sizeof(VkDrawIndirectCommand));
}

void CommandBuffer::Dispatch(U32 groupX, U32 groupY, U32 groupZ)
{
	vkCmdDispatch(vkCommandBuffer, groupX, groupY, groupZ);
}

void CommandBuffer::BufferToImage(const Buffer& buffer, Resource<Texture>& texture, U32 regionCount, const VkBufferImageCopy* regions)
{
	vkCmdCopyBufferToImage(vkCommandBuffer, buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regionCount, regions);
}

void CommandBuffer::ImageToBuffer(const ResourceRef<Texture>& texture, const Buffer& buffer, U32 regionCount, const VkBufferImageCopy* regions)
{
	vkCmdCopyImageToBuffer(vkCommandBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, regionCount, regions);
}

void CommandBuffer::BufferToBuffer(VkBuffer src, VkBuffer dst, U32 regionCount, const VkBufferCopy* regions)
{
	vkCmdCopyBuffer(vkCommandBuffer, src, dst, regionCount, regions);
}

void CommandBuffer::BufferToBuffer(const Buffer& src, const Buffer& dst, U32 regionCount, const VkBufferCopy* regions)
{
	vkCmdCopyBuffer(vkCommandBuffer, src, dst, regionCount, regions);
}

void CommandBuffer::ImageToImage(const ResourceRef<Texture>& src, const ResourceRef<Texture>& dst, U32 regionCount, const VkImageCopy* regions)
{
	vkCmdCopyImage(vkCommandBuffer, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regionCount, regions);
}

void CommandBuffer::Blit(Resource<Texture>& src, Resource<Texture>& dst, VkFilter filter, U32 blitCount, const VkImageBlit* blits)
{
	vkCmdBlitImage(vkCommandBuffer, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, blitCount, blits, filter);
}

void CommandBuffer::Blit(const ResourceRef<Texture>& src, const ResourceRef<Texture>& dst, VkFilter filter, U32 blitCount, const VkImageBlit* blits)
{
	vkCmdBlitImage(vkCommandBuffer, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, blitCount, blits, filter);
}

void CommandBuffer::PipelineBarrier(I32 dependencyFlags, U32 bufferBarrierCount, const VkBufferMemoryBarrier2* bufferBarriers, U32 imageBarrierCount, const VkImageMemoryBarrier2* imageBarriers)
{
	VkDependencyInfo dependencyInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dependencyInfo.dependencyFlags = dependencyFlags;
	dependencyInfo.bufferMemoryBarrierCount = bufferBarrierCount;
	dependencyInfo.pBufferMemoryBarriers = bufferBarriers;
	dependencyInfo.imageMemoryBarrierCount = imageBarrierCount;
	dependencyInfo.pImageMemoryBarriers = imageBarriers;

	vkCmdPipelineBarrier2(vkCommandBuffer, &dependencyInfo);
}

CommandBuffer::operator VkCommandBuffer_T* () const
{
	return vkCommandBuffer;
}

VkCommandBuffer_T* const* CommandBuffer::operator&() const
{
	return &vkCommandBuffer;
}