#include "pch.h"
#include "Dune/Graphics/Window.h"
#include "WindowWin32.h"

namespace Dune::Graphics
{
	static const KeyCode g_win32ToKeyCode[256] =
	{
		KeyCode::None,               // 0x00
		KeyCode::LButton,            // 0x01
		KeyCode::RButton,            // 0x02
		KeyCode::Cancel,             // 0x03
		KeyCode::MButton,            // 0x04
		KeyCode::XButton1,           // 0x05
		KeyCode::XButton2,           // 0x06
		KeyCode::None,               // 0x07 - undefined
		KeyCode::Back,               // 0x08
		KeyCode::Tab,                // 0x09
		KeyCode::None,               // 0x0A - reserved
		KeyCode::None,               // 0x0B - reserved
		KeyCode::Clear,              // 0x0C
		KeyCode::Enter,              // 0x0D
		KeyCode::None,               // 0x0E - undefined
		KeyCode::None,               // 0x0F - undefined
		KeyCode::ShiftKey,           // 0x10
		KeyCode::ControlKey,         // 0x11
		KeyCode::AltKey,             // 0x12
		KeyCode::Pause,              // 0x13
		KeyCode::CapsLock,           // 0x14
		KeyCode::HangulMode,         // 0x15 (KanaMode/HangulMode)
		KeyCode::None,               // 0x16 - undefined
		KeyCode::JunjaMode,          // 0x17
		KeyCode::FinalMode,          // 0x18
		KeyCode::HanjaMode,          // 0x19 (HanjaMode/KanjiMode)
		KeyCode::None,               // 0x1A - undefined
		KeyCode::Escape,             // 0x1B
		KeyCode::IMEConvert,         // 0x1C
		KeyCode::IMINoConvert,       // 0x1D
		KeyCode::IMEAccept,          // 0x1E
		KeyCode::IMIModeChange,      // 0x1F
		KeyCode::Space,              // 0x20
		KeyCode::PageUp,             // 0x21 (Prior)
		KeyCode::PageDown,           // 0x22 (Next)
		KeyCode::End,                // 0x23
		KeyCode::Home,               // 0x24
		KeyCode::Left,               // 0x25
		KeyCode::Up,                 // 0x26
		KeyCode::Right,              // 0x27
		KeyCode::Down,               // 0x28
		KeyCode::Select,             // 0x29
		KeyCode::Print,              // 0x2A
		KeyCode::Execute,            // 0x2B
		KeyCode::PrintScreen,        // 0x2C (Snapshot)
		KeyCode::Insert,             // 0x2D
		KeyCode::Delete,             // 0x2E
		KeyCode::Help,               // 0x2F
		KeyCode::Num0,               // 0x30
		KeyCode::Num1,               // 0x31
		KeyCode::Num2,               // 0x32
		KeyCode::Num3,               // 0x33
		KeyCode::Num4,               // 0x34
		KeyCode::Num5,               // 0x35
		KeyCode::Num6,               // 0x36
		KeyCode::Num7,               // 0x37
		KeyCode::Num8,               // 0x38
		KeyCode::Num9,               // 0x39
		KeyCode::None,               // 0x3A - undefined
		KeyCode::None,               // 0x3B - undefined
		KeyCode::None,               // 0x3C - undefined
		KeyCode::None,               // 0x3D - undefined
		KeyCode::None,               // 0x3E - undefined
		KeyCode::None,               // 0x3F - undefined
		KeyCode::None,               // 0x40 - undefined
		KeyCode::A,                  // 0x41
		KeyCode::B,                  // 0x42
		KeyCode::C,                  // 0x43
		KeyCode::D,                  // 0x44
		KeyCode::E,                  // 0x45
		KeyCode::F,                  // 0x46
		KeyCode::G,                  // 0x47
		KeyCode::H,                  // 0x48
		KeyCode::I,                  // 0x49
		KeyCode::J,                  // 0x4A
		KeyCode::K,                  // 0x4B
		KeyCode::L,                  // 0x4C
		KeyCode::M,                  // 0x4D
		KeyCode::N,                  // 0x4E
		KeyCode::O,                  // 0x4F
		KeyCode::P,                  // 0x50
		KeyCode::Q,                  // 0x51
		KeyCode::R,                  // 0x52
		KeyCode::S,                  // 0x53
		KeyCode::T,                  // 0x54
		KeyCode::U,                  // 0x55
		KeyCode::V,                  // 0x56
		KeyCode::W,                  // 0x57
		KeyCode::X,                  // 0x58
		KeyCode::Y,                  // 0x59
		KeyCode::Z,                  // 0x5A
		KeyCode::LWin,               // 0x5B
		KeyCode::RWin,               // 0x5C
		KeyCode::Apps,               // 0x5D
		KeyCode::None,               // 0x5E - reserved
		KeyCode::Sleep,              // 0x5F
		KeyCode::NumPad0,            // 0x60
		KeyCode::NumPad1,            // 0x61
		KeyCode::NumPad2,            // 0x62
		KeyCode::NumPad3,            // 0x63
		KeyCode::NumPad4,            // 0x64
		KeyCode::NumPad5,            // 0x65
		KeyCode::NumPad6,            // 0x66
		KeyCode::NumPad7,            // 0x67
		KeyCode::NumPad8,            // 0x68
		KeyCode::NumPad9,            // 0x69
		KeyCode::Multiply,           // 0x6A
		KeyCode::Add,                // 0x6B
		KeyCode::Separator,          // 0x6C
		KeyCode::Subtract,           // 0x6D
		KeyCode::Decimal,            // 0x6E
		KeyCode::Divide,             // 0x6F
		KeyCode::F1,                 // 0x70
		KeyCode::F2,                 // 0x71
		KeyCode::F3,                 // 0x72
		KeyCode::F4,                 // 0x73
		KeyCode::F5,                 // 0x74
		KeyCode::F6,                 // 0x75
		KeyCode::F7,                 // 0x76
		KeyCode::F8,                 // 0x77
		KeyCode::F9,                 // 0x78
		KeyCode::F10,                // 0x79
		KeyCode::F11,                // 0x7A
		KeyCode::F12,                // 0x7B
		KeyCode::F13,                // 0x7C
		KeyCode::F14,                // 0x7D
		KeyCode::F15,                // 0x7E
		KeyCode::F16,                // 0x7F
		KeyCode::F17,                // 0x80
		KeyCode::F18,                // 0x81
		KeyCode::F19,                // 0x82
		KeyCode::F20,                // 0x83
		KeyCode::F21,                // 0x84
		KeyCode::F22,                // 0x85
		KeyCode::F23,                // 0x86
		KeyCode::F24,                // 0x87
		KeyCode::None,               // 0x88 - unassigned
		KeyCode::None,               // 0x89 - unassigned
		KeyCode::None,               // 0x8A - unassigned
		KeyCode::None,               // 0x8B - unassigned
		KeyCode::None,               // 0x8C - unassigned
		KeyCode::None,               // 0x8D - unassigned
		KeyCode::None,               // 0x8E - unassigned
		KeyCode::None,               // 0x8F - unassigned
		KeyCode::NumLock,            // 0x90
		KeyCode::Scroll,             // 0x91
		KeyCode::None,               // 0x92 - OEM specific
		KeyCode::None,               // 0x93 - OEM specific
		KeyCode::None,               // 0x94 - OEM specific
		KeyCode::None,               // 0x95 - OEM specific
		KeyCode::None,               // 0x96 - OEM specific
		KeyCode::None,               // 0x97 - unassigned
		KeyCode::None,               // 0x98 - unassigned
		KeyCode::None,               // 0x99 - unassigned
		KeyCode::None,               // 0x9A - unassigned
		KeyCode::None,               // 0x9B - unassigned
		KeyCode::None,               // 0x9C - unassigned
		KeyCode::None,               // 0x9D - unassigned
		KeyCode::None,               // 0x9E - unassigned
		KeyCode::None,               // 0x9F - unassigned
		KeyCode::LShiftKey,          // 0xA0
		KeyCode::RShiftKey,          // 0xA1
		KeyCode::LControlKey,        // 0xA2
		KeyCode::RControlKey,        // 0xA3
		KeyCode::LMenu,              // 0xA4
		KeyCode::RMenu,              // 0xA5
		KeyCode::BrowserBack,        // 0xA6
		KeyCode::BrowserForward,     // 0xA7
		KeyCode::BrowserRefresh,     // 0xA8
		KeyCode::BrowserStop,        // 0xA9
		KeyCode::BrowserSearch,      // 0xAA
		KeyCode::BrowserFavorites,   // 0xAB
		KeyCode::BrowserHome,        // 0xAC
		KeyCode::VolumeMute,         // 0xAD
		KeyCode::VolumeDown,         // 0xAE
		KeyCode::VolumeUp,           // 0xAF
		KeyCode::MediaNextTrack,     // 0xB0
		KeyCode::MediaPreviousTrack, // 0xB1
		KeyCode::MediaStop,          // 0xB2
		KeyCode::MediaPlayPause,     // 0xB3
		KeyCode::LaunchMail,         // 0xB4
		KeyCode::SelectMedia,        // 0xB5
		KeyCode::LaunchApplication1, // 0xB6
		KeyCode::LaunchApplication2, // 0xB7
		KeyCode::None,               // 0xB8 - reserved
		KeyCode::None,               // 0xB9 - reserved
		KeyCode::OemSemicolon,       // 0xBA (Oem1)
		KeyCode::OemPlus,            // 0xBB
		KeyCode::OemComma,           // 0xBC
		KeyCode::OemMinus,           // 0xBD
		KeyCode::OemPeriod,          // 0xBE
		KeyCode::OemQuestion,        // 0xBF (Oem2)
		KeyCode::OemTilde,           // 0xC0 (Oem3)
		KeyCode::None,               // 0xC1 - reserved
		KeyCode::None,               // 0xC2 - reserved
		KeyCode::None,               // 0xC3 - reserved
		KeyCode::None,               // 0xC4 - reserved
		KeyCode::None,               // 0xC5 - reserved
		KeyCode::None,               // 0xC6 - reserved
		KeyCode::None,               // 0xC7 - reserved
		KeyCode::None,               // 0xC8 - reserved
		KeyCode::None,               // 0xC9 - reserved
		KeyCode::None,               // 0xCA - reserved
		KeyCode::None,               // 0xCB - reserved
		KeyCode::None,               // 0xCC - reserved
		KeyCode::None,               // 0xCD - reserved
		KeyCode::None,               // 0xCE - reserved
		KeyCode::None,               // 0xCF - reserved
		KeyCode::None,               // 0xD0 - reserved
		KeyCode::None,               // 0xD1 - reserved
		KeyCode::None,               // 0xD2 - reserved
		KeyCode::None,               // 0xD3 - reserved
		KeyCode::None,               // 0xD4 - reserved
		KeyCode::None,               // 0xD5 - reserved
		KeyCode::None,               // 0xD6 - reserved
		KeyCode::None,               // 0xD7 - reserved
		KeyCode::None,               // 0xD8 - unassigned
		KeyCode::None,               // 0xD9 - unassigned
		KeyCode::None,               // 0xDA - unassigned
		KeyCode::OemOpenBrackets,    // 0xDB (Oem4)
		KeyCode::OemPipe,            // 0xDC (Oem5)
		KeyCode::OemCloseBrackets,   // 0xDD (Oem6)
		KeyCode::OemQuotes,          // 0xDE (Oem7)
		KeyCode::Oem8,               // 0xDF
		KeyCode::None,               // 0xE0 - reserved
		KeyCode::None,               // 0xE1 - OEM specific
		KeyCode::OemBackslash,       // 0xE2 (Oem102)
		KeyCode::None,               // 0xE3 - OEM specific
		KeyCode::None,               // 0xE4 - OEM specific
		KeyCode::ProcessKey,         // 0xE5
		KeyCode::None,               // 0xE6 - OEM specific
		KeyCode::Packet,             // 0xE7
		KeyCode::None,               // 0xE8 - unassigned
		KeyCode::None,               // 0xE9 - OEM specific
		KeyCode::None,               // 0xEA - OEM specific
		KeyCode::None,               // 0xEB - OEM specific
		KeyCode::None,               // 0xEC - OEM specific
		KeyCode::None,               // 0xED - OEM specific
		KeyCode::None,               // 0xEE - OEM specific
		KeyCode::None,               // 0xEF - OEM specific
		KeyCode::None,               // 0xF0 - OEM specific
		KeyCode::None,               // 0xF1 - OEM specific
		KeyCode::None,               // 0xF2 - OEM specific
		KeyCode::None,               // 0xF3 - OEM specific
		KeyCode::None,               // 0xF4 - OEM specific
		KeyCode::None,               // 0xF5 - OEM specific
		KeyCode::Attn,               // 0xF6
		KeyCode::CrSel,              // 0xF7
		KeyCode::ExSel,              // 0xF8
		KeyCode::EraseEof,           // 0xF9
		KeyCode::Play,               // 0xFA
		KeyCode::Zoom,               // 0xFB
		KeyCode::NoName,             // 0xFC
		KeyCode::Pa1,                // 0xFD
		KeyCode::OemClear,           // 0xFE
		KeyCode::None,               // 0xFF - reserved
	};


	LRESULT CALLBACK InternalWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (Window* pWindow{ (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA) })
		{
			Win32NativeEvent nativeEvent{ hwnd, uMsg, wParam, lParam };
			if (pWindow->InvokeHooks(&nativeEvent))
				return true;
			pWindow->WindowNativeHook(&nativeEvent);
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

	void Window::WindowNativeHook(void* pNativeEvent)
	{
		const Win32NativeEvent& nativeEvent = *(const Win32NativeEvent*)pNativeEvent;
		switch (nativeEvent.uMsg)
		{
		case WM_DESTROY:
			m_bClosing = true;
			OnEvent(EWindowMessageType::Close, {});
			break;
		case WM_SIZE:
			m_width = LOWORD(nativeEvent.lParam);
			m_width = (m_width == 0) ? 1 : m_width;
			m_height = HIWORD(nativeEvent.lParam);
			m_height = (m_height == 0) ? 1 : m_height;
			OnEvent(EWindowMessageType::Resize, { .resize = { m_width, m_height } });
			break;
		case WM_PAINT:
			OnEvent(EWindowMessageType::Paint, {});
			break;
		case WM_KILLFOCUS:
			m_input.ClearInput();
			break;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			m_input.SetKeyDown(g_win32ToKeyCode[nativeEvent.wParam]);
			break;
		case WM_SYSKEYUP:
		case WM_KEYUP:
			m_input.SetKeyUp(g_win32ToKeyCode[nativeEvent.wParam]);
			break;
		case WM_MOUSEWHEEL:
			m_input.SetMouseWheelDelta(GET_WHEEL_DELTA_WPARAM(nativeEvent.wParam));
			break;
		case WM_MOUSEMOVE:
			m_input.SetMousePosX((short)LOWORD(nativeEvent.lParam));
			m_input.SetMousePosY((short)HIWORD(nativeEvent.lParam));
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
