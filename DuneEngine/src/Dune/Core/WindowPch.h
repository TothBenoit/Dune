#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include "imgui/imgui_impl_win32.h"

#define ThrowIfFailed(hr) if(FAILED(hr)) { __debugbreak(); assert(false);}

#ifdef PROFILE_ENABLED
	#include "Remotery/Remotery.h"
	#define Profile(msg) rmt_ScopedCPUSample(msg,0)
#endif // PROFILE_ENABLED
