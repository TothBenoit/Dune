#include "pch.h"
#include "GraphicsRenderer.h"

#ifdef DUNE_PLATFORM_WINDOWS
#include "Platform/DirectX12/DX12GraphicsRenderer.h"
#include "Platform/Windows/WindowsWindow.h"
#endif

namespace Dune
{
	std::unique_ptr<GraphicsRenderer> GraphicsRenderer::Create(const Window * window)
	{
#ifdef DUNE_PLATFORM_WINDOWS
		return std::make_unique<DX12GraphicsRenderer>(static_cast<const WindowsWindow*>(window));
#else
#error Platform not supported
#endif
	}
	void GraphicsRenderer::AddGraphicsElement(EntityID id, const GraphicsElement& elem)
	{
		m_graphicsElements.insert({ id, elem });
	}
}