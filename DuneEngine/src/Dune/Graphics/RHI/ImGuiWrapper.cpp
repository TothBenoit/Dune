#include "pch.h"
#include "Dune/Graphics/RHI/ImGuiWrapper.h"
#include <mutex>
#include <imgui/imgui.h>

namespace Dune::Graphics
{
	std::recursive_mutex g_mutex{};

	void ImGuiWrapper::Lock()
	{
		g_mutex.lock();
		ImGui::SetCurrentContext(m_pContext);
	}

	void ImGuiWrapper::Unlock()
	{
		g_mutex.unlock();
	}
}