#include "pch.h"

#include "Dune/Core/Application.h"
#include "Dune/Core/EngineCore.h"
#include "Dune/Core/Logger.h"
#include "Dune/Core/Window.h"
#include "Dune/Core/Input.h"


namespace Dune
{
	void Application::Start()
	{
#ifdef PROFILE_ENABLED
		Remotery* rmt;
		rmt_CreateGlobalInstance(&rmt);
#endif

		std::unique_ptr<Window> window = Window::Create();
		EngineCore::Init(window.get());

		//TODO : Handle ImGui in its own layer
		// Setup Dear ImGui style
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGui::StyleColorsDark();

		auto lastFrameTimer = std::chrono::high_resolution_clock::now();

		while (window->Update())
		{
			Profile(Frame);

			auto timer = std::chrono::high_resolution_clock::now();
			float dt = (float)std::chrono::duration<float>(timer - lastFrameTimer).count();
			lastFrameTimer = std::chrono::high_resolution_clock::now();

			OnUpdate(dt);
			EngineCore::Update(dt);
			Input::EndFrame();
		};

		EngineCore::Shutdown();
		ImGui::DestroyContext();

#ifdef PROFILE_ENABLED
		rmt_DestroyGlobalInstance(rmt);
#endif
	}
}

