#include "Material.hpp"

#include "Rendering/VulkanInclude.hpp"

#include "Resources.hpp"

#include "Rendering/Renderer.hpp"

bool Material::Create(const PipelineLayout& pipelineLayout, const Pipeline& pipeline, const Vector<VkDescriptorSet>& descriptorSets, const Vector<PushConstant>& pushConstants)
{
	this->pipelineLayout = pipelineLayout;
	this->pipeline = pipeline;
	this->sets = descriptorSets;
	this->pushConstants = pushConstants;

	if (pipeline.VertexSize())
	{
		vertexBuffer.Create(BufferType::Vertex, pipeline.VertexSize() * 4);
		Renderer::NameResource(VK_OBJECT_TYPE_BUFFER, vertexBuffer, "Material Vertex Buffer");
		Renderer::NameResource(VK_OBJECT_TYPE_BUFFER, vertexBuffer, "Material Vertex Staging Buffer");
		indexBuffer.Create(BufferType::Index, sizeof(U32) * 6);
		Renderer::NameResource(VK_OBJECT_TYPE_BUFFER, indexBuffer, "Material Index Buffer");
		Renderer::NameResource(VK_OBJECT_TYPE_BUFFER, indexBuffer, "Material Index Staging Buffer");
	}

	if (pipeline.InstanceSize())
	{
		for (U32 i = 0; i < Renderer::imageCount; ++i)
		{
			instanceBuffers[i].Create(BufferType::Vertex, pipeline.InstanceSize() * 10000);
			Renderer::NameResource(VK_OBJECT_TYPE_BUFFER, instanceBuffers[i], { FORMAT, "Material Instance Buffer ", i });
			Renderer::NameResource(VK_OBJECT_TYPE_BUFFER, instanceBuffers[i], { FORMAT, "Material Instance Staging Buffer ", i });
		}
	}

	if (pipeline.VertexSize() && pipeline.InstanceSize()) { vertexUsage = VertexUsage::VerticesAndInstances; }
	else if (pipeline.VertexSize()) { vertexUsage = VertexUsage::Vertices; }
	else if (pipeline.InstanceSize()) { vertexUsage = VertexUsage::Instances; }
	else { vertexUsage = VertexUsage::None; }

	return true;
}

void Material::Destroy()
{
	vertexBuffer.Destroy();
	indexBuffer.Destroy();

	for (U32 i = 0; i < Renderer::imageCount; ++i)
	{
		instanceBuffers[i].Destroy();
	}

	pipeline.Destroy();
	pipelineLayout.Destroy();
}

void Material::Bind(CommandBuffer commandBuffer) const
{
	if ((vertexUsage == VertexUsage::VerticesAndInstances || vertexUsage == VertexUsage::Instances) && instanceBuffers[Renderer::imageIndex].Offset() == U64_MAX) { return; }
	if ((vertexUsage == VertexUsage::VerticesAndInstances || vertexUsage == VertexUsage::Vertices) && vertexBuffer.Offset() == U64_MAX) { return; }

	commandBuffer.BindPipeline(pipeline);
	if (sets.Size()) { commandBuffer.BindDescriptorSets(BindPoint::Graphics, pipelineLayout, 0, (U32)sets.Size(), sets.Data()); }
	for (const PushConstant& pc : pushConstants)
	{
		commandBuffer.PushConstants(pipelineLayout, pc.stages, pc.offset, pc.size, pc.data);
	}

	switch (vertexUsage)
	{
	case VertexUsage::VerticesAndInstances: {
		VkBuffer vertexBuffers[] = { vertexBuffer, instanceBuffers[Renderer::imageIndex] };
		U64 offsets[] = { vertexBuffer.Offset(), instanceBuffers[Renderer::imageIndex].Offset() };
		commandBuffer.BindVertexBuffers(CountOf32(vertexBuffers), vertexBuffers, offsets);

		commandBuffer.BindIndexBuffer(indexBuffer, (U32)indexBuffer.Offset());

		commandBuffer.DrawIndexed((U32)(indexBuffer.Size() / sizeof(U32)), (U32)(instanceBuffers[Renderer::imageIndex].Size() / pipeline.InstanceSize()), 0, 0, 0);
	} break;
	case VertexUsage::Vertices: {
		VkBuffer vertexBuffers[] = { vertexBuffer };
		U64 offsets[] = { vertexBuffer.Offset() };
		commandBuffer.BindVertexBuffers(CountOf32(vertexBuffers), vertexBuffers, offsets);

		commandBuffer.BindIndexBuffer(indexBuffer, (U32)indexBuffer.Offset());

		commandBuffer.DrawIndexed((U32)(indexBuffer.Size() / sizeof(U32)), 1, 0, 0, 0);
	} break;
	case VertexUsage::Instances: {
		VkBuffer vertexBuffers[] = { instanceBuffers[Renderer::imageIndex] };
		U64 offsets[] = { instanceBuffers[Renderer::imageIndex].Offset() };
		commandBuffer.BindVertexBuffers(CountOf32(vertexBuffers), vertexBuffers, offsets);

		commandBuffer.Draw(0, 3, 0, (U32)(instanceBuffers[Renderer::imageIndex].Size() / pipeline.InstanceSize()));
	} break;
	case VertexUsage::None: {
		commandBuffer.Draw(0, 3, 0, 1);
	} break;
	}
}

void Material::UploadVertices(const void* data, U32 size, U32 offset)
{
	if (pipeline.VertexSize()) { vertexBuffer.UploadVertexData(data, size, offset); }
	else { Logger::Error("This Material Does Not Use Vertices!"); }
}

void Material::UploadInstances(const void* data, U32 size, U32 offset)
{
	if (pipeline.InstanceSize()) { instanceBuffers[Renderer::imageIndex].UploadVertexData(data, size, offset); }
	else { Logger::Error("This Material Does Not Use Instances!"); }
}

void Material::UploadInstancesAll(const void* data, U32 size, U32 offset)
{
	if (pipeline.InstanceSize())
	{
		for (U32 i = 0; i < MaxSwapchainImages; ++i)
		{
			instanceBuffers[i].UploadVertexData(data, size, offset);
		}
	}
	else { Logger::Error("This Material Does Not Use Instances!"); }
}

void Material::UploadIndices(const void* data, U32 size, U32 offset)
{
	if (pipeline.VertexSize()) { indexBuffer.UploadIndexData(data, size, offset); }
	else { Logger::Error("This Material Does Not Use Indices!"); }
}

void Material::ClearVertices()
{
	vertexBuffer.Clear();
}

void Material::ClearInstances()
{
	for (U32 i = 0; i < MaxSwapchainImages; ++i)
	{
		instanceBuffers[i].Clear();
	}
}

void Material::ClearIndices()
{
	indexBuffer.Clear();
}

const PipelineLayout& Material::GetPipelineLayout() const
{
	return pipelineLayout;
}