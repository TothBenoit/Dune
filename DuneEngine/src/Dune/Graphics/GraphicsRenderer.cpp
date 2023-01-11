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

	void GraphicsRenderer::ClearGraphicsElements()
	{
		m_graphicsElements.clear();
		m_lookupGraphicsElements.clear();
		m_graphicsEntities.clear();
	}

	void GraphicsRenderer::SubmitGraphicsElement(EntityID id, std::weak_ptr<Mesh> mesh, const InstanceData& instanceData)
	{
		Assert(id != ID::invalidID);

		auto it = m_lookupGraphicsElements.find(id);
		if ( it != m_lookupGraphicsElements.end())
		{
			dU32 index = (*it).second;
			m_graphicsElements[index].UpdateInstanceData(instanceData);
			m_graphicsEntities[index] = id;
		}
		else
		{
			m_lookupGraphicsElements[id] = (dU32) m_graphicsElements.size();
			m_graphicsElements.emplace_back(mesh.lock(), instanceData);
			m_graphicsEntities.emplace_back(id);
		}
	}

	void GraphicsRenderer::ClearPointLights()
	{
		m_pointLights.clear();
		m_pointLightEntities.clear();
		m_lookupPointLights.clear();
	}

	void GraphicsRenderer::RemovePointLight(EntityID id)
	{
		auto it = m_lookupPointLights.find(id);
		if (it == m_lookupPointLights.end())
			return;

		const dU32 index = (*it).second;
		const EntityID entity = m_pointLightEntities[index];

		if (index < m_pointLights.size() - 1)
		{
			m_pointLights[index] = std::move(m_pointLights.back());
			m_pointLightEntities[index] = m_pointLightEntities.back();

			m_lookupPointLights[m_pointLightEntities[index]] = index;
		}

		m_pointLights.pop_back();
		m_pointLightEntities.pop_back();
		m_lookupPointLights.erase(id);
	}

	void GraphicsRenderer::SubmitPointLight(EntityID id, const PointLight& light)
	{
		Assert(id != ID::invalidID);

		auto it = m_lookupPointLights.find(id);
		if (it != m_lookupPointLights.end())
		{
			dU32 index = (*it).second;
			m_pointLights[index] = light;
			m_pointLightEntities[index] = id;
		}
		else
		{
			m_lookupPointLights[id] = (dU32)m_pointLights.size();
			m_pointLights.emplace_back(light);
			m_pointLightEntities.emplace_back(id);
		}
	}

	void GraphicsRenderer::ClearDirectionalLights()
	{
		m_directionalLights.clear();
		m_directionalLightEntities.clear();
		m_lookupDirectionalLights.clear();
	}

	void GraphicsRenderer::RemoveDirectionalLight(EntityID id)
	{
		auto it = m_lookupDirectionalLights.find(id);
		if (it == m_lookupDirectionalLights.end())
			return;

		const dU32 index = (*it).second;
		const EntityID entity = m_directionalLightEntities[index];

		if (index < m_directionalLights.size() - 1)
		{
			m_directionalLights[index] = std::move(m_directionalLights.back());
			m_directionalLightEntities[index] = m_directionalLightEntities.back();

			m_lookupDirectionalLights[m_directionalLightEntities[index]] = index;
		}

		m_directionalLights.pop_back();
		m_directionalLightEntities.pop_back();
		m_lookupDirectionalLights.erase(id);
	}

	void GraphicsRenderer::SubmitDirectionalLight(EntityID id, const DirectionalLight& light)
	{
		Assert(id != ID::invalidID);

		auto it = m_lookupDirectionalLights.find(id);
		if (it != m_lookupDirectionalLights.end())
		{
			dU32 index = (*it).second;
			m_directionalLights[index] = light;
			m_directionalLightEntities[index] = id;
		}
		else
		{
			m_lookupDirectionalLights[id] = (dU32)m_directionalLights.size();
			m_directionalLights.emplace_back(light);
			m_directionalLightEntities.emplace_back(id);
		}
	}

	void GraphicsRenderer::CreateCamera()
	{
		dMatrix identity;
		GraphicsBufferDesc camBufferDesc{EBufferUsage::Upload};
		dU32 size{ sizeof(CameraConstantBuffer) };
		m_cameraMatrixBuffer = CreateBuffer(camBufferDesc, &identity, size);
	}

	void GraphicsRenderer::UpdateCamera()
	{
		const CameraComponent* camera = EngineCore::GetCamera();
		if (camera)
		{
			// TODO : Camera should be linked to a viewport (TODO : Make viewport)
			// TODO : Get viewport dimensions
			constexpr float aspectRatio = 1600.f / 900.f;
			dMatrix projectionMatrix{ DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(camera->verticalFieldOfView), aspectRatio, 1.f, 1000.0f) };
			dMatrix viewProjMatrix{ camera->viewMatrix * projectionMatrix };
			UpdateBuffer(m_cameraMatrixBuffer.get(), &viewProjMatrix, sizeof(viewProjMatrix));
		}
	}

	void GraphicsRenderer::Render()
	{
		rmt_ScopedCPUSample(Render, 0);
		BeginFrame();
		ExecuteShadowPass();
		ExecuteMainPass();
		ExecuteImGuiPass();
		Present();
		EndFrame();
	}

	void GraphicsRenderer::RemoveGraphicsElement(EntityID id)
	{
		auto it = m_lookupGraphicsElements.find(id);
		if (it == m_lookupGraphicsElements.end())
			return;

		const dU32 index = (*it).second;
		const EntityID entity = m_graphicsEntities[index];

		if (index < m_graphicsElements.size() - 1)
		{
			m_graphicsElements[index] = std::move(m_graphicsElements.back());
			m_graphicsEntities[index] = m_graphicsEntities.back();

			m_lookupGraphicsElements[m_graphicsEntities[index]] = index;
		}
		
		m_graphicsElements.pop_back();
		m_graphicsEntities.pop_back();
		m_lookupGraphicsElements.erase(id);
	}

}