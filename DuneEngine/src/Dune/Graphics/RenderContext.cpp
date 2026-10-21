#include "pch.h"
#include "Dune/Graphics/RenderContext.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/ForwardPipeline.h"

namespace Dune::Graphics
{
	void RenderContext::Initialize(const ResourceHandles& handles)
	{
		m_device.Initialize();
		m_commandQueue.Initialize(m_device, RHI::ECommandType::Direct);
		m_psoCache.Initialize(m_device);
		m_resourceManager.Initialize(m_device, handles);
		m_uploadContext.Initialize(m_device, m_commandQueue, {});
		m_fence.Initialize(m_device, 0);
	}

	void RenderContext::Destroy()
	{
		m_uploadContext.Destroy();
		m_fence.Destroy();
		m_resourceManager.Destroy();
		m_psoCache.Destroy();
		m_commandQueue.Destroy();
		m_device.Destroy();
	}

	void RenderContext::Signal()
	{
		m_commandQueue.Signal(m_fence, ++m_fenceValue);
	}

	void RenderContext::InitializeRenderer(Renderer& renderer, Window& window)
	{
		renderer.Initialize(m_device, m_commandQueue, m_psoCache, window);
		ForwardPipeline::Register(renderer);
	}
}
