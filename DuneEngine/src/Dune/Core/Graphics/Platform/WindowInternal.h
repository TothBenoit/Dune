#pragma once

#include "Dune/Core/Graphics/Window.h"

namespace Dune::Graphics
{
	class WindowInternal : public Window
	{
	public:
		void Initialize(const WindowDesc& desc);
		void Destroy();

		[[nodiscard]] void* GetHandle() { return m_pHandle; }

		[[nodiscard]] bool Update();
		void WindowProc(dUInt uMsg, void* wParam, void* lParam);
		void SetOnResizeFunc(void* pData, void (*pOnResize)(void*)) { m_pOnResize = pOnResize;  m_pOnResizeData = pData; };

	private:
		void* m_pHandle{ nullptr };
		void (*m_pOnResize)(void*) { nullptr };
		void* m_pOnResizeData{ nullptr };
		bool m_bClosing{ false };
		dWString m_title;
	};
}