#pragma once

#include "Defines.hpp"

#ifdef NH_EXPORT
#ifdef NH_PLATFORM_WINDOWS

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Windows.h"

#endif
#endif