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

		Window() = default;
		virtual ~Window() = default;
		DISABLE_COPY_AND_MOVE(Window);

		dU32 GetWidth() const
		{
			return m_data.m_width;
		}

		dU32 GetHeight() const
		{
			return m_data.m_height;
		}

		virtual bool Update() = 0;

		static std::unique_ptr<Window> Create(WindowData data = WindowData());

	protected:
		WindowData m_data;
	};

}

