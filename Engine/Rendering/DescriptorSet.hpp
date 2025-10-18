#pragma once

#include "Defines.hpp"

#include "Containers/Vector.hpp"

struct VkBuffer_T;
struct VkSampler_T;
struct VkImageView_T;
struct VkBufferView_T;
struct VkDescriptorSet_T;
struct VkDescriptorSetLayout_T;
struct VkDescriptorImageInfo;
struct VkDescriptorBufferInfo;

enum class NH_API BindingType
{
	Sampler = 0,
	CombinedImageSampler = 1,
	SampledImage = 2,
	StorageImage = 3,
	UniformTexelBuffer = 4,
	StorageTexelBuffer = 5,
	UniformBuffer = 6,
	StorageBuffer = 7,
	UniformBufferDynamic = 8,
	StorageBufferDynamic = 9,
	InputAttachment = 10
};

enum class NH_API ImageLayout
{
	Undefined = 0,
	General = 1,
	ColorAttachment = 2,
	DepthStencilAttachment = 3,
	DepthStencilReadOnly = 4,
	ShaderReadOnly = 5,
	TransferSrc = 6,
	TransferDst = 7,
	DepthReadOnlyStencilAttachment = 1000117000,
	DepthAttachmentStencilReadOnly = 1000117001,
	DepthAttachment = 1000241000,
	DepthReadOnly = 1000241001,
	StencilAttachment = 1000241002,
	StencilReadOnly = 1000241003,
	ReadOnly = 1000314000,
	Attachment = 1000314001,
	Present = 1000001002
};

struct NH_API ImageInfo
{
	VkSampler_T* sampler;
	VkImageView_T* imageView;
	ImageLayout imageLayout;
};

struct NH_API BufferInfo
{
	VkBuffer_T* buffer;
	U64 offset;
	U64 range;
};

struct NH_API DescriptorBinding
{
	BindingType type;
	U32 stages;
	U32 count = 1;

	ImageInfo imageInfo{};
	BufferInfo bufferInfo{};
	VkBufferView_T* texelBufferView{};
};

struct WriteDescriptorSet {
	U32 sType;
	const void* pNext;
	VkDescriptorSet_T* dstSet;
	U32 dstBinding;
	U32 dstArrayElement;
	U32 descriptorCount;
	U32 descriptorType;
	const VkDescriptorImageInfo* pImageInfo;
	const VkDescriptorBufferInfo* pBufferInfo;
	const VkBufferView_T* pTexelBufferView;
};

struct DescriptorSetDestructionData
{
	VkDescriptorSetLayout_T* vkDescriptorLayout;
	VkDescriptorSet_T* vkDescriptorSet;
	bool bindless;
};

struct NH_API DescriptorSet
{
public:
	bool Create(const Vector<DescriptorBinding>& bindings, U32 firstBinding = 0, bool bindless = false);
	void Destroy();

	void Upload();
	void Upload(const Vector<WriteDescriptorSet>& writes);

	operator VkDescriptorSetLayout_T* () const;
	operator VkDescriptorSet_T* () const;

private:
	VkDescriptorSetLayout_T* vkDescriptorLayout;
	VkDescriptorSet_T* vkDescriptorSet;

	Vector<DescriptorBinding> bindings;
	Vector<WriteDescriptorSet> writes;

	bool bindless;

	friend class Renderer;
	friend struct PipelineLayout;
};