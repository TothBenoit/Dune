#pragma once
#include "Dune/Graphics/GraphicsRenderer.h"
#include "Platform/Windows/WindowsWindow.h"

#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>

namespace Dune
{
	class DX12GraphicsRenderer : public GraphicsRenderer
	{
	public:
		DX12GraphicsRenderer(const WindowsWindow * window);
		~DX12GraphicsRenderer() override;

		void WaitForGPU() override;
		void Render() override;
		void Present() override;
		void OnShutdown() override;
		void OnResize(int width, int height) override;

	private:
		static const UINT FrameCount = 2;

		struct Vertex
		{
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT4 color;
		};

		void CreateFactory();
		void CreateDevice();
		void CreateCommandQueue();
		void CreateSwapChain(HWND handle);
		void CreateRenderTargets();
		void CreateCommandAllocator();
		void CreateRootSignature();
		void CreatePipeline();
		void CreateCommandList();
		void PopulateCommandList();

	private:
		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_scissorRect;
		Microsoft::WRL::ComPtr<IDXGIFactory4>				m_factory;
		Microsoft::WRL::ComPtr<ID3D12Device>				m_device;
		Microsoft::WRL::ComPtr<IDXGISwapChain3>				m_swapChain;
		Microsoft::WRL::ComPtr<ID3D12Resource>				m_renderTargets[FrameCount];
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_rtvHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		m_imguiHeap;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>			m_commandQueue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		m_commandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	m_commandList;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState>			m_pipelineState;
		UINT												m_rtvDescriptorSize;
		
		// App resources.
		Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

		UINT m_frameIndex;
		Microsoft::WRL::Wrappers::Event m_fenceEvent;
		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
		UINT64 m_fenceValue;
	};
}

