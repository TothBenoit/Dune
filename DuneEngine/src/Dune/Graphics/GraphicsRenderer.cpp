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
		m_lookupEntityToIndex.clear();
		m_lookupIndexToEntity.clear();
	}

	void GraphicsRenderer::AddGraphicsElement(EntityID id, const GraphicsElement& elem)
	{
		auto it = m_lookupEntityToIndex.find(id);
		if ( it != m_lookupEntityToIndex.end())
		{
			dU32 index = (*it).second;
			m_graphicsElements[index] = elem;
			m_lookupIndexToEntity[index] = id;
		}
		else
		{
			m_graphicsElements.push_back(elem);
			m_lookupIndexToEntity.push_back(id);
			m_lookupEntityToIndex[id] = m_graphicsElements.size() - 1;
		}

		Assert(m_graphicsElements.size() == m_lookupIndexToEntity.size());
		Assert(m_graphicsElements.size() == m_lookupEntityToIndex.size())
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
		auto it = m_lookupEntityToIndex.find(id);
		Assert(it != m_lookupEntityToIndex.end());

		dU32 index = (*it).second;
		dU32 lastIndex = m_graphicsElements.size() - 1;

		m_graphicsElements[index] = m_graphicsElements[lastIndex];

		EntityID movedEntity = m_lookupEntityToIndex[lastIndex];
		m_lookupIndexToEntity[index] = movedEntity;
		m_lookupEntityToIndex[movedEntity] = index;
		
		m_graphicsElements.pop_back();
		m_lookupIndexToEntity.pop_back();
		m_lookupEntityToIndex.erase(id);

		Assert(m_graphicsElements.size() == m_lookupIndexToEntity.size());
		Assert(m_graphicsElements.size() == m_lookupEntityToIndex.size())
	}

}