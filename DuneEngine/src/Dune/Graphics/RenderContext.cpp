#include "pch.h"
#include "Dune/Graphics/RenderContext.h"

namespace Dune::Graphics
{
	void RenderContext::Initialize()
	{
		m_device.Initialize();
		m_resourceManager.Initialize(m_device);
	}

	void RenderContext::Destroy()
	{
		m_resourceManager.Destroy();
		m_device.Destroy();
	}
}
