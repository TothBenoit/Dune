#pragma once

#include "Dune/Core/ECS/Components/PointLightComponent.h"
#include "Dune/Graphics/GraphicsElement.h"
#include "Dune/Graphics/PointLight.h"
#include "Dune/Graphics/DirectionalLight.h"
#include "Dune/Graphics/DescriptorHeap.h"

namespace Dune
{
	class Window;
	struct BufferDesc;

	// Change once per camera
	struct CameraConstantBuffer
	{
		dMatrix4x4	viewProjMatrix;
	};


	class Renderer
	{
	public:
		DISABLE_COPY_AND_MOVE(Renderer);

		static Renderer& GetInstance();
		void Initialize(const Window * window);

		// TODO : Generalize Clear/Remove/Submit pattern 
		void ClearGraphicsElements();
		void RemoveGraphicsElement(EntityID id);
		void SubmitGraphicsElement(EntityID id, std::weak_ptr<Mesh> mesh, const InstanceData& instanceData);

		void ClearPointLights();
		void RemovePointLight(EntityID id);
		void SubmitPointLight(EntityID id, const PointLight& light);

		void ClearDirectionalLights();
		void RemoveDirectionalLight(EntityID id);
		void SubmitDirectionalLight(EntityID id, const DirectionalLight& light);

		void CreateCamera();
		void UpdateCamera();

		void Render();

		void OnShutdown();
		void OnResize(int width, int height);

		std::unique_ptr<Buffer> CreateBuffer(const BufferDesc& desc, const void* pData, dU32 size);
		void UpdateBuffer(Buffer* buffer, const void* pData, dU32 size);

		ID3D12Device* GetDevice() { Assert(m_device.Get()); return m_device.Get(); }

	private:
		Renderer() = default;
		~Renderer();
		
		void CreateFactory();
		void EnableDebugLayer() const;
		void CreateDevice();
		void CreateCommandQueues();
		void CreateSwapChain(HWND handle);
		void CreateRenderTargets();
		void CreateDepthStencil(int width, int height);
		void CreateCommandAllocators();
		void CreateSamplers();
		void CreateRootSignature();
		void CreatePipeline();
		void CreateCommandLists();
		void CreateFences();

		void InitShadowPass();
		void InitMainPass();
		void InitImGuiPass();

		void BeginFrame();
		void ExecuteShadowPass();
		void ExecuteMainPass();
		void ExecuteImGuiPass();
		void Present();
		void EndFrame();
		
		void WaitForFrame(const dU64 frameIndex);
		void WaitForCopy();

		//TEMP
		void PrepareShadowPass();
		void CreatePointLightsBuffer();
		void UpdatePointLights();
		void CreateDirectionalLightsBuffer();
		void UpdateDirectionalLights();

	private:
		inline static constexpr dU32						ms_frameCount{ 2 };
		inline static constexpr dU32						ms_shadowMapCount{ 1 };

		D3D12_VIEWPORT										m_viewport;
		D3D12_RECT											m_scissorRect;
		Microsoft::WRL::ComPtr<IDXGIFactory4>				m_factory;
		Microsoft::WRL::ComPtr<ID3D12Device>				m_device;
		Microsoft::WRL::ComPtr<IDXGISwapChain3>				m_swapChain;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_renderTargets[ms_frameCount];
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_depthStencilBuffer;
		DescriptorHeap										m_rtvHeap;
		DescriptorHeap										m_dsvHeap;
		DescriptorHandle									m_rtvHandles[ms_frameCount];
		DescriptorHandle									m_dsvHandle;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue>			m_commandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		m_commandAllocators[ms_frameCount];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_commandList;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>			m_copyCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		m_copyCommandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_copyCommandList;
		
		// Shadow Pass
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_shadowDsvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_samplerHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_shadowMaps[ms_shadowMapCount];
		std::unique_ptr<Buffer>								m_shadowCameraBuffers[ms_shadowMapCount][ms_frameCount];
		D3D12_CPU_DESCRIPTOR_HANDLE							m_shadowDepthViews[ms_shadowMapCount];
		D3D12_GPU_DESCRIPTOR_HANDLE							m_shadowResourceViews[ms_shadowMapCount];

		// Main Pass
		Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;
		std::unique_ptr<Buffer>								m_pointLightsBuffer[ms_frameCount]; 		// TEMP Should not be hardcoded in the renderer
		std::unique_ptr<Buffer>								m_directionalLightBuffer[ms_frameCount]; 	// TEMP Should not be hardcoded in the renderer
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_lightHeap;								// TEMP

		// ImGui Pass
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_imguiHeap;

		// Synchronization
		dU64												m_frameIndex = 0;
		dU64												m_frameNumber = 0;
		Microsoft::WRL::Wrappers::Event						m_fenceEvent;
		Microsoft::WRL::ComPtr<ID3D12Fence>					m_fence;
		dU64												m_fenceValues[ms_frameCount];
		Microsoft::WRL::Wrappers::Event						m_copyFenceEvent;
		Microsoft::WRL::ComPtr<ID3D12Fence>					m_copyFence;
		dU64												m_copyFenceValue;


		dVector<DirectionalLight>							m_directionalLights;
		dVector<EntityID>									m_directionalLightEntities;
		dHashMap<EntityID, dU32>							m_lookupDirectionalLights;

		dVector<PointLight>									m_pointLights;
		dVector<EntityID>									m_pointLightEntities;
		dHashMap<EntityID, dU32>							m_lookupPointLights;

		dVector<GraphicsElement>							m_graphicsElements;
		dVector<EntityID>									m_graphicsEntities;
		dHashMap<EntityID, dU32>							m_lookupGraphicsElements;

		std::unique_ptr<Buffer>								m_cameraMatrixBuffer;

		dVector<std::shared_ptr<Buffer>>					m_usedBuffer[ms_frameCount];
	};
}

