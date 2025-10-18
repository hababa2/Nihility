#include "Resources.hpp"

#include "Core/Logger.hpp"
#include "Rendering/Renderer.hpp"
#include "Containers/Stack.hpp"
#include "Containers/Pair.hpp"
#include "Platform/Platform.hpp"

#include "tracy/Tracy.hpp"
#include "vma/vk_mem_alloc.h"

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#define STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#include "stb/stb_vorbis.c"

#define DR_WAV_IMPLEMENTATION
#include "dr_libs/dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_libs/dr_mp3.h"

#define DR_FLAC_IMPLEMENTATION
#include "dr_libs/dr_flac.h"

#include "msdfgen/msdfgen.h"

//nht texture
//nhf font
//nha audio
//nhm material
//nhs shader

static constexpr U32 TextureVersion = MakeVersionNumber(1, 0, 0);
static constexpr U32 FontVersion = MakeVersionNumber(1, 0, 0);
static constexpr U32 AudioVersion = MakeVersionNumber(1, 0, 0);
static constexpr U32 MaterialVersion = MakeVersionNumber(1, 0, 0);
static constexpr U32 ShaderVersion = MakeVersionNumber(1, 0, 0);

DescriptorSet Resources::dummySet;
DescriptorSet Resources::bindlessTexturesSet;

ResourceRef<Texture> Resources::whiteTexture;
ResourceRef<Texture> Resources::placeholderTexture;

Hashmap<String, Resource<Texture>> Resources::textures(1024);
Hashmap<String, Resource<Font>> Resources::fonts(16);
Hashmap<String, Resource<AudioClip>> Resources::audioClips(1024);

Queue<ResourceRef<Texture>> Resources::bindlessTexturesToUpdate(128);

bool Resources::Initialize()
{
	Logger::Trace("Initializing Resources...");

	Platform::OnDragDrop += UploadResource;

	Sampler pointSampler{
		.filterMode = FilterMode::Point,
		.mipMapSampleMode = MipMapSampleMode::Multiple,
		.edgeSampleMode = EdgeSampleMode::ClampToEdge,
		.borderColor = BorderColor::Clear,
	};

	whiteTexture = LoadTexture("textures/white.nht", pointSampler);
	placeholderTexture = LoadTexture("textures/missing_texture.nht", pointSampler);

	DescriptorBinding textureBinding{
		.type = BindingType::CombinedImageSampler,
		.stages = (U32)ShaderStage::All,
		.count = 1024
	};

	DescriptorBinding texture3DBinding{
		.type = BindingType::StorageImage,
		.stages = (U32)ShaderStage::All,
		.count = 1024
	};

	bindlessTexturesSet.Create({ textureBinding, texture3DBinding }, 10, true);

	dummySet.Create({});

	return true;
}

void Resources::Shutdown()
{
	Logger::Trace("Cleaning Up Resources...");

	DestroyResources(textures, Renderer::DestroyTexture);

	vkDeviceWaitIdle(Renderer::device);

	bindlessTexturesSet.Destroy();
	dummySet.Destroy();
}

void Resources::Update()
{
	ZoneScopedN("ResourcesUpdate");

	if (bindlessTexturesToUpdate.Size())
	{
		Vector<VkWriteDescriptorSet> writes(bindlessTexturesToUpdate.Size());
		Vector<VkDescriptorImageInfo> textureData(bindlessTexturesToUpdate.Size());

		ResourceRef<Texture> texture;
		while (bindlessTexturesToUpdate.Pop(texture))
		{
			VkDescriptorImageInfo descriptorImageInfo{
				.sampler = texture->sampler,
				.imageView = texture->imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};

			VkWriteDescriptorSet descriptorWrite{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = bindlessTexturesSet,
				.dstBinding = 10,
				.dstArrayElement = (U32)texture.Handle(),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &textureData.Push(descriptorImageInfo),
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr
			};

			writes.Push(descriptorWrite);
		}

		vkUpdateDescriptorSets(Renderer::device, (U32)writes.Size(), writes.Data(), 0, nullptr);
	}
}

ResourceRef<Texture> Resources::LoadTexture(const String& path, const Sampler& sampler, bool generateMipmaps)
{
	if (path.Blank()) { Logger::Error("Blank Path Passed To LoadTexture!"); return nullptr; }

	U64 handle;
	Resource<Texture>& texture = *textures.Request(path, handle);

	if (!texture->name.Blank()) { return { texture, handle }; }
	
	File file{ path, FILE_OPEN_RESOURCE_READ };
	if (file.Opened())
	{
		if (!file.ReadString().Compare("NHT"))
		{
			Logger::Error("Asset '", path, "' Is Not A Nihility Texture!");
			textures.Remove(handle);
			file.Close();
			return nullptr;
		}

		file.Seek(4); //Skip version number for now, there is only one

		texture->name = file.ReadString();
		file.Read(texture->width);
		file.Read(texture->height);
		file.Read(texture->depth);
		file.Read(texture->format);
		texture->size = texture->width * texture->height * 4; //TODO: use format to get size
		texture->mipmapLevels = generateMipmaps ? (U32)Math::Floor(Math::Log2((F32)Math::Max(texture->width, texture->height))) + 1 : 1;

		U8* data;
		Memory::Allocate(&data, texture->size);
		file.Read(data, texture->size);

		if (!Renderer::UploadTexture(texture, data, sampler))
		{
			Memory::Free(&data);
			Logger::Error("Failed To Upload Texture Data!");
			return nullptr;
		}

		Memory::Free(&data);

		ResourceRef<Texture> textureRef = { texture, handle };

		bindlessTexturesToUpdate.Push(textureRef);

		return textureRef;
	}

	Logger::Error("Failed To Open File: ", path, '!');
	return nullptr;
}

ResourceRef<Font> Resources::LoadFont(const String& path)
{
	if (path.Blank()) { Logger::Error("Blank Path Passed To LoadFont!"); return nullptr; }

	U64 handle;
	Resource<Font>& font = *fonts.Request(path, handle);

	if (!font->name.Blank()) { return { font, handle }; }

	File file(path, FILE_OPEN_RESOURCE_READ);
	if (file.Opened())
	{
		if (!file.ReadString().Compare("NHF"))
		{
			Logger::Error("Asset '", path, "' Is Not A Nihility Font!");
			fonts.Remove(handle);
			file.Close();
			return nullptr;
		}

		file.Seek(4); //Skip version number for now, there is only one

		U32 width, height;
		font->name = file.ReadString();
		file.Read(font->ascent);
		file.Read(font->descent);
		file.Read(font->lineGap);
		file.Read(font->scale);
		file.Read(font->glyphSize);
		file.Read(width);
		file.Read(height);
		file.Read(font->glyphs, sizeof(Glyph) * 96);

		F32* atlas;
		Memory::Allocate(&atlas, width * height * 4);

		file.Read(atlas, width * height * 4 * sizeof(F32));

		U64 textureHandle;
		Resource<Texture>& texture = *textures.Request(path, textureHandle);

		texture->name = path.FileName().Append("_texture");
		texture->width = width;
		texture->height = height;
		texture->depth = 1;
		texture->size = width * height * 4 * sizeof(F32);
		texture->mipmapLevels = 1;
		texture->format = TextureFormat::R32G32B32A32Sfloat;

		Sampler sampler{};

		if (!Renderer::UploadTexture(texture, atlas, sampler))
		{
			Logger::Error("Failed To Upload Texture Data!");
			fonts.Remove(handle);
			textures.Remove(textureHandle);
			file.Close();
			return nullptr;
		}

		font->texture = { texture, textureHandle };

		bindlessTexturesToUpdate.Push(font->texture);

		file.Close();
		return { font, handle };
	}

	Logger::Error("Failed To Find Or Open File: ", path, "!");

	fonts.Remove(handle);
	return nullptr;
}

ResourceRef<AudioClip> Resources::LoadAudio(const String& path)
{
	if (path.Blank()) { Logger::Error("Blank Path Passed To LoadAudio!"); return nullptr; }

	U64 handle;
	Resource<AudioClip>& audioClip = *audioClips.Request(path, handle);

	if (!audioClip->name.Blank()) { return { audioClip, handle }; }

	File file{ path, FILE_OPEN_RESOURCE_READ };
	if (file.Opened())
	{
		if (!file.ReadString().Compare("NHA"))
		{
			Logger::Error("Asset '", path, "' Is Not A Nihility Audio Clip!");
			audioClips.Remove(handle);
			file.Close();
			return nullptr;
		}

		file.Seek(4); //Skip version number for now, there is only one

		audioClip->name = file.ReadString();
		file.Read(audioClip->format);
		file.Read(audioClip->size);

		Memory::Allocate(&audioClip->buffer, audioClip->size);

		file.Read(audioClip->buffer, audioClip->size);

		file.Close();
		return { audioClip, handle };
	}

	Logger::Error("Failed To Find Or Open File: ", path, "!");

	audioClips.Remove(handle);
	return nullptr;
}

String Resources::UploadResource(const String& path)
{
	String extension = path.FileExtension();

	if (extension.Blank())
	{
		//TODO: No file extension, assume binary?
		Logger::Error("Can't Upload File That Has No Extension!");
		return {};
	}

	switch (Hash::StringCI(extension.Data(), extension.Size()))
	{
	//Images
	case "jpg"_Hash:
	case "jpeg"_Hash:
	case "png"_Hash:
	case "bmp"_Hash:
	case "tga"_Hash:
	case "jfif"_Hash:
	case "tiff"_Hash:
	case "ktx"_Hash:
	case "ktx2"_Hash:
	case "pnm"_Hash:
	case "ppm"_Hash:
	case "pgm"_Hash:
	case "sbsar"_Hash: {
		return Move(UploadTexture(path));
	} break;

	//Fonts
	case "ttf"_Hash:
	case "otf"_Hash: {
		return Move(UploadFont(path));
	} break;

	//Audio
	case "ogg"_Hash:
	case "wav"_Hash:
	case "flac"_Hash:
	case "mp3"_Hash: {
		return Move(UploadAudio(path));
	} break;

	default: {
		Logger::Error("Unknown File Extension!: ", extension);
		return {};
	}
	}
}

String Resources::UploadTexture(const String& path)
{
	if (path.Blank()) { Logger::Error("Blank Path Passed To UploadTexture!"); return {}; }

	File file(path, FILE_OPEN_RESOURCE_READ);
	if (file.Opened())
	{
		String data = file.ReadAll();
		file.Close();

		I32 texWidth, texHeight;
		U8* textureData = stbi_load_from_memory((U8*)data.Data(), (I32)data.Size(), &texWidth, &texHeight, nullptr, STBI_rgb_alpha);

		if (!textureData)
		{
			Logger::Error("Failed To Load Texture Data!");
			stbi_image_free(textureData);
			return {};
		}

		Texture texture{};
		texture.name = path.FileName();
		texture.width = texWidth;
		texture.height = texHeight;
		texture.depth = 1;
		texture.format = TextureFormat::R8G8B8A8Srgb;

		String newPath = path.FileName().Prepend("textures/").Append(".nht");
		file.Open(newPath, FILE_OPEN_RESOURCE_WRITE);

		file.Write("NHT");
		file.Write(TextureVersion);
		file.Write(texture.name);
		file.Write(texture.width);
		file.Write(texture.height);
		file.Write(texture.depth);
		file.Write(texture.format);
		file.Write(textureData, texWidth * texHeight * 4);

		file.Close();

		stbi_image_free(textureData);

		return newPath;
	}

	Logger::Error("Failed To Open File: ", path, '!');
	return {};
}

String Resources::UploadFont(const String& path)
{
	using namespace msdfgen;

	if (path.Blank()) { Logger::Error("Blank Path Passed To UploadFont!"); return {}; }

	Range pxRange(4);
	MSDFGeneratorConfig generatorConfig;
	generatorConfig.overlapSupport = true;
	MSDFGeneratorConfig postErrorCorrectionConfig(generatorConfig);
	generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::EDGE_PRIORITY;
	postErrorCorrectionConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::CHECK_DISTANCE_AT_EDGE;

	File file(path, FILE_OPEN_RESOURCE_READ);
	if (file.Opened())
	{
		U8 glyphSize = 32;
		U8 padding = 1;

		String data = file.ReadAll();
		file.Close();

		Font* font;
		Memory::Allocate(&font);
		font->name = path.FileName();

		const U8* fontData = (const U8*)data.Data();

		stbtt_fontinfo info{};

		stbtt_InitFont(&info, fontData, stbtt_GetFontOffsetForIndex(fontData, 0));

		U32 rowWidth = (glyphSize + padding) * 8 + padding;
		U32 columnHeight = (glyphSize + padding) * 12 + padding;

		font->LoadData(&info, glyphSize);

		F32* atlas;
		Memory::Allocate(&atlas, rowWidth * columnHeight * 4);

		F32* bitmap;
		Memory::Allocate(&bitmap, glyphSize * glyphSize * 4);

		Hashmap<I32, C8> glyphToCodepoint{ 128 };

		U32 glyphRowSize = rowWidth * (glyphSize + padding) * 4;
		U32 x = padding * 4;
		U32 y = rowWidth * padding * 4;

		for (C8 i = 0; i < 96; ++i)
		{
			C8 codepoint = i + 32;
			Shape shape = font->LoadGlyph(&info, codepoint, bitmap, glyphToCodepoint);
			shape.normalize();
			shape.inverseYAxis = true;

			SDFTransformation transformation(Projection(1, msdfgen::Vector2(font->glyphs[i].x, font->glyphs[i].y) * glyphSize), Range(1.0));

			edgeColoringSimple(shape, 1.0);

			Bitmap<F32, 4> mtsdf(glyphSize, glyphSize);

			generateMTSDF(mtsdf, shape, transformation);
			distanceSignCorrection(mtsdf, shape, transformation, FILL_NONZERO);
			msdfErrorCorrection(mtsdf, shape, transformation, postErrorCorrectionConfig);

			U32 start = x + y;

			for (I32 j = 0; j < glyphSize; ++j)
			{
				memcpy(atlas + start + j * rowWidth * 4, mtsdf + j * glyphSize * 4, glyphSize * 4 * sizeof(F32));
			}

			x += (glyphSize + padding) * 4;
			if (x == rowWidth * 4) { x = padding * 4; y += glyphRowSize; }
		}

		font->CreateKerning(&info, glyphSize, glyphToCodepoint);

		String newPath = path.FileName().Prepend("fonts/").Append(".nhf");
		file.Open(newPath, FILE_OPEN_RESOURCE_WRITE);

		file.Write("NHF");
		file.Write(FontVersion);
		file.Write(font->name);
		file.Write(font->ascent);
		file.Write(font->descent);
		file.Write(font->lineGap);
		file.Write(font->scale);
		file.Write(font->glyphSize);
		file.Write(rowWidth);
		file.Write(columnHeight);
		file.Write(font->glyphs, sizeof(Glyph) * 96);
		file.Write(atlas, rowWidth * columnHeight * 4 * sizeof(F32));

		file.Close();

		return newPath;
	}

	Logger::Error("Failed To Find Or Open File: ", path, "!");
	return {};
}

String Resources::UploadAudio(const String& path)
{
	if (path.Blank()) { Logger::Error("Blank Path Passed To UploadAudio!"); return {}; }

	File file(path, FILE_OPEN_RESOURCE_READ);
	if (file.Opened())
	{
		String data = file.ReadAll();
		file.Close();

		AudioClip clip{};
		clip.name = path.FileName();

		String extension = path.FileExtension();

		switch (Hash::StringCI(extension.Data(), extension.Size()))
		{
		case "wav"_Hash: {
			drwav wav;
			drwav_init_memory(&wav, data.Data(), data.Size(), nullptr);

			clip.format.formatTag = 3;
			clip.format.channelCount = wav.fmt.channels;
			clip.format.samplesPerSec = wav.fmt.sampleRate;
			clip.format.bitsPerSample = 32;
			clip.format.avgBytesPerSec = clip.format.samplesPerSec * clip.format.channelCount * (clip.format.bitsPerSample >> 3);
			clip.format.blockAlign = (clip.format.channelCount * clip.format.bitsPerSample) >> 3;
			clip.format.extraSize = 0;
			clip.size = (U32)(clip.format.channelCount * wav.totalPCMFrameCount * sizeof(F32));
			Memory::Allocate(&clip.buffer, clip.size);

			drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, (F32*)clip.buffer);
			drwav_uninit(&wav);
		} break;
		case "flac"_Hash: {
			drflac* flac = drflac_open_memory(data.Data(), data.Size(), nullptr);
			
			clip.format.formatTag = 3;
			clip.format.channelCount = flac->channels;
			clip.format.samplesPerSec = flac->sampleRate;
			clip.format.bitsPerSample = 32;
			clip.format.avgBytesPerSec = clip.format.samplesPerSec * clip.format.channelCount * (clip.format.bitsPerSample >> 3);
			clip.format.blockAlign = (clip.format.channelCount * clip.format.bitsPerSample) >> 3;
			clip.format.extraSize = 0;
			clip.size = (U32)(clip.format.channelCount * flac->totalPCMFrameCount * sizeof(F32));
			Memory::Allocate(&clip.buffer, clip.size);

			drflac_read_pcm_frames_f32(flac, flac->totalPCMFrameCount, (F32*)clip.buffer);
			drflac_close(flac);
		} break;
		case "mp3"_Hash: {
			drmp3 mp3;
			drmp3_init_memory(&mp3, data.Data(), data.Size(), nullptr);

			clip.format.formatTag = 3;
			clip.format.channelCount = mp3.channels;
			clip.format.samplesPerSec = mp3.sampleRate;
			clip.format.bitsPerSample = 32;
			clip.format.avgBytesPerSec = clip.format.samplesPerSec * clip.format.channelCount * (clip.format.bitsPerSample >> 3);
			clip.format.blockAlign = (clip.format.channelCount * clip.format.bitsPerSample) >> 3;
			clip.format.extraSize = 0;
			clip.size = (U32)(clip.format.channelCount * mp3.totalPCMFrameCount * sizeof(F32));
			Memory::Allocate(&clip.buffer, clip.size);

			drmp3_read_pcm_frames_f32(&mp3, mp3.totalPCMFrameCount, (F32*)clip.buffer);
			drmp3_uninit(&mp3);
		} break;
		case "ogg"_Hash: {
			I32 channelCount;
			I32 sampleRate;
			I16* samples;
			I32 sampleCount = stb_vorbis_decode_memory((U8*)data.Data(), (U32)data.Size(), &channelCount, &sampleRate, &samples);

			clip.format.formatTag = 1;
			clip.format.channelCount = channelCount;
			clip.format.samplesPerSec = sampleRate;
			clip.format.bitsPerSample = 16;
			clip.format.avgBytesPerSec = clip.format.samplesPerSec * clip.format.channelCount * (clip.format.bitsPerSample >> 3);
			clip.format.blockAlign = (clip.format.channelCount * clip.format.bitsPerSample) >> 3;
			clip.format.extraSize = 0;
			clip.size = clip.format.channelCount * sampleCount * sizeof(I16);
			Memory::Allocate(&clip.buffer, clip.size);
			memcpy(clip.buffer, samples, clip.size);

			free(samples);
		} break;
		}

		String newPath = path.FileName().Prepend("audio/").Append(".nha");
		file.Open(newPath, FILE_OPEN_RESOURCE_WRITE);

		file.Write("NHA");
		file.Write(AudioVersion);
		file.Write(clip.name);
		file.Write(clip.format);
		file.Write(clip.size);
		file.Write(clip.buffer, clip.size);

		file.Close();

		Memory::Free(&clip.buffer);
		return newPath;
	}

	Logger::Error("Failed To Find Or Open File: ", path, "!");
	return {};
}

ResourceRef<Texture>& Resources::WhiteTexture()
{
	return whiteTexture;
}

ResourceRef<Texture>& Resources::PlaceholderTexture()
{
	return placeholderTexture;
}

const DescriptorSet& Resources::DummyDescriptorSet()
{
	return dummySet;
}

const DescriptorSet& Resources::BindlessTexturesDescriptorSet()
{
	return bindlessTexturesSet;
}
