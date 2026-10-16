#include "pch.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/ForwardPipeline.h"

namespace Dune::Graphics
{
	void RenderContext::Initialize()
	{
		m_device.Initialize();
		m_commandQueue.Initialize(m_device, ECommandType::Direct);
		m_psoCache.Initialize(m_device);
		m_resourceManager.Initialize(m_device);
	}

	void RenderContext::Destroy()
	{
		m_resourceManager.Destroy();
		m_psoCache.Destroy();
		m_commandQueue.Destroy();
		m_device.Destroy();
	}

	void RenderContext::InitializeRenderer(Renderer& renderer, Window& window)
	{
		renderer.Initialize(m_device, m_commandQueue, m_psoCache, window);
		ForwardPipeline::Register(renderer);
	}
}
