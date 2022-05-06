#pragma once

#ifdef DUNE_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

#define ThrowIfFailed(hr) if(FAILED(hr)) { __debugbreak(); assert(false);}
#endif // DUNE_PLATFORM_WINDOWS

#include <iostream>
#include <memory>
#include <assert.h>
#include "imgui/imgui.h"
#include "Dune/Common/Types.h"
#include "Dune/Core/ECS/ID.h"

