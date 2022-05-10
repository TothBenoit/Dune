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
		ImGui::StyleColorsDark();

		while (window->Update())
		{
			ImGui::ShowDemoWindow(nullptr);
			OnUpdate();
			EngineCore::Update();
			GraphicsCore::Update();
			Input::EndFrame();
		};

		GraphicsCore::Shutdown();
		ImGui::DestroyContext();

	}
}

