#include "pch.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Graphics/RHI/ImGuiWrapper.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <imgui/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Dune::Graphics
{
	LRESULT CALLBACK InternalWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (Window* pWindow{ (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA) })
		{
			if (ImGuiWrapper* pImGui{ pWindow->GetImGui() })
			{
				pImGui->Lock();
				if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) 
				{
					pImGui->Unlock();
					return true;
				}
				pImGui->Unlock();
			}
			pWindow->WindowProc(uMsg, (void*)wParam, (void*)lParam);
		}
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	void Window::Initialize(const WindowDesc& desc)
	{
		m_width = desc.width;
		m_height = desc.height;
		m_title = desc.title;

		WNDCLASSEX wc;
		ZeroMemory(&wc, sizeof(wc));
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = InternalWindowProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = sizeof(Window*);
		wc.hInstance = 0;
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
		wc.lpszMenuName = NULL;
		wc.lpszClassName = L"DuneWindow";
		wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

		RegisterClassEx(&wc);

		m_pHandle = CreateWindowEx(
			0,
			wc.lpszClassName,
			desc.title.c_str(),
			(desc.parent) ? WS_CHILDWINDOW : WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, desc.width, desc.height,
			(HWND)desc.parent,
			NULL,
			NULL,
			NULL
		);

		Assert(m_pHandle != NULL);
		SetWindowLongPtr((HWND)m_pHandle, GWLP_USERDATA, (LONG_PTR)this);
		ShowWindow((HWND)m_pHandle, 1);
	}

	void Window::Destroy()
	{
	}

	bool Window::Update()
	{
		MSG msg{};
		m_input.Update();
		while (PeekMessage(&msg, (HWND)m_pHandle, 0, 0, PM_REMOVE) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (m_bClosing)
				break;
		}
		return !m_bClosing;
	}

	void Window::WindowProc(dUInt uMsg, void* wParam, void* lParam)
	{
		switch (uMsg)
		{
		case WM_DESTROY:
			m_bClosing = true;
			break;
		case WM_SIZE:
			// Thought : Should Window::Update return flags like EResize, EDestroy, ..., instead of handling Resize here ? A current issue is that we resize multiple times during the same Update.
			m_width = LOWORD(lParam);
			m_width = (m_width == 0) ? 1 : m_width;
			m_height = HIWORD(lParam);
			m_height = (m_height == 0) ? 1 : m_height;
			if (m_pOnResize)
				m_pOnResize(m_pOnResizeData, m_width, m_height);
			break;
		case WM_PAINT:
			break;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			m_input.SetKeyDown(static_cast<KeyCode>((WPARAM)wParam));
			break;
		case WM_SYSKEYUP:
		case WM_KEYUP:
			m_input.SetKeyUp(static_cast<KeyCode>((WPARAM)wParam));
			break;
		case WM_MOUSEWHEEL:
			m_input.SetMouseWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam));
			break;
		case WM_MOUSEMOVE:
			m_input.SetMousePosX((short)LOWORD(lParam));
			m_input.SetMousePosY((short)HIWORD(lParam));
			break;
		case WM_LBUTTONDOWN:
			m_input.SetMouseButtonDown(0);
			break;
		case WM_LBUTTONUP:
			m_input.SetMouseButtonUp(0);
			break;
		case WM_MBUTTONDOWN:
			m_input.SetMouseButtonDown(1);
			break;
		case WM_MBUTTONUP:
			m_input.SetMouseButtonUp(1);
			break;
		case WM_RBUTTONDOWN:
			m_input.SetMouseButtonDown(2);
			break;
		case WM_RBUTTONUP:
			m_input.SetMouseButtonUp(2);
			break;
		}
	}
}