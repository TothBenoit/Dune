#pragma once
#include "Dune/Graphics/GraphicsRenderer.h"

namespace Dune
{
	class WindowsWindow;

	class DX12GraphicsRenderer : public GraphicsRenderer
	{
	public:
		DX12GraphicsRenderer(const WindowsWindow * window);
		~DX12GraphicsRenderer() override;

		void Render() override;
		void OnShutdown() override;
		void OnResize(int width, int height) override;

		std::unique_ptr<GraphicsBuffer> CreateBuffer(const void* data, const GraphicsBufferDesc& desc) override;
		void UpdateBuffer(GraphicsBuffer * buffer, const void* data) override;

	private:
		static const dU32 FrameCount = 2;
		void CreateFactory();
		void CreateDevice();
		void CreateCommandQueues();
		void CreateSwapChain(HWND handle);
		void CreateRenderTargets();
		void CreateDepthStencil(int width, int height);
		void CreateCommandAllocators();
		void CreateRootSignature();
		void CreatePipeline();
		void CreateCommandLists();
		void CreateFences();
		void WaitForFrame(const dU64 frameIndex);
		void WaitForCopy();
		void PopulateCommandList();

		//TEMP
		void CreateLightsBuffer(dU32 size);
		void UpdateLights();

	private:
		//TODO : FrameContext which contains command allocator, fences, render target 
		D3D12_VIEWPORT										m_viewport;
		D3D12_RECT											m_scissorRect;
		Microsoft::WRL::ComPtr<IDXGIFactory4>				m_factory;
		Microsoft::WRL::ComPtr<ID3D12Device>				m_device;
		Microsoft::WRL::ComPtr<IDXGISwapChain3>				m_swapChain;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_renderTargets[FrameCount];
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_depthStencilBuffer;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_rtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_dsvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_imguiHeap;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>			m_commandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		m_commandAllocators[FrameCount];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_commandList;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>			m_copyCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		m_copyCommandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_copyCommandList;
		Microsoft::WRL::ComPtr<ID3D12RootSignature>			m_rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;
		dU32												m_rtvDescriptorSize;

		// Synchronization
		dU64												m_frameNumber = 0;
		Microsoft::WRL::Wrappers::Event						m_fenceEvent;
		Microsoft::WRL::ComPtr<ID3D12Fence>					m_fence;
		dU64												m_fenceValues[FrameCount];
		Microsoft::WRL::Wrappers::Event						m_copyFenceEvent;
		Microsoft::WRL::ComPtr<ID3D12Fence>					m_copyFence;
		dU64												m_copyFenceValue;

		//TEMP
		std::unique_ptr<GraphicsBuffer>						m_lightsBuffer[FrameCount];
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_lightsHeap;
	};
}

