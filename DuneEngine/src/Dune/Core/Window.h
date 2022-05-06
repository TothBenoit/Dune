#pragma once

namespace Dune
{
	class Window
	{
	public:

		struct WindowData
		{
			dU32 m_width;
			dU32 m_height;

			dString m_title;

			WindowData(dU32 width = 1600, dU32 height = 900, const dString & = "Dune engine");
		};

		virtual ~Window() = default;

		virtual dU32 GetWidth() const = 0;
		virtual dU32 GetHeight() const = 0;
		virtual bool Update() = 0;

		static std::unique_ptr<Window> Create(WindowData data = WindowData());

	protected:
		WindowData m_data;
	};

}

