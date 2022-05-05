#include "pch.h"

#include "Dune/Core/Application.h"
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
		GraphicsCore::Init(window.get());

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		bool show_demo_window = true;
		while (window->Update())
		{
			if(show_demo_window)
				ImGui::ShowDemoWindow(&show_demo_window);
			GraphicsCore::Update();
			Input::EndFrame();
		};
		GraphicsCore::Shutdown();
		ImGui::DestroyContext();

	}
}

