#include "pch.h"
#include "GraphicsCore.h"
#include "Dune/Core/Logger.h"

namespace Dune
{

	std::unique_ptr<GraphicsRenderer> GraphicsCore::m_graphicsRenderer = nullptr;
	bool GraphicsCore::isInitialized = false;

	void GraphicsCore::Init(const Window* window)
	{
		if (IsInitialized())
		{
			LOG_CRITICAL("Tried to initialize GraphicsCore which is already initialized");
			return;
		}

		m_graphicsRenderer = GraphicsRenderer::Create(window);
		isInitialized = true;
	}

	void GraphicsCore::Shutdown()
	{
#ifdef _DEBUG
		if (!IsInitialized())
		{
			LOG_CRITICAL("Tried to shutdown GraphicsCore which is not initialized");
			return;
		}
#endif // _DEBUG

		m_graphicsRenderer->OnShutdown();
	}

	void GraphicsCore::Update(float dt)
	{
#ifdef _DEBUG
		if (!IsInitialized())
		{
			LOG_CRITICAL("Tried to update GraphicsCore which is not initialized");
			return;
		}
#endif // _DEBUG

		m_graphicsRenderer->WaitForGPU();
		m_graphicsRenderer->Render();
		m_graphicsRenderer->Present();
	}
}