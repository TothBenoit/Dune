#include "pch.h"
#include "Window.h"

#ifdef DUNE_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsWindow.h"
#endif // DUNE_PLATFORM_WINDOWS


namespace Dune
{
	std::unique_ptr<Window> Window::Create(WindowData data)
	{
#ifdef DUNE_PLATFORM_WINDOWS
		return std::make_unique<WindowsWindow>(data);
#else
#error Platform not supported
#endif
	}
	Window::WindowData::WindowData(uint32_t width, uint32_t height, const std::string& title)
		: m_width(width), m_height(height), m_title(title)
	{
	}
}

