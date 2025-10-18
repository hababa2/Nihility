#include "Pipeline.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"

#include "Math/Math.hpp"
#include "Core/Logger.hpp"

bool Pipeline::Create(const PipelineLayout& layout, const PipelineSettings& settings, const Vector<Shader>& shaders,
	const Vector<VkVertexInputBindingDescription>& bindings, const Vector<VkVertexInputAttributeDescription>& attributes)
{
	bindPoint = settings.bindPoint;

	Vector<VkPipelineShaderStageCreateInfo> shaderInfos(shaders.Size());

	for (const Shader& shader : shaders)
	{
		VkPipelineShaderStageCreateInfo shaderStage{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = (VkShaderStageFlagBits)shader.type,
			.module = shader,
			.pName = shader.entryPoint.Data(),
			.pSpecializationInfo = nullptr //TODO
		};

		shaderInfos.Push(shaderStage);
	}

	switch (bindPoint)
	{
	case BindPoint::Graphics: {
		for (const VkVertexInputBindingDescription& binding : bindings)
		{
			if (binding.inputRate == VK_VERTEX_INPUT_RATE_INSTANCE) { instanceSize += binding.stride; }
			else if(binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX) { vertexSize += binding.stride; }
			else { Logger::Error("Invalid Vertex Binding Description!"); return false; }
		}

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.vertexBindingDescriptionCount = (U32)bindings.Size(),
			.pVertexBindingDescriptions = bindings.Data(),
			.vertexAttributeDescriptionCount = (U32)attributes.Size(),
			.pVertexAttributeDescriptions = attributes.Data()
		};

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.topology = (VkPrimitiveTopology)settings.topologyMode,
			.primitiveRestartEnable = VK_FALSE
		};

		VkViewport viewport{};
		VkRect2D scissor{};

		VkPipelineViewportStateCreateInfo viewportStateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.viewportCount = 1,
			.pViewports = &viewport,
			.scissorCount = 1,
			.pScissors = &scissor
		};

		VkPipelineRasterizationStateCreateInfo rasterizerInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = (VkPolygonMode)settings.polygonMode,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0f,
			.depthBiasSlopeFactor = 0.0f,
			.lineWidth = 1.0f
		};

		VkPipelineMultisampleStateCreateInfo multisamplingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.rasterizationSamples = (VkSampleCountFlagBits)Renderer::device.physicalDevice.maxSampleCount, //TODO: Setting
			.sampleShadingEnable = VK_TRUE,
			.minSampleShading = 0.2f,
			.pSampleMask = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE
		};

		VkPipelineColorBlendAttachmentState colorBlendAttachment{
			.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};

		VkPipelineColorBlendStateCreateInfo colorBlendingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments = &colorBlendAttachment,
			.blendConstants = 0.0f
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.depthTestEnable = settings.useDepth,
			.depthWriteEnable = settings.useDepth,
			.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {},
			.back = {},
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f
		};

		Vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		VkPipelineDynamicStateCreateInfo dynamicStatesInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.dynamicStateCount = (U32)dynamicStates.Size(),
			.pDynamicStates = dynamicStates.Data()
		};

		VkGraphicsPipelineCreateInfo pipelineCreateInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stageCount = (U32)shaderInfos.Size(),
			.pStages = shaderInfos.Data(),
			.pVertexInputState = &vertexInputInfo,
			.pInputAssemblyState = &inputAssemblyInfo,
			.pTessellationState = nullptr,
			.pViewportState = &viewportStateInfo,
			.pRasterizationState = &rasterizerInfo,
			.pMultisampleState = &multisamplingInfo,
			.pDepthStencilState = &depthStencilInfo,
			.pColorBlendState = &colorBlendingInfo,
			.pDynamicState = &dynamicStatesInfo,
			.layout = layout,
			.renderPass = Renderer::renderpass,
			.subpass = 0,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = 0
		};

		//TODO: Cache
		VkValidateFR(vkCreateGraphicsPipelines(Renderer::device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, Renderer::allocationCallbacks, &vkPipeline));
	} break;
	case BindPoint::Compute: {
		Logger::Error("Compute Not Yet Supported!");
		return false;
	} break;
	case BindPoint::RayTracing: {
		Logger::Error("Ray Tracing Not Yet Supported!");
		return false;
	} break;
	}

	return true;
}

void Pipeline::Destroy()
{
	Renderer::ScheduleDestruction(*this);
	vkPipeline = nullptr;
}

Pipeline::operator VkPipeline_T* () const
{
	return vkPipeline;
}

U8 Pipeline::VertexSize() const
{
	return vertexSize;
}

U8 Pipeline::InstanceSize() const
{
	return instanceSize;
}