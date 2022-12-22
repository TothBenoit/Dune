#pragma once

#include "Dune/Core/ECS/Components/PointLightComponent.h"
#include "GraphicsElement.h"
#include "Dune/Graphics/PointLight.h"
#include "Dune/Graphics/DirectionalLight.h"

namespace Dune
{
	class Window;
	class GraphicsBuffer;
	struct GraphicsBufferDesc;

	// Change once per draw call
	struct InstanceConstantBuffer
	{
		dMatrix4x4	modelMatrix;
		dMatrix4x4	normalMatrix;
		dVec4		baseColor;
	};

	// Change once per camera
	struct CameraConstantBuffer
	{
		dMatrix4x4	viewProjMatrix;
	};

	class GraphicsRenderer
	{
	public:
		virtual ~GraphicsRenderer() = default;
		GraphicsRenderer(const GraphicsRenderer&) = delete;
		GraphicsRenderer& operator=(const GraphicsRenderer&) = delete;
		GraphicsRenderer(GraphicsRenderer&&) = delete;
		GraphicsRenderer& operator=(GraphicsRenderer&&) = delete;

		static std::unique_ptr<GraphicsRenderer> Create(const Window * window);

		// TODO : Generalize Clear/Remove/Submit pattern 

		void ClearGraphicsElements();
		void RemoveGraphicsElement(EntityID id);
		void SubmitGraphicsElement(EntityID id, GraphicsElement&& elem);

		void ClearPointLights();
		void RemovePointLight(EntityID id);
		void SubmitPointLight(EntityID id, const PointLight& light);

		void ClearDirectionalLights();
		void RemoveDirectionalLight(EntityID id);
		void SubmitDirectionalLight(EntityID id, const DirectionalLight& light);

		void CreateCamera();
		void UpdateCamera();

		void Render();

		virtual void OnShutdown() = 0;
		virtual void OnResize(int width, int height) = 0;

		//TODO : Get rid of const void* data
		virtual std::unique_ptr<GraphicsBuffer> CreateBuffer(const GraphicsBufferDesc& desc, const void* data, dU32 size) = 0;
		virtual void UpdateBuffer(GraphicsBuffer * buffer, const void* data, dU32 size) = 0;

	protected:
		GraphicsRenderer() = default;
		
	private:

		virtual void InitShadowPass() = 0;
		virtual void InitMainPass() = 0;
		virtual void InitImGuiPass() = 0;

		virtual void BeginFrame() = 0;
		virtual void ExecuteShadowPass() = 0;
		virtual void ExecuteMainPass() = 0;
		virtual void ExecuteImGuiPass() = 0;
		virtual void Present() = 0;
		virtual void EndFrame() = 0;

	protected:
		static constexpr dU32 ms_frameCount = 2;

		dVector<DirectionalLight> m_directionalLights;
		dVector<EntityID> m_directionalLightEntities;
		dHashMap<EntityID, dU32> m_lookupDirectionalLights;

		dVector<PointLight> m_pointLights;
		dVector<EntityID> m_pointLightEntities;
		dHashMap<EntityID, dU32> m_lookupPointLights;
		
		dVector<GraphicsElement> m_graphicsElements;
		dVector<EntityID> m_graphicsEntities;
		dHashMap<EntityID, dU32> m_lookupGraphicsElements;

		std::unique_ptr<GraphicsBuffer>	m_cameraMatrixBuffer;
	};
}

