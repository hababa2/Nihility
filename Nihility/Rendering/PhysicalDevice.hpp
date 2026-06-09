#pragma once

#include "RenderingDefines.hpp"

#include "Core/Containers.hpp"

struct VkPhysicalDevice_T;
struct VkSurfaceKHR_T;
struct VkQueueFamilyProperties;

struct PhysicalDeviceFeature
{
	bool multiDrawIndirect;
	bool samplerAnisotropy;

	F32 maxSamplerAnisotropy;
};

struct PhysicalDevice
{
public:
	PhysicalDevice() = default;
	PhysicalDevice(PhysicalDevice&& other) noexcept;
	PhysicalDevice(VkPhysicalDevice_T* vkPhysicalDevice, VkSurfaceKHR_T* vkSurface);
	PhysicalDevice& operator=(PhysicalDevice&& other) noexcept;

	~PhysicalDevice();

private:
	U32 FindPresentQueueIndex(VkSurfaceKHR_T* vkSurface, const Vector<VkQueueFamilyProperties>& queueFamilies) const;
	U32 FindQueueIndex(U32 desiredFlags, U32 undesiredFlags, const Vector<VkQueueFamilyProperties>& queueFamilies) const;

	operator VkPhysicalDevice_T* () const;

	VkPhysicalDevice_T* vkPhysicalDevice = nullptr;

	U32 presentQueueIndex;
	U32 graphicsQueueIndex;
	U32 computeQueueIndex;
	U32 transferQueueIndex;

	PhysicalDeviceFeature features;
	U32 maxSampleCount;
	bool suitable;
	bool discrete;

	friend class Renderer;
	friend struct Device;
	friend struct Swapchain;
};