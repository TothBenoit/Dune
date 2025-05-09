#pragma once

#include "Dune/Core/Input.h"

namespace Dune::Graphics
{
	struct WindowDesc
	{
		void* parent{ nullptr };
		dU32 width{ 1600 };
		dU32 height{ 900 };
		dWString title{ L"Dune Window" };
	};

	class Window
	{
		friend class ImGuiWrapper;
	public:
		void Initialize(const WindowDesc& desc);
		void Destroy();

		[[nodiscard]] bool Update();

		[[nodiscard]] void* GetHandle() { return m_pHandle; }
		[[nodiscard]] dU32  GetWidth() const { return m_width; }
		[[nodiscard]] dU32  GetHeight() const { return m_height; }
		[[nodiscard]] const Input& GetInput() const { return m_input; }

		void WindowProc(dUInt uMsg, void* wParam, void* lParam);
		void SetOnResizeFunc(void* pData, void (*pOnResize)(void*, dU32 width, dU32 height)) { m_pOnResize = pOnResize;  m_pOnResizeData = pData; };

		ImGuiWrapper* GetImGui() { return m_pImGui; }

	protected:
		Input m_input{};
		dU32 m_width;
		dU32 m_height;

		void* m_pHandle{ nullptr };
		void (*m_pOnResize)(void*, dU32 width, dU32 height) { nullptr };
		void* m_pOnResizeData{ nullptr };
		bool m_bClosing{ false };
		dWString m_title;

		ImGuiWrapper* m_pImGui{ nullptr };
	};


}