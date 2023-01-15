#pragma once

namespace Dune
{
	class Window
	{
	public:
		~Window() = default;
		DISABLE_COPY_AND_MOVE(Window);

		[[nodiscard]] inline dU32 GetWidth() const{ return m_width;}
		[[nodiscard]] inline dU32 GetHeight() const { return m_height; }

		bool Update();

		static std::unique_ptr<Window> Create();
		HWND		GetHandle()	const;
	
	private:
		Window();

	private:
		HWND m_handle = NULL;
		dU32 m_width{1600};
		dU32 m_height{900};
		dString m_title{ "Dune Engine" };
	};

}

