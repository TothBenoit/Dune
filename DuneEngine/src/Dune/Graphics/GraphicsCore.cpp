#include "pch.h"
#include "GraphicsCore.h"

namespace Dune
{

	std::unique_ptr<GraphicsRenderer> GraphicsCore::m_graphicsRenderer;

	void GraphicsCore::Init(const void* window)
	{
		m_graphicsRenderer = GraphicsRenderer::Create(window);
	}

	void GraphicsCore::Shutdown()
	{
		m_graphicsRenderer->OnShutdown();
	}

	void GraphicsCore::Update()
	{
		m_graphicsRenderer->WaitForPreviousFrame();
		m_graphicsRenderer->Render();
		m_graphicsRenderer->Present();
	}
}