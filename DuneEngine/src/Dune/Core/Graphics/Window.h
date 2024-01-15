#pragma once

namespace Dune
{
	class Input;
}

namespace Dune::Graphics
{
	struct WindowDesc;

	class Window
	{
	public:
		void Initialize(const WindowDesc& desc);
		void Destroy();

		[[nodiscard]] dU32	GetWidth() const { return m_width; }
		[[nodiscard]] dU32	GetHeight() const { return m_height; }
		[[nodiscard]] void* GetHandle() { return m_pHandle; }
		[[nodiscard]] const Input* GetInput() const { return m_pInput; }

		[[nodiscard]] bool Update();
		void WindowProc(dUInt uMsg, void* wParam, void* lParam);
		void SetOnResizeFunc(void* pData, void (*pOnResize)(void*)) { m_pOnResize = pOnResize;  m_pOnResizeData = pData; };

	private:
		Input* m_pInput{ nullptr };
		void* m_pHandle{ nullptr };
		void (*m_pOnResize)(void*) { nullptr };
		void* m_pOnResizeData{ nullptr };
		dU32 m_width;
		dU32 m_height;
		bool m_bClosing{ false };
		dWString m_title;
	};
}