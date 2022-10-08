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
		void RemovePointLight(EntityID id);
		void SubmitPointLight(EntityID id, const PointLight& light);

		void UpdateCamera();

		virtual void Render() = 0;
		virtual void OnShutdown() = 0;
		virtual void OnResize(int width, int height) = 0;
		//TODO : Get rid of const void* data
		virtual std::unique_ptr<GraphicsBuffer> CreateBuffer(const void* data, const GraphicsBufferDesc& desc) = 0;
		virtual void UpdateBuffer(GraphicsBuffer * buffer, const void* data) = 0;

	protected:
		GraphicsRenderer() = default;
	protected:
		
		dVector<PointLight> m_pointLights;
		dVector<EntityID> m_pointLightEntities;
		dHashMap<EntityID, dU32> m_lookupPointLights;
		
		dVector<GraphicsElement> m_graphicsElements;
		dVector<EntityID> m_graphicsEntities;
		dHashMap<EntityID, dU32> m_lookupGraphicsElements;

		std::unique_ptr<GraphicsBuffer>	m_cameraMatrixBuffer;
	};
}

