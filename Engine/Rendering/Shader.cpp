#include "Shader.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"

bool Shader::Create(const String& path, ShaderStage type, const StringView& entryPoint)
{
	this->type = type;
	this->entryPoint = entryPoint;

	File file(path, FILE_OPEN_RESOURCE_READ);
	String data = file.ReadAll();

	VkShaderModuleCreateInfo shaderCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.codeSize = data.Size(),
		.pCode = (U32*)data.Data()
	};

	VkValidateFR(vkCreateShaderModule(Renderer::device, &shaderCreateInfo, Renderer::allocationCallbacks, &vkShaderModule));

	return true;
}

void Shader::Destroy()
{
	if (vkShaderModule)
	{
		vkDestroyShaderModule(Renderer::device, vkShaderModule, Renderer::allocationCallbacks);
	}
}

Shader::operator VkShaderModule_T* () const
{
	return vkShaderModule;
}