#include "Resources.hpp"

#include "Core/File.hpp"
#include "Core/Logger.hpp"
#include "Core/DataReader.hpp"
#include "Rendering/Renderer.hpp"
#include "Rendering/VulkanInclude.hpp"

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include "glm/glm.hpp"
#include "bc7enc_rdo/bc7enc.h"
#include "libsndfile/sndfile.h"
#include "msdfgen/msdfgen.h"

#include <memory>

bool Resources::Initialize()
{
	Logger::Trace("Initializing Resource System...");

	bc7enc_compress_block_init();

	//Default Textures
	Resources::Load<Texture>(L"white");
	Resources::Load<Texture>(L"missing_texture");

	return true;
}

void Resources::Shutdown()
{
	Logger::Trace("Shutting Down Resource System...");

	static ResourcePool<Texture>& texturePool = GetPool<Texture>();

	for (const auto& [name, texture] : texturePool.cache)
	{
		Renderer::DestroyTexture(*texture);
	}

	static ResourcePool<AudioClip>& audioPool = GetPool<AudioClip>();
	for (const auto& [name, clip] : audioPool.cache)
	{
		if (!clip->isStream && clip->audioData)
		{
			Memory::Free(&clip->audioData);
		}
	}
}

WString Resources::UploadResource(const Path& path)
{
	WString ext{ path.extension().native() };
	WString pathname{ path.native() };
	WString filename{ path.filename().native() };
	filename = filename.substr(0, filename.size() - ext.size());

	switch (HashCI(ext.data(), ext.size()))
	{
	//Images
	case ".jpg"_HashCI:
	case ".jpeg"_HashCI:
	case ".png"_HashCI:
	case ".bmp"_HashCI:
	case ".tga"_HashCI:
	case ".jfif"_HashCI:
	case ".tiff"_HashCI:
	case ".ktx"_HashCI:
	case ".ktx2"_HashCI:
	case ".pnm"_HashCI:
	case ".ppm"_HashCI:
	case ".pgm"_HashCI:
	case ".sbsar"_HashCI: {
		WString* fn = new WString(filename);
		FileIO::ReadFileAsync(path, UploadTexture, fn);

		return Move(filename);
	} break;

	//Audio
	case ".ogg"_HashCI:
	case ".opus"_HashCI:
	case ".wav"_HashCI:
	case ".flac"_HashCI:
	case ".mp3"_HashCI: {
		WString* fn = new WString(filename);
		FileIO::ReadFileAsync(path, UploadAudio, fn);

		return Move(filename);
	} break;

	//Fonts
	case ".ttf"_Hash:
	case ".otf"_Hash: {
		WString* fn = new WString(filename);
		FileIO::ReadFileAsync(path, UploadFont, fn);

		return Move(filename);
	} break;

	default: {
		Logger::Error("Unknown File Extension: ", ext, "!");
		return {};
	} break;
	}

	return WString{ path.filename().native() };
}

void ExtractAndPadBlock(const U8* rgbaPixels, U32 imgWidth, U32 imgHeight, U32 blockX, U32 blockY, U8 outBlock[64])
{
	for (U32 y = 0; y < 4; ++y)
	{
		for (U32 x = 0; x < 4; ++x)
		{
			U32 px = glm::min(blockX + x, imgWidth - 1);
			U32 py = glm::min(blockY + y, imgHeight - 1);

			U32 srcIdx = (py * imgWidth + px) * 4;
			U32 dstIdx = (y * 4 + x) * 4;

			outBlock[dstIdx + 0] = rgbaPixels[srcIdx + 0];
			outBlock[dstIdx + 1] = rgbaPixels[srcIdx + 1];
			outBlock[dstIdx + 2] = rgbaPixels[srcIdx + 2];
			outBlock[dstIdx + 3] = rgbaPixels[srcIdx + 3];
		}
	}
}

void Resources::UploadTexture(FileData& data, void* userData)
{
	WString* filename = (WString*)userData;

	if (data.dataSize)
	{
		I32 texWidth, texHeight;
		U8* textureData = stbi_load_from_memory((U8*)data.buffer, (I32)data.dataSize, &texWidth, &texHeight, nullptr, STBI_rgb_alpha);

		if (!textureData)
		{
			Logger::Error("Failed To Load Texture Data!");
			stbi_image_free(textureData);
			delete filename;
			FileIO::FreeData(data);
			return;
		}

		bc7enc_compress_block_params blockParams;
		bc7enc_compress_block_params_init(&blockParams);
		blockParams.m_uber_level = 4;
		//bc7enc_compress_block_params_init_linear_weights(&blockParams); //TODO: use if normal map or mask

		U32 currentW = texWidth;
		U32 currentH = texHeight;
		U8* currentPixels = textureData;
		U32 mipCount = (U32)glm::floor(glm::log2((F32)glm::max(texWidth, texHeight))) + 1;
		Vector<MipLevel> mipChain(mipCount);

		for (U32 mipLevel = 0; mipLevel < mipCount; ++mipLevel)
		{
			MipLevel mip;
			mip.width = glm::max(1u, currentW);
			mip.height = glm::max(1u, currentH);

			U32 blocksX = glm::max(1u, (mip.width + 3) / 4);
			U32 blocksY = glm::max(1u, (mip.height + 3) / 4);
			mip.compressedData.resize(blocksX * blocksY * 16);

			U32 blockIndex = 0;
			for (U32 by = 0; by < mip.height; by += 4)
			{
				for (U32 bx = 0; bx < mip.width; bx += 4)
				{
					U8 paddedBlock[64];
					ExtractAndPadBlock(currentPixels, mip.width, mip.height, bx, by, paddedBlock);

					bc7enc_compress_block(&mip.compressedData[blockIndex * 16], paddedBlock, &blockParams);

					++blockIndex;
				}
			}

			mipChain[mipLevel] = mip;

			if (currentW == 1 && currentH == 1) { break; }

			U32 nextW = glm::max(1u, currentW / 2);
			U32 nextH = glm::max(1u, currentH / 2);
			U8* nextPixels;
			Memory::Allocate(&nextPixels, nextW * nextH * 4);

			stbir_resize_uint8_linear(currentPixels, currentW, currentH, 0,
				nextPixels, nextW, nextH, 0, STBIR_RGBA);

			if (currentPixels != textureData) { Memory::Free(&currentPixels); }
			currentPixels = nextPixels;
			currentW = nextW;
			currentH = nextH;
		}

		if (currentPixels != textureData) { Memory::Free(&currentPixels); }
		stbi_image_free(textureData);

		TextureFileHeader header;
		header.width = texWidth;
		header.height = texHeight;
		header.format = VK_FORMAT_BC7_SRGB_BLOCK;
		header.mipCount = (U32)mipChain.size();

		U64 currentFileOffset = sizeof(TextureFileHeader);

		for (I32 i = header.mipCount - 1; i >= 0; --i)
		{
			header.mipOffsets[i] = currentFileOffset;
			header.mipSizes[i] = mipChain[i].compressedData.size();
			currentFileOffset += header.mipSizes[i];
		}

		header.totalSize = (U32)currentFileOffset;

		U8* buffer;
		Memory::Allocate(&buffer, header.totalSize);
		U8* bufferPointer = buffer;

		memcpy(bufferPointer, &header, sizeof(TextureFileHeader));
		bufferPointer += sizeof(TextureFileHeader);

		for (I32 i = header.mipCount - 1; i >= 0; --i)
		{
			memcpy(bufferPointer, mipChain[i].compressedData.data(), mipChain[i].compressedData.size());
			bufferPointer += mipChain[i].compressedData.size();
		}

		FileIO::WriteFileAsync(*filename, buffer, header.totalSize, 0, UploadFileFinished, nullptr);

		Memory::Free(&buffer);
	}
	else { Logger::Error("Failed To Read File: ", *filename, '!'); }

	delete filename;
	FileIO::FreeData(data);
}

struct EncodeBufferState
{
	U8* buffer;
	I64 capacity;
	I64 currentPosition;
	I64 maxSizeUsed;
};

static I64 EncodeGetFileLen(void* userData)
{
	EncodeBufferState* state = static_cast<EncodeBufferState*>(userData);
	return state->maxSizeUsed;
}

static I64 EncodeSeek(I64 offset, I32 whence, void* userData)
{
	EncodeBufferState* state = static_cast<EncodeBufferState*>(userData);
	switch (whence)
	{
	case SEEK_SET: { state->currentPosition = offset; } break;
	case SEEK_CUR: { state->currentPosition += offset; } break;
	case SEEK_END: { state->currentPosition = state->maxSizeUsed + offset; } break;
	}

	if (state->currentPosition < 0) { state->currentPosition = 0; }
	if (state->currentPosition > state->capacity) { state->currentPosition = state->capacity; }

	return state->currentPosition;
}

static I64 EncodeRead(void* ptr, I64 count, void* userData)
{
	return 0;
}

static I64 EncodeWrite(const void* ptr, I64 count, void* userData)
{
	EncodeBufferState* state = static_cast<EncodeBufferState*>(userData);

	if (state->currentPosition + count > state->capacity)
	{
		count = state->capacity - state->currentPosition;
	}

	if (count > 0)
	{
		std::memcpy(state->buffer + state->currentPosition, ptr, count);
		state->currentPosition += count;

		if (state->currentPosition > state->maxSizeUsed)
		{
			state->maxSizeUsed = state->currentPosition;
		}
	}
	return count;
}

static I64 EncodeTell(void* userData)
{
	EncodeBufferState* state = static_cast<EncodeBufferState*>(userData);
	return state->currentPosition;
}

struct MemoryIOState
{
	const U8* data;
	I64 size;
	I64 position;
};

static I64 MemGetFileLen(void* userData)
{
	MemoryIOState* state = static_cast<MemoryIOState*>(userData);
	return state->size;
}

static I64 MemSeek(I64 offset, I32 whence, void* userData)
{
	MemoryIOState* state = static_cast<MemoryIOState*>(userData);
	switch (whence)
	{
	case SEEK_SET: { state->position = offset; } break;
	case SEEK_CUR: { state->position += offset; } break;
	case SEEK_END: { state->position = state->size + offset; } break;
	}

	if (state->position < 0) { state->position = 0; }
	if (state->position > state->size) { state->position = state->size; }

	return state->position;
}

static I64 MemRead(void* ptr, I64 count, void* userData)
{
	MemoryIOState* state = static_cast<MemoryIOState*>(userData);

	I64 available = state->size - state->position;
	I64 toRead = (count < available) ? count : available;

	if (toRead > 0)
	{
		std::memcpy(ptr, state->data + state->position, toRead);
		state->position += toRead;
	}
	return toRead;
}

static I64 MemWrite(const void* ptr, I64 count, void* userData)
{
	return 0;
}

static I64 MemTell(void* userData)
{
	MemoryIOState* state = static_cast<MemoryIOState*>(userData);
	return state->position;
}

void Resources::UploadAudio(FileData& data, void* userData)
{
	WString* filename = (WString*)userData;

	if (data.dataSize)
	{
		MemoryIOState memoryState{};
		memoryState.data = (U8*)data.buffer;
		memoryState.size = data.dataSize;
		memoryState.position = 0;

		SF_VIRTUAL_IO virtualIO{};
		virtualIO.get_filelen = MemGetFileLen;
		virtualIO.seek = MemSeek;
		virtualIO.read = MemRead;
		virtualIO.write = MemWrite;
		virtualIO.tell = MemTell;

		SF_INFO sourceInfo{};
		SNDFILE* sourceFile = sf_open_virtual(&virtualIO, SFM_READ, &sourceInfo, &memoryState);

		if (!sourceFile)
		{
			Logger::Error("Failed To Decode Audio File For Upload!");
			delete filename;
			FileIO::FreeData(data);
			return;
		}

		if (data.dataSize > Megabytes(2))
		{
			U64 worstCaseSize = (sourceInfo.frames * sourceInfo.channels * sizeof(F32)) + Megabytes(1);

			EncodeBufferState encodeState{};
			encodeState.capacity = worstCaseSize;
			Memory::Allocate(&encodeState.buffer, worstCaseSize);

			SF_VIRTUAL_IO virtualIO{};
			virtualIO.get_filelen = EncodeGetFileLen;
			virtualIO.seek = EncodeSeek;
			virtualIO.read = EncodeRead;
			virtualIO.write = EncodeWrite;
			virtualIO.tell = EncodeTell;

			SF_INFO vorbisInfo = sourceInfo;
			vorbisInfo.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;

			SNDFILE* vorbisOut = sf_open_virtual(&virtualIO, SFM_WRITE, &vorbisInfo, &encodeState);

			F64 quality = 0.5;
			sf_command(vorbisOut, SFC_SET_VBR_ENCODING_QUALITY, &quality, sizeof(quality));

			constexpr U64 ChunkSize = 4096;
			F32 pcmBuffer[ChunkSize];
			I64 readFrames = 0;

			while ((readFrames = sf_readf_float(sourceFile, pcmBuffer, ChunkSize / sourceInfo.channels)) > 0)
			{
				sf_writef_float(vorbisOut, pcmBuffer, readFrames);
			}

			sf_close(vorbisOut);

			AudioFileHeader header{};
			header.format = AudioFormatType::Vorbis;
			header.channelCount = sourceInfo.channels;
			header.sampleRate = sourceInfo.samplerate;
			header.dataSize = encodeState.maxSizeUsed;

			U64 totalFileSize = sizeof(AudioFileHeader) + header.dataSize;
			U8* outBuffer = nullptr;
			Memory::Allocate(&outBuffer, totalFileSize);

			memcpy(outBuffer, &header, sizeof(AudioFileHeader));
			memcpy(outBuffer + sizeof(AudioFileHeader), encodeState.buffer, header.dataSize);

			FileIO::WriteFileAsync(*filename, outBuffer, totalFileSize, 0, UploadFileFinished, nullptr);

			Memory::Free(&outBuffer);
			Memory::Free(&encodeState.buffer);
		}
		else
		{
			AudioFileHeader header{};
			header.format = AudioFormatType::PCM;
			header.channelCount = sourceInfo.channels;
			header.sampleRate = sourceInfo.samplerate;
			header.frameCount = sourceInfo.frames;
			header.dataSize = header.frameCount * header.channelCount * sizeof(F32);

			U64 totalFileSize = sizeof(AudioFileHeader) + header.dataSize;
			U8* outBuffer = nullptr;
			Memory::Allocate(&outBuffer, totalFileSize);

			U8* writePtr = outBuffer;
			memcpy(writePtr, &header, sizeof(AudioFileHeader));
			writePtr += sizeof(AudioFileHeader);

			sf_readf_float(sourceFile, reinterpret_cast<F32*>(writePtr), header.frameCount);

			FileIO::WriteFileAsync(*filename, outBuffer, totalFileSize, 0, UploadFileFinished, nullptr);

			Memory::Free(&outBuffer);
		}

		sf_close(sourceFile);
	}
	else { Logger::Error("Failed To Read File: ", *filename, '!'); }

	delete filename;
	FileIO::FreeData(data);
}

void Resources::UploadFont(FileData& data, void* userData)
{
	using namespace msdfgen;

	WString* filename = (WString*)userData;

	if (data.dataSize)
	{
		Range pxRange(4);
		MSDFGeneratorConfig generatorConfig;
		generatorConfig.overlapSupport = true;
		MSDFGeneratorConfig postErrorCorrectionConfig(generatorConfig);
		generatorConfig.errorCorrection.mode = ErrorCorrectionConfig::EDGE_PRIORITY;
		postErrorCorrectionConfig.errorCorrection.distanceCheckMode = ErrorCorrectionConfig::CHECK_DISTANCE_AT_EDGE;

		U8 glyphSize = 32;
		U8 padding = 1;

		Font font{};
		font.name = *filename;

		U8* fontData = (U8*)data.buffer;

		stbtt_fontinfo info{};

		stbtt_InitFont(&info, fontData, stbtt_GetFontOffsetForIndex(fontData, 0));

		U32 rowWidth = (glyphSize + padding) * 8 + padding;
		U32 columnHeight = (glyphSize + padding) * 12 + padding;

		font.LoadData(&info, glyphSize);

		F32* atlas;
		Memory::Allocate(&atlas, rowWidth * columnHeight * 4);

		F32* bitmap;
		Memory::Allocate(&bitmap, glyphSize * glyphSize * 4);

		Hashmap<I32, C> glyphToCodepoint{ 128 };

		U32 glyphRowSize = rowWidth * (glyphSize + padding) * 4;
		U32 x = padding * 4;
		U32 y = rowWidth * padding * 4;

		for (C8 i = 0; i < 96; ++i)
		{
			C8 codepoint = i + 32;
			Shape shape = font.LoadGlyph(&info, codepoint, bitmap, glyphToCodepoint);
			shape.normalize();
			shape.inverseYAxis = true;

			SDFTransformation transformation(Projection(1, msdfgen::Vector2(font.glyphs[i].x, font.glyphs[i].y)), Range(1.0));

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

		font.CreateKerning(&info, glyphToCodepoint);

		U64 textureSize = rowWidth * columnHeight * 4 * sizeof(F32);
		U64 totalFileSize = sizeof(FontFileHeader) + textureSize + sizeof(Glyph) * 96;
		U8* outBuffer = nullptr;
		Memory::Allocate(&outBuffer, totalFileSize);

		FontFileHeader header{};
		header.scale = font.scale;
		header.glyphSize = font.glyphSize;
		header.textureWidth = rowWidth;
		header.textureHeight = columnHeight;

		U8* writePtr = outBuffer;
		memcpy(writePtr, &header, sizeof(FontFileHeader));
		writePtr += sizeof(FontFileHeader);

		memcpy(writePtr, font.glyphs, sizeof(Glyph) * 96);
		writePtr += sizeof(Glyph) * 96;

		memcpy(writePtr, atlas, textureSize);
		writePtr += textureSize;

		FileIO::WriteFileAsync(*filename, outBuffer, totalFileSize, 0, UploadFileFinished, nullptr);

		Memory::Free(&outBuffer);
		Memory::Free(&bitmap);
		Memory::Free(&atlas);
	}
	else { Logger::Error("Failed To Read File: ", *filename, '!'); }

	delete filename;
	FileIO::FreeData(data);
}

void Resources::UploadFileFinished(FileData& data, void* userData)
{
	//TODO: Record file metadata

	FileIO::FreeData(data);
}

U32 Resources::GetTextureId()
{
	static std::atomic<U32> textureId{ 0 };

	return textureId.fetch_add(1, std::memory_order_relaxed);
}

std::shared_ptr<Texture> Resources::FetchTexture(const Path& name)
{
	static ResourcePool<Texture>& pool = GetPool<Texture>();

	FileData data = FileIO::ReadFileSync(name);

	if (data.buffer && data.dataSize)
	{
		DataReader reader(data);

		TextureFileHeader header{};
		reader.Read(header);

		if (header.identifier != TextureIdentifier)
		{
			Logger::Error("Asset '", name, "' Is Not A Nihility Texture!");
			FileIO::FreeData(data);
			return nullptr;
		}

		if (header.version != TextureVersion)
		{
			Logger::Error("Nihility Texture '", name, "' Is Using An Older Version!");
			FileIO::FreeData(data);
			return nullptr;
		}

		U64 offset = header.mipOffsets[0];
		U64 size = header.mipSizes[0];

		Texture texture{};
		texture.name = name.c_str();
		texture.width = header.width;
		texture.height = header.height;
		texture.format = header.format;
		texture.depth = 1;
		texture.size = size;
		texture.mipmapLevels = 1;

		reader.SeekFromStart(offset);

		U8* textureData;
		Memory::Allocate(&textureData, texture.size);
		reader.Read(textureData, texture.size);

		std::shared_ptr<Texture> texturePtr = pool.Insert(texture.name, std::make_shared<Texture>(texture));
		texturePtr->id = GetTextureId();
		if (!Renderer::UploadTexture(texturePtr, textureData, 0))
		{
			Memory::Free(&textureData);
			Logger::Error("Failed To Upload Texture Data!");
			return nullptr;
		}

		Memory::Free(&textureData);

		return texturePtr;
	}
	else { Logger::Error("Failed To Read File: ", name, '!'); }

	FileIO::FreeData(data);
	return nullptr;
}

std::shared_ptr<AudioClip> Resources::FetchAudioClip(const Path& name)
{
	static ResourcePool<AudioClip>& pool = GetPool<AudioClip>();

	FileData data = FileIO::ReadFileSync(name);

	if (data.buffer && data.dataSize)
	{
		DataReader reader(data);

		AudioFileHeader header{};
		reader.Read(header);

		if (header.identifier != AudioIdentifier)
		{
			Logger::Error("Asset '", name, "' Is Not A Nihility Audio Clip!");
			FileIO::FreeData(data);
			return nullptr;
		}

		if (header.version != AudioVersion)
		{
			Logger::Error("Nihility Audio Clip '", name, "' Is Using An Older Version!");
			FileIO::FreeData(data);
			return nullptr;
		}

		AudioClip clip{};
		clip.name = name.c_str();
		clip.audioBytes = (U32)header.dataSize;
		Memory::Allocate(&clip.audioData, clip.audioBytes);
		reader.Read(clip.audioData, clip.audioBytes);

		clip.format.formatTag = 3; //WAVE_FORMAT_IEEE_FLOAT
		clip.format.channelCount = header.channelCount;
		clip.format.samplesPerSec = header.sampleRate;
		clip.format.bitsPerSample = 32;
		clip.format.blockAlign = (clip.format.channelCount * clip.format.bitsPerSample) / 8;
		clip.format.avgBytesPerSec = clip.format.samplesPerSec * clip.format.blockAlign;
		clip.isStream = header.format != AudioFormatType::PCM;

		FileIO::FreeData(data);
		return pool.Insert(clip.name, std::make_shared<AudioClip>(clip));
	}
	else { Logger::Error("Failed To Read File: ", name, '!'); }

	FileIO::FreeData(data);
	return nullptr;
}

std::shared_ptr<Font> Resources::FetchFont(const Path& name)
{
	static ResourcePool<Font>& pool = GetPool<Font>();
	static ResourcePool<Texture>& texturePool = GetPool<Texture>();

	FileData data = FileIO::ReadFileSync(name);

	if (data.buffer && data.dataSize)
	{
		DataReader reader(data);

		FontFileHeader header{};
		reader.Read(header);

		if (header.identifier != FontIdentifier)
		{
			Logger::Error("Asset '", name, "' Is Not A Nihility Font!");
			FileIO::FreeData(data);
			return nullptr;
		}

		if (header.version != FontVersion)
		{
			Logger::Error("Nihility Font '", name, "' Is Using An Older Version!");
			FileIO::FreeData(data);
			return nullptr;
		}

		Font font{};
		font.name = name.c_str();
		font.scale = header.scale;
		font.glyphSize = header.glyphSize;

		reader.Read(font.glyphs, sizeof(Glyph) * 96);
		
		U32 textureSize = header.textureWidth * header.textureHeight * 4 * sizeof(F32);

		Texture texture{};
		texture.name = name.c_str();
		texture.width = header.textureWidth;
		texture.height = header.textureHeight;
		texture.format = 109;
		texture.depth = 1;
		texture.size = textureSize;
		texture.mipmapLevels = 1;
		texture.sampler.filterMode = FilterMode::Linear;

		U8* textureData;
		Memory::Allocate(&textureData, texture.size);
		reader.Read(textureData, texture.size);

		std::shared_ptr<Texture> texturePtr = texturePool.Insert(texture.name, std::make_shared<Texture>(texture));
		texturePtr->id = GetTextureId();
		if (!Renderer::UploadTexture(texturePtr, textureData, 0))
		{
			Memory::Free(&textureData);
			Logger::Error("Failed To Upload Texture Data!");
			return nullptr;
		}

		Memory::Free(&textureData);

		font.texture = texturePtr;

		FileIO::FreeData(data);
		return pool.Insert(font.name, std::make_shared<Font>(font));
	}
	else { Logger::Error("Failed To Read File: ", name, '!'); }

	FileIO::FreeData(data);
	return nullptr;
}