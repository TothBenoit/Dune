#include "pch.h"
#include "WindowsWindow.h"
#include "Dune/Core/Logger.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

Dune::WindowsWindow::WindowsWindow(WindowData data)
{
	m_data = data;
	const std::string& title = m_data.m_title;
	const int wTitleSize = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, NULL, 0);
	std::wstring wTitle(wTitleSize, 0);
	MultiByteToWideChar(CP_UTF8, 0, title.c_str(), (int)title.size(), &wTitle[0], wTitleSize);

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
		CW_USEDEFAULT, CW_USEDEFAULT, m_data.m_width, m_data.m_height,

		NULL,       // Parent window    
		NULL,       // Menu
		hInstance,  // Instance handle
		NULL        // Additional application data
	);

	assert(m_handle != NULL);

	ShowWindow(m_handle, 1);
}

bool Dune::WindowsWindow::Update()
{
	MSG msg = { };
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_QUIT)
			return false;
	}
	return true;
}


uint32_t Dune::WindowsWindow::GetWidth() const
{
	return m_data.m_width;
}

uint32_t Dune::WindowsWindow::GetHeight() const
{
	return m_data.m_height;
}

HWND Dune::WindowsWindow::GetHandle() const
{
	return m_handle;
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		// All painting occurs here, between BeginPaint and EndPaint.

		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

		EndPaint(hwnd, &ps);
	}
	return 0;

	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
