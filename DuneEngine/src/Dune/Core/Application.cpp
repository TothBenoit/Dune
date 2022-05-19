#include "pch.h"

#include "Dune/Core/Application.h"
#include "Dune/Core/EngineCore.h"
#include "Dune/Graphics/GraphicsCore.h"
#include "Dune/Core/Logger.h"
#include "Dune/Core/Window.h"
#include "Dune/Core/Input.h"


namespace Dune
{
	Application::Application()
	{

	}

	Application::~Application()
	{

	}

	void Application::Start()
	{
		std::unique_ptr<Window> window = Window::Create();
		EngineCore::Init();
		GraphicsCore::Init(window.get());

		//TODO : Handle ImGui in its own layer
		// Setup Dear ImGui style
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGui::StyleColorsDark();

		auto lastFrameTimer = std::chrono::high_resolution_clock::now();

		while (window->Update())
		{
			auto timer = std::chrono::high_resolution_clock::now();
			float dt = (float)std::chrono::duration<float>(timer - lastFrameTimer).count();
			lastFrameTimer = std::chrono::high_resolution_clock::now();

			OnUpdate(dt);
			EngineCore::Update(dt);
			GraphicsCore::Update(dt);
			Input::EndFrame();
		};

		GraphicsCore::Shutdown();
		EngineCore::Shutdown();
		ImGui::DestroyContext();

	}
}

