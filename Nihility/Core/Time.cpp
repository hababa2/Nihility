#include "Time.hpp"

#include <time.h>

#if defined(NH_PLATFORM_WINDOWS)
#include "Platform/WindowsInclude.hpp"
#endif

F64 Time::clockFrequency = ClockFrequency();
F64 Time::programStart = ProgramStart();
F64 Time::frameEndTime;
F64 Time::delta;
F64 Time::frameTimer;
U32 Time::frameRate;
U32 Time::frameCounter;

bool Time::Initialize()
{
	frameEndTime = programStart;
	delta = 0.0;
	frameTimer = 0.0;
	frameRate = 0;
	frameCounter = 0;

	return true;
}

void Time::Shutdown()
{

}

void Time::Update()
{
	F64 now = AbsoluteTime();

	delta = now - frameEndTime;
	frameEndTime = now;
	frameTimer += delta;
	++frameCounter;

	if (frameTimer >= 1.0)
	{
		frameTimer -= 1.0;
		frameRate = frameCounter;
		frameCounter = 0;
	}
}

const F64& Time::DeltaTime() { return delta; }
const F64& Time::FrameEndTime() { return frameEndTime; }
const U32& Time::FrameRate() { return frameRate; }
F64 Time::UpTime() { return AbsoluteTime() - programStart; }
F64 Time::FrameUpTime() { return AbsoluteTime() - frameEndTime; }

F64 Time::AbsoluteTime()
{
#if defined(NH_PLATFORM_WINDOWS)
	LARGE_INTEGER nowTime;
	QueryPerformanceCounter(&nowTime);

	return (F64)nowTime.QuadPart * clockFrequency;
#endif
}

U64 Time::SecondsSinceEpoch()
{
	return _time64(nullptr);
}

I64 Time::CoreCounter()
{
#if defined(NH_PLATFORM_WINDOWS)
	LARGE_INTEGER nowTime;
	QueryPerformanceCounter(&nowTime);

	return nowTime.QuadPart;
#endif
}

F64 Time::ClockFrequency()
{
#if defined(NH_PLATFORM_WINDOWS)
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	return 1.0 / (F64)frequency.QuadPart;
#endif
}

F64 Time::ProgramStart()
{
#if defined(NH_PLATFORM_WINDOWS)
	LARGE_INTEGER startTime;
	QueryPerformanceCounter(&startTime);

	return (F64)startTime.QuadPart * clockFrequency;
#endif
}

Timer::Timer() : start{ 0.0 }, elapsedTime{ 0.0 }, running{ false } {}

void Timer::Start()
{
	if (!running) { start = Time::AbsoluteTime(); }
	running = true;
}

void Timer::Stop()
{
	if (running) { elapsedTime += Time::AbsoluteTime() - start; }
	running = false;
}

void Timer::Reset()
{
	start = 0.0;
	elapsedTime = 0.0;
	running = false;
}

void Timer::Restart()
{
	start = Time::AbsoluteTime();
	elapsedTime = 0.0;
	running = true;
}

F64 Timer::CurrentTime() const
{
	if (running) { return elapsedTime + Time::AbsoluteTime() - start; }

	return elapsedTime;
}