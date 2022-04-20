#pragma once
#include "Dune/Core/Window.h"

namespace Dune
{
	class WindowsWindow : public Window
	{
	public:

		WindowsWindow(WindowData data);

		void Update();

		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
	};
}

