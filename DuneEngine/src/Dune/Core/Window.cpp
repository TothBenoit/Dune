#include "pch.h"
#include "Window.h"
#include "Dune/Core/Logger.h"
#include "Dune/Core/Input.h"
#include "Dune/Graphics/Renderer.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace Dune
{
	std::unique_ptr<Window> Window::Create()
	{
		return std::unique_ptr<Window>(new Window());
	}

	Window::Window()
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		const int wTitleSize = MultiByteToWideChar(CP_UTF8, 0, m_title.c_str(), -1, NULL, 0);
		std::wstring wTitle(wTitleSize, 0);
		MultiByteToWideChar(CP_UTF8, 0, m_title.c_str(), (int)m_title.size(), &wTitle[0], wTitleSize);

		const wchar_t CLASS_NAME[] = L"Main Window class";

		// Register the window class.
		HINSTANCE hInstance = GetModuleHandle(NULL);
		WNDCLASS wc = { };

		wc.lpfnWndProc = WindowProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = CLASS_NAME;

		RegisterClass(&wc);

		// Create the window.

		m_handle = CreateWindowEx(
			0,                              // Optional window styles.
			CLASS_NAME,                     // Window class
			wTitle.c_str(),					// Window text
			WS_OVERLAPPEDWINDOW,            // Window style

			// Size and position
			CW_USEDEFAULT, CW_USEDEFAULT, m_width, m_height,

			NULL,       // Parent window    
			NULL,       // Menu
			hInstance,  // Instance handle
			NULL        // Additional application data
		);

		Assert(m_handle != NULL);

		ShowWindow(m_handle, 1);

		ImGui_ImplWin32_Init(m_handle);

	}

	bool Window::Update()
	{
		Profile("WindowUpdate");
		MSG msg = { };
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				return false;
		}
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		return true;
	}

	HWND Window::GetHandle() const
	{
		return m_handle;
	}
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
		return true;

	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		if (Dune::Renderer::GetInstance().IsInitialized() && wParam != SIZE_MINIMIZED)
		{
			Dune::Renderer::GetInstance().OnResize(LOWORD(lParam), HIWORD(lParam));
		}
		break;

	case WM_PAINT:
		break;
	case WM_KEYDOWN:
		Dune::Input::SetKeyDown(static_cast<Dune::KeyCode>(wParam));
		break;
	case WM_KEYUP:
		Dune::Input::SetKeyUp(static_cast<Dune::KeyCode>(wParam));
		break;
	case WM_MOUSEWHEEL:
		Dune::Input::SetMouseWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam));
		break;
	case WM_MOUSEMOVE:
		Dune::Input::SetMousePosX((short)LOWORD(lParam));
		Dune::Input::SetMousePosY((short)HIWORD(lParam));
		break;
	case WM_LBUTTONDOWN:
		Dune::Input::SetMouseButtonDown(0);
		break;
	case WM_LBUTTONUP:
		Dune::Input::SetMouseButtonUp(0);
		break;
	case WM_MBUTTONDOWN:
		Dune::Input::SetMouseButtonDown(1);
		break;
	case WM_MBUTTONUP:
		Dune::Input::SetMouseButtonUp(1);
		break;
	case WM_RBUTTONDOWN:
		Dune::Input::SetMouseButtonDown(2);
		break;
	case WM_RBUTTONUP:
		Dune::Input::SetMouseButtonUp(2);
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

