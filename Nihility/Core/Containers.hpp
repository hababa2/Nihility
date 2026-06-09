#pragma once

#include "Defines.hpp"

#include "Platform/Memory.hpp"

#include "glm/glm.hpp"

#include <vector>
#include <queue>
#include <array>
#include <unordered_map>
#include <string>
#include <sstream>
#include <format>
#include <filesystem>
#include <cuchar>
#include <span>

template <typename T> using Vector = std::vector<T, Allocator<T>>;
template <typename T> using Queue = std::queue<T, std::deque<T, Allocator<T>>>;
template <typename T, U64 Size> using Array = std::array<T, Size>;
template <typename K, typename V> using Hashmap = std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, Allocator<std::pair<const K, V>>>;

using Path = std::filesystem::path;

namespace Containers
{
	template <Character T> using StringBase = std::basic_string<T, std::char_traits<T>, Allocator<T>>;
	template <Character T> using StringViewBase = std::basic_string_view<T>;

	template<typename From>
	StringBase<C32> ToUTF32(const StringViewBase<From>& input)
	{
		StringBase<C32> result;
		result.reserve(input.size());

		std::mbstate_t state{};
		const C* ptr = reinterpret_cast<const C*>(input.data());
		const C* end = ptr + (input.size() * sizeof(From));

		C32 c32;
		while (ptr < end)
		{
			U64 remaining = static_cast<U64>(end - ptr);
			U64 rc = std::mbrtoc32(&c32, ptr, remaining, &state);
			if (rc == (U64)-1 || rc == (U64)-2) { break; }

			if (rc == 0)
			{
				result.push_back(0);
				ptr += 1;
				continue;
			}

			if (rc == (U64)-3)
			{
				result.push_back(c32);
				continue;
			}

			result.push_back(c32);
			ptr += rc;
		}
		return result;
	}

	template<typename To>
	StringBase<To> FromUTF32(const StringViewBase<C32>& input)
	{
		StringBase<To> result;
		result.reserve(input.size());

		std::mbstate_t state{};
		C buffer[MB_LEN_MAX];

		for (C32 c32 : input)
		{
			U64 rc = std::c32rtomb(buffer, c32, &state);
			if (rc != (U64)-1)
			{
				for (U64 i = 0; i < rc; ++i)
				{
					result.push_back(static_cast<To>(buffer[i]));
				}
			}
		}
		return result;
	}

	template<typename From>
	StringBase<C32> ToUTF32(const StringBase<From>& input)
	{
		StringBase<C32> result;
		mbstate_t state = mbstate_t();
		result.reserve(input.size());

		const C* ptr = reinterpret_cast<const C*>(input.data());
		const C* end = ptr + (input.size() * sizeof(From));

		C32 c32;
		while (ptr < end)
		{
			U64 remaining = static_cast<U64>(end - ptr);
			U64 rc = std::mbrtoc32(&c32, ptr, remaining, &state);
			if (rc == (U64)-1 || rc == (U64)-2) { break; }

			if (rc == 0)
			{
				ptr += 1;
				continue;
			}

			if (rc == (U64)-3)
			{
				result.push_back(c32);
				continue;
			}

			result.push_back(c32);
			ptr += rc;
		}
		return result;
	}

	template<typename To>
	StringBase<To> FromUTF32(const StringBase<C32>& input)
	{
		StringBase<To> result;
		result.reserve(input.size());

		std::mbstate_t state{};
		C buffer[MB_LEN_MAX];

		for (C32 c32 : input)
		{
			U64 rc = std::c32rtomb(buffer, c32, &state);
			if (rc != (U64)-1)
			{
				for (U64 i = 0; i < rc; ++i)
				{
					result.push_back(static_cast<To>(buffer[i]));
				}
			}
		}
		return result;
	}
}

using String = Containers::StringBase<C>;
using String8 = Containers::StringBase<C8>;
using String16 = Containers::StringBase<C16>;
using String32 = Containers::StringBase<C32>;
using WString = Containers::StringBase<CW>;
using StringView = Containers::StringViewBase<C>;
using StringView8 = Containers::StringViewBase<C8>;
using StringView16 = Containers::StringViewBase<C16>;
using StringView32 = Containers::StringViewBase<C32>;
using WStringView = Containers::StringViewBase<CW>;

template <Character Ch, typename... Args>
static Containers::StringBase<Ch> Format(std::basic_format_string<Ch, std::type_identity_t<Args>...> fmt, Args&&... args)
{
	Containers::StringBase<Ch> result;
	std::format_to(std::back_inserter(result), fmt, Forward<Args>(args)...);
	return result;
}

template<U64 N>
struct FixedString {
	char data[N + 1]{};
	static constexpr U64 size = N;

	constexpr std::string_view view() const
	{
		return { data, N };
	}
};

template <typename... Args>
constexpr auto CreateFormatStr(Args&&... args)
{
	constexpr U64 count = sizeof...(Args);
	FixedString<count * 2> result{};

	for (U64 i = 0; i < count; ++i)
	{
		result.data[i * 2] = '{';
		result.data[i * 2 + 1] = '}';
	}

	return result;
}

template<Character To, Character From>
static Containers::StringBase<To> ConvertString(const Containers::StringBase<From>& input)
{
	if constexpr (IsSame<From, To>) { return input; }
	else { return Containers::FromUTF32<To>(Containers::ToUTF32(input)); }
}

template<Character To>
static Containers::StringBase<To> ConvertString(const StringView& input)
{
	if constexpr (IsSame<C, To>) { return Containers::StringBase<To>(input); }
	else { return Containers::FromUTF32<To>(Containers::ToUTF32(input)); }
}

template<Character To>
static Containers::StringBase<To> ConvertString(const StringView8& input)
{
	if constexpr (IsSame<C8, To>) { return Containers::StringBase<To>(input); }
	else { return Containers::FromUTF32<To>(Containers::ToUTF32(input)); }
}

template<Character To>
static Containers::StringBase<To> ConvertString(const StringView16& input)
{
	if constexpr (IsSame<C16, To>) { return Containers::StringBase<To>(input); }
	else { return Containers::FromUTF32<To>(Containers::ToUTF32(input)); }
}

template<Character To>
static Containers::StringBase<To> ConvertString(const StringView32& input)
{
	if constexpr (IsSame<C32, To>) { return Containers::StringBase<To>(input); }
	else { return Containers::FromUTF32<To>(Containers::ToUTF32(input)); }
}

template<Character To>
static Containers::StringBase<To> ConvertString(const WStringView& input)
{
	if constexpr (IsSame<CW, To>) { return Containers::StringBase<To>(input); }
	else { return Containers::FromUTF32<To>(Containers::ToUTF32(input)); }
}

template<Character To, Character From>
Containers::StringViewBase<To> ConvertView(Containers::StringViewBase<From> input)
{
	static To buffer[1024];
	static std::span<To> outBuffer(buffer);
	std::mbstate_t state{};
	U64 outPos = 0;

	const C* ptr = reinterpret_cast<const C*>(input.data());
	const C* end = ptr + (input.size() * sizeof(From));

	while (ptr < end && outPos < outBuffer.size())
	{
		C32 c32;
		U64 readRc = std::mbrtoc32(&c32, ptr, end - ptr, &state);

		if (readRc == (U64)-1 || readRc == (U64)-2) { break; }

		if (readRc == 0) { ptr += 1; continue; }

		C tempBuf[MB_LEN_MAX];
		std::mbstate_t out_state{};
		U64 writeRc = std::c32rtomb(tempBuf, c32, &out_state);

		if (writeRc != (U64)-1)
		{
			for (U64 i = 0; i < writeRc && outPos < outBuffer.size(); ++i)
			{
				outBuffer[outPos++] = static_cast<To>(tempBuf[i]);
			}
		}
		ptr += readRc;
	}

	if (outPos < outBuffer.size())
	{
		outBuffer[outPos] = static_cast<To>(0);
	}

	return Containers::StringViewBase<To>(outBuffer.data(), outPos);
}

template<Character To, Character From>
Containers::StringViewBase<To> ConvertToView(Containers::StringBase<From> input)
{
	static To buffer[1024];
	static std::span<To> outBuffer(buffer);
	std::mbstate_t state{};
	U64 outPos = 0;

	const C* ptr = reinterpret_cast<const C*>(input.data());
	const C* end = ptr + (input.size() * sizeof(From));

	while (ptr < end && outPos < outBuffer.size())
	{
		C32 c32;
		U64 readRc = std::mbrtoc32(&c32, ptr, end - ptr, &state);

		if (readRc == (U64)-1 || readRc == (U64)-2) { break; }

		if (readRc == 0) { ptr += 1; continue; }

		C tempBuf[MB_LEN_MAX];
		std::mbstate_t outState{};
		U64 writeRc = std::c32rtomb(tempBuf, c32, &outState);

		if (writeRc != (U64)-1)
		{
			for (U64 i = 0; i < writeRc && outPos < outBuffer.size(); ++i)
			{
				outBuffer[outPos++] = static_cast<To>(tempBuf[i]);
			}
		}
		ptr += readRc;
	}

	return Containers::StringViewBase<To>(outBuffer.data(), outPos);
}

//----- CUSTOM FORMATS -----

template <>
struct std::formatter<String8> : std::formatter<std::string_view> {
	auto format(const String8& s, std::format_context& ctx) const
	{
		StringView view{ reinterpret_cast<const C*>(s.data()), s.size() };
		return std::formatter<std::string_view>::format(view, ctx);
	}
};

template <>
struct std::formatter<String16> : std::formatter<std::string_view> {
	auto format(const String16& s, std::format_context& ctx) const
	{
		return std::formatter<std::string_view>::format(ConvertToView<C>(s), ctx);
	}
};

template <>
struct std::formatter<String32> : std::formatter<std::string_view> {
	auto format(const String32& s, std::format_context& ctx) const
	{
		return std::formatter<std::string_view>::format(ConvertToView<C>(s), ctx);
	}
};

template <>
struct std::formatter<WString> : std::formatter<std::string_view> {
	auto format(const WString& s, std::format_context& ctx) const
	{
		return std::formatter<std::string_view>::format(ConvertToView<C>(s), ctx);
	}
};

template <>
struct std::formatter<Path> : std::formatter<std::string_view> {
	auto format(const Path& p, std::format_context& ctx) const
	{
		return std::formatter<std::string_view>::format(ConvertToView<C>(p.string<C, std::char_traits<C>, Allocator<C>>()), ctx);
	}
};

template <>
struct std::formatter<glm::vec2> : std::formatter<std::string_view> {
	auto format(glm::vec2 v, std::format_context& ctx) const
	{
		return std::format_to(ctx.out(), "({0}, {1})", v.x, v.y);
	}
};

template <>
struct std::formatter<glm::vec3> : std::formatter<std::string_view> {
	auto format(glm::vec3 v, std::format_context& ctx) const
	{
		return std::format_to(ctx.out(), "({0}, {1}, {2})", v.x, v.y, v.z);
	}
};

template <>
struct std::formatter<glm::vec4> : std::formatter<std::string_view> {
	auto format(glm::vec4 v, std::format_context& ctx) const
	{
		return std::format_to(ctx.out(), "({0}, {1}, {2}, {3})", v.x, v.y, v.z, v.w);
	}
};