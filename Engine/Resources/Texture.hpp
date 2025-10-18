#pragma once

#include "Defines.hpp"

#include "Containers/String.hpp"

struct VmaAllocation_T;
struct VkImage_T;
struct VkImageView_T;
struct VkSampler_T;

enum class NH_API FilterMode
{
	Point = 0,
	Linear = 1,
	Cubic = 1000015000
};

enum class NH_API MipMapSampleMode
{
	Single = 0,
	Multiple = 1,
};

enum class NH_API EdgeSampleMode
{
	Repeat = 0,
	MirroredRepeat = 1,
	ClampToEdge = 2,
	ClampToBorder = 3,
	MirrorClampToEdge = 4,
};

enum class NH_API BorderColor
{
	Clear = 1,
	Black = 3,
	White = 5,
};

enum class TextureFormat
{
	Undefined = 0,
	R8Unorm = 9,
	R8Snorm = 10,
	R8Uint = 13,
	R8Sint = 14,
	R8Srgb = 15,
	R8G8Unorm = 16,
	R8G8Snorm = 17,
	R8G8Uint = 20,
	R8G8Sint = 21,
	R8G8Srgb = 22,
	R8G8B8Unorm = 23,
	R8G8B8Snorm = 24,
	R8G8B8Uint = 27,
	R8G8B8Sint = 28,
	R8G8B8Srgb = 29,
	B8G8R8Unorm = 30,
	B8G8R8Snorm = 31,
	B8G8R8Uint = 34,
	B8G8R8Sint = 35,
	B8G8R8Srgb = 36,
	R8G8B8A8Unorm = 37,
	R8G8B8A8Snorm = 38,
	R8G8B8A8Uint = 41,
	R8G8B8A8Sint = 42,
	R8G8B8A8Srgb = 43,
	B8G8R8A8Unorm = 44,
	B8G8R8A8Snorm = 45,
	B8G8R8A8Uint = 48,
	B8G8R8A8Sint = 49,
	B8G8R8A8Srgb = 50,
	R16Unorm = 70,
	R16Snorm = 71,
	R16Uint = 74,
	R16Sint = 75,
	R16Sfloat = 76,
	R16G16Unorm = 77,
	R16G16Snorm = 78,
	R16G16Uint = 81,
	R16G16Sint = 82,
	R16G16Sfloat = 83,
	R16G16B16Unorm = 84,
	R16G16B16Snorm = 85,
	R16G16B16Uint = 88,
	R16G16B16Sint = 89,
	R16G16B16Sfloat = 90,
	R16G16B16A16Unorm = 91,
	R16G16B16A16Snorm = 92,
	R16G16B16A16Uint = 95,
	R16G16B16A16Sint = 96,
	R16G16B16A16Sfloat = 97,
	R32Uint = 98,
	R32Sint = 99,
	R32Sfloat = 100,
	R32G32Uint = 101,
	R32G32Sint = 102,
	R32G32Sfloat = 103,
	R32G32B32Uint = 104,
	R32G32B32Sint = 105,
	R32G32B32Sfloat = 106,
	R32G32B32A32Uint = 107,
	R32G32B32A32Sint = 108,
	R32G32B32A32Sfloat = 109,
	R64Uint = 110,
	R64Sint = 111,
	R64Sfloat = 112,
	R64G64Uint = 113,
	R64G64Sint = 114,
	R64G64Sfloat = 115,
	R64G64B64Uint = 116,
	R64G64B64Sint = 117,
	R64G64B64Sfloat = 118,
	R64G64B64A64Uint = 119,
	R64G64B64A64Sint = 120,
	R64G64B64A64Sfloat = 121,
	D16Unorm = 124,
	D32Sfloat = 126,
	S8Uint = 127,
	D16UnormS8Uint = 128,
	D24UnormS8Uint = 129,
	D32SfloatS8Uint = 130,
};

struct NH_API Sampler
{
	FilterMode filterMode = FilterMode::Linear;
	MipMapSampleMode mipMapSampleMode = MipMapSampleMode::Multiple;
	EdgeSampleMode edgeSampleMode = EdgeSampleMode::ClampToEdge;
	BorderColor borderColor = BorderColor::Clear;
};

struct TextureDestructionData
{
	VkImage_T* image = nullptr;
	VkImageView_T* imageView = nullptr;
	VmaAllocation_T* allocation = nullptr;
};

struct NH_API Texture
{
	const String& Name() const { return name; }
	const U32& Width() const { return width; }
	const U32& Height() const { return height; }
	const U64& Size() const { return size; }
	const U8& MipmapLevels() const { return mipmapLevels; }

private:
	String name;
	U32 width;
	U32 height;
	U32	depth;
	U64	size;
	U8 mipmapLevels;

	TextureFormat format = TextureFormat::Undefined;
	VkImage_T* image = nullptr;
	VkImageView_T* imageView = nullptr;
	VkSampler_T* sampler = nullptr;
	VmaAllocation_T* allocation = nullptr;

	friend class Renderer;
	friend class Resources;
	friend struct FrameBuffer;
	friend struct CommandBuffer;
	friend struct Swapchain;
};