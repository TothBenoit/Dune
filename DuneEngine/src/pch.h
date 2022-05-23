#pragma once

#ifdef DUNE_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#pragma comment(lib,"d3d12.lib") 
#pragma comment(lib,"d3dcompiler.lib") 
#pragma comment(lib,"dxgi.lib") 
#pragma comment(lib,"dxguid.lib")

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

#define ThrowIfFailed(hr) if(FAILED(hr)) { __debugbreak(); assert(false);}
#endif // DUNE_PLATFORM_WINDOWS

#define Assert(condition) if (!(condition)) {__debugbreak(); assert(false);} 

#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <memory>
#include <assert.h>
#include <type_traits>
#include "imgui/imgui.h"
#include "Dune/Common/Types.h"
#include "Dune/Core/ECS/ID.h"

#define MAX_ENTITIES 4096