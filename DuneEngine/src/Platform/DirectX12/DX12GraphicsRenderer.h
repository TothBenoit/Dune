#pragma once
#include "Dune/Graphics/GraphicsRenderer.h"

namespace Dune
{
	class WindowsWindow;

	class DX12GraphicsRenderer final : public GraphicsRenderer
	{
	public:
		DX12GraphicsRenderer(const WindowsWindow * window);
		~DX12GraphicsRenderer() override;

		void OnShutdown() override;
		void OnResize(int width, int height) override;

		std::unique_ptr<GraphicsBuffer> CreateBuffer(const void* data, const GraphicsBufferDesc& desc) override;
		void UpdateBuffer(GraphicsBuffer * buffer, const void* data) override;

	private:
		static constexpr dU32 ms_shadowMapCount = 8;

		void CreateFactory();
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

		void InitShadowPass() override;
		void InitMainPass() override;
		void InitImGuiPass() override;

		void BeginFrame() override;
		void ExecuteShadowPass() override;
		void ExecuteMainPass() override;
		void ExecuteImGuiPass() override;
		void Present() override;
		void EndFrame() override;
		
		void WaitForFrame(const dU64 frameIndex);
		void WaitForCopy();

		//TEMP
		void PrepareShadowPass();
		void CreatePointLightsBuffer();
		void UpdatePointLights();
		void CreateDirectionalLightsBuffer();
		void UpdateDirectionalLights();

	private:
		D3D12_VIEWPORT										m_viewport;
		D3D12_RECT											m_scissorRect;
		Microsoft::WRL::ComPtr<IDXGIFactory4>				m_factory;
		Microsoft::WRL::ComPtr<ID3D12Device>				m_device;
		Microsoft::WRL::ComPtr<IDXGISwapChain3>				m_swapChain;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_renderTargets[ms_frameCount];
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_depthStencilBuffer;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_rtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_dsvHeap;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>			m_commandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		m_commandAllocators[ms_frameCount];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_commandList;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>			m_copyCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		m_copyCommandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_copyCommandList;
		dU32												m_rtvDescriptorSize;

		// Shadow Pass
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_shadowDsvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_shadowSrvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_samplerHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_shadowMaps[ms_shadowMapCount];
		std::unique_ptr<GraphicsBuffer>						m_shadowCameraBuffers[ms_shadowMapCount];
		D3D12_CPU_DESCRIPTOR_HANDLE							m_shadowDepthViews[ms_shadowMapCount];
		D3D12_GPU_DESCRIPTOR_HANDLE							m_shadowResourceViews[ms_shadowMapCount];

		// Main Pass
		Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;
		std::unique_ptr<GraphicsBuffer>						m_pointLightsBuffer[ms_frameCount]; 		// TEMP Should not be hardcoded in the renderer
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_pointLightsHeap;							// TEMP
		std::unique_ptr<GraphicsBuffer>						m_directionalLightBuffer[ms_frameCount]; 	// TEMP Should not be hardcoded in the renderer
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_directionalLightsHeap;					// TEMP

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

	};
}

