#include "pch.h"

#include "Dune/Core/Application.h"
#include "Dune/Graphics/GraphicsCore.h"
#include "Dune/Core/Logger.h"
#include "Dune/Core/Window.h"

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
		GraphicsCore::Init();

		m_Running = true;

		while (m_Running)
		{
			window->Update();
			GraphicsCore::Render();
		};

		GraphicsCore::Shutdown();
	}
}

