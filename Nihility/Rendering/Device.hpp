#pragma once

#include "RenderingDefines.hpp"

#include "PhysicalDevice.hpp"

enum class QueueType
{
	Present,
	Graphics,
	Compute,
	Transfer
};

struct VkQueue_T;
struct VkDevice_T;
struct VkSurfaceKHR_T;

struct Device
{
public:
	operator VkDevice_T* () const;

private:
	bool Create();
	void Destroy();

	bool CreateSurface();
	bool SelectPhysicalDevice();

	VkDevice_T* vkDevice = nullptr;
	PhysicalDevice physicalDevice;
	VkSurfaceKHR_T* vkSurface = nullptr;

	VkQueue_T* presentQueue;
	VkQueue_T* graphicsQueue;
	VkQueue_T* computeQueue;
	VkQueue_T* transferQueue;

	friend class Renderer;
	friend struct Swapchain;
};