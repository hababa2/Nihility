#pragma once

#include "Defines.hpp"

static constexpr U32 AudioVersion = MakeVersionNumber(2, 0, 0);
static constexpr U32 AudioIdentifier = 'NHA';

enum class AudioFormatType
{
	PCM,
	Vorbis
};

struct AudioFileHeader
{
	U32 identifier = AudioIdentifier;
	U32 version = AudioVersion;

	AudioFormatType format;

	U16 channelCount;
	U32 sampleRate;
	U64 frameCount;

	U64 dataSize;
};

struct WaveFormat
{
	U16 formatTag;
	U16 channelCount;
	UL32 samplesPerSec;
	UL32 avgBytesPerSec;
	U16 blockAlign;
	U16 bitsPerSample;
	U16 cbSize;
};

struct NH_API AudioClip
{
	WString name = L"";
	bool isStream = false;

	WaveFormat format{};

	U8* audioData = nullptr;
	U32 audioBytes = 0;
};