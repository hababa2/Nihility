#pragma once

#include "Defines.hpp"

#include "Containers/CircularQueue.hpp"
#include "Resources/Resources.hpp"

#include "miniaudio/miniaudio.h"
#include "miniaudio/verblib.h"

using PlaybackHandle = U32;
using ChannelHandle = U32;
constexpr ChannelHandle MasterChannel = 0xFFFFFFFF;

enum class NH_API AudioEffectType
{
	None = 0,
	Reverb = 1,
	Echo = 2,
	EQ = 4,
	Lowpass = 8
};

#pragma pack(push, 1)

struct NH_API ReverbParameters
{
	F32 wetDryMix; // [0, 1] ratio of wet (processed) signal to dry (original) signal
	F32 damping; // [0, 1] damping
	F32 roomSize; // [0.0001, 1] room size
};

struct NH_API EchoParameters
{
	F32 wetDryMix;	// [0, 1] ratio of wet (processed) signal to dry (original) signal
	F32 decay;		// [0, 1] amount of output fed back into input
	F32 delay;		// delay (all channels) in milliseconds
};

struct NH_API EQParameters
{
	F32 frequencyCenter0;	// [20, 20000] center frequency in Hz, band 0
	F32 gain0;				// [0.126, 7.94] boost/cut
	F32 bandwidth0;			// [0.1, 2] bandwidth, region of EQ is center frequency +/- bandwidth/2
	F32 frequencyCenter1;	// [20, 20000] center frequency in Hz, band 1
	F32 gain1;				// [0.126, 7.94] boost/cut
	F32 bandwidth1;			// [0.1, 2] bandwidth, region of EQ is center frequency +/- bandwidth/2
	F32 frequencyCenter2;	// [20, 20000] center frequency in Hz, band 2
	F32 gain2;				// [0.126, 7.94] boost/cut
	F32 bandwidth2;			// [0.1, 2] bandwidth, region of EQ is center frequency +/- bandwidth/2
	F32 frequencyCenter3;	// [20, 20000] center frequency in Hz, band 3
	F32 gain3;				// [0.126, 7.94] boost/cut
	F32 bandwidth3;			// [0.1, 2] bandwidth, region of EQ is center frequency +/- bandwidth/2
};

struct NH_API LowpassParameters
{
	F32 cutoffFrequency;
};

#pragma pack(pop)

struct NH_API EffectConfig
{
	AudioEffectType type = AudioEffectType::None;
	union
	{
		ReverbParameters reverb;
		EchoParameters echo;
		EQParameters eq;
		LowpassParameters lowpass;
	};
};

constexpr EffectConfig DefaultReverb = {
	.type = AudioEffectType::Reverb,
	.reverb = {
		.wetDryMix = 0.5f,
		.damping = 0.9f,
		.roomSize = 0.6f
	}
};

constexpr EffectConfig DefaultEcho = {
	.type = AudioEffectType::Echo,
	.echo = {
		.wetDryMix = 0.5f,
		.decay = 0.5f,
		.delay = 0.2f
	}
};

constexpr EffectConfig DefaultEQ = {
	.type = AudioEffectType::EQ,
	.eq = {
		.frequencyCenter0 = 100.0f,
		.gain0 = 1.0f,
		.bandwidth0 = 1.0f,
		.frequencyCenter1 = 800.0f,
		.gain1 = 1.0f,
		.bandwidth1 = 1.0f,
		.frequencyCenter2 = 2000.0f,
		.gain2 = 1.0f,
		.bandwidth2 = 1.0f,
		.frequencyCenter3 = 10000.0f,
		.gain3 = 1.0f,
		.bandwidth3 = 1.0f
	}
};

constexpr EffectConfig DefaultLowpass = {
	.type = AudioEffectType::Lowpass,
	.lowpass = {
		.cutoffFrequency = 600.0f
	}
};

struct NH_API AudioSystemConfig
{
	U32 bufferLengthInMs = 5;
	bool useLowLatencyMode = true;
};

struct NH_API AudioClipConfig
{
	ChannelHandle channel = MasterChannel;
	F32 volume = 1.0f;
	F32 pitch = 1.0f;
	bool loop = false;
	bool is3D = false;
};

class NH_API Audio
{
private:
	enum class AudioCommandType
	{
		PlayClip,
		StopClip,
		SetClipVolume,
		SetClipPitch,
		SetChannelVolume,
		SetChannelPitch,
		AddChannelEffect,
		RemoveChannelEffect,
		UpdateChannelEffect,
		SetListenerTransform,
		SetClipPosition
	};

	struct AudioCommand
	{
		AudioCommandType type;

		AudioClip* clip;
		ChannelHandle channel;
		PlaybackHandle handle;
		bool stream;
		bool loop;
		bool is3D;

		F32 volume;
		F32 pitch;

		EffectConfig effect;

		glm::vec3 position;
		glm::vec3 forward;
		glm::vec3 up;
		glm::vec3 velocity;
	};

	struct AudioInstance
	{
		bool isStream = false;
		bool isLooping = false;
		bool isStopping = false;
		bool is3D = false;

		U32 channelCount = 2;
		F32 targetVolume = 1.0f;
		F32 currentVolume = 1.0f;
		F32 pitch = 1.0f;

		ChannelHandle channel;

		verblib reverb;
		ma_spatializer spatializer;
		ma_resampler resampler;

		union {
			ma_decoder decoder;
			ma_audio_buffer buffer;
		};

		ma_data_source* dataSource = nullptr;

		std::atomic<bool> active{ false };
		std::atomic<bool> inUse{ false };
	};

	struct Channel
	{
		F32 targetVolume = 1.0f;
		F32 currentVolume = 1.0f;
		U32 effects;

		ma_lpf lowpass;
		ma_delay echo;
		ma_peak2 eq[4];

		ReverbParameters reverbParams = DefaultReverb.reverb;

		alignas(16) F32 mixBuffer[4096 * 8];

		std::atomic<bool> active{ false };
	};

public:
	static PlaybackHandle Play(std::shared_ptr<AudioClip> clip, AudioClipConfig config = {});
	static void Stop(PlaybackHandle handle);
	static void SetClipVolume(PlaybackHandle handle, F32 volume);
	static void SetClipPitch(PlaybackHandle handle, F32 pitch);

	static ChannelHandle CreateChannel(const String& name);
	static ChannelHandle GetChannel(const String& name);
	static void SetChannelVolume(ChannelHandle handle, F32 volume);
	static void SetChannelPitch(ChannelHandle handle, F32 pitch);
	static void AddChannelEffect(ChannelHandle handle, EffectConfig config);
	static void RemoveChannelEffect(ChannelHandle handle, AudioEffectType type);
	static void UpdateChannelEffect(ChannelHandle handle, EffectConfig config);

	static void SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);
	static void SetClipPosition(PlaybackHandle handle, const glm::vec3& position, const glm::vec3& velocity = { });

	static void ChangeDevice(const U32& inputDeviceId, const U32& outputDeviceId);

private:
	static bool Initialize(const AudioSystemConfig& config = {});
	static bool EnumerateDevices();
	static void Shutdown();

	static void Update();
	static void LogCallback(void* pUserData, U32 level, const C* pMessage);
	static void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, U32 frameCount);
	static void MixActiveVoices(F32* outputBuffer, U32 frameCount);
	static void ProcessAudio(F32* outputBuffer, U32 frameCount);
	static void ProcessChannels(F32* outputBuffer, U32 frameCount);
	static void ProcessMaster(F32* outputBuffer, U32 frameCount);

	static void CleanupAudioInstance(AudioInstance& audio);

	static bool ValidPlaybackHandle(PlaybackHandle handle);
	static bool ValidChannelHandle(ChannelHandle handle);

	static ma_context context;
	static ma_device_config deviceConfig;
	static ma_device device;
	static ma_resource_manager* resourceManager;
	static ma_allocation_callbacks callbacks;

	static F32 targetMasterVolume;
	static F32 currentMasterVolume;

	static ma_channel_converter converter;
	static ma_spatializer_listener listener;

	static constexpr U32 MaxChannels = 32;
	static Channel channels[MaxChannels];
	static Hashmap<String, ChannelHandle> channelRegistry;

	static constexpr U32 MaxClips = 256;
	static constexpr U32 MaxStreams = 8;
	static AudioInstance audioInstances[MaxClips + MaxStreams];

	static CircularQueue<AudioCommand> commandQueue;

	static Vector<ma_device_info> outputDevices;
	static Vector<ma_device_info> inputDevices;

	static U32 defaultOutputDevice;
	static U32 defaultInputDevice;

	static F32 envelope;
	static F32 attackCoeff;
	static F32 releaseCoeff;
	static F32 ceilingLinear;

	friend class Nihility;
	friend class AudioVoiceCallback;
	friend struct StreamDecodeTask;

	STATIC_CLASS(Audio);
};