#pragma once

#include "RenderingDefines.hpp"

#include "Core/Containers.hpp"

enum VkPresentModeKHR;
struct VkImage_T;
struct VkImageView_T;
struct VkSurfaceKHR_T;
struct VkSwapchainKHR_T;
struct VkPhysicalDevice_T;
struct VkSurfaceFormatKHR;
struct SwapchainSupportDetails;

struct SwapchainDestructionData
{
	Vector<VkImageView_T*> imageViews;
	VkSwapchainKHR_T* vkSwapchain;
};

struct Swapchain
{
	operator VkSwapchainKHR_T* () const;
	VkSwapchainKHR_T* const* operator&() const;

private:
	bool Create();
	void Recreate();
	void Destroy();

	SwapchainSupportDetails QuerySwapChainSupport(VkPhysicalDevice_T* physicalDevice, VkSurfaceKHR_T* surface);

	Vector<VkImage_T*> images;
	Vector<VkImageView_T*> imageViews;

	VkSwapchainKHR_T* vkSwapchain = nullptr;

	friend class Renderer;
};