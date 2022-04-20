#include "pch.h"

#include "Application.h"
#include "Logger.h"
#include "Window.h"

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

		while (true)
		{
			window->Update();
		};
	}
}

