#include "Instance.hpp"

#include "VulkanInclude.hpp"
#include "Renderer.hpp"

#include "Core/Containers.hpp"
#include "Core/Logger.hpp"

#ifdef NH_DEBUG
PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;
#endif

VkBool32 __stdcall DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	switch (messageSeverity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: { Logger::Debug(pCallbackData->pMessage); } break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: { Logger::Info(pCallbackData->pMessage); } break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: { Logger::Warn(pCallbackData->pMessage); } break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: { Logger::Error(pCallbackData->pMessage); } break;
	}

	return VK_FALSE;
}

bool Instance::Create(const StringView& name, U32 version)
{
	VkApplicationInfo applicationInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = name.data(),
		.applicationVersion = version,
		.pEngineName = "Nihility",
		.engineVersion = VK_MAKE_VERSION(0, 0, 4),
		.apiVersion = VK_API_VERSION_1_3
	};

	VkValidateFR(vkEnumerateInstanceVersion(&applicationInfo.apiVersion));

	Vector<const C*> extensions;
	Vector<const C*> layers;

	extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef NH_PLATFORM_WINDOWS
	extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME); //TODO: Other platforms
#endif

	void* pNext = nullptr;

#ifdef NH_DEBUG
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	layers.push_back("VK_LAYER_KHRONOS_validation");

	VkDebugUtilsMessengerCreateInfoEXT messengerInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	messengerInfo.flags = 0;
	messengerInfo.messageSeverity =
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messengerInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	messengerInfo.pfnUserCallback = DebugCallback;
	messengerInfo.pUserData = nullptr;
	pNext = &messengerInfo;

	Vector<VkValidationFeatureEnableEXT> validationFeatures;
	validationFeatures.push_back(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
	validationFeatures.push_back(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);

	VkValidationFeaturesEXT features = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
	features.pNext = nullptr;
	features.enabledValidationFeatureCount = (U32)validationFeatures.size();
	features.pEnabledValidationFeatures = validationFeatures.data();
	features.disabledValidationFeatureCount = 0;
	features.pDisabledValidationFeatures = nullptr;
	messengerInfo.pNext = &features;
#endif

	VkInstanceCreateInfo instanceInfo{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = pNext,
		.flags = 0,
		.pApplicationInfo = &applicationInfo,
		.enabledLayerCount = (U32)layers.size(),
		.ppEnabledLayerNames = layers.data(),
		.enabledExtensionCount = (U32)extensions.size(),
		.ppEnabledExtensionNames = extensions.data()
	};

	VkValidateFR(vkCreateInstance(&instanceInfo, nullptr, &vkInstance));

#ifdef NH_DEBUG
	DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkDestroyDebugUtilsMessengerEXT");
	CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkCreateDebugUtilsMessengerEXT");
	SetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(vkInstance, "vkSetDebugUtilsObjectNameEXT");

	messengerInfo.pNext = nullptr;

	VkValidateFR(CreateDebugUtilsMessengerEXT(vkInstance, &messengerInfo, Renderer::allocationCallbacks, &debugMessenger));
#endif

	return true;
}

void Instance::Destroy()
{
#ifdef NH_DEBUG
	if (debugMessenger) { DestroyDebugUtilsMessengerEXT(vkInstance, debugMessenger, Renderer::allocationCallbacks); }
	debugMessenger = nullptr;
#endif

	if (vkInstance) { vkDestroyInstance(vkInstance, Renderer::allocationCallbacks); }

	vkInstance = nullptr;
}

Instance::operator VkInstance_T* () const
{
	return vkInstance;
}