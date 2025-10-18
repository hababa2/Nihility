#pragma once

#include "Defines.hpp"

#include "Pipeline.hpp"
#include "Renderpass.hpp"
#include "Swapchain.hpp"
#include "Buffer.hpp"

#include "Resources/Texture.hpp"

struct VkCommandBuffer_T;
struct VkBuffer_T;
struct VkFramebuffer_T;
struct VkDescriptorSet_T;
struct VkBufferImageCopy;
struct VkClearAttachment;
struct VkClearRect;
struct VkBufferCopy;
struct VkImageCopy;
struct VkImageBlit;
struct VkBufferMemoryBarrier2;
struct VkImageMemoryBarrier2;
enum VkFilter;

struct CommandBuffer
{
	U32 Begin();
	U32 End();

	void ClearAttachments(U32 attachmentCount, VkClearAttachment* attachments, U32 rectCount, VkClearRect* rects);

	void BeginRenderpass(const Renderpass& renderpass, VkFramebuffer_T* frameBuffer);
	void NextSubpass();
	void EndRenderpass();

	void BindPipeline(const Pipeline& pipeline);
	void BindIndexBuffer(const Buffer& buffer, U32 offset);
	void BindVertexBuffers(U32 count, VkBuffer_T* const* buffers, U64* offsets);
	void BindDescriptorSets(BindPoint bindPoint, const PipelineLayout& pipelineLayout, U32 setOffset, U32 setCount, VkDescriptorSet_T* const* sets);

	void PushConstants(const PipelineLayout& pipelineLayout, U32 stages, U32 offset, U32 size, const void* data);

	void Draw(U32 firstVertex, U32 vertexCount, U32 firstInstance, U32 instanceCount);
	void DrawIndexed(U32 indexCount, U32 instanceCount, U32 firstIndex, I32 vertexOffset, U32 firstInstance);
	void DrawIndirect(const Buffer& buffer, U32 offset, U32 stride);
	void DrawIndexedIndirect(const Buffer& buffer, U32 count, U32 offset = 0);
	void DrawIndexedIndirectCount(const Buffer& drawBuffer, const Buffer& countBuffer, U32 drawOffset = 0, U32 countOffset = 0);
	void DrawIndirectCount(const Buffer& drawBuffer, const Buffer& countBuffer, U32 drawOffset = 0, U32 countOffset = 0);

	void Dispatch(U32 groupX, U32 groupY, U32 groupZ);
	void DispatchIndirect(const Buffer& buffer, U32 offset);

	void BufferToImage(const Buffer& buffer, Resource<Texture>& texture, U32 regionCount, const VkBufferImageCopy* regions);
	void ImageToBuffer(const ResourceRef<Texture>& texture, const Buffer& buffer, U32 regionCount, const VkBufferImageCopy* regions);
	void BufferToBuffer(VkBuffer_T* src, VkBuffer_T* dst, U32 regionCount, const VkBufferCopy* regions);
	void BufferToBuffer(const Buffer& src, const Buffer& dst, U32 regionCount, const VkBufferCopy* regions);
	void ImageToImage(const ResourceRef<Texture>& src, const ResourceRef<Texture>& dst, U32 regionCount, const VkImageCopy* regions);
	void Blit(Resource<Texture>& src, Resource<Texture>& dst, VkFilter filter, U32 blitCount, const VkImageBlit* blits);
	void Blit(const ResourceRef<Texture>& src, const ResourceRef<Texture>& dst, VkFilter filter, U32 blitCount, const VkImageBlit* blits);

	void PipelineBarrier(I32 dependencyFlags, U32 bufferBarrierCount, const VkBufferMemoryBarrier2* bufferBarriers, U32 imageBarrierCount, const VkImageMemoryBarrier2* imageBarriers);

private:
	CommandBuffer();
	CommandBuffer(VkCommandBuffer_T* vkCommandBuffer);

	VkCommandBuffer_T* vkCommandBuffer = nullptr;

	operator VkCommandBuffer_T*() const;
	VkCommandBuffer_T* const* operator&() const;

	friend class Renderer;
	friend class CommandBufferRing;
	friend struct Buffer;
};