#include "pch.h"
#include "GraphicsCore.h"

namespace Dune
{

	std::unique_ptr<GraphicsDevice> GraphicsCore::m_GraphicsDevice;

	void GraphicsCore::Init()
	{
		m_GraphicsDevice = GraphicsDevice::Create();
	}

	void GraphicsCore::Shutdown()
	{
	}

	void GraphicsCore::Render()
	{
	}
}