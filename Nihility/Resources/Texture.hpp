#pragma once

#include "Defines.hpp"
#include "Core/Containers.hpp"

static constexpr U32 TextureVersion = MakeVersionNumber(2, 0, 0);
static constexpr U32 TextureIdentifier = 'NHT';

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

struct VmaAllocation_T;
struct VkImage_T;
struct VkImageView_T;
struct VkSampler_T;

struct TextureFileHeader
{
	U32 identifier = TextureIdentifier;
	U32 version = TextureVersion;
	U32 width = 0;
	U32 height = 0;
	U32 format = 0;
	U32 mipCount = 0;
	U32 totalSize = 0;

	U64 mipOffsets[14] = {};
	U64 mipSizes[14] = {};
};

struct MipLevel
{
	U32 width;
	U32 height;
	Vector<U8> compressedData;
};

struct NH_API Sampler
{
	FilterMode filterMode = FilterMode::Point;
	MipMapSampleMode mipMapSampleMode = MipMapSampleMode::Multiple;
	EdgeSampleMode edgeSampleMode = EdgeSampleMode::ClampToEdge;
	BorderColor borderColor = BorderColor::Clear;

	VkSampler_T* vkSampler = nullptr;

	operator VkSampler_T* () const;
	VkSampler_T** operator&() const;
};

struct NH_API Texture
{
	Texture() = default;
	Texture(const Texture& other);
	Texture(Texture&& other);

	const WString& Name() const { return name; }
	const U32& Width() const { return width; }
	const U32& Height() const { return height; }
	const U64& Size() const { return size; }
	const U32& Id() const { return id; }
	const U8& MipmapLevels() const { return mipmapLevels; }

private:
	WString name = L"";
	U32 width = 0;
	U32 height = 0;
	U32	depth = 0;
	U64	size = 0;
	U32	id = 0;
	U8 mipmapLevels = 0;

	U32 format = 0;
	Sampler sampler = {};
	VkImage_T* image = nullptr;
	VkImageView_T* imageView = nullptr;
	VmaAllocation_T* allocation = nullptr;

	friend class Resources;
	friend class Renderer;
};

struct MipLoadContext
{
	std::shared_ptr<Texture> texture;
	U32 targetMipLevel;
	U32 mipWidth;
	U32 mipHeight;
};