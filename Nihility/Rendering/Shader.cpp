#include "Shader.hpp"

#include "VulkanInclude.hpp"
#include "Renderer.hpp"

#include "Core/File.hpp"

struct CompiledShader
{
	Slang::ComPtr<slang::IBlob> spirvCode;
	StringView entryPoint;
	VkShaderStageFlagBits stage;
};

struct PipelineConfig
{
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
	VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
	bool depthTestEnable = false;
	bool depthWriteEnable = false;
};

struct ShaderLayoutInfo
{
	Vector<CompiledShader> stages;
	VkPushConstantRange pushConstantRange{};
	bool hasPushConstant = false;

	Hashmap<U32, Vector<VkDescriptorSetLayoutBinding>> setBindings;

	Vector<VkVertexInputBindingDescription> vertexBindings;
	Vector<VkVertexInputAttributeDescription> vertexAttributes;

	PipelineConfig config;
};

Slang::ComPtr<slang::IGlobalSession> Shader::globalSession;
Slang::ComPtr<slang::ISession> Shader::session;

bool Shader::Initialize()
{
	slang::createGlobalSession(globalSession.writeRef());

	slang::TargetDesc targetDesc{};
	targetDesc.format = SLANG_SPIRV;
	targetDesc.profile = globalSession->findProfile("glsl_460");

	Vector<const C*> includes = { "ShaderIndluces/" };

	slang::SessionDesc sessionDesc{};
	sessionDesc.targets = &targetDesc;
	sessionDesc.targetCount = 1;
	sessionDesc.searchPaths = includes.data();
	sessionDesc.searchPathCount = includes.size();
	sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

	globalSession->createSession(sessionDesc, session.writeRef());

	return true;
}

void Shader::Shutdown()
{
	slang::shutdown();
}

bool Shader::Create(const Path& filePath)
{
	FileData data = FileIO::ReadFileSync(filePath);
	StringView shader = { data.buffer, data.bufferSize };

	ShaderLayoutInfo layoutInfo = CompileAndReflect(shader, filePath);

	for (const auto& [setIndex, bindings] : layoutInfo.setBindings)
	{
		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutCreateInfo.bindingCount = static_cast<U32>(bindings.size());
		layoutCreateInfo.pBindings = bindings.data();

		vkCreateDescriptorSetLayout(Renderer::device, &layoutCreateInfo, Renderer::allocationCallbacks, &setLayout);

		if (setIndex >= vkDescriptorSetLayouts.size())
		{
			vkDescriptorSetLayouts.resize(setIndex + 1, VK_NULL_HANDLE);
			vkDescriptorSets.resize(setIndex + 1, {});
		}

		vkDescriptorSetLayouts[setIndex] = setLayout;

		vkDescriptorSets[setIndex].resize(MaxFramesInFlight, VK_NULL_HANDLE);

		Vector<VkDescriptorSetLayout> layouts(MaxFramesInFlight, setLayout);

		VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = Renderer::globalDescriptorPool;
		allocInfo.descriptorSetCount = MaxFramesInFlight;
		allocInfo.pSetLayouts = layouts.data();

		if (vkAllocateDescriptorSets(Renderer::device, &allocInfo, vkDescriptorSets[setIndex].data()) != VK_SUCCESS)
		{
			BreakPoint;
		}
	}

	if (globalTexturesSet != U32_MAX)
	{
		if (globalTexturesSet >= vkDescriptorSetLayouts.size())
		{
			vkDescriptorSetLayouts.resize(globalTexturesSet + 1, VK_NULL_HANDLE);
		}

		vkDescriptorSetLayouts[globalTexturesSet] = Renderer::globalBindlessSetLayout;
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutInfo.setLayoutCount = static_cast<U32>(vkDescriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = vkDescriptorSetLayouts.data();

	if (layoutInfo.hasPushConstant)
	{
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &layoutInfo.pushConstantRange;
	}

	vkCreatePipelineLayout(Renderer::device, &pipelineLayoutInfo, Renderer::allocationCallbacks, &vkPipelineLayout);

	Vector<VkPipelineShaderStageCreateInfo> shaderStages(layoutInfo.stages.size(), {});

	for (U32 i = 0; i < shaderStages.size(); ++i)
	{
		VkShaderModuleCreateInfo shaderModuleInfo{};
		shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleInfo.pNext = nullptr;
		shaderModuleInfo.flags = 0;
		shaderModuleInfo.codeSize = layoutInfo.stages[i].spirvCode->getBufferSize();
		shaderModuleInfo.pCode = (const U32*)layoutInfo.stages[i].spirvCode->getBufferPointer();

		shaderStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[i].stage = layoutInfo.stages[i].stage;
		shaderStages[i].pName = "main";
		vkCreateShaderModule(Renderer::device, &shaderModuleInfo, Renderer::allocationCallbacks, &shaderStages[i].module);
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<U32>(layoutInfo.vertexBindings.size());
	vertexInputInfo.pVertexBindingDescriptions = layoutInfo.vertexBindings.empty() ? nullptr : layoutInfo.vertexBindings.data();
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<U32>(layoutInfo.vertexAttributes.size());
	vertexInputInfo.pVertexAttributeDescriptions = layoutInfo.vertexAttributes.empty() ? nullptr : layoutInfo.vertexAttributes.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssembly.topology = layoutInfo.config.topology;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	Vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.dynamicStateCount = static_cast<U32>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = layoutInfo.config.cullMode;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = (VkFormat*)&Renderer::surfaceFormat;

	VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.pNext = &pipelineRenderingCreateInfo;
	pipelineInfo.stageCount = (U32)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = vkPipelineLayout;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;

	//TODO: pipeline caching
	VkValidateFR(vkCreateGraphicsPipelines(Renderer::device, VK_NULL_HANDLE, 1, &pipelineInfo, Renderer::allocationCallbacks, &vkPipeline));

	for (U32 i = 0; i < shaderStages.size(); ++i)
	{
		vkDestroyShaderModule(Renderer::device, shaderStages[i].module, Renderer::allocationCallbacks);
	}

	FileIO::FreeData(data);

	return true;
}

void Shader::Destroy()
{
	vkDestroyPipeline(Renderer::device, vkPipeline, Renderer::allocationCallbacks);
	vkDestroyPipelineLayout(Renderer::device, vkPipelineLayout, Renderer::allocationCallbacks);

	for (VkDescriptorSetLayout& layout : vkDescriptorSetLayouts)
	{
		if (layout != Renderer::globalBindlessSetLayout)
		{
			vkDestroyDescriptorSetLayout(Renderer::device, layout, Renderer::allocationCallbacks);
		}
	}
}

ShaderLayoutInfo Shader::CompileAndReflect(const StringView& shader, const Path& filePath)
{
	ShaderLayoutInfo resultInfo{};

	slang::IModule* module = session->loadModuleFromSourceString(filePath.filename().string().c_str(), filePath.string().c_str(), shader.data());

	Vector<slang::IComponentType*> componentTypes;
	componentTypes.push_back(module);

	I32 entryPointCount = module->getDefinedEntryPointCount();
	resultInfo.stages.resize(entryPointCount, {});

	Vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints(entryPointCount, {});

	for (I32 i = 0; i < entryPointCount; ++i)
	{
		module->getDefinedEntryPoint(i, entryPoints[i].writeRef());

		slang::FunctionReflection* reflection = entryPoints[i]->getFunctionReflection();
		resultInfo.stages[i].entryPoint = reflection->getName();

		slang::Attribute* attribute = reflection->findAttributeByName(globalSession, "shader");
		if (attribute)
		{
			StringView stage = attribute->getArgumentValueString(0, nullptr);

			switch (HashCI(stage.data(), stage.size()))
			{
			case "vertex"_HashCI: { resultInfo.stages[i].stage = VK_SHADER_STAGE_VERTEX_BIT; } break;
			case "fragment"_HashCI: { resultInfo.stages[i].stage = VK_SHADER_STAGE_FRAGMENT_BIT; } break;
			case "compute"_HashCI: { resultInfo.stages[i].stage = VK_SHADER_STAGE_COMPUTE_BIT; } break;
			default: { BreakPoint; } break;
			}
		}

		slang::Attribute* pipelineAttr = reflection->findAttributeByName(globalSession, "PipelineState");
		if (pipelineAttr)
		{
			StringView topologyStr = pipelineAttr->getArgumentValueString(0, nullptr);
			switch (HashCI(topologyStr.data(), topologyStr.size()))
			{
			case "TriangleList"_HashCI: { resultInfo.config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; } break;
			case "TriangleStrip"_HashCI: { resultInfo.config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; } break;
			case "PointList"_HashCI: { resultInfo.config.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; } break;
			case "LineList"_HashCI: { resultInfo.config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; } break;
			}

			StringView cullStr = pipelineAttr->getArgumentValueString(1, nullptr);
			switch (HashCI(cullStr.data(), cullStr.size()))
			{
			case "None"_HashCI: { resultInfo.config.cullMode = VK_CULL_MODE_NONE; } break;
			case "Front"_HashCI: { resultInfo.config.cullMode = VK_CULL_MODE_FRONT_BIT; } break;
			case "Back"_HashCI: { resultInfo.config.cullMode = VK_CULL_MODE_BACK_BIT; } break;
			}
		}

		componentTypes.push_back(entryPoints[i].get());
	}

	Slang::ComPtr<slang::IComponentType> linkedProgram;
	session->createCompositeComponentType(componentTypes.data(), componentTypes.size(), linkedProgram.writeRef());

	Slang::ComPtr<slang::IComponentType> program;
	linkedProgram->link(program.writeRef());

	slang::ProgramLayout* programLayout = program->getLayout(0);

	Vector<Slang::ComPtr<slang::IMetadata>> entryPointMetadatas(entryPointCount, {});

	for (I32 i = 0; i < entryPointCount; ++i)
	{
		program->getEntryPointCode(i, 0, resultInfo.stages[i].spirvCode.writeRef());
	}

	U32 parameterCount = programLayout->getParameterCount();
	for (U32 i = 0; i < parameterCount; ++i)
	{
		slang::VariableLayoutReflection* variableLayout = programLayout->getParameterByIndex(i);
		ProcessSlangParameter(variableLayout, resultInfo);
	}

	for (I32 i = 0; i < entryPointCount; ++i)
	{
		if (resultInfo.stages[i].stage == VK_SHADER_STAGE_VERTEX_BIT)
		{
			slang::EntryPointLayout* entryLayout = programLayout->getEntryPointByIndex(i);
			U32 paramCount = entryLayout->getParameterCount();

			for (U32 p = 0; p < paramCount; ++p)
			{
				slang::VariableLayoutReflection* varLayout = entryLayout->getParameterByIndex(p);

				if (varLayout->getCategory() == slang::ParameterCategory::VaryingInput)
				{
					slang::TypeLayoutReflection* typeLayout = varLayout->getTypeLayout();

					if (typeLayout->getKind() == slang::TypeReflection::Kind::Struct)
					{
						U32 fieldCount = typeLayout->getFieldCount();
						U32 currentByteOffset = 0;

						for (U32 f = 0; f < fieldCount; ++f)
						{
							slang::VariableLayoutReflection* fieldLayout = typeLayout->getFieldByIndex(f);

							VkVertexInputAttributeDescription attr{};
							attr.binding = 0;
							attr.location = (U32)fieldLayout->getBindingIndex();
							attr.format = MapSlangTypeToVkFormat(fieldLayout->getTypeLayout()->getType());
							attr.offset = currentByteOffset;

							resultInfo.vertexAttributes.push_back(attr);

							currentByteOffset += GetVkFormatSize(attr.format);
						}

						VkVertexInputBindingDescription binding{};
						binding.binding = 0;
						binding.stride = currentByteOffset;
						binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
						resultInfo.vertexBindings.push_back(binding);
					}
				}
			}
		}
	}

	return Move(resultInfo);
}

void Shader::ProcessSlangParameter(slang::VariableLayoutReflection* variableLayout, ShaderLayoutInfo& info)
{
	slang::VariableReflection* variable = variableLayout->getVariable();
	U32 attributeCount = variable->getUserAttributeCount();

	for (U32 i = 0; i < attributeCount; ++i)
	{
		slang::UserAttribute* attribute = variable->getUserAttributeByIndex(i);

		if (std::strcmp(attribute->getName(), "GlobalTextures") == 0)
		{
			globalTexturesSet = variableLayout->getBindingSpace();
			return;
		}
	}

	slang::ParameterCategory category = variableLayout->getCategory();
	slang::TypeLayoutReflection* typeLayout = variableLayout->getTypeLayout();

	if (category == slang::ParameterCategory::PushConstantBuffer)
	{
		info.hasPushConstant = true;
		info.pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
		info.pushConstantRange.offset = (U32)variableLayout->getOffset();

		slang::TypeLayoutReflection* elementType = typeLayout->getElementTypeLayout();
		info.pushConstantRange.size = elementType ? (U32)elementType->getSize() : (U32)typeLayout->getSize();
		return;
	}

	if (category == slang::ParameterCategory::DescriptorTableSlot ||
		category == slang::ParameterCategory::Uniform ||
		category == slang::ParameterCategory::ShaderResource ||
		category == slang::ParameterCategory::UnorderedAccess)
	{
		VkDescriptorSetLayoutBinding binding{};
		binding.binding = variableLayout->getBindingIndex();
		binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;

		slang::TypeReflection::Kind kind = typeLayout->getKind();

		if (kind == slang::TypeReflection::Kind::Array)
		{
			U32 elemCount = (U32)typeLayout->getElementCount();
			binding.descriptorCount = elemCount;

			typeLayout = typeLayout->getElementTypeLayout();
			kind = typeLayout->getKind();
		}
		else
		{
			binding.descriptorCount = 1;
		}

		if (kind == slang::TypeReflection::Kind::Resource)
		{
			slang::TypeReflection* type = typeLayout->getType();

			SlangResourceShape shape = type->getResourceShape();
			SlangResourceAccess access = type->getResourceAccess();

			if (shape == SLANG_STRUCTURED_BUFFER)
			{
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			}
			else if (access == SLANG_RESOURCE_ACCESS_READ_WRITE)
			{
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			}
			else
			{
				binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			}
		}
		else if (kind == slang::TypeReflection::Kind::ConstantBuffer)
		{
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		}

		U32 setIndex = variableLayout->getBindingSpace();
		info.setBindings[setIndex].push_back(binding);
	}
}

VkFormat Shader::MapSlangTypeToVkFormat(slang::TypeReflection* type)
{
	slang::TypeReflection::Kind kind = type->getKind();
	slang::TypeReflection::ScalarType scalarType = slang::TypeReflection::ScalarType::None;
	U32 rowCount = 1;

	if (kind == slang::TypeReflection::Kind::Vector)
	{
		scalarType = type->getElementType()->getScalarType();
		rowCount = (U32)type->getElementCount();
	}
	else if (kind == slang::TypeReflection::Kind::Scalar)
	{
		scalarType = type->getScalarType();
		rowCount = 1;
	}

	if (scalarType == slang::TypeReflection::ScalarType::Float32)
	{
		if (rowCount == 1) { return VK_FORMAT_R32_SFLOAT; }
		if (rowCount == 2) { return VK_FORMAT_R32G32_SFLOAT; }
		if (rowCount == 3) { return VK_FORMAT_R32G32B32_SFLOAT; }
		if (rowCount == 4) { return VK_FORMAT_R32G32B32A32_SFLOAT; }
	}
	else if (scalarType == slang::TypeReflection::ScalarType::UInt32)
	{
		if (rowCount == 1) { return VK_FORMAT_R32_UINT; }
		if (rowCount == 2) { return VK_FORMAT_R32G32_UINT; }
		if (rowCount == 3) { return VK_FORMAT_R32G32B32_UINT; }
		if (rowCount == 4) { return VK_FORMAT_R32G32B32A32_UINT; }
	}
	else if (scalarType == slang::TypeReflection::ScalarType::Int32)
	{
		if (rowCount == 1) { return VK_FORMAT_R32_SINT; }
		if (rowCount == 2) { return VK_FORMAT_R32G32_SINT; }
		if (rowCount == 3) { return VK_FORMAT_R32G32B32_SINT; }
		if (rowCount == 4) { return VK_FORMAT_R32G32B32A32_SINT; }
	}

	return VK_FORMAT_UNDEFINED;
}

U32 Shader::GetVkFormatSize(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32_UINT:
	case VK_FORMAT_R32_SINT: { return 4; }
	case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R32G32_UINT:
	case VK_FORMAT_R32G32_SINT: { return 8; }
	case VK_FORMAT_R32G32B32_SFLOAT:
	case VK_FORMAT_R32G32B32_UINT:
	case VK_FORMAT_R32G32B32_SINT: { return 12; }
	case VK_FORMAT_R32G32B32A32_SFLOAT:
	case VK_FORMAT_R32G32B32A32_UINT:
	case VK_FORMAT_R32G32B32A32_SINT: { return 16; }
	default: { return 0; }
	}
}

VkDescriptorSet_T* Shader::GetDescriptorSet(U32 setIndex, U32 frameIndex) const
{
	return vkDescriptorSets[setIndex][frameIndex];
}

void Shader::UpdateStorageBuffer(U32 setIndex, U32 binding, U32 frameIndex, VkBuffer_T* buffer, U64 size)
{
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = size;

	VkWriteDescriptorSet descriptorWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	descriptorWrite.dstSet = vkDescriptorSets[setIndex][frameIndex];
	descriptorWrite.dstBinding = binding;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(Renderer::device, 1, &descriptorWrite, 0, nullptr);
}
