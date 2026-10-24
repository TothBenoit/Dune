#include "pch.h"
#include "Dune/Platform/RHI/ImGuiContext.h"
#include <mutex>
#include <imgui/imgui.h>

namespace Dune::RHI
{
	std::recursive_mutex g_mutex{};

	void ImGuiContext::Lock()
	{
		g_mutex.lock();
		ImGui::SetCurrentContext(m_pContext);
	}

	void ImGuiContext::Unlock()
	{
		g_mutex.unlock();
	}
}
