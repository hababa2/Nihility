#include "Audio.hpp"

#include "Nihility.hpp"
#include "Core/Logger.hpp"
#include "Core/Time.hpp"
#include "Core/Settings.hpp"

#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

#define VERBLIB_IMPLEMENTATION
#include "miniaudio/verblib.h"

#include <cmath>

ma_context Audio::context;
ma_device_config Audio::deviceConfig;
ma_device Audio::device;
ma_resource_manager* Audio::resourceManager;
ma_allocation_callbacks Audio::callbacks;

F32 Audio::targetMasterVolume = 1.0f;
F32 Audio::currentMasterVolume = 1.0f;

ma_channel_converter Audio::converter;
ma_spatializer_listener Audio::listener;

Audio::Channel Audio::channels[MaxChannels] = {};
Hashmap<String, ChannelHandle> Audio::channelRegistry;

Audio::AudioInstance Audio::audioInstances[MaxClips + MaxStreams] = {};

CircularQueue<Audio::AudioCommand> Audio::commandQueue;

Vector<ma_device_info> Audio::outputDevices;
Vector<ma_device_info> Audio::inputDevices;

U32 Audio::defaultOutputDevice = U32_MAX;
U32 Audio::defaultInputDevice = U32_MAX;

F32 Audio::envelope = 0.0f;
F32 Audio::attackCoeff = 0.0f;
F32 Audio::releaseCoeff = 0.0f;
F32 Audio::ceilingLinear = 0.0f;

#undef C

void* alloc(U64 size, void*)
{
	U8* buffer = nullptr;
	Memory::Allocate(&buffer, size);
	return buffer;
}

void* realloc(void* p, U64 size, void*)
{
	Memory::Reallocate((U8**)&p, size);
	return p;
}

void free(void* p, void*)
{
	Memory::Free(&p);
}

bool Audio::Initialize(const AudioSystemConfig& config)
{
	Logger::Trace("Initializing Audio System...");

	commandQueue.Create(1024);

	callbacks.pUserData = nullptr;
	callbacks.onMalloc = alloc;
	callbacks.onRealloc = realloc;
	callbacks.onFree = free;

	ma_backend backends[] = {
		ma_backend_wasapi,
		ma_backend_dsound,
		ma_backend_pulseaudio,
		ma_backend_alsa
	};

	ma_context_config contextConfig = ma_context_config_init();

	if (ma_context_init(backends, CountOf32(backends), &contextConfig, &context) != MA_SUCCESS)
	{
		Logger::Error("Failed To Initialize Miniaudio Context!");
		return false;
	}

	if (!EnumerateDevices())
	{
		Logger::Error("Failed To Enumerate Audio Devices!");
		return false;
	}

	U32 channelCount = 2;
	U32 sampleRate = 48000;

	if (defaultInputDevice != U32_MAX)
	{
		const ma_device_info& defaultOutput = outputDevices[defaultOutputDevice];
		channelCount = defaultOutput.nativeDataFormats[0].channels;
		sampleRate = defaultOutput.nativeDataFormats[0].sampleRate;
	}

	resourceManager = (ma_resource_manager*)ma_malloc(sizeof(ma_resource_manager), nullptr);
	ma_resource_manager_config rmConfig = ma_resource_manager_config_init();
	rmConfig.decodedFormat = ma_format_f32;
	rmConfig.decodedChannels = channelCount;
	rmConfig.decodedSampleRate = sampleRate;

	if (ma_resource_manager_init(&rmConfig, resourceManager) != MA_SUCCESS)
	{
		Logger::Error("Failed To Initialize Audio Resource Manager!");
		return false;
	}

	deviceConfig = ma_device_config_init(ma_device_type_playback);
	//deviceConfig.capture.pDeviceID = defaultInputDevice == U32_MAX ? nullptr : &inputDevices[defaultInputDevice].id;
	//deviceConfig.capture.format = ma_format_f32;
	//deviceConfig.capture.channels = channelCount;
	//deviceConfig.capture.shareMode = ma_share_mode_shared;
	deviceConfig.playback.pDeviceID = defaultOutputDevice == U32_MAX ? nullptr : &outputDevices[defaultOutputDevice].id;
	deviceConfig.playback.format = ma_format_f32;
	deviceConfig.playback.channels = channelCount;
	deviceConfig.sampleRate = sampleRate;
	deviceConfig.dataCallback = DataCallback;
	if (config.useLowLatencyMode)
	{
		deviceConfig.performanceProfile = ma_performance_profile_low_latency;
		deviceConfig.periodSizeInMilliseconds = config.bufferLengthInMs;
		deviceConfig.wasapi.noAutoConvertSRC = true;
	}
	else
	{
		deviceConfig.performanceProfile = ma_performance_profile_conservative;
	}

	if (ma_device_init(&context, &deviceConfig, &device) != MA_SUCCESS)
	{
		Logger::Error("Failed To Initialize Audio Hardware Device!");
		return false;
	}

	attackCoeff = glm::exp(-1.0f / (0.001f * device.sampleRate));
	releaseCoeff = glm::exp(-1.0f / (0.1f * device.sampleRate));
	ceilingLinear = glm::pow(10.0f, -1.0f / 20.0f);

	ma_spatializer_listener_config listenerConfig = ma_spatializer_listener_config_init(deviceConfig.playback.channels);
	ma_spatializer_listener_init(&listenerConfig, &callbacks, &listener);

	ma_spatializer_listener_set_speed_of_sound(&listener, 343.3f);
	ma_spatializer_listener_set_world_up(&listener, 0.0f, -1.0f, 0.0f);

	ma_channel_converter_config converterConfig = ma_channel_converter_config_init(
		ma_format_f32,
		2,
		nullptr,
		device.playback.channels,
		nullptr,
		ma_channel_mix_mode_rectangular
	);

	ma_channel_converter_init(&converterConfig, &callbacks, &converter);

	if (ma_device_start(&device) != MA_SUCCESS)
	{
		Logger::Error("Failed To Start Audio Device!");
		return false;
	}

	return true;
}

bool Audio::EnumerateDevices()
{
	ma_device_info* pPlaybackInfos;
	ma_uint32 playbackCount;
	ma_device_info* pCaptureInfos;
	ma_uint32 captureCount;

	ma_result res = ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount);
	if (res != MA_SUCCESS) { return false; }

	outputDevices.assign(pPlaybackInfos, pPlaybackInfos + playbackCount);
	inputDevices.assign(pCaptureInfos, pCaptureInfos + captureCount);

	U32 i = 0;
	for (ma_device_info& device : outputDevices)
	{
		ma_context_get_device_info(&context, ma_device_type_playback, &device.id, &device);

		if (device.isDefault) { defaultOutputDevice = i; }
		++i;
	}

	i = 0;
	for (ma_device_info& device : inputDevices)
	{
		ma_context_get_device_info(&context, ma_device_type_capture, &device.id, &device);

		if (device.isDefault) { defaultInputDevice = i; }
		++i;
	}

	return true;
}

void Audio::Shutdown()
{
	Logger::Trace("Shutting Down Audio System...");

	ma_device_stop(&device);

	ma_spatializer_listener_uninit(&listener, &callbacks);
	ma_channel_converter_uninit(&converter, &callbacks);

	ma_device_uninit(&device);

	ma_resource_manager_uninit(resourceManager);
	ma_free(resourceManager, &callbacks);

	ma_context_uninit(&context);

	commandQueue.Destroy();
}

void Audio::Update()
{
	AudioCommand cmd;
	while (commandQueue.Pop(cmd))
	{
		switch (cmd.type)
		{
		case AudioCommandType::PlayClip: {
			AudioInstance& audio = audioInstances[cmd.handle];

			if (cmd.stream)
			{
				ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, cmd.clip->format.channelCount, 48000);
				decoderConfig.encodingFormat = ma_encoding_format_vorbis;

				if (ma_decoder_init_memory(cmd.clip->audioData, cmd.clip->audioBytes, &decoderConfig, &audio.decoder) != MA_SUCCESS)
				{
					audio.active.store(false, std::memory_order_release);
					audio.inUse.store(false, std::memory_order_release);
					break;
				}

				audio.dataSource = &audio.decoder;
			}
			else
			{
				U32 channelCount = cmd.clip->format.channelCount;
				U64 frames = cmd.clip->audioBytes / (channelCount * sizeof(F32));

				ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(ma_format_f32, channelCount, frames, cmd.clip->audioData, &callbacks);
				ma_audio_buffer_init(&bufferConfig, &audio.buffer);
				audio.dataSource = &audio.buffer;
			}

			audio.isStream = cmd.stream;
			audio.isLooping = cmd.loop;
			audio.isStopping = false;
			audio.channel = cmd.channel;

			audio.targetVolume = cmd.volume;
			audio.currentVolume = 0.0f;
			audio.pitch = glm::clamp(cmd.pitch, 0.1f, 4.0f);

			U32 channelCount = cmd.clip->format.channelCount;

			ma_resampler_config resampleConfig = ma_resampler_config_init(
				ma_format_f32,
				channelCount,
				device.sampleRate,
				device.sampleRate,
				ma_resample_algorithm_linear
			);

			ma_resampler_init(&resampleConfig, &callbacks, &audio.resampler);

			if (audio.pitch != 1.0f)
			{
				ma_resampler_set_rate(&audio.resampler, (U32)(device.sampleRate * audio.pitch), device.sampleRate);
			}

			audio.channelCount = cmd.clip->format.channelCount;

			verblib_initialize(&audio.reverb, device.sampleRate, channelCount);

			if (audio.channel != MasterChannel && channels[audio.channel].effects & *AudioEffectType::Reverb)
			{
				verblib_set_room_size(&audio.reverb, DefaultReverb.reverb.roomSize);
				verblib_set_damping(&audio.reverb, DefaultReverb.reverb.damping);
				verblib_set_wet(&audio.reverb, DefaultReverb.reverb.wetDryMix);
				verblib_set_dry(&audio.reverb, 1.0f - DefaultReverb.reverb.wetDryMix);
				verblib_set_width(&audio.reverb, 1.0f);
			}
			else
			{
				verblib_set_room_size(&audio.reverb, DefaultReverb.reverb.roomSize);
				verblib_set_damping(&audio.reverb, DefaultReverb.reverb.damping);
				verblib_set_wet(&audio.reverb, DefaultReverb.reverb.wetDryMix);
				verblib_set_dry(&audio.reverb, 1.0f - DefaultReverb.reverb.wetDryMix);
				verblib_set_width(&audio.reverb, 1.0f);
			}

			audio.is3D = cmd.is3D;

			if (cmd.is3D)
			{
				ma_spatializer_config spatConfig = ma_spatializer_config_init(audio.channelCount, device.playback.channels);
				spatConfig.minDistance = 10.0f;
				spatConfig.maxDistance = 1000.0f;
				spatConfig.rolloff = 1.0f;
				spatConfig.attenuationModel = ma_attenuation_model_linear;

				ma_spatializer_init(&spatConfig, &callbacks, &audio.spatializer);
			}

			audio.active.store(true, std::memory_order_release);
		} break;
		case AudioCommandType::StopClip: {
			AudioInstance& audio = audioInstances[cmd.handle];
			if (audio.active && !audio.isStopping)
			{
				audio.isStopping = true;
				audio.targetVolume = 0.0f;
			}
		} break;
		case AudioCommandType::SetClipVolume: {
			AudioInstance& audio = audioInstances[cmd.handle];

			if (audio.active)
			{
				audio.targetVolume = cmd.volume;
			}
		} break;
		case AudioCommandType::SetClipPitch: {
			AudioInstance& audio = audioInstances[cmd.handle];

			if (audio.active)
			{
				audio.pitch = glm::clamp(cmd.pitch, 0.1f, 4.0f);

				ma_resampler_set_rate(&audio.resampler, (U32)(device.sampleRate * audio.pitch), device.sampleRate);
			}
		} break;
		case AudioCommandType::SetChannelVolume: {
			if (cmd.channel == MasterChannel || cmd.channel >= MaxChannels) { targetMasterVolume = cmd.volume; }
			else { channels[cmd.channel].targetVolume = cmd.volume; }
		} break;
		case AudioCommandType::AddChannelEffect: {
			Channel& channel = channels[cmd.channel];
			U32 channelCount = device.playback.channels;
			U32 sampleRate = device.sampleRate;

			channel.effects |= *cmd.effect.type;

			switch (cmd.effect.type)
			{
			case AudioEffectType::Reverb: {
				channel.reverbParams.roomSize = cmd.effect.reverb.roomSize;
				channel.reverbParams.damping = cmd.effect.reverb.damping;
				channel.reverbParams.wetDryMix = cmd.effect.reverb.wetDryMix;

				for (U32 i = 0; i < MaxClips; ++i)
				{
					AudioInstance& audio = audioInstances[i];

					if (audio.active.load(std::memory_order_acquire) && audio.channel == cmd.channel)
					{
						verblib_set_room_size(&audio.reverb, cmd.effect.reverb.roomSize);
						verblib_set_damping(&audio.reverb, cmd.effect.reverb.damping);

						verblib_set_wet(&audio.reverb, cmd.effect.reverb.wetDryMix);
						verblib_set_dry(&audio.reverb, 1.0f - cmd.effect.reverb.wetDryMix);
					}
				}
			} break;
			case AudioEffectType::Echo: {
				U32 delayFrames = (U32)(cmd.effect.echo.delay * sampleRate);

				ma_delay_config echoConfig = ma_delay_config_init(channelCount, sampleRate, delayFrames, cmd.effect.echo.decay);
				ma_delay_init(&echoConfig, nullptr, &channel.echo);
			} break;
			case AudioEffectType::EQ: {
				ma_peak2_config eqConfig0 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, cmd.effect.eq.gain0, cmd.effect.eq.bandwidth0, cmd.effect.eq.frequencyCenter0);
				ma_peak2_init(&eqConfig0, nullptr, &channel.eq[0]);

				ma_peak2_config eqConfig1 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, cmd.effect.eq.gain1, cmd.effect.eq.bandwidth1, cmd.effect.eq.frequencyCenter1);
				ma_peak2_init(&eqConfig1, nullptr, &channel.eq[1]);

				ma_peak2_config eqConfig2 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, cmd.effect.eq.gain2, cmd.effect.eq.bandwidth2, cmd.effect.eq.frequencyCenter2);
				ma_peak2_init(&eqConfig2, nullptr, &channel.eq[2]);

				ma_peak2_config eqConfig3 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, cmd.effect.eq.gain3, cmd.effect.eq.bandwidth3, cmd.effect.eq.frequencyCenter3);
				ma_peak2_init(&eqConfig3, nullptr, &channel.eq[3]);
			} break;
			case AudioEffectType::Lowpass: {
				ma_lpf_config lpfConfig = ma_lpf_config_init(ma_format_f32, channelCount, sampleRate, cmd.effect.lowpass.cutoffFrequency, 2);
				ma_lpf_init(&lpfConfig, &callbacks, &channel.lowpass);
			} break;
			}
		} break;
		case AudioCommandType::RemoveChannelEffect: {
			Channel& channel = channels[cmd.channel];
			channel.effects &= channel.effects & ~*cmd.effect.type;
		} break;
		case AudioCommandType::UpdateChannelEffect: {
			Channel& channel = channels[cmd.channel];
			U32 channelCount = device.playback.channels;
			U32 sampleRate = device.sampleRate;

			switch (cmd.effect.type)
			{
			case AudioEffectType::Reverb: {
				channel.reverbParams.roomSize = cmd.effect.reverb.roomSize;
				channel.reverbParams.damping = cmd.effect.reverb.damping;
				channel.reverbParams.wetDryMix = cmd.effect.reverb.wetDryMix;

				for (U32 i = 0; i < MaxClips; ++i)
				{
					AudioInstance& audio = audioInstances[i];

					if (audio.active.load(std::memory_order_acquire) && audio.channel == cmd.channel)
					{
						verblib_set_room_size(&audio.reverb, cmd.effect.reverb.roomSize);
						verblib_set_damping(&audio.reverb, cmd.effect.reverb.damping);

						verblib_set_wet(&audio.reverb, cmd.effect.reverb.wetDryMix);
						verblib_set_dry(&audio.reverb, 1.0f - cmd.effect.reverb.wetDryMix);
					}
				}
			} break;
			case AudioEffectType::Echo: {
				U32 delayFrames = (U32)(cmd.effect.echo.delay * sampleRate);

				ma_delay_config echoConfig = ma_delay_config_init(channelCount, sampleRate, delayFrames, cmd.effect.echo.decay);
				ma_delay_init(&echoConfig, nullptr, &channel.echo);
			} break;
			case AudioEffectType::EQ: {
				ma_peak2_config eqConfig0 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, cmd.effect.eq.gain0, cmd.effect.eq.bandwidth0, cmd.effect.eq.frequencyCenter0);
				ma_peak2_init(&eqConfig0, nullptr, &channel.eq[0]);

				ma_peak2_config eqConfig1 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, cmd.effect.eq.gain1, cmd.effect.eq.bandwidth1, cmd.effect.eq.frequencyCenter1);
				ma_peak2_init(&eqConfig1, nullptr, &channel.eq[1]);

				ma_peak2_config eqConfig2 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, cmd.effect.eq.gain2, cmd.effect.eq.bandwidth2, cmd.effect.eq.frequencyCenter2);
				ma_peak2_init(&eqConfig2, nullptr, &channel.eq[2]);

				ma_peak2_config eqConfig3 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, cmd.effect.eq.gain3, cmd.effect.eq.bandwidth3, cmd.effect.eq.frequencyCenter3);
				ma_peak2_init(&eqConfig3, nullptr, &channel.eq[3]);
			} break;
			case AudioEffectType::Lowpass: {
				ma_lpf_config lpfConfig = ma_lpf_config_init(ma_format_f32, channelCount, sampleRate, cmd.effect.lowpass.cutoffFrequency, 2);
				ma_lpf_init(&lpfConfig, &callbacks, &channel.lowpass);
			} break;
			}
		} break;
		case AudioCommandType::SetListenerTransform: {
			ma_spatializer_listener_set_position(&listener, cmd.position.x, cmd.position.y, cmd.position.z);
			ma_spatializer_listener_set_direction(&listener, cmd.forward.x, cmd.forward.y, cmd.forward.z);
			ma_spatializer_listener_set_world_up(&listener, cmd.up.x, -cmd.up.y, cmd.up.z);
		} break;
		case AudioCommandType::SetClipPosition: {
			AudioInstance& audio = audioInstances[cmd.handle];
			if (audio.active && audio.is3D)
			{
				ma_spatializer_set_position(&audio.spatializer, cmd.position.x, cmd.position.y, cmd.position.z);
				ma_spatializer_set_velocity(&audio.spatializer, cmd.velocity.x, cmd.velocity.y, cmd.velocity.z);
			}
		} break;
		}
	}
}

void Audio::LogCallback(void* pUserData, U32 level, const C* pMessage)
{
	Logger::Debug(pMessage);
}

void Audio::DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, U32 frameCount)
{
	F32* outputBuffer = (F32*)pOutput;

	for (U32 i = 0; i < frameCount * pDevice->playback.channels; ++i)
	{
		outputBuffer[i] = 0.0f;
	}

	frameCount = glm::min(frameCount, 4096u);

	for (U32 c = 0; c < MaxChannels; ++c)
	{
		if (channels[c].active.load(std::memory_order_relaxed))
		{
			std::memset(channels[c].mixBuffer, 0, frameCount * 2 * sizeof(F32));
		}
	}

	ProcessAudio(outputBuffer, frameCount);
	ProcessChannels(outputBuffer, frameCount);
	ProcessMaster(outputBuffer, frameCount);
}

void Audio::ProcessAudio(F32* outputBuffer, U32 frameCount)
{
	U32 channelCount = device.playback.channels;

	F32* tempBuffer = nullptr;
	F32* tempResampleBuffer = nullptr;
	F32* spatializedBuffer = nullptr;

	Memory::Allocate(&tempBuffer, 4096 * 8);
	Memory::Allocate(&tempResampleBuffer, 8192 * 8);
	Memory::Allocate(&spatializedBuffer, 4096 * 8);

	for (U32 i = 0; i < MaxClips + MaxStreams; ++i)
	{
		AudioInstance& audio = audioInstances[i];

		if (audio.active.load(std::memory_order_acquire))
		{
			U64 framesToMix = 0;

			if (audio.pitch == 1.0f)
			{
				ma_data_source_read_pcm_frames(audio.dataSource, tempBuffer, frameCount, &framesToMix);

				if (framesToMix < frameCount)
				{
					if (audio.isLooping && !audio.isStopping)
					{
						ma_data_source_seek_to_pcm_frame(audio.dataSource, 0);

						U64 framesRead2 = 0;
						ma_data_source_read_pcm_frames(audio.dataSource, tempBuffer + (framesToMix * 2), frameCount - framesToMix, &framesRead2);
						framesToMix += framesRead2;
					}
					else if (framesToMix == 0)
					{
						CleanupAudioInstance(audio);
						continue;
					}
				}
			}
			else
			{
				U64 requiredInputFrames = 0;
				ma_resampler_get_expected_output_frame_count(&audio.resampler, frameCount, &requiredInputFrames);

				if (requiredInputFrames > 8192) { requiredInputFrames = 8192; }

				U64 inputFramesRead = 0;
				ma_data_source_read_pcm_frames(audio.dataSource, tempResampleBuffer, requiredInputFrames, &inputFramesRead);

				if (inputFramesRead < requiredInputFrames)
				{
					if (audio.isLooping && !audio.isStopping)
					{
						ma_data_source_seek_to_pcm_frame(audio.dataSource, 0);
						U64 secondRead = 0;
						ma_data_source_read_pcm_frames(audio.dataSource, tempResampleBuffer + (inputFramesRead * 2), requiredInputFrames - inputFramesRead, &secondRead);
						inputFramesRead += secondRead;
					}
					else if (inputFramesRead == 0)
					{
						CleanupAudioInstance(audio);
						continue;
					}
				}

				U64 outputFramesGenerated = frameCount;
				ma_resampler_process_pcm_frames(&audio.resampler, tempResampleBuffer, &inputFramesRead, tempBuffer, &outputFramesGenerated);

				framesToMix = outputFramesGenerated;
			}

			if (audio.channel != MasterChannel && channels[audio.channel].effects & *AudioEffectType::Reverb)
			{
				verblib_process(&audio.reverb, tempBuffer, tempBuffer, frameCount);
			}


			if (audio.is3D)
			{
				ma_spatializer_process_pcm_frames(&audio.spatializer, &listener, spatializedBuffer, tempBuffer, framesToMix);
			}
			else
			{
				ma_channel_converter_process_pcm_frames(&converter, spatializedBuffer, tempBuffer, framesToMix);
			}

			F32 volumeStep = 0.0f;
			if (audio.currentVolume != audio.targetVolume)
			{
				volumeStep = (audio.targetVolume - audio.currentVolume) / 4800.0f;
			}

			F32* targetBuffer = (audio.channel == MasterChannel || audio.channel >= MaxChannels) ? outputBuffer : channels[audio.channel].mixBuffer;

			for (U32 j = 0; j < framesToMix; ++j)
			{
				if (volumeStep != 0.0f)
				{
					audio.currentVolume += volumeStep;

					if ((volumeStep > 0 && audio.currentVolume > audio.targetVolume) ||
						(volumeStep < 0 && audio.currentVolume < audio.targetVolume))
					{
						audio.currentVolume = audio.targetVolume;
						volumeStep = 0.0f;
					}
				}

				for (U32 c = 0; c < channelCount; ++c)
				{
					targetBuffer[j * channelCount + c] += spatializedBuffer[j * channelCount + c] * audio.currentVolume;
				}
			}

			if (audio.isStopping && audio.currentVolume <= 0.001f)
			{
				CleanupAudioInstance(audio);
			}
		}
	}

	Memory::Free(&tempBuffer);
	Memory::Free(&tempResampleBuffer);
	Memory::Free(&spatializedBuffer);
}

void Audio::ProcessChannels(F32* outputBuffer, U32 frameCount)
{
	U32 channelCount = device.playback.channels;

	for (U32 c = 0; c < MaxChannels; ++c)
	{
		Channel& channel = channels[c];
		if (channel.active.load(std::memory_order_acquire))
		{
			F32 volumeStep = 0.0f;
			if (channel.currentVolume != channel.targetVolume)
			{
				volumeStep = (channel.targetVolume - channel.currentVolume) / 4800.0f;
			}

			F32* mixBuf = channel.mixBuffer;

			if (channel.effects & *AudioEffectType::EQ)
			{
				ma_peak2_process_pcm_frames(&channel.eq[0], mixBuf, mixBuf, frameCount);
				ma_peak2_process_pcm_frames(&channel.eq[1], mixBuf, mixBuf, frameCount);
				ma_peak2_process_pcm_frames(&channel.eq[2], mixBuf, mixBuf, frameCount);
				ma_peak2_process_pcm_frames(&channel.eq[3], mixBuf, mixBuf, frameCount);
			}

			if (channel.effects & *AudioEffectType::Echo)
			{
				ma_delay_process_pcm_frames(&channel.echo, mixBuf, mixBuf, frameCount);
			}

			if (channel.effects & *AudioEffectType::Lowpass)
			{
				ma_lpf_process_pcm_frames(&channel.lowpass, mixBuf, mixBuf, frameCount);
			}

			for (U32 j = 0; j < frameCount; ++j)
			{
				if (volumeStep != 0.0f)
				{
					channel.currentVolume += volumeStep;

					if ((volumeStep > 0 && channel.currentVolume > channel.targetVolume) ||
						(volumeStep < 0 && channel.currentVolume < channel.targetVolume))
					{
						channel.currentVolume = channel.targetVolume;
						volumeStep = 0.0f;
					}
				}

				for (U32 c = 0; c < channelCount; ++c)
				{
					outputBuffer[j * channelCount + c] += mixBuf[j * channelCount + c] * channel.currentVolume;
				}
			}
		}
	}
}

void Audio::ProcessMaster(F32* outputBuffer, U32 frameCount)
{
	U32 channelCount = device.playback.channels;

	F32 volumeStep = 0.0f;
	if (currentMasterVolume != targetMasterVolume)
	{
		volumeStep = (targetMasterVolume - currentMasterVolume) / 4800.0f;
	}

	for (U32 j = 0; j < frameCount; ++j)
	{
		if (volumeStep != 0.0f)
		{
			currentMasterVolume += volumeStep;

			if ((volumeStep > 0 && currentMasterVolume > targetMasterVolume) ||
				(volumeStep < 0 && currentMasterVolume < targetMasterVolume))
			{
				currentMasterVolume = targetMasterVolume;
				volumeStep = 0.0f;
			}
		}

		F32 peak = 0.0f;

		for (U32 c = 0; c < channelCount; ++c)
		{
			F32 absSample = glm::abs(outputBuffer[j * channelCount + c]);

			if (absSample > peak) { peak = absSample; }
		}

		if (peak > envelope) { envelope = attackCoeff * envelope + (1.0f - attackCoeff) * peak; }
		else { envelope = releaseCoeff * envelope; }

		F32 gain = 1.0f;
		if (envelope > ceilingLinear) { gain = ceilingLinear / envelope; }

		for (U32 c = 0; c < device.playback.channels; ++c)
		{
			outputBuffer[j * channelCount + c] *= gain * currentMasterVolume;
		}
	}
}

bool Audio::ValidPlaybackHandle(PlaybackHandle handle)
{
	return handle < (MaxClips + MaxStreams);
}

bool Audio::ValidChannelHandle(ChannelHandle handle)
{
	return handle < MaxChannels;
}

PlaybackHandle Audio::Play(std::shared_ptr<AudioClip> clip, AudioClipConfig config)
{
	if (!clip) { return U32_MAX; }

	AudioCommand cmd{};
	cmd.type = AudioCommandType::PlayClip;
	cmd.clip = clip.get();
	cmd.channel = config.channel;
	cmd.volume = config.volume;
	cmd.pitch = config.pitch;
	cmd.loop = config.loop;
	cmd.is3D = config.is3D;
	cmd.stream = clip->isStream;
	cmd.handle = U32_MAX;

	U32 i = clip->isStream ? MaxClips : 0;
	U32 end = MaxClips + (clip->isStream ? MaxStreams : 0);

	for (; i < end; ++i)
	{
		bool expected = false;
		if (audioInstances[i].inUse.compare_exchange_strong(expected, true, std::memory_order_acquire))
		{
			cmd.handle = i;
			break;
		}
	}

	if (cmd.handle == U32_MAX) { return U32_MAX; }

	commandQueue.Push(cmd);

	return cmd.handle;
}

void Audio::Stop(PlaybackHandle handle)
{
	if (!ValidPlaybackHandle(handle)) { return; }

	AudioCommand cmd{};
	cmd.type = AudioCommandType::StopClip;
	cmd.handle = handle;

	commandQueue.Push(cmd);
}

void Audio::SetClipVolume(PlaybackHandle handle, F32 volume)
{
	if (!ValidPlaybackHandle(handle)) { return; }

	AudioCommand cmd{};
	cmd.type = AudioCommandType::SetClipVolume;
	cmd.handle = handle;
	cmd.volume = glm::clamp(volume, 0.0f, 1.0f);

	commandQueue.Push(cmd);
}

void Audio::SetClipPitch(PlaybackHandle handle, F32 pitch)
{
	if (!ValidPlaybackHandle(handle)) { return; }

	AudioCommand cmd{};
	cmd.type = AudioCommandType::SetClipPitch;
	cmd.handle = handle;
	cmd.pitch = glm::clamp(pitch, 0.0f, 1.0f);

	commandQueue.Push(cmd);
}

void Audio::SetChannelVolume(ChannelHandle channel, F32 volume)
{
	if (!ValidChannelHandle(channel)) { return; }

	AudioCommand cmd{};
	cmd.type = AudioCommandType::SetChannelVolume;
	cmd.channel = channel;
	cmd.volume = glm::clamp(volume, 0.0f, 1.0f);

	commandQueue.Push(cmd);
}

void Audio::SetChannelPitch(ChannelHandle channel, F32 pitch)
{
	if (!ValidChannelHandle(channel)) { return; }

	AudioCommand cmd{};
	cmd.type = AudioCommandType::SetChannelVolume;
	cmd.channel = channel;
	cmd.pitch = glm::clamp(pitch, 0.0f, 1.0f);

	commandQueue.Push(cmd);
}

void Audio::AddChannelEffect(ChannelHandle channel, EffectConfig config)
{
	if (!ValidChannelHandle(channel)) { return; }

	AudioCommand cmd{};
	cmd.type = AudioCommandType::AddChannelEffect;
	cmd.channel = channel;
	cmd.effect = config;

	commandQueue.Push(cmd);
}

void Audio::RemoveChannelEffect(ChannelHandle channel, AudioEffectType type)
{
	if (!ValidChannelHandle(channel)) { return; }

	AudioCommand cmd{};
	cmd.type = AudioCommandType::RemoveChannelEffect;
	cmd.channel = channel;
	cmd.effect = { type };

	commandQueue.Push(cmd);
}

void Audio::UpdateChannelEffect(ChannelHandle channel, EffectConfig config)
{
	if (!ValidChannelHandle(channel)) { return; }

	AudioCommand cmd{};
	cmd.type = AudioCommandType::UpdateChannelEffect;
	cmd.channel = channel;
	cmd.effect = config;

	commandQueue.Push(cmd);
}

void Audio::SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up)
{
	AudioCommand cmd{};
	cmd.type = AudioCommandType::SetListenerTransform;
	cmd.position = position;
	cmd.forward = forward;
	cmd.up = up;

	commandQueue.Push(cmd);
}

void Audio::SetClipPosition(PlaybackHandle handle, const glm::vec3& position, const glm::vec3& velocity)
{
	if (!ValidPlaybackHandle(handle)) { return; }

	AudioCommand cmd{};
	cmd.type = AudioCommandType::SetClipPosition;
	cmd.handle = handle;
	cmd.position = position;
	cmd.velocity = velocity;

	commandQueue.Push(cmd);
}

ChannelHandle Audio::CreateChannel(const String& name)
{
	auto it = channelRegistry.find(name);
	if (it != channelRegistry.end()) { return it->second; }

	U32 index = U32_MAX;
	for (U32 i = 0; i < MaxChannels; ++i)
	{
		bool expected = false;
		if (channels[i].active.compare_exchange_strong(expected, true, std::memory_order_acquire))
		{
			index = i;
			break;
		}
	}

	if (index == U32_MAX) { return U32_MAX; }

	Channel& channel = Audio::channels[index];
	U32 channelCount = device.playback.channels;
	U32 sampleRate = device.sampleRate;

	ma_lpf_config lpfConfig = ma_lpf_config_init(ma_format_f32, channelCount, sampleRate, DefaultLowpass.lowpass.cutoffFrequency, 2);
	ma_lpf_init(&lpfConfig, &callbacks, &channel.lowpass);

	U32 delayFrames = (U32)(DefaultEcho.echo.delay * sampleRate);
	ma_delay_config echoConfig = ma_delay_config_init(channelCount, sampleRate, delayFrames, DefaultEcho.echo.decay);
	ma_delay_init(&echoConfig, &callbacks, &channel.echo);

	ma_peak2_config eqConfig0 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, DefaultEQ.eq.gain0, DefaultEQ.eq.bandwidth0, DefaultEQ.eq.frequencyCenter0);
	ma_peak2_init(&eqConfig0, nullptr, &channel.eq[0]);

	ma_peak2_config eqConfig1 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, DefaultEQ.eq.gain1, DefaultEQ.eq.bandwidth1, DefaultEQ.eq.frequencyCenter1);
	ma_peak2_init(&eqConfig1, nullptr, &channel.eq[1]);

	ma_peak2_config eqConfig2 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, DefaultEQ.eq.gain2, DefaultEQ.eq.bandwidth2, DefaultEQ.eq.frequencyCenter2);
	ma_peak2_init(&eqConfig2, nullptr, &channel.eq[2]);

	ma_peak2_config eqConfig3 = ma_peak2_config_init(ma_format_f32, channelCount, sampleRate, DefaultEQ.eq.gain3, DefaultEQ.eq.bandwidth3, DefaultEQ.eq.frequencyCenter3);
	ma_peak2_init(&eqConfig3, nullptr, &channel.eq[3]);

	std::memset(channel.mixBuffer, 0, sizeof(channel.mixBuffer));

	channel.targetVolume = 1.0f;
	channel.currentVolume = 1.0f;
	channel.effects = *AudioEffectType::None;

	channelRegistry[name] = index;
	return index;
}

ChannelHandle Audio::GetChannel(const String& name)
{
	auto it = channelRegistry.find(name);
	if (it != channelRegistry.end())
	{
		return it->second;
	}
	return MasterChannel;
}

void Audio::CleanupAudioInstance(AudioInstance& audio)
{
	audio.active.store(false, std::memory_order_release);

	if (audio.dataSource != nullptr)
	{
		if (audio.isStream)
		{
			ma_decoder_uninit(&audio.decoder);
		}

		audio.dataSource = nullptr;
	}

	

	ma_resampler_uninit(&audio.resampler, &callbacks);

	if (audio.is3D)
	{
		ma_spatializer_uninit(&audio.spatializer, &callbacks);
		audio.is3D = false;
	}

	audio.inUse.store(false, std::memory_order_release);
}

void Audio::ChangeDevice(const U32& inputDeviceId, const U32& outputDeviceId)
{
	ma_device_uninit(&device);

	deviceConfig.capture.pDeviceID = inputDeviceId == U32_MAX ? nullptr : &inputDevices[inputDeviceId].id;
	deviceConfig.playback.pDeviceID = outputDeviceId == U32_MAX ? nullptr : &outputDevices[outputDeviceId].id;

	ma_device_init(&context, &deviceConfig, &device);

	if (ma_device_init(&context, &deviceConfig, &device) != MA_SUCCESS)
	{
		Logger::Fatal("Failed To Reinitialize Audio Hardware Device!");
	}

	if (ma_device_start(&device) != MA_SUCCESS)
	{
		Logger::Fatal("Failed To Restart Audio Device!");
	}
}