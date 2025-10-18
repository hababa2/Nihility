#include "PipelineLayout.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"

bool PipelineLayout::Create(const Vector<DescriptorSet>& descriptorSets, const Vector<VkPushConstantRange>& pushConstants)
{
	Vector<VkDescriptorSetLayout> layouts(descriptorSets.Size());

	for (const DescriptorSet& set : descriptorSets)
	{
		layouts.Push(set.vkDescriptorLayout);
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = (U32)layouts.Size(),
		.pSetLayouts = layouts.Data(),
		.pushConstantRangeCount = (U32)pushConstants.Size(),
		.pPushConstantRanges = pushConstants.Data(),
	};

	VkValidateFR(vkCreatePipelineLayout(Renderer::device, &pipelineLayoutInfo, Renderer::allocationCallbacks, &vkPipelineLayout));

	return true;
}

void PipelineLayout::Destroy()
{
	if (vkPipelineLayout)
	{
		vkDestroyPipelineLayout(Renderer::device, vkPipelineLayout, Renderer::allocationCallbacks);
	}
}

PipelineLayout::operator VkPipelineLayout_T* () const
{
	return vkPipelineLayout;
}