#include "PhysicalDevice.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"

PhysicalDevice::PhysicalDevice(VkPhysicalDevice_T* vkPhysicalDevice, VkSurfaceKHR_T* vkSurface) : vkPhysicalDevice(vkPhysicalDevice), suitable(false)
{
	U32 queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount, nullptr);
	Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount, {});
	vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount, queueFamilies.Data());

	VkPhysicalDeviceProperties properties{};
	VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
	features2.pNext = &indexingFeatures;

	vkGetPhysicalDeviceFeatures2(vkPhysicalDevice, &features2);
	vkGetPhysicalDeviceProperties(vkPhysicalDevice, &properties);

	maxSampleCount = BitFloor(properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts);

	features.multiDrawIndirect = features2.features.multiDrawIndirect;
	features.samplerAnisotropy = features2.features.samplerAnisotropy;
	features.maxSamplerAnisotropy = properties.limits.maxSamplerAnisotropy;

	if (properties.apiVersion < VK_API_VERSION_1_3) { return; }
	if (!features2.features.samplerAnisotropy || !features2.features.wideLines || 
		!indexingFeatures.descriptorBindingPartiallyBound || !indexingFeatures.runtimeDescriptorArray ||
		!indexingFeatures.shaderSampledImageArrayNonUniformIndexing) { return; }

	discrete = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

	presentQueueIndex = FindPresentQueueIndex(vkSurface, queueFamilies);
	graphicsQueueIndex = FindQueueIndex(VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, queueFamilies);
	computeQueueIndex = FindQueueIndex(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT, queueFamilies);
	transferQueueIndex = FindQueueIndex(VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, queueFamilies);

	if (presentQueueIndex == U32_MAX) { return; }

	suitable = true;
}

PhysicalDevice::PhysicalDevice(PhysicalDevice&& other) noexcept :
	vkPhysicalDevice(other.vkPhysicalDevice), presentQueueIndex(other.presentQueueIndex), graphicsQueueIndex(other.graphicsQueueIndex),
	computeQueueIndex(other.computeQueueIndex), transferQueueIndex(other.transferQueueIndex),
	maxSampleCount(other.maxSampleCount), suitable(other.suitable), discrete(other.discrete)
{
}

PhysicalDevice& PhysicalDevice::operator=(PhysicalDevice&& other) noexcept
{
	vkPhysicalDevice = other.vkPhysicalDevice;
	presentQueueIndex = other.presentQueueIndex;
	graphicsQueueIndex = other.graphicsQueueIndex;
	computeQueueIndex = other.computeQueueIndex;
	transferQueueIndex = other.transferQueueIndex;
	maxSampleCount = other.maxSampleCount;
	suitable = other.suitable;
	discrete = other.discrete;

	return *this;
}

PhysicalDevice::~PhysicalDevice()
{

}

U32 PhysicalDevice::FindPresentQueueIndex(VkSurfaceKHR_T* vkSurface, const Vector<VkQueueFamilyProperties>& queueFamilies) const
{
	VkBool32 presentSupport = VK_FALSE;

	for (U32 i = 0; i < (U32)queueFamilies.Size(); ++i)
	{
		if (vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice, i, vkSurface, &presentSupport) != VK_SUCCESS) { return U32_MAX; }
		if (presentSupport) { return i; }
	}

	return U32_MAX;
}

U32 PhysicalDevice::FindQueueIndex(U32 desiredFlags, U32 undesiredFlags, const Vector<VkQueueFamilyProperties>& queueFamilies) const
{
	U32 index = U32_MAX;
	U32 i = 0;
	for (const VkQueueFamilyProperties& queueFamily : queueFamilies)
	{
		if ((queueFamily.queueFlags & desiredFlags) == desiredFlags)
		{
			if ((queueFamily.queueFlags & undesiredFlags) == 0) { return i; }
			else { index = i; }
		}

		++i;
	}

	return index;
}

PhysicalDevice::operator VkPhysicalDevice_T* () const
{
	return vkPhysicalDevice;
}