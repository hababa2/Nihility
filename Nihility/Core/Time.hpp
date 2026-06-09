#pragma once

#include "Defines.hpp"

class NH_API Time
{
public:
	static const F64& DeltaTime();
	static const F64& FrameEndTime();
	static const U32& FrameRate();
	static F64 UpTime();
	static F64 FrameUpTime();

	static F64 AbsoluteTime();
	static U64 SecondsSinceEpoch();

	static I64 CoreCounter();

private:
	static bool Initialize();
	static void Shutdown();

	static void Update();

	static F64 ClockFrequency();

	static F64 ProgramStart();

	static F64 clockFrequency;
	static F64 programStart;
	static F64 frameEndTime;
	static F64 delta;
	static F64 frameTimer;
	static U32 frameRate;
	static U32 frameCounter;

	friend class Nihility;

	STATIC_CLASS(Time);
};

struct NH_API Timer
{
	Timer();

	void Start();
	void Stop();
	void Reset();
	void Restart();
	F64 CurrentTime() const;

private:
	F64 start;
	F64 elapsedTime;
	bool running;
};