#include "pch.h"
#include "Dune/Graphics/Window.h"

namespace Dune::Graphics
{
	dU32 Window::RegisterHook(void* pData, WindowHook pHook, dU32 priority)
	{
		dU32 handle = m_nextHookHandle++;
		HookEntry entry{ pData, pHook, priority, handle };
		auto it = std::upper_bound(m_hooks.begin(), m_hooks.end(), entry,
			[](const HookEntry& a, const HookEntry& b) { return a.priority > b.priority; });
		m_hooks.insert(it, entry);
		return handle;
	}

	void Window::UnregisterHook(dU32 handle)
	{
		auto it = std::find_if(m_hooks.begin(), m_hooks.end(), [handle](const HookEntry& e) { return e.handle == handle; });
		if (it != m_hooks.end())
			m_hooks.erase(it);
	}

	dU32 Window::RegisterEvent(void* pData, WindowEvent pEvent, EWindowMessageType type)
	{
		dU32 handle = m_nextEventHandle++;
		m_events[(dU32)type].push_back({ pData, pEvent, handle });
		return handle;
	}

	void Window::UnregisterEvent(EWindowMessageType msgType, dU32 handle)
	{
		dVector<EventEntry>& events = m_events[(dU32)msgType];
		dU32 eventCount = (dU32)events.size();
		for (dU32 i = 0; i < eventCount; i++)
		{
			const EventEntry& event = events[i];
			if (event.handle == handle)
			{
				if (i < eventCount - 1)
				{
					events[i] = events.back();
				}
				events.pop_back();
				break;
			}
		}
	}

	bool Window::InvokeHooks(void* pNativeEvent) const
	{
		for (const HookEntry& entry : m_hooks)
			if (entry.pHook(entry.pData, pNativeEvent))
				return true;
		return false;
	}

	void Window::OnEvent(EWindowMessageType type, const WindowMessage& message) const
	{
		const dVector<EventEntry>& events = m_events[(dU32)type];
		for (const EventEntry& entry : events)
			entry.pEvent(entry.pData, message);
	}
}
