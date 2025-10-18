#include "DescriptorSet.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"

bool DescriptorSet::Create(const Vector<DescriptorBinding>& bs, U32 firstBinding, bool bindless)
{
	this->bindless = bindless;
	bindings = bs;

	Vector<VkDescriptorSetLayoutBinding> layoutBindings(bindings.Size());

	U32 i = firstBinding;
	for (const DescriptorBinding& b : bindings)
	{
		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{
			.binding = i++,
			.descriptorType = (VkDescriptorType)b.type,
			.descriptorCount = b.count,
			.stageFlags = b.stages,
			.pImmutableSamplers = nullptr
		};

		layoutBindings.Push(descriptorSetLayoutBinding);
	}

	VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;
	VkDescriptorBindingFlags bindingFlags[2]{ bindlessFlags, bindlessFlags };

	VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.pNext = nullptr,
		.bindingCount = (U32)layoutBindings.Size(),
		.pBindingFlags = bindingFlags
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = bindless ? &extendedInfo : nullptr,
		.flags = bindless ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT : (VkDescriptorSetLayoutCreateFlags)0,
		.bindingCount = (U32)layoutBindings.Size(),
		.pBindings = layoutBindings.Data()
	};

	VkValidateR(vkCreateDescriptorSetLayout(Renderer::device, &descriptorSetLayoutCreateInfo, Renderer::allocationCallbacks, &vkDescriptorLayout));

	VkDescriptorSetAllocateInfo descriptorAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = bindless ? Renderer::vkBindlessDescriptorPool : Renderer::vkDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &vkDescriptorLayout
	};

	VkValidateFR(vkAllocateDescriptorSets(Renderer::device, &descriptorAllocateInfo, &vkDescriptorSet));

	if (!bindless)
	{
		i = firstBinding;
		for (const DescriptorBinding& b : bindings)
		{
			VkWriteDescriptorSet matrixWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = vkDescriptorSet,
				.dstBinding = i++,
				.dstArrayElement = 0,
				.descriptorCount = b.count,
				.descriptorType = (VkDescriptorType)b.type
			};

			switch (b.type)
			{
			case BindingType::Sampler:
			case BindingType::CombinedImageSampler:
			case BindingType::SampledImage:
			case BindingType::StorageImage: {
				matrixWriteDescriptorSet.pImageInfo = (VkDescriptorImageInfo*)&b.imageInfo;
			} break;
			case BindingType::InputAttachment:
			case BindingType::UniformBuffer:
			case BindingType::StorageBuffer:
			case BindingType::UniformBufferDynamic:
			case BindingType::StorageBufferDynamic: {
				matrixWriteDescriptorSet.pBufferInfo = (VkDescriptorBufferInfo*)&b.bufferInfo;
			} break;
			case BindingType::UniformTexelBuffer:
			case BindingType::StorageTexelBuffer: {
				matrixWriteDescriptorSet.pTexelBufferView = &b.texelBufferView;
			} break;
			}
		}
	}

	return true;
}

void DescriptorSet::Destroy()
{
	Renderer::ScheduleDestruction(*this);

	vkDescriptorSet = nullptr;
	vkDescriptorLayout = nullptr;
}

void DescriptorSet::Upload()
{
	vkUpdateDescriptorSets(Renderer::device, (U32)writes.Size(), (VkWriteDescriptorSet*)writes.Data(), 0, nullptr);
}

void DescriptorSet::Upload(const Vector<WriteDescriptorSet>& writes)
{
	vkUpdateDescriptorSets(Renderer::device, (U32)writes.Size(), (VkWriteDescriptorSet*)writes.Data(), 0, nullptr);
}

DescriptorSet::operator VkDescriptorSetLayout_T* () const
{
	return vkDescriptorLayout;
}

DescriptorSet::operator VkDescriptorSet_T* () const
{
	return vkDescriptorSet;
}