#pragma once

#include "Defines.hpp"

#include "PipelineLayout.hpp"
#include "Shader.hpp"

#include "Containers/Vector.hpp"

enum class NH_API PolygonMode
{
	Fill = 0,
	Line = 1,
	Point = 2
};

enum class NH_API TopologyMode
{
	PointList = 0,
	LineList = 1,
	LineStrip = 2,
	TriangleList = 3,
	TriangleStrip = 4,
	TriangleFan = 5
};

enum class NH_API BindPoint
{
	Graphics = 0,
	Compute = 1,
	RayTracing = 1000165000
};

struct NH_API PipelineSettings
{
	PolygonMode polygonMode = PolygonMode::Fill;
	TopologyMode topologyMode = TopologyMode::TriangleList;
	BindPoint bindPoint = BindPoint::Graphics;
	bool useDepth = true;
};

struct VkPipeline_T;
struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;

struct PipelineDestructionData
{
	VkPipeline_T* vkPipeline;
};

struct NH_API Pipeline
{
	bool Create(const PipelineLayout& layout, const PipelineSettings& settings, const Vector<Shader>& shaders,
		const Vector<VkVertexInputBindingDescription>& bindings, const Vector<VkVertexInputAttributeDescription>& attributes);
	void Destroy();

	U8 VertexSize() const;
	U8 InstanceSize() const;

	operator VkPipeline_T* () const;

private:
	VkPipeline_T* vkPipeline;
	BindPoint bindPoint;
	U8 vertexSize = 0;
	U8 instanceSize = 0;

	friend class Renderer;
	friend struct CommandBuffer;
};