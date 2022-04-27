#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <assert.h>

#ifdef DUNE_PLATFORM_WINDOWS
	#include <Windows.h>

#define ThrowIfFailed(hr) if(FAILED(hr)) { __debugbreak(); assert(false);}

#endif