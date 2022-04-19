#pragma once

#include <string>
#include <memory>

namespace Dune
{
	class Window
	{
	public:

		struct WindowData
		{
			uint32_t m_width;
			uint32_t m_height;

			std::string m_title;

			WindowData(uint32_t width = 1600, uint32_t height = 900, const std::string & = "Dune engine");
		};

		virtual ~Window() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		static std::unique_ptr<Window> Create(WindowData data = WindowData());

	protected:
		WindowData m_data;
	};

}

