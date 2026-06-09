#pragma once
#pragma once

#include "RenderingDefines.hpp"

#include "Core/Containers.hpp"

#include "slang/slang.h"
#include "slang/slang-com-ptr.h"

enum VkFormat;
struct ShaderLayoutInfo;
struct VkBuffer_T;
struct VkPipeline_T;
struct VkShaderModule_T;
struct VkDescriptorSet_T;
struct VkPipelineLayout_T;
struct VkDescriptorSetLayout_T;
struct VkSpecializationInfo;

struct Shader
{
public:
	bool Create(const Path& filePath);
	void Destroy();

	VkPipeline_T* Pipeline() const { return vkPipeline; }
	VkPipelineLayout_T* PipelineLayout() const { return vkPipelineLayout; }

private:
	static bool Initialize();
	static void Shutdown();

	ShaderLayoutInfo CompileAndReflect(const StringView& shader, const Path& filePath);
	void ProcessSlangParameter(slang::VariableLayoutReflection* param, ShaderLayoutInfo& info);
	VkFormat MapSlangTypeToVkFormat(slang::TypeReflection* type);
	U32 GetVkFormatSize(VkFormat format);

	VkDescriptorSet_T* GetDescriptorSet(U32 setIndex, U32 frameIndex) const;
	void UpdateStorageBuffer(U32 setIndex, U32 binding, U32 frameIndex, VkBuffer_T* buffer, U64 size);

	static Slang::ComPtr<slang::IGlobalSession> globalSession;
	static Slang::ComPtr<slang::ISession> session;

	Vector<VkDescriptorSetLayout_T*> vkDescriptorSetLayouts;
	Vector<Vector<VkDescriptorSet_T*>> vkDescriptorSets;
	VkPipelineLayout_T* vkPipelineLayout;
	VkPipeline_T* vkPipeline;

	U32 globalTexturesSet = U32_MAX;
	U32 globalTexturesBinding = U32_MAX;

	friend class Renderer;
};