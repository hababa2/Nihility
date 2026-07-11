#pragma once

#include "Defines.hpp"

#include "Texture.hpp"
#include "AudioClip.hpp"
#include "Font.hpp"
#include "Platform/Multithreading.hpp"

#include <shared_mutex>

template<typename T>
struct ResourcePool
{
private:
	using MapType = Hashmap<WString, std::shared_ptr<T>>;

	MapType cache;
	std::shared_mutex poolMutex;

	std::shared_ptr<T> Get(const WString& key)
	{
		std::shared_lock lock(poolMutex);

		auto it = cache.find(key);

		if (it != cache.end()) { return it->second; }
		return nullptr;
	}

	std::shared_ptr<T> Insert(const WString& key, std::shared_ptr<T> value)
	{
		std::unique_lock lock(poolMutex);

		auto [it, inserted] = cache.try_emplace(key, value);
		return inserted ? it->second : nullptr;
	}

	friend class Resources;
};

struct FileData;

class NH_API Resources
{
public:
	template<typename T>
	static std::shared_ptr<T> Load(const WString& name)
	{
		static ResourcePool<T>& pool = GetPool<T>();
		if (std::shared_ptr<T> res = pool.Get(name)) { return res; }

		return FetchResource<T>(name);
	}

private:
	static bool Initialize();
	static void Shutdown();

	template<typename T>
	static ResourcePool<T>& GetPool()
	{
		static ResourcePool<T> pool;
		return pool;
	}

	static WString UploadResource(const Path& path);
	static void UploadTexture(FileData& data, void* userData);
	static void UploadAudio(FileData& data, void* userData);
	static void UploadFont(FileData& data, void* userData);

	static void UploadFileFinished(FileData& data, void* userData);

	static U32 GetTextureId();

	template<typename T>
	static std::shared_ptr<T> FetchResource(const Path& name);
	static std::shared_ptr<Texture> FetchTexture(const Path& name);
	static std::shared_ptr<AudioClip> FetchAudioClip(const Path& name);
	static std::shared_ptr<Font> FetchFont(const Path& name);

	friend struct RenderTarget;
	friend class Renderer;
	friend class Platform;
	friend class Nihility;

	STATIC_CLASS(Resources);
};

template<typename T>
inline std::shared_ptr<T> Resources::FetchResource(const Path& name)
{
	if constexpr (IsSame<T, Texture>)
	{
		return FetchTexture(name);
	}
	else if constexpr (IsSame<T, AudioClip>)
	{
		return FetchAudioClip(name);
	}
	else if constexpr (IsSame<T, Font>)
	{
		return FetchFont(name);
	}
	else
	{
		return nullptr;
	}
}