#pragma once
#include "Dune/Core/Window.h"

namespace Dune
{
	class WindowsWindow : public Window
	{
	public:
		WindowsWindow(WindowData data);
		~WindowsWindow() override;
		
		bool Update();

		HWND		GetHandle()	const;
	private:

		HWND m_handle = NULL;
	};
}

