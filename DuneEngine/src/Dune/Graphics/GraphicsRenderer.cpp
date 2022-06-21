#include "pch.h"
#include "GraphicsRenderer.h"

#ifdef DUNE_PLATFORM_WINDOWS
#include "Platform/DirectX12/DX12GraphicsRenderer.h"
#include "Platform/Windows/WindowsWindow.h"
#endif
#include "Dune/Core/EngineCore.h"

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

	void GraphicsRenderer::ClearGraphicsElement()
	{
		m_graphicsElements.clear();
		m_lookup.clear();
		m_entities.clear();
	}

	void GraphicsRenderer::SubmitGraphicsElement(EntityID id, const GraphicsElement& elem)
	{
		Assert(id != ID::invalidID);

		auto it = m_lookup.find(id);
		if ( it != m_lookup.end())
		{
			dU32 index = (*it).second;
			m_graphicsElements[index] = elem;
			m_entities[index] = id;
		}
		else
		{
			m_graphicsElements.emplace_back(elem);
			m_entities.emplace_back(id);
			m_lookup[id] = m_graphicsElements.size() - 1;
		}
	}

	void GraphicsRenderer::UpdateCamera()
	{
		CameraComponent* camera = EngineCore::GetCamera();
		if (camera)
		{
			dMatrix viewProjMatrix = camera->viewMatrix * camera->projectionMatrix;
			UpdateBuffer(m_cameraMatrixBuffer, &viewProjMatrix);
		}
	}

	void GraphicsRenderer::RemoveGraphicsElement(EntityID id)
	{
		auto it = m_lookup.find(id);
		Assert(it != m_lookup.end());

		const dU32 index = (*it).second;
		const EntityID entity = m_entities[index];

		if (index < m_graphicsElements.size() - 1)
		{
			m_graphicsElements[index] = std::move(m_graphicsElements.back());
			m_entities[index] = m_entities.back();

			m_lookup[m_entities[index]] = index;
		}
		
		m_graphicsElements.pop_back();
		m_entities.pop_back();
		m_lookup.erase(id);
	}

}