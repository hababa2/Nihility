#include "Device.hpp"

#include "VulkanInclude.hpp"

#include "Renderer.hpp"
#include "Platform/Platform.hpp"

bool Device::Create()
{
	if (!CreateSurface()) { Logger::Fatal("Failed To Create Vulkan Surface!"); return false; }
	if (!SelectPhysicalDevice()) { return false; }

	F32 priorities[]{ 1.0f };

	Vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	queueCreateInfos.Push({
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.queueFamilyIndex = physicalDevice.presentQueueIndex,
		.queueCount = CountOf32(priorities),
		.pQueuePriorities = priorities
	});

	if (physicalDevice.graphicsQueueIndex != U32_MAX && physicalDevice.graphicsQueueIndex != physicalDevice.presentQueueIndex)
	{
		queueCreateInfos.Push({
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueFamilyIndex = physicalDevice.graphicsQueueIndex,
			.queueCount = CountOf32(priorities),
			.pQueuePriorities = priorities
		});
	}

	if (physicalDevice.computeQueueIndex != U32_MAX && physicalDevice.computeQueueIndex != physicalDevice.graphicsQueueIndex
		&& physicalDevice.computeQueueIndex != physicalDevice.presentQueueIndex)
	{
		queueCreateInfos.Push({
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueFamilyIndex = physicalDevice.computeQueueIndex,
			.queueCount = CountOf32(priorities),
			.pQueuePriorities = priorities
		});
	}

	if (physicalDevice.transferQueueIndex != U32_MAX && physicalDevice.transferQueueIndex != physicalDevice.computeQueueIndex &&
		physicalDevice.transferQueueIndex != physicalDevice.graphicsQueueIndex && physicalDevice.transferQueueIndex != physicalDevice.presentQueueIndex)
	{
		queueCreateInfos.Push({
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueFamilyIndex = physicalDevice.transferQueueIndex,
			.queueCount = CountOf32(priorities),
			.pQueuePriorities = priorities
		});
	}

	VkPhysicalDeviceFeatures features{
		.robustBufferAccess = VK_FALSE,
		.fullDrawIndexUint32 = VK_FALSE,
		.imageCubeArray = VK_FALSE,
		.independentBlend = VK_FALSE,
		.geometryShader = VK_FALSE,
		.tessellationShader = VK_FALSE,
		.sampleRateShading = VK_TRUE,
		.dualSrcBlend = VK_FALSE,
		.logicOp = VK_FALSE,
		.multiDrawIndirect = VK_TRUE,
		.drawIndirectFirstInstance = VK_TRUE,
		.depthClamp = VK_FALSE,
		.depthBiasClamp = VK_FALSE,
		.fillModeNonSolid = VK_TRUE,
		.depthBounds = VK_FALSE,
		.wideLines = VK_TRUE,
		.largePoints = VK_FALSE,
		.alphaToOne = VK_FALSE,
		.multiViewport = VK_FALSE,
		.samplerAnisotropy = VK_TRUE,
		.textureCompressionETC2 = VK_FALSE,
		.textureCompressionASTC_LDR = VK_FALSE,
		.textureCompressionBC = VK_FALSE,
		.occlusionQueryPrecise = VK_FALSE,
		.pipelineStatisticsQuery = VK_TRUE,
		.vertexPipelineStoresAndAtomics = VK_FALSE,
		.fragmentStoresAndAtomics = VK_FALSE,
		.shaderTessellationAndGeometryPointSize = VK_FALSE,
		.shaderImageGatherExtended = VK_FALSE,
		.shaderStorageImageExtendedFormats = VK_FALSE,
		.shaderStorageImageMultisample = VK_FALSE,
		.shaderStorageImageReadWithoutFormat = VK_FALSE,
		.shaderStorageImageWriteWithoutFormat = VK_FALSE,
		.shaderUniformBufferArrayDynamicIndexing = VK_FALSE,
		.shaderSampledImageArrayDynamicIndexing = VK_FALSE,
		.shaderStorageBufferArrayDynamicIndexing = VK_FALSE,
		.shaderStorageImageArrayDynamicIndexing = VK_FALSE,
		.shaderClipDistance = VK_FALSE,
		.shaderCullDistance = VK_FALSE,
		.shaderFloat64 = VK_TRUE,
		.shaderInt64 = VK_TRUE,
		.shaderInt16 = VK_TRUE,
		.shaderResourceResidency = VK_FALSE,
		.shaderResourceMinLod = VK_FALSE,
		.sparseBinding = VK_FALSE,
		.sparseResidencyBuffer = VK_FALSE,
		.sparseResidencyImage2D = VK_FALSE,
		.sparseResidencyImage3D = VK_FALSE,
		.sparseResidency2Samples = VK_FALSE,
		.sparseResidency4Samples = VK_FALSE,
		.sparseResidency8Samples = VK_FALSE,
		.sparseResidency16Samples = VK_FALSE,
		.sparseResidencyAliased = VK_FALSE,
		.variableMultisampleRate = VK_FALSE,
		.inheritedQueries = VK_FALSE
	};

	VkPhysicalDeviceFeatures2 features2 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = nullptr,
		.features = features
	};

	VkPhysicalDeviceVulkan12Features features12{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &features2,
		.samplerMirrorClampToEdge = VK_FALSE,
		.drawIndirectCount = VK_TRUE,
		.storageBuffer8BitAccess = VK_TRUE,
		.uniformAndStorageBuffer8BitAccess = VK_TRUE,
		.storagePushConstant8 = VK_FALSE,
		.shaderBufferInt64Atomics = VK_FALSE,
		.shaderSharedInt64Atomics = VK_FALSE,
		.shaderFloat16 = VK_TRUE,
		.shaderInt8 = VK_TRUE,
		.descriptorIndexing = VK_FALSE,
		.shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
		.shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE,
		.shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE,
		.shaderUniformBufferArrayNonUniformIndexing = VK_FALSE,
		.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
		.shaderStorageBufferArrayNonUniformIndexing = VK_FALSE,
		.shaderStorageImageArrayNonUniformIndexing = VK_FALSE,
		.shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE,
		.shaderUniformTexelBufferArrayNonUniformIndexing = VK_FALSE,
		.shaderStorageTexelBufferArrayNonUniformIndexing = VK_FALSE,
		.descriptorBindingUniformBufferUpdateAfterBind = VK_FALSE,
		.descriptorBindingSampledImageUpdateAfterBind = VK_FALSE,
		.descriptorBindingStorageImageUpdateAfterBind = VK_FALSE,
		.descriptorBindingStorageBufferUpdateAfterBind = VK_FALSE,
		.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
		.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
		.descriptorBindingUpdateUnusedWhilePending = VK_FALSE,
		.descriptorBindingPartiallyBound = VK_TRUE,
		.descriptorBindingVariableDescriptorCount = VK_FALSE,
		.runtimeDescriptorArray = VK_TRUE,
		.samplerFilterMinmax = VK_TRUE,
		.scalarBlockLayout = VK_TRUE,
		.imagelessFramebuffer = VK_FALSE,
		.uniformBufferStandardLayout = VK_FALSE,
		.shaderSubgroupExtendedTypes = VK_FALSE,
		.separateDepthStencilLayouts = VK_FALSE,
		.hostQueryReset = VK_FALSE,
		.timelineSemaphore = VK_TRUE,
		.bufferDeviceAddress = VK_FALSE,
		.bufferDeviceAddressCaptureReplay = VK_FALSE,
		.bufferDeviceAddressMultiDevice = VK_FALSE,
		.vulkanMemoryModel = VK_FALSE,
		.vulkanMemoryModelDeviceScope = VK_FALSE,
		.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE,
		.shaderOutputViewportIndex = VK_FALSE,
		.shaderOutputLayer = VK_FALSE,
		.subgroupBroadcastDynamicId = VK_FALSE
	};

	VkPhysicalDeviceVulkan13Features features13{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &features12,
		.robustImageAccess = VK_FALSE,
		.inlineUniformBlock = VK_FALSE,
		.descriptorBindingInlineUniformBlockUpdateAfterBind = VK_FALSE,
		.pipelineCreationCacheControl = VK_FALSE,
		.privateData = VK_FALSE,
		.shaderDemoteToHelperInvocation = VK_FALSE,
		.shaderTerminateInvocation = VK_FALSE,
		.subgroupSizeControl = VK_FALSE,
		.computeFullSubgroups = VK_FALSE,
		.synchronization2 = VK_TRUE,
		.textureCompressionASTC_HDR = VK_FALSE,
		.shaderZeroInitializeWorkgroupMemory = VK_FALSE,
		.dynamicRendering = VK_TRUE,
		.shaderIntegerDotProduct = VK_FALSE,
		.maintenance4 = VK_TRUE
	};

	VkPhysicalDevice16BitStorageFeatures features16BitStorage{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
		.pNext = &features13,
		.storageBuffer16BitAccess = VK_TRUE,
		.uniformAndStorageBuffer16BitAccess = VK_TRUE,
		.storagePushConstant16 = VK_FALSE,
		.storageInputOutput16 = VK_FALSE
	};

	Vector<const C8*> extensionsToEnable{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkDeviceCreateInfo deviceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features16BitStorage,
		.flags = 0,
		.queueCreateInfoCount = (U32)queueCreateInfos.Size(),
		.pQueueCreateInfos = queueCreateInfos.Data(),
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = (U32)extensionsToEnable.Size(),
		.ppEnabledExtensionNames = extensionsToEnable.Data(),
		.pEnabledFeatures = nullptr
	};

	VkValidateFR(vkCreateDevice(physicalDevice, &deviceCreateInfo, Renderer::allocationCallbacks, &vkDevice));

	vkGetDeviceQueue(vkDevice, physicalDevice.presentQueueIndex, 0, &presentQueue);
	vkGetDeviceQueue(vkDevice, physicalDevice.graphicsQueueIndex, 0, &graphicsQueue);
	vkGetDeviceQueue(vkDevice, physicalDevice.computeQueueIndex, 0, &computeQueue);
	vkGetDeviceQueue(vkDevice, physicalDevice.transferQueueIndex, 0, &transferQueue);

	return true;
}

void Device::Destroy()
{
	if (vkDevice) { vkDestroyDevice(vkDevice, Renderer::allocationCallbacks); }
	if (vkSurface) { vkDestroySurfaceKHR(Renderer::instance, vkSurface, Renderer::allocationCallbacks); }

	vkDevice = nullptr;
	vkSurface = nullptr;
}

bool Device::CreateSurface()
{
#ifdef NH_PLATFORM_WINDOWS
	WindowInfo wd = Platform::GetWindowInfo();

	VkWin32SurfaceCreateInfoKHR surfaceInfo{
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.hinstance = wd.instance,
		.hwnd = wd.window
	};

	VkValidateFR(vkCreateWin32SurfaceKHR(Renderer::instance, &surfaceInfo, Renderer::allocationCallbacks, &vkSurface));
#elif NH_PLATFORM_LINUX
	//TODO:
#elif NH_PLATFORM_APPLE
	//TODO:
#endif

	return true;
}

bool Device::SelectPhysicalDevice()
{
	U32 physicalDeviceCount;
	VkValidateFR(vkEnumeratePhysicalDevices(Renderer::instance, &physicalDeviceCount, nullptr));
	Vector<VkPhysicalDevice> vkPhysicalDevices(physicalDeviceCount, {});
	VkValidateFR(vkEnumeratePhysicalDevices(Renderer::instance, &physicalDeviceCount, vkPhysicalDevices.Data()));

	if (vkPhysicalDevices.Size() == 0)
	{
		Logger::Fatal("Failed To Find A Valid Physical Device!");
		return false;
	}

	U32 best = U32_MAX;
	Vector<PhysicalDevice> physicalDevices;
	for (VkPhysicalDevice& vkPhysicalDevice : vkPhysicalDevices)
	{
		PhysicalDevice physicalDevice(vkPhysicalDevice, vkSurface);
		if (physicalDevice.suitable)
		{
			if (best == U32_MAX && physicalDevice.discrete) { best = (U32)physicalDevices.Size(); }
			physicalDevices.Push(Move(physicalDevice));
		}
	}

	if (physicalDevices.Size() == 0)
	{
		Logger::Fatal("Failed To Find A Valid Physical Device!");
		return false;
	}

	if (best != U32_MAX) { physicalDevice = Move(physicalDevices[best]); }
	else { physicalDevice = Move(physicalDevices[0]); }

	return true;
}

Device::operator VkDevice_T* () const
{
	return vkDevice;
}