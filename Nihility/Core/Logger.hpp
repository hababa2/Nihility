#pragma once

#include "Defines.hpp"

#include "Platform/Multithreading.hpp"
#include "Core/Containers.hpp"

#ifdef NH_DEBUG
#	define LOG_DEBUG_ENABLED 1
#else
#	define LOG_DEBUG_ENABLED 0
#endif

#ifdef NH_DEBUG
#	define LOG_TRACE_ENABLED 1
#else
#	define LOG_TRACE_ENABLED 0
#endif

#define LOG_INFO_ENABLED 1

#define LOG_WARN_ENABLED 1

#define LOG_ERROR_ENABLED 1

#define LOG_FATAL_ENABLED 1

class NH_API Logger
{
public:
	template<typename... Args> static void Debug(Args&&... args)
	{
#if LOG_DEBUG_ENABLED == 1
		static constexpr auto format = CreateFormatStr(0, 0, args...);
		static constexpr std::string_view fmt = format.view();
		Log(Format<C>(fmt, "\033[0;36m[DEBUG]:\033[0m ", Forward<Args>(args)..., '\n'));
#endif
	}
	template<typename... Args> static void Trace(Args&&... args)
	{
#if LOG_TRACE_ENABLED == 1
		static constexpr auto format = CreateFormatStr(0, 0, args...);
		static constexpr std::string_view fmt = format.view();
		Log(Format<C>(fmt, "\033[1;30m[TRACE]:\033[0m ", Forward<Args>(args)..., '\n'));
#endif
	}
	template<typename... Args> static void Info(Args&&... args)
	{
#if LOG_INFO_ENABLED == 1
		static constexpr auto format = CreateFormatStr(0, 0, args...);
		static constexpr std::string_view fmt = format.view();
		Log(Format<C>(fmt, "\033[1;32m[INFO]:\033[0m  ", Forward<Args>(args)..., '\n'));
#endif
	}
	template<typename... Args> static void Warn(Args&&... args)
	{
#if LOG_WARN_ENABLED == 1
		static constexpr auto format = CreateFormatStr(0, 0, args...);
		static constexpr std::string_view fmt = format.view();
		Log(Format<C>(fmt, "\033[1;33m[WARN]:\033[0m  ", Forward<Args>(args)..., '\n'));
#endif
	}
	template<typename... Args> static void Error(Args&&... args)
	{
#if LOG_ERROR_ENABLED == 1
		static constexpr auto format = CreateFormatStr(0, 0, args...);
		static constexpr std::string_view fmt = format.view();
		Log(Format<C>(fmt, "\033[0;31m[ERROR]:\033[0m ", Forward<Args>(args)..., '\n'));
#endif
	}
	template<typename... Args> static void Fatal(Args&&... args)
	{
#if LOG_FATAL_ENABLED == 1
		static constexpr auto format = CreateFormatStr(0, 0, args...);
		static constexpr std::string_view fmt = format.view();
		Log(Format<C>(fmt, "\033[0;41m[FATAL]:\033[0m ", Forward<Args>(args)..., '\n'));
#endif
	}

private:
	static bool Initialize();
	static void Shutdown();

	static void Log(const String& format);
	static void Flush();

	static void* hConsole;
	static C* buffer;
	static U64 bufferOffset;
	static U64 fileOffset;

	static SpinLock lock;

	friend class Nihility;

	STATIC_CLASS(Logger);
};