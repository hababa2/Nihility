#pragma once

#include "Defines.hpp"

struct VkRenderPass_T;

struct Renderpass
{
private:
	bool Create();
	void Destroy();

	operator VkRenderPass_T* () const;

	VkRenderPass_T* vkRenderpass;

	friend class Renderer;
	friend struct Pipeline;
	friend struct FrameBuffer;
	friend struct CommandBuffer;
	friend struct Swapchain;
};