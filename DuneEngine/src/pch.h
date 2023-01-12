#pragma once

// Disable exception warning
#pragma warning(disable:4530)

#ifdef _DEBUG
	//#define PROFILE_ENABLED
#endif

#ifdef DUNE_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsPch.h"
	#include "Platform/DirectX12/DX12Pch.h"
#endif // DUNE_PLATFORM_WINDOWS

#ifndef PROFILE_ENABLED
	#define Profile(msg)
#endif

#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <memory>
#include <assert.h>
#include <type_traits>
#include <thread>
#include <mutex>
#include <atomic>
#include "imgui/imgui.h"
#include "Dune/Common/Macro.h"
#include "Dune/Common/Types.h"
#include "Dune/Core/ECS/ID.h"