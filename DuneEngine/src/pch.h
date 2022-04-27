#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <assert.h>
#include "imgui/imgui.h"

#ifdef DUNE_PLATFORM_WINDOWS
	#include <Windows.h>
	#include "imgui/imgui_impl_win32.h"
	#include "imgui/imgui_impl_dx12.h"
#define ThrowIfFailed(hr) if(FAILED(hr)) { __debugbreak(); assert(false);}

#endif