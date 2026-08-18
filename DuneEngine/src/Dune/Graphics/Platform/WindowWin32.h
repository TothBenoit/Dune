#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace Dune::Graphics
{
	struct Win32NativeEvent
	{
		HWND hwnd;
		UINT uMsg;
		WPARAM wParam;
		LPARAM lParam;
	};
}
