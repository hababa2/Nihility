#pragma once

#include "Defines.hpp"

#include "Containers/Vector.hpp"

struct VkSwapchainKHR_T;
struct VkSemaphore_T;
struct VkImage_T;
struct VkImageView_T;
struct VkFramebuffer_T;
struct VkSurfaceFormatKHR;
struct VkExtent2D;
struct VkSurfaceCapabilitiesKHR;

struct SwapchainDestructionData
{
	VkSwapchainKHR_T* swapchain = nullptr;
	Vector<VkImageView_T*> imageViews;
	Vector<VkFramebuffer_T*> framebuffers;
};

struct Swapchain
{
	operator VkSwapchainKHR_T* () const;
	VkSwapchainKHR_T* const* operator&() const;

private:
	bool Create();
	void Destroy();

	Vector<VkImage_T*> images;
	Vector<VkImageView_T*> imageViews;
	Vector<VkFramebuffer_T*> framebuffers;

	VkSwapchainKHR_T* vkSwapchain = nullptr;

	friend class Renderer;
	friend class Resources;
	friend struct Renderpass;
	friend struct FrameBuffer;
	friend struct CommandBuffer;
};