#include "LineRenderer.hpp"

#include "Rendering/VulkanInclude.hpp"

#include "Renderer.hpp"

#include "Resources/Resources.hpp"

Material LineRenderer::material;
Shader LineRenderer::vertexShader;
Shader LineRenderer::fragmentShader;
Vector<LineVertex> LineRenderer::vertices;
Vector<U32> LineRenderer::indices;
U32 LineRenderer::nextIndex = 0;

bool LineRenderer::Initialize()
{
#ifdef NH_DEBUG
	VkPushConstantRange pushConstant{};
	pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstant.offset = 0;
	pushConstant.size = sizeof(GlobalPushConstant);

	PipelineLayout pipelineLayout;
	pipelineLayout.Create({}, { pushConstant });

	vertexShader.Create("shaders/line.vert.spv", ShaderStage::Vertex);
	fragmentShader.Create("shaders/line.frag.spv", ShaderStage::Fragment);

	Vector<VkVertexInputBindingDescription> inputs = {
		{ 0, sizeof(LineVertex), VK_VERTEX_INPUT_RATE_VERTEX }
	};

	Vector<VkVertexInputAttributeDescription> attributes = {
		{ 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(LineVertex, position) },
		{ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LineVertex, color) }
	};

	Pipeline pipeline;
	pipeline.Create(pipelineLayout, { PolygonMode::Line, TopologyMode::LineList }, { vertexShader, fragmentShader }, inputs, attributes);
	material.Create(pipelineLayout, pipeline, {}, { PushConstant{ Renderer::GetGlobalPushConstant(), sizeof(GlobalPushConstant), 0, VK_SHADER_STAGE_VERTEX_BIT } });
#endif

	return true;
}

void LineRenderer::Shutdown()
{
#ifdef NH_DEBUG
	vertexShader.Destroy();
	fragmentShader.Destroy();
	material.Destroy();
#endif
}

void LineRenderer::Update()
{
#ifdef NH_DEBUG
	material.ClearVertices();
	material.ClearIndices();
	material.UploadVertices(vertices.Data(), (U32)(vertices.Size() * sizeof(LineVertex)), 0);
	material.UploadIndices(indices.Data(), (U32)(indices.Size() * sizeof(U32)), 0);

	nextIndex = 0;
#endif
}

void LineRenderer::Render(CommandBuffer commandBuffer)
{
#ifdef NH_DEBUG
	material.Bind(commandBuffer);

	vertices.Clear();
	indices.Clear();
#endif
}

void LineRenderer::DrawLine(const Vector<Vector2>& line, bool loop, const Vector4& color)
{
#ifdef NH_DEBUG
	if (line.Size() > 1)
	{
		for (const Vector2& point : line)
		{
			vertices.Push({ point, color });
		}

		U32 startIndex = nextIndex;
		U32 index = startIndex;

		for (U32 i = 0; i < line.Size() - 1; ++i)
		{
			indices.Push(index);
			indices.Push(++index);
		}

		nextIndex = index + 1;

		if (loop)
		{
			indices.Push(index);
			indices.Push(startIndex);
		}
	}
#endif
}