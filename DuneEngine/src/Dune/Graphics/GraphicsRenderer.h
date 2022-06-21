#pragma once

#include "Dune/Core/ECS/Components/PointLightComponent.h"
#include "GraphicsElement.h"
#include "Dune/Graphics/PointLight.h"

namespace Dune
{
	class Window;
	class GraphicsBuffer;
	struct GraphicsBufferDesc;

	class GraphicsRenderer
	{
	public:
		virtual ~GraphicsRenderer() = default;
		GraphicsRenderer(const GraphicsRenderer&) = delete;
		GraphicsRenderer& operator=(const GraphicsRenderer&) = delete;

		static std::unique_ptr<GraphicsRenderer> Create(const Window * window);

		void ClearGraphicsElements();
		void RemoveGraphicsElement(EntityID id);
		void SubmitGraphicsElement(EntityID id, const GraphicsElement& elem);

		void ClearPointLights();
		void SubmitPointLight(const PointLightComponent& light, dVec3 pos);

		void UpdateCamera();

		virtual void Render() = 0;
		virtual void OnShutdown() = 0;
		virtual void OnResize(int width, int height) = 0;
		//TODO : Get rid of const void* data
		virtual std::unique_ptr<GraphicsBuffer> CreateBuffer(const void* data, const GraphicsBufferDesc& desc) = 0;
		virtual void UpdateBuffer(std::unique_ptr<GraphicsBuffer>& buffer, const void* data) = 0;


	protected:
		GraphicsRenderer() = default;

	protected:
		
		dVector<PointLight> m_pointsLights;
		
		dVector<GraphicsElement> m_graphicsElements;
		dVector<EntityID> m_graphicsEntities;
		dHashMap<EntityID, dU32> m_lookupGraphicsElements;

		std::unique_ptr<GraphicsBuffer>	m_cameraMatrixBuffer;
	};
}

