#pragma once

#include "Dune/Core/Input.h"

namespace Dune::Graphics
{
	struct WindowMessageResize
	{
		dU32 width{ 0 };
		dU32 height{ 0 };
	};

	struct WindowMessage
	{
		union 
		{
			WindowMessageResize resize;
		};
	};

	enum class EWindowMessageType
	{
		Resize,
		Close,
		Paint,
		Count,
	};

	using WindowEvent = void (*)(void* pData, const WindowMessage& event);
	using WindowHook = bool (*)(void* pData, void* pNativeEvent);

	struct WindowDesc
	{
		void* parent{ nullptr };
		dU32 width{ 1600 };
		dU32 height{ 900 };
		dWString title{ L"Dune Window" };
	};

	class Window
	{
	public:
		void Initialize(const WindowDesc& desc);
		void Destroy();

		[[nodiscard]] bool Update();

		[[nodiscard]] void* GetHandle() { return m_pHandle; }
		[[nodiscard]] dU32  GetWidth() const { return m_width; }
		[[nodiscard]] dU32  GetHeight() const { return m_height; }
		[[nodiscard]] const Input& GetInput() const { return m_input; }

		[[nodiscard]] dU32 RegisterHook(void* pData, WindowHook pHook, dU32 priority);
		void UnregisterHook(dU32 handle);

		[[nodiscard]] dU32 RegisterEvent(void* pData, WindowEvent pEvent, EWindowMessageType type);
		void UnregisterEvent(EWindowMessageType type, dU32 handle);

		void WindowNativeHook(void* pNativeEvent);
		[[nodiscard]] bool InvokeHooks(void* pNativeEvent) const;
	private:
		void OnEvent(EWindowMessageType type, const WindowMessage&) const;

	private:

		struct HookEntry
		{
			void* pData;
			WindowHook pHook;
			dU32 priority;
			dU32 handle;
		};

		struct EventEntry
		{
			void* pData;
			WindowEvent pEvent;
			dU32 handle;
		};

		Input m_input{};
		dU32 m_width;
		dU32 m_height;

		void* m_pHandle{ nullptr };
		bool m_bClosing{ false };
		dWString m_title;

		dVector<HookEntry> m_hooks{};
		dVector<EventEntry> m_events[(dU32)EWindowMessageType::Count];
		dU32 m_nextHookHandle{ 0 };
		dU32 m_nextEventHandle{ 0 };
	};
}
