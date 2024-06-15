#include "pch.h"
#include "GraphicsDX12.h"
#include "WindowInternal.h"
#include "Dune/Core/Graphics.h"
#include "Dune/Core/Graphics/Mesh.h"
#include "Dune/Common/Pool.h"
#include "Dune/Utilities/Utils.h"

namespace Dune::Graphics
{
	class DescriptorHeap;
	class ViewInternal;

	Pool<Buffer>			g_bufferPool;
	Pool<Mesh>				g_meshPool;
	Pool<Texture>			g_texturePool;
	Pool<Shader>			g_shaderPool;
	Pool<Pipeline>			g_pipelinePool;

	struct DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuAdress{ 0 };
		D3D12_GPU_DESCRIPTOR_HANDLE gpuAdress{ 0 };

		[[nodiscard]] bool IsValid() { return cpuAdress.ptr != 0; }
		[[nodiscard]] bool IsShaderVisible() { return gpuAdress.ptr != 0; }

	#ifdef _DEBUG
		dU32 slot{ 0 };
		ID3D12DescriptorHeap* heap{ nullptr };
	#endif
	};

	class DescriptorHeap
	{
	public:
		DescriptorHeap() = default;

		void Initialize(ID3D12Device* pDevice, dU32 capacity, D3D12_DESCRIPTOR_HEAP_TYPE type);
		void Destroy();

		[[nodiscard]] inline dU32 GetCapacity() { return m_capacity; }
		[[nodiscard]] inline dU32 GetSize() { return (dU32)m_freeSlots.size(); }
		[[nodiscard]] DescriptorHandle Allocate();;
		void Free(DescriptorHandle handle);

		ID3D12DescriptorHeap* Get() { return m_pDescriptorHeap; }

	private:
		dU32 m_capacity{ 0 };
		dVector<dU32> m_freeSlots{};
		D3D12_DESCRIPTOR_HEAP_TYPE m_type{};
		D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStartAdress{ 0 };
		D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStartAdress{ 0 };
		dU64 m_descriptorSize{ 0 };
		ID3D12DescriptorHeap* m_pDescriptorHeap{ nullptr };
		bool m_bIsShaderVisible{ false };
	};

	struct RingBufferAllocation
	{
		ID3D12Resource* pResource{ nullptr };
		dU8* pCpuAdress{ nullptr };
		dU32 offset{ 0 };
		dU32 byteSize{ 0 };
	};

	class RingBufferAllocator
	{
	public:
		void Initialize(ID3D12Device* pDevice, dU32 byteSize)
		{
			Assert(!m_pResource);

			D3D12_HEAP_PROPERTIES heapsProps{ CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD) };
			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = byteSize;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Alignment = 0;

			ThrowIfFailed(pDevice->CreateCommittedResource(&heapsProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pResource)));
			D3D12_RANGE readRange{};
			ThrowIfFailed(m_pResource->Map(0, &readRange, reinterpret_cast<void**>(&m_pCpuMemory)));
			m_capacity = byteSize;
		}

		void Shutdown()
		{
			Assert(m_pResource);
			Wait();
			m_pResource->Release();
			m_pResource = nullptr;
			m_pCpuMemory = nullptr;
			m_capacity = 0;
			m_tailOffset = 0;
			m_headOffset = 0;
		}

		bool Allocate(RingBufferAllocation& allocation, dU32 byteSize )
		{	
			std::lock_guard lock(m_lock);

			while (!m_workingAllocation.empty())
			{
				const WorkingAllocation& workingAllocation = m_workingAllocation.front();

				if (workingAllocation.pFence->GetCompletedValue() < workingAllocation.fenceValue)
					break;
				
				m_tailOffset = workingAllocation.byteSize + workingAllocation.offset;
				m_workingAllocation.pop();
			}

			constexpr dU32 invalidOffset = 0xFFFFFFFF;
			dU32 offset = invalidOffset;

			if ( byteSize > m_capacity)
				return false;

			if (m_headOffset >= m_tailOffset)
			{
				if (m_headOffset + byteSize <= m_capacity)
				{
					offset = m_headOffset;
					m_headOffset += byteSize;
				}
				else if (byteSize <= m_tailOffset)
				{
					offset = 0;
					m_headOffset = byteSize;
				}
			}
			else if (m_headOffset + byteSize <= m_tailOffset)
			{
				offset = m_headOffset;
				m_headOffset += byteSize;
			}

			if (offset == invalidOffset)
				return false;
			
			allocation.pResource = m_pResource;
			allocation.pCpuAdress = m_pCpuMemory + offset;
			allocation.offset = offset;
			allocation.byteSize = byteSize;

			return true;
		}

		void Free(RingBufferAllocation& allocation, ID3D12Fence* pFence, dU64 fenceValue)
		{
			std::lock_guard lock(m_lock);

			WorkingAllocation workingAllocation;
			workingAllocation.offset = allocation.offset;
			workingAllocation.byteSize = allocation.byteSize;			
			m_pLastFence = workingAllocation.pFence = pFence;
			m_lastFenceValue = workingAllocation.fenceValue = fenceValue;
			m_workingAllocation.push(workingAllocation);

			allocation.pResource = nullptr;
			allocation.pCpuAdress = nullptr;
		}

		void Wait()
		{
			if (m_pLastFence)
				m_pLastFence->SetEventOnCompletion(m_lastFenceValue, NULL);
		}

	private:
		std::mutex m_lock;
		ID3D12Resource* m_pResource { nullptr };
		dU8* m_pCpuMemory{ nullptr };
		dU64 m_capacity{ 0 };
		dU32 m_tailOffset{ 0 };
		dU32 m_headOffset{ 0 };

		struct WorkingAllocation
		{
			dU32 offset{ 0 };
			dU32 byteSize{ 0 };
			ID3D12Fence* pFence{ nullptr };
			dU64 fenceValue{ 0 };
		};

		std::queue<WorkingAllocation> m_workingAllocation;
		ID3D12Fence* m_pLastFence{ nullptr };
		dU64 m_lastFenceValue{ 0 };
	};

	struct Command
	{
		dU32 commandIndex{ 0 };
		dU32 commandCount{ 0 };
		ID3D12GraphicsCommandList* pCommandList{ nullptr };
		ID3D12CommandAllocator* pCommandAllocators[3]{ nullptr, nullptr, nullptr };
		dU64 fenceValues[3]{ 0, 0, 0 };
		ID3D12Fence* pFence{ nullptr };
	};

	struct DirectCommand : public Command
	{
		ID3D12DescriptorHeap* heaps[2]{ nullptr, nullptr };
	};

	struct ComputeCommand : public Command
	{
		ID3D12DescriptorHeap* heaps[2]{ nullptr, nullptr };
	};

	struct CopyCommand : public Command
	{
	};

	struct DyingResource
	{
		ID3D12Object* pResource;
		dU64 fenceValue;
	};

	struct Device
	{
		IDXGIFactory7*	pFactory{ nullptr };
		ID3D12Device*	pDevice{ nullptr };
	#ifdef _DEBUG
		IDXGIDebug1*	pDebug{ nullptr };
	#endif

		ID3D12CommandQueue* pDirectCommandQueue{ nullptr };
		ID3D12Fence* pDirectFence{ nullptr };
		dU64 directFenceValue{ 0 };

		ID3D12CommandQueue* pComputeCommandQueue{ nullptr };
		ID3D12Fence* pComputeFence{ nullptr };
		dU64 computeFenceValue{ 0 };

		ID3D12CommandQueue* pCopyCommandQueue{ nullptr };
		ID3D12Fence* pCopyFence{ nullptr };
		dU64 copyFenceValue{ 0 };

		ID3D12CommandAllocator* pCopyCommandAllocator{ nullptr };
		ID3D12GraphicsCommandList* pCopyCommandList{ nullptr };

		std::mutex commandLock;
		std::mutex releaseLock;

		dQueue<DyingResource> dyingResources;

		RingBufferAllocator ringBufferAllocator;

		DescriptorHeap	rtvHeap;
		DescriptorHeap	dsvHeap;
		DescriptorHeap	srvHeap;
		DescriptorHeap	samplerHeap;

		void WaitForCopy()
		{
			Assert(pCopyFence);

			if (pCopyFence->GetCompletedValue() < copyFenceValue)
			{
				ThrowIfFailed(pCopyFence->SetEventOnCompletion(copyFenceValue, NULL))
			}
		}

		void CopyBufferRegion(ID3D12Resource* pDestBuffer, dU64 dstOffset, ID3D12Resource* pSrcBuffer, dU64 srcOffset, dU64 size)
		{
			ThrowIfFailed(pCopyCommandAllocator->Reset());
			ThrowIfFailed(pCopyCommandList->Reset(pCopyCommandAllocator, nullptr));

			pCopyCommandList->CopyBufferRegion(pDestBuffer, dstOffset, pSrcBuffer, srcOffset, size);
			pCopyCommandList->Close();
			dVector<ID3D12CommandList*> ppCommandLists{ pCopyCommandList };
			pCopyCommandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());
			pCopyCommandQueue->Signal(pCopyFence, ++copyFenceValue);
			WaitForCopy();
		}

		void UploadTexture(ID3D12Resource* pDest, ID3D12Resource* pUploadBuffer, dU64 uploadByteOffset, dU32 firstSubresource, dU32 subresourceCount, D3D12_SUBRESOURCE_DATA* pSrcData)
		{
			ThrowIfFailed(pCopyCommandAllocator->Reset());
			ThrowIfFailed(pCopyCommandList->Reset(pCopyCommandAllocator, nullptr));

			UpdateSubresources(pCopyCommandList, pDest, pUploadBuffer, uploadByteOffset, firstSubresource, subresourceCount, pSrcData);
			pCopyCommandList->Close();
			dVector<ID3D12CommandList*> ppCommandLists{ pCopyCommandList };
			pCopyCommandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());
			pCopyCommandQueue->Signal(pCopyFence, ++copyFenceValue);
			WaitForCopy();
		}

		void ReleaseResource(ID3D12Object* pResource)
		{
			std::lock_guard lock{ releaseLock };
			dyingResources.push({ pResource, directFenceValue });
		}

		void ReleaseDyingResources()
		{
			std::lock_guard lock{ releaseLock };
			while (!dyingResources.empty())
			{
				const auto& [pRes, fenceValue] = dyingResources.front();
				if (pDirectFence->GetCompletedValue() < fenceValue)
				{
					break;
				}
				pRes->Release();
				dyingResources.pop();
			}
		}
	};

	void Initialize()
	{
		g_bufferPool.Initialize( 4096 );
		g_meshPool.Initialize( 32 );
		g_texturePool.Initialize( 64 );
		g_shaderPool.Initialize( 16 );
		g_pipelinePool.Initialize( 16 );
	}

	void Shutdown()
	{
		g_bufferPool.Release();
		g_meshPool.Release();
		g_texturePool.Release();
		g_shaderPool.Release();
		g_pipelinePool.Release();
	}

	Device* CreateDevice()
	{
		UINT dxgiFactoryFlags{ 0 };
	#ifdef _DEBUG
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	#endif // _DEBUG
		IDXGIFactory7* pFactory{ nullptr };
		ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&pFactory)));

	#ifdef _DEBUG
		ID3D12Debug* pDebugInterface{ nullptr };
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugInterface)));
		ID3D12Debug1* pDebugController{ nullptr };
		ThrowIfFailed(pDebugInterface->QueryInterface(IID_PPV_ARGS(&pDebugController)));
		pDebugController->EnableDebugLayer();
		pDebugInterface->Release();
	#endif // _DEBUG

		// TODO : Allow the app to choose which GPU to use.
		IDXGIAdapter1* pAdapter{ nullptr };
		for (dU32 adapterIndex{ 0 }; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &pAdapter); adapterIndex++)
		{
			DXGI_ADAPTER_DESC1 desc{};
			pAdapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				continue;

			if (SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
				break;

			pAdapter->Release();
		}

		ID3D12Device* pDevice;
		ThrowIfFailed(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&pDevice)));
		NameDXObject(pDevice, L"Device");
		pAdapter->Release();

		Device* pDeviceInterface = new Device();
		pDeviceInterface->pFactory = pFactory;
		pDeviceInterface->pDevice = pDevice;
	#ifdef _DEBUG
		 DXGIGetDebugInterface1( 0, IID_PPV_ARGS(&pDeviceInterface->pDebug));
	#endif // _DEBUG

		pDeviceInterface->rtvHeap.Initialize(pDevice, 64, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		pDeviceInterface->dsvHeap.Initialize(pDevice, 64, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		pDeviceInterface->samplerHeap.Initialize(pDevice, 64, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		pDeviceInterface->srvHeap.Initialize(pDevice, 4096, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_COMMAND_QUEUE_DESC queueDesc
		{
			.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
		};


		ThrowIfFailed(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pDeviceInterface->pDirectCommandQueue)));
		NameDXObject(pDeviceInterface->pDirectCommandQueue, L"DirectCommandQueue");

		ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pDeviceInterface->pDirectFence)));
		NameDXObject(pDeviceInterface->pDirectFence, L"DirectFence");

		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		ThrowIfFailed(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pDeviceInterface->pComputeCommandQueue)));
		NameDXObject(pDeviceInterface->pComputeCommandQueue, L"ComputeCommandQueue");

		ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pDeviceInterface->pComputeFence)));
		NameDXObject(pDeviceInterface->pComputeFence, L"ComputeFence");

		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		ThrowIfFailed(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pDeviceInterface->pCopyCommandQueue)));
		NameDXObject(pDeviceInterface->pCopyCommandQueue, L"CopyCommandQueue");

		ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pDeviceInterface->pCopyFence)));
		NameDXObject(pDeviceInterface->pCopyFence, L"CopyFence");

		ThrowIfFailed(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&pDeviceInterface->pCopyCommandAllocator)));
		NameDXObject(pDeviceInterface->pCopyCommandAllocator, L"CopyCommandAllocator");

		ThrowIfFailed(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, pDeviceInterface->pCopyCommandAllocator, nullptr, IID_PPV_ARGS(&pDeviceInterface->pCopyCommandList)));
		ThrowIfFailed(pDeviceInterface->pCopyCommandList->Close());
		NameDXObject(pDeviceInterface->pCopyCommandList, L"CopyCommandList");

		pDeviceInterface->ringBufferAllocator.Initialize(pDevice, 128 * 1024 * 1024);

		return pDeviceInterface;
	}

	void DestroyDevice(Device* pDeviceInterface)
	{
		pDeviceInterface->ringBufferAllocator.Shutdown();
		pDeviceInterface->ReleaseDyingResources();
		pDeviceInterface->pDirectCommandQueue->Release();
		pDeviceInterface->pDirectFence->Release();

		pDeviceInterface->pComputeFence->Release();
		pDeviceInterface->pComputeCommandQueue->Release();

		pDeviceInterface->pCopyFence->Release();
		pDeviceInterface->pCopyCommandList->Release();
		pDeviceInterface->pCopyCommandAllocator->Release();
		pDeviceInterface->pCopyCommandQueue->Release();

		pDeviceInterface->pDevice->Release();
		pDeviceInterface->pFactory->Release();
		pDeviceInterface->rtvHeap.Destroy();
		pDeviceInterface->dsvHeap.Destroy();
		pDeviceInterface->samplerHeap.Destroy();
		pDeviceInterface->srvHeap.Destroy();
#ifdef _DEBUG
		pDeviceInterface->pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		pDeviceInterface->pDebug->Release();
#endif // _DEBUG
		delete(pDeviceInterface);
	}


	void DescriptorHeap::Initialize(ID3D12Device* pDevice, dU32 capacity, D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		Assert(!m_pDescriptorHeap);
		Assert(capacity != 0);

		m_capacity = capacity;
		m_type = type;

		D3D12_DESCRIPTOR_HEAP_FLAGS flags{};
		if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
		{
			m_bIsShaderVisible = false;
			flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		}
		else
		{
			m_bIsShaderVisible = true;
			flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		}

		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
		descriptorHeapDesc.NumDescriptors = m_capacity;
		descriptorHeapDesc.Type = m_type;
		descriptorHeapDesc.Flags = flags;
		descriptorHeapDesc.NodeMask = 0;
		ThrowIfFailed(pDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_pDescriptorHeap)));
		NameDXObject(m_pDescriptorHeap, L"DescriptorHeap");

		m_cpuStartAdress = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		m_gpuStartAdress = (m_bIsShaderVisible) ? m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
		m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(m_type);

		m_freeSlots.reserve(m_capacity);
		for (dU32 i{ 0 }; i < m_capacity; i++)
		{
			m_freeSlots.push_back(i);
		}
	}

	void DescriptorHeap::Destroy()
	{
		Assert(m_freeSlots.size() == m_capacity);
		m_pDescriptorHeap->Release();
		m_capacity = 0;
		m_freeSlots.clear();
	}

	[[nodiscard]] DescriptorHandle DescriptorHeap::Allocate()
	{
		Assert(!m_freeSlots.empty());
		DescriptorHandle handle{};
		dU32 slot{ m_freeSlots.back() };
		m_freeSlots.pop_back();
		handle.cpuAdress = D3D12_CPU_DESCRIPTOR_HANDLE{ m_cpuStartAdress.ptr + m_descriptorSize * (dU64)slot };
		handle.gpuAdress = (m_bIsShaderVisible) ? D3D12_GPU_DESCRIPTOR_HANDLE{ m_gpuStartAdress.ptr + m_descriptorSize * (dU64)slot } : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
	#ifdef _DEBUG
		handle.heap = m_pDescriptorHeap;
		handle.slot = slot;
	#endif
		return handle;
	}

	void DescriptorHeap::Free(DescriptorHandle handle)
	{
		Assert(handle.IsValid());
		Assert(handle.IsShaderVisible() == m_bIsShaderVisible);
		Assert(handle.heap == m_pDescriptorHeap);

		dU32 slot{ (dU32)((handle.cpuAdress.ptr - m_cpuStartAdress.ptr) / m_descriptorSize) };
		Assert(handle.slot == slot);

		m_freeSlots.push_back(slot);
	}

	void OnResize(void* pData);
	class ViewInternal : public View
	{
	public:
		void Initialize(const ViewDesc& desc)
		{
			WindowInternal* pWindow = new WindowInternal();
			m_pWindow = pWindow;
			pWindow->Initialize(desc.windowDesc);
			pWindow->SetOnResizeFunc(this, &OnResize);
			m_pOnResize = desc.pOnResize;
			m_pOnResizeData = desc.pOnResizeData;

			m_pDeviceInterface = desc.pDevice;
			IDXGIFactory7* pFactory = m_pDeviceInterface->pFactory;
			ID3D12Device* pDevice = m_pDeviceInterface->pDevice;

			m_backBufferCount = desc.backBufferCount;

			RECT clientRect;
			HWND handle{ (HWND) pWindow->GetHandle() };
			GetClientRect(handle, &clientRect);

			D3D12_COMMAND_QUEUE_DESC queueDesc
			{
				.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
				.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
			};

			m_directFenceValues = new dU64[m_backBufferCount];
			for (dU32 i = 0; i < m_backBufferCount; i++)
			{
				m_directFenceValues[i] = 0;
			}

			DXGI_SWAP_CHAIN_DESC1 swapChainDesc
			{
				.Width = 0,
				.Height = 0,
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.Stereo = false,
				.SampleDesc = {.Count = 1, .Quality = 0},
				.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
				.BufferCount = m_backBufferCount,
				.Scaling = DXGI_SCALING_STRETCH,
				.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
				.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
				.Flags = 0
			};

			Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
			Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain{ nullptr };
			ThrowIfFailed(pFactory->CreateSwapChainForHwnd(
				m_pDeviceInterface->pDirectCommandQueue,
				handle,
				&swapChainDesc,
				nullptr,
				nullptr,
				&swapChain
			));

			ThrowIfFailed(pFactory->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER));
			ThrowIfFailed(swapChain.As(&pSwapChain));
			m_pSwapChain = pSwapChain.Detach();			

			m_pBackBuffers = new ID3D12Resource*[m_backBufferCount];
			m_pBackBufferViews = new DescriptorHandle[m_backBufferCount];
			m_pBackBufferStates = new D3D12_RESOURCE_STATES[m_backBufferCount];
			for (UINT i = 0; i < m_backBufferCount; i++)
			{
				m_pBackBufferViews[i] = m_pDeviceInterface->rtvHeap.Allocate();
				ThrowIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pBackBuffers[i])));
				pDevice->CreateRenderTargetView(m_pBackBuffers[i], nullptr, m_pBackBufferViews[i].cpuAdress);
				NameDXObjectIndexed(m_pBackBuffers[i], i, L"BackBuffers");
				m_pBackBufferStates[i] = D3D12_RESOURCE_STATE_COMMON;
			}
			m_frameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
			m_pCommand = CreateDirectCommand({ .pView = this });
		}

		void Destroy()
		{
			for (dU32 i = 0; i < m_backBufferCount; i++)
			{
				WaitForFrame(i);
				m_pBackBuffers[i]->Release();
				m_pDeviceInterface->rtvHeap.Free(m_pBackBufferViews[i]);
				ReleaseDyingResources();
			}
			DestroyCommand(m_pCommand);

			delete[] m_pBackBufferStates;
			delete[] m_pBackBufferViews;
			delete[] m_pBackBuffers;

			m_pSwapChain->Release();
			
			delete[] m_directFenceValues;

			WindowInternal* pWindow = (WindowInternal*) m_pWindow;
			pWindow->Destroy();
			delete(pWindow);
		}

		bool ProcessEvents()
		{
			return ((WindowInternal*)m_pWindow)->Update();
		}

		[[nodiscard]] ID3D12Resource* GetCurrentBackBuffer()
		{
			return m_pBackBuffers[m_frameIndex];
		}

		[[nodiscard]] const DescriptorHandle& GetCurrentBackBufferView() const
		{ 
			return m_pBackBufferViews[m_frameIndex]; 
		}

		[[nodiscard]] D3D12_RESOURCE_STATES GetCurrentBackBufferState() const
		{
			return m_pBackBufferStates[m_frameIndex];
		}

		void SetCurrentBackBufferState(D3D12_RESOURCE_STATES state)
		{
			m_pBackBufferStates[m_frameIndex] = state;
		}

		[[nodiscard]] const float* GetBackBufferClearValue() const
		{
			return m_backBufferClearValue;
		}

		[[nodiscard]] dU32 GetWidth() const
		{
			return m_pWindow->GetWidth();
		}

		[[nodiscard]] dU32 GetHeight() const
		{
			return m_pWindow->GetHeight();
		}

		void BeginFrame()
		{
			WaitForFrame(m_frameIndex);
			ReleaseDyingResources();
		}

		void EndFrame()
		{
			ResetCommand(m_pCommand);
			if (m_pBackBufferStates[m_frameIndex] != D3D12_RESOURCE_STATE_PRESENT)
			{
				D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(m_pBackBuffers[m_frameIndex], m_pBackBufferStates[m_frameIndex], D3D12_RESOURCE_STATE_PRESENT) };
				m_pCommand->pCommandList->ResourceBarrier(1, &barrier);
				m_pBackBufferStates[m_frameIndex] = D3D12_RESOURCE_STATE_PRESENT;
				SubmitCommand(m_pDeviceInterface, m_pCommand);
			}

			ThrowIfFailed(m_pSwapChain->Present(1, 0));
			ThrowIfFailed(m_pDeviceInterface->pDirectCommandQueue->Signal(m_pDeviceInterface->pDirectFence, ++m_pDeviceInterface->directFenceValue));
			m_directFenceValues[m_frameIndex] = m_pDeviceInterface->directFenceValue;
			m_elapsedFrame++;
			m_frameIndex = (m_frameIndex + 1) % m_backBufferCount;
		}

		void ReleaseResource(ID3D12Object* pResource)
		{
			m_pDeviceInterface->ReleaseResource(pResource);
		}

		void ReleaseDyingResources()
		{
			m_pDeviceInterface->ReleaseDyingResources();
		}

		void WaitForFrame(const dU32 frameIndex)
		{
			const dU64 frameFenceValue{ m_directFenceValues[frameIndex] };
			if (m_pDeviceInterface->pDirectFence->GetCompletedValue() < frameFenceValue)
			{
				ThrowIfFailed(m_pDeviceInterface->pDirectFence->SetEventOnCompletion(frameFenceValue, NULL))
			}

			m_pDeviceInterface->WaitForCopy();
		}

		void Resize()
		{
			// Wait until all previous frames are processed.
			for (dU32 i = 0; i < m_backBufferCount; i++)
			{
				WaitForFrame(i);
			}

			// Release resources that are tied to the swap chain.
			for (dU32 i = 0; i < m_backBufferCount; i++)
			{
				m_pBackBuffers[i]->Release();
			}

			ThrowIfFailed(m_pSwapChain->ResizeBuffers(
				m_backBufferCount,
				GetWidth(), GetHeight(),
				DXGI_FORMAT_R8G8B8A8_UNORM,
				0
			));

			for (dU32 i = 0; i < m_backBufferCount; i++)
			{
				ThrowIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pBackBuffers[i])));
				m_pDeviceInterface->pDevice->CreateRenderTargetView(m_pBackBuffers[i], nullptr, m_pBackBufferViews[i].cpuAdress);
				NameDXObjectIndexed(m_pBackBuffers[i], i, L"BackBuffers");
			}			
			m_frameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
			m_elapsedFrame++;

			if (m_pOnResize)
				m_pOnResize(this, m_pOnResizeData);
		}

		[[nodiscard]] const Input* GetInput() const { return m_pWindow->GetInput(); }
		[[nodiscard]] dU32 GetFrameCount() const { return m_backBufferCount; }
		[[nodiscard]] dU32 GetFrameIndex() const { return m_frameIndex; }
		[[nodiscard]] dU64 GetElaspedFrame() const { return m_elapsedFrame; }

	private:
		void(*m_pOnResize)(View*, void*) { nullptr };
		void* m_pOnResizeData{ nullptr };
		IDXGISwapChain4* m_pSwapChain{ nullptr };
		ID3D12Resource** m_pBackBuffers{ nullptr };
		DescriptorHandle* m_pBackBufferViews{ nullptr };
		D3D12_RESOURCE_STATES* m_pBackBufferStates{ nullptr };
		const float	m_backBufferClearValue[4] { 0.f, 0.f, 0.f, 0.f };

		dU64* m_directFenceValues;

		dU32 m_backBufferCount{ 0 };
		dU32 m_frameIndex{ 0 };
		dU64 m_elapsedFrame{ 0 };		

		DirectCommand* m_pCommand{ nullptr };
	};

	View* CreateView(const ViewDesc& desc)
	{
		ViewInternal* pView = new ViewInternal();
		pView->Initialize(desc);
		return pView;
	}

	void DestroyView(View* pView)
	{		
		((ViewInternal*)pView)->Destroy();
		delete(pView);
	}

	bool ProcessViewEvents(View* pView)
	{
		return ((ViewInternal*)pView)->ProcessEvents();
	}

	void OnResize(void* pData)
	{
		ViewInternal* pView{ (ViewInternal*)pData };
		pView->Resize();
	}

	void BeginFrame(View* pView)
	{
		((ViewInternal*)pView)->BeginFrame();
	}

	void SubmitCommand(Device* pDevice, DirectCommand* pCommand)
	{
		std::lock_guard lock{ pDevice->commandLock };
		ID3D12GraphicsCommandList* pCommandList{ pCommand->pCommandList };
		pCommandList->Close();

		pDevice->pDirectCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&pCommandList);
		pDevice->pDirectCommandQueue->Signal(pDevice->pDirectFence, ++pDevice->directFenceValue);
		pDevice->pDirectCommandQueue->Signal(pCommand->pFence, ++pCommand->fenceValues[pCommand->commandIndex]);
	}

	void SubmitCommand(Device* pDevice, ComputeCommand* pCommand)
	{
		ID3D12GraphicsCommandList* pCommandList{ pCommand->pCommandList };
		pCommandList->Close();
		pDevice->pComputeCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&pCommandList);
		pDevice->pComputeCommandQueue->Signal(pDevice->pComputeFence, ++pDevice->computeFenceValue);
		pDevice->pComputeCommandQueue->Signal(pCommand->pFence, ++pCommand->fenceValues[pCommand->commandIndex]);
	}

	void SubmitCommand(Device* pDevice, CopyCommand* pCommand)
	{
		ID3D12GraphicsCommandList* pCommandList{ pCommand->pCommandList };
		pCommandList->Close();
		pDevice->pCopyCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&pCommandList);
		pDevice->pCopyCommandQueue->Signal(pDevice->pCopyFence, ++pDevice->copyFenceValue);
		pDevice->pCopyCommandQueue->Signal(pCommand->pFence, ++pCommand->fenceValues[pCommand->commandIndex]);
	}

	void EndFrame(View* pView)
	{
		((ViewInternal*)pView)->EndFrame();
	}

	const Input* GetInput(View* pView)
	{
		return ((ViewInternal*)pView)->GetInput();
	}	

	class Buffer
	{
	public:
		[[nodiscard]] inline dU32 GetByteSize() const { return m_byteSize; }
		[[nodiscard]] inline EBufferUsage GetUsage() const { return m_usage; }
		[[nodiscard]] const ID3D12Resource* GetResource() const { return m_pBuffer; }
		[[nodiscard]] ID3D12Resource* GetResource() { return m_pBuffer; }
		[[nodiscard]] dU32 GetOffset() const { return m_currentBuffer * m_byteSize; }
		[[nodiscard]] dU32 GetCurrentBufferIndex() const { return m_currentBuffer; }
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetGPUAdress() const { return m_pBuffer->GetGPUVirtualAddress() + GetOffset(); }
		[[nodiscard]] const DescriptorHandle& GetSRV() const { Assert(m_usage == EBufferUsage::Structured); return m_pDescriptors[m_currentBuffer]; }
		[[nodiscard]] const DescriptorHandle& GetCBV() const { Assert(m_usage == EBufferUsage::Constant); return m_pDescriptors[m_currentBuffer]; }
		[[nodiscard]] const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { Assert(m_usage == EBufferUsage::Index); return m_indexBufferView; }
		[[nodiscard]] const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { Assert(m_usage == EBufferUsage::Vertex); return m_vertexBufferView; }

		void MapData(const void* pData, dU32 byteOffset, dU32 byteSize)
		{
			Assert(byteSize <= m_byteSize);

			dU32 bufferOffset{ CycleBuffer() };
			memcpy(m_cpuAdress + bufferOffset + byteOffset, pData, byteSize);
		}

		void UploadData(const void* pData, dU32  byteOffset, dU32 byteSize)
		{
			Assert(byteSize <= m_byteSize);

			dU64 bufferOffset{ CycleBuffer() };

			RingBufferAllocation allocation;
			m_pDeviceInterface->ringBufferAllocator.Allocate(allocation, m_byteSize);
			
			memcpy(allocation.pCpuAdress, pData, byteSize);
			m_pDeviceInterface->WaitForCopy();
			m_pDeviceInterface->CopyBufferRegion(m_pBuffer, bufferOffset + byteOffset, allocation.pResource, allocation.offset, byteSize);
			
			m_pDeviceInterface->ringBufferAllocator.Free(allocation, m_pDeviceInterface->pCopyFence, m_pDeviceInterface->copyFenceValue);
		}

	private:
		Buffer(Device* pDeviceInterface, const BufferDesc& desc)
			: m_usage{ desc.usage }
			, m_memory{ desc.memory }
			, m_byteSize{ desc.byteSize }
			, m_byteStride{ desc.byteStride }
			, m_currentBuffer{ 0 }
			, m_pDeviceInterface{ pDeviceInterface }
		{
			D3D12_HEAP_PROPERTIES heapProps{};
			D3D12_RESOURCE_STATES resourceState{};

			if (m_usage == EBufferUsage::Constant)
			{
				m_byteSize = Utils::AlignTo(m_byteSize, 256);
			}

			if (m_memory == EBufferMemory::CPU)
			{
				heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
				resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
			}
			else
			{
				heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
				resourceState = D3D12_RESOURCE_STATE_COMMON;
			}

			dU32 bufferCount = (m_memory == EBufferMemory::GPUStatic) ? 1 : 3;

			D3D12_RESOURCE_DESC resourceDesc{ CD3DX12_RESOURCE_DESC::Buffer(bufferCount * m_byteSize) };

			ThrowIfFailed(m_pDeviceInterface->pDevice->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				resourceState,
				nullptr,
				IID_PPV_ARGS(&m_pBuffer)));
			m_pBuffer->SetName(desc.debugName);

			CreateDescriptors();

			if (m_memory == EBufferMemory::CPU)
			{
				D3D12_RANGE readRange{};
				ThrowIfFailed(m_pBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cpuAdress)));
				if (desc.pData)
					memcpy(m_cpuAdress, desc.pData, m_byteSize);
			}
			else if ( desc.pData )
			{				
				RingBufferAllocation allocation;
				m_pDeviceInterface->ringBufferAllocator.Allocate(allocation, m_byteSize);

				memcpy(allocation.pCpuAdress, desc.pData, m_byteSize);
				m_pDeviceInterface->WaitForCopy();
				m_pDeviceInterface->CopyBufferRegion(m_pBuffer, 0, allocation.pResource, allocation.offset, m_byteSize);

				m_pDeviceInterface->ringBufferAllocator.Free(allocation, m_pDeviceInterface->pCopyFence, m_pDeviceInterface->copyFenceValue);
			}
		}

		~Buffer()
		{
			m_pDeviceInterface->ReleaseResource(m_pBuffer);

			if (m_usage == EBufferUsage::Structured || m_usage == EBufferUsage::Constant)
			{
				dU32 viewCount{ (m_memory == EBufferMemory::GPUStatic) ? 1u : 3 };
				for (dU32 i = 0u; i < viewCount; i++)
					m_pDeviceInterface->srvHeap.Free(m_pDescriptors[i]);
				delete[] m_pDescriptors;
			}
		}
		DISABLE_COPY_AND_MOVE(Buffer);

		dU32 CycleBuffer()
		{
			Assert(m_memory != EBufferMemory::GPUStatic);

			m_currentBuffer = (m_currentBuffer + 1) % 3;
			return m_currentBuffer * m_byteSize;
		}

		void CreateDescriptors()
		{
			dU32 bufferCount{ (m_memory == EBufferMemory::GPUStatic) ? 1u : 3u };
			ID3D12Device* pDevice{ m_pDeviceInterface->pDevice };

			switch (m_usage)
			{
			case EBufferUsage::Vertex:
			{
				m_vertexBufferView.BufferLocation = m_pBuffer->GetGPUVirtualAddress();
				m_vertexBufferView.StrideInBytes = m_byteStride;
				m_vertexBufferView.SizeInBytes = m_byteSize;
				break;
			}
			case EBufferUsage::Index:
			{
				Assert(m_byteStride == sizeof(dU32) || m_byteStride == sizeof(dU16));
				m_indexBufferView.BufferLocation = m_pBuffer->GetGPUVirtualAddress();
				m_indexBufferView.Format = (m_byteStride == sizeof(dU32)) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
				m_indexBufferView.SizeInBytes = m_byteSize;
				break;
			}
			case EBufferUsage::Constant:
			{
				m_pDescriptors = new DescriptorHandle[bufferCount];
				for (dU32 i = 0; i < bufferCount; i++)
					m_pDescriptors[i] = m_pDeviceInterface->srvHeap.Allocate();

				D3D12_CONSTANT_BUFFER_VIEW_DESC desc
				{
					.BufferLocation = m_pBuffer->GetGPUVirtualAddress(),
					.SizeInBytes = (dU32)m_byteSize
				};

				pDevice->CreateConstantBufferView(&desc, m_pDescriptors[m_currentBuffer].cpuAdress);
				break;
			}

			case EBufferUsage::Structured:
			{
				m_pDescriptors = new DescriptorHandle[bufferCount];
				for (dU32 i = 0; i < bufferCount; i++)
					m_pDescriptors[i] = m_pDeviceInterface->srvHeap.Allocate();

				dU32 elementCount = (dU32)(m_byteSize / m_byteStride);
				D3D12_SHADER_RESOURCE_VIEW_DESC desc
				{
					.Format = DXGI_FORMAT_UNKNOWN,
					.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
					.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
					.Buffer
					{
						.NumElements = elementCount,
						.StructureByteStride = m_byteStride,
						.Flags = D3D12_BUFFER_SRV_FLAG_NONE,
					}
				};
				for (dU32 i = 0; i < bufferCount; i++)
				{
					desc.Buffer.FirstElement = elementCount * i;
					pDevice->CreateShaderResourceView(m_pBuffer, &desc, m_pDescriptors[i].cpuAdress);
				}
				break;
			}
			default:
				break;
			}
		}
	private:
		friend Pool<Buffer, Buffer>;

		dU8*				m_cpuAdress;
		ID3D12Resource*		m_pBuffer;
		const EBufferUsage	m_usage;
		const EBufferMemory m_memory;
		dU32				m_byteSize;
		dU32				m_byteStride;
		dU32				m_currentBuffer;
		Device*				m_pDeviceInterface{ nullptr };

		union
		{
			DescriptorHandle*			m_pDescriptors;
			D3D12_INDEX_BUFFER_VIEW		m_indexBufferView;
			D3D12_VERTEX_BUFFER_VIEW	m_vertexBufferView;
		};
	};

	Handle<Buffer> CreateBuffer(Device* pDevice,const BufferDesc& desc)
	{
		return g_bufferPool.Create(pDevice, desc);
	}

	void ReleaseBuffer(Handle<Buffer> handle)
	{
		g_bufferPool.Remove(handle);
	}

	void UploadBuffer(Handle<Buffer> handle, const void* pData, dU32 byteOffset, dU32 byteSize)
	{
		g_bufferPool.Get(handle).UploadData(pData, byteOffset, byteSize);
	}

	void MapBuffer(Handle<Buffer> handle, const void* pData, dU32 byteOffset, dU32 byteSize)
	{
		g_bufferPool.Get(handle).MapData(pData, byteOffset, byteSize);
	}

	Handle<Mesh> CreateMesh(Device* pDevice, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		return g_meshPool.Create(pDevice, pIndices, indexCount, pVertices, vertexCount, vertexByteStride);
	}

	Handle<Mesh> CreateMesh(Device* pDevice, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		return g_meshPool.Create(pDevice, pIndices, indexCount, pVertices, vertexCount, vertexByteStride);
	}

	void ReleaseMesh(Handle<Mesh> handle)
	{
		g_meshPool.Remove(handle);
	}

	const Mesh& GetMesh(Handle<Mesh> handle)
	{
		return g_meshPool.Get(handle);
	}

	class Texture
	{
	public:
		[[nodiscard]] ID3D12Resource* GetResource() { return m_pTexture; }
		[[nodiscard]] const DescriptorHandle& GetSRV() { return m_SRV; }
		[[nodiscard]] const DescriptorHandle& GetRTV(dU32 index = 0) { Assert(m_usage == ETextureUsage::RTV); return m_RTV[index]; }
		[[nodiscard]] const DescriptorHandle& GetDSV(dU32 index = 0) { Assert(m_usage == ETextureUsage::DSV); return m_DSV[index]; }
		[[nodiscard]] const dU32* GetDimensions() { return m_dimensions; }
		[[nodiscard]] D3D12_RESOURCE_STATES GetState() { return m_state; }
		void SetState(D3D12_RESOURCE_STATES state) { m_state = state; }
		[[nodiscard]] const float* GetClearValue() { return m_clearValue; }
	private:

		Texture(Device* pDeviceInterface, const TextureDesc& desc)
			: m_usage{ desc.usage }
			, m_dimensions{ desc.dimensions[0], desc.dimensions[1], desc.dimensions[2] }
			, m_pDeviceInterface{ pDeviceInterface }
		{

			memcpy(m_clearValue, desc.clearValue, sizeof(float) * 4);

			ID3D12Device* pDevice{ m_pDeviceInterface->pDevice };
			D3D12_CLEAR_VALUE clearValue{};
			D3D12_CLEAR_VALUE* pClearValue{ nullptr };

			DXGI_FORMAT format{ (DXGI_FORMAT)desc.format };
			D3D12_RESOURCE_FLAGS resourceFlag;
			switch (m_usage)
			{
			case ETextureUsage::RTV:
				resourceFlag = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				clearValue = CD3DX12_CLEAR_VALUE{ format, desc.clearValue };
				pClearValue = &clearValue;
				break;
			case ETextureUsage::DSV:
				resourceFlag = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				clearValue = CD3DX12_CLEAR_VALUE{ format, desc.clearValue };
				pClearValue = &clearValue;
				break;
			case ETextureUsage::SRV:
				resourceFlag = D3D12_RESOURCE_FLAG_NONE;
				break;
			case ETextureUsage::UAV:
				resourceFlag = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				break;
			default:
				Assert(false);
				break;
			}

			CD3DX12_RESOURCE_DESC textureResourceDesc(
				D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				0,
				m_dimensions[0],
				m_dimensions[1],
				m_dimensions[2],
				desc.mipLevels,
				format,
				1,
				0,
				D3D12_TEXTURE_LAYOUT_UNKNOWN,
				resourceFlag);

			D3D12_HEAP_PROPERTIES heapProps{ CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT) };

			ThrowIfFailed(pDevice->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&textureResourceDesc,
				D3D12_RESOURCE_STATE_COMMON,
				pClearValue,
				IID_PPV_ARGS(&m_pTexture)));
			NameDXObject(m_pTexture, desc.debugName);

			if (desc.pData)
			{
				dU32 numSubresource = desc.mipLevels;
				Assert(numSubresource <= 16);

				D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[16];
				dU32 rowsCount[16];
				dU64 rowsSizeInBytes[16];
				dU64 byteSize = 0;
				pDevice->GetCopyableFootprints(&textureResourceDesc, 0, numSubresource, 0, layouts, rowsCount, rowsSizeInBytes, &byteSize);

				RingBufferAllocation allocation;
				m_pDeviceInterface->ringBufferAllocator.Allocate(allocation, byteSize);

				D3D12_SUBRESOURCE_DATA srcDatas[16];
				LONG_PTR prevSlice = 0;
				for (dU32 i = 0; i < numSubresource; i++)
				{
					LONG_PTR slicePitch = rowsSizeInBytes[i] * rowsCount[i];
					srcDatas[i] = { .pData = (dU8*) desc.pData + prevSlice, .RowPitch = (LONG_PTR)rowsSizeInBytes[i], .SlicePitch = slicePitch };
					prevSlice += slicePitch;
				}
						
				m_pDeviceInterface->UploadTexture(m_pTexture, allocation.pResource, allocation.offset, 0, numSubresource, srcDatas);

				m_pDeviceInterface->ringBufferAllocator.Free(allocation, m_pDeviceInterface->pCopyFence, m_pDeviceInterface->copyFenceValue);
			}

			switch (m_usage)
			{
			case ETextureUsage::RTV:
			{
				D3D12_RENDER_TARGET_VIEW_DESC rtvDesc
				{
					.Format = format
				};

				m_RTV = new DescriptorHandle[m_dimensions[2]];

				if (m_dimensions[2] > 1)
				{
					rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
					rtvDesc.Texture2DArray.ArraySize = 1;
					rtvDesc.Texture2DArray.MipSlice = 0;
					rtvDesc.Texture2DArray.PlaneSlice = 0;

					for (dU32 i{ 0 }; i < m_dimensions[2]; i++)
					{
						rtvDesc.Texture2DArray.FirstArraySlice = D3D12CalcSubresource(0, i, 0, 1, 1);
						m_RTV[i] = m_pDeviceInterface->rtvHeap.Allocate();
						pDevice->CreateRenderTargetView(m_pTexture, &rtvDesc, m_RTV[i].cpuAdress);
					}
				}
				else
				{
					rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
					rtvDesc.Texture2D.PlaneSlice = 0;
					rtvDesc.Texture2D.MipSlice = 0;
					m_RTV[0] = m_pDeviceInterface->rtvHeap.Allocate();
					pDevice->CreateRenderTargetView(m_pTexture, &rtvDesc, m_RTV[0].cpuAdress);
				}

				break;
			}
			case ETextureUsage::DSV:
			{
				D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
				dsvDesc.Format = format;
				m_DSV = new DescriptorHandle[m_dimensions[2]];

				if (m_dimensions[2] > 1)
				{
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
					dsvDesc.Texture2DArray.ArraySize = 1;
					dsvDesc.Texture2DArray.MipSlice = 0;

					for (dU32 i{ 0 }; i < m_dimensions[2]; i++)
					{
						dsvDesc.Texture2DArray.FirstArraySlice = D3D12CalcSubresource(0, i, 0, 1, 1);
						m_DSV[i] = m_pDeviceInterface->dsvHeap.Allocate();
						pDevice->CreateDepthStencilView(m_pTexture, &dsvDesc, m_DSV[i].cpuAdress);
					}
				}
				else
				{
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					dsvDesc.Texture2D.MipSlice = 0;
					m_DSV[0] = m_pDeviceInterface->dsvHeap.Allocate();
					pDevice->CreateDepthStencilView(m_pTexture, &dsvDesc, m_DSV[0].cpuAdress);
				}

				break;
			}
			case ETextureUsage::SRV:
				break;
			default:
				Assert(false);
				break;
			}

			m_SRV = m_pDeviceInterface->srvHeap.Allocate();
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

			srvDesc.Format = (m_usage == ETextureUsage::DSV) ? (DXGI_FORMAT)(format + 1) : format;

			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			if (m_dimensions[2] > 1)
			{
				srvDesc.Texture2DArray.ArraySize = m_dimensions[2];
				srvDesc.Texture2DArray.FirstArraySlice = 0;
				srvDesc.Texture2DArray.MipLevels = desc.mipLevels;
				srvDesc.Texture2DArray.MostDetailedMip = 0;
				srvDesc.Texture2DArray.PlaneSlice = 0;
				srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			}
			else
			{
				srvDesc.Texture2D.MipLevels = desc.mipLevels;
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.PlaneSlice = 0;
				srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			}

			pDevice->CreateShaderResourceView(m_pTexture, &srvDesc, m_SRV.cpuAdress);
		}

		~Texture()
		{
			m_pDeviceInterface->ReleaseResource(m_pTexture);
			m_pDeviceInterface->srvHeap.Free(m_SRV);
			if (m_usage == ETextureUsage::RTV)
			{
				for (dU32 i = 0; i < m_dimensions[2]; i++)
					m_pDeviceInterface->rtvHeap.Free(m_RTV[i]);
				delete[] m_RTV;
			}
			else if (m_usage == ETextureUsage::DSV)
			{
				for (dU32 i = 0; i < m_dimensions[2]; i++)
					m_pDeviceInterface->dsvHeap.Free(m_DSV[i]);
				delete[] m_DSV;
			}
		}

		DISABLE_COPY_AND_MOVE(Texture);

	private:
		friend Pool<Texture, Texture>;

		DescriptorHandle		m_SRV;
		union
		{
			DescriptorHandle* m_RTV;
			DescriptorHandle* m_DSV;
		};

		ID3D12Resource*			m_pTexture;
		const ETextureUsage		m_usage;
		const dU32				m_dimensions[3];
		float					m_clearValue[4];
		D3D12_RESOURCE_STATES	m_state{ D3D12_RESOURCE_STATE_COMMON };
		Device*					m_pDeviceInterface{ nullptr };
	};

	Handle<Texture> CreateTexture(Device* pDevice, const TextureDesc& desc)
	{
		return g_texturePool.Create(pDevice, desc);
	}

	void ReleaseTexture(Handle<Texture> handle)
	{
		g_texturePool.Remove(handle);
	}

	constexpr const wchar_t* GetTargetProfile(EShaderStage type)
	{
		switch(type)
		{
		case EShaderStage::Vertex:
			return L"vs_6_6";
		case EShaderStage::Pixel:
			return L"ps_6_6";
		case EShaderStage::Compute:
			return L"cs_6_6";
		default:
			Assert(false);
		}
		return L"";
	}

	class Shader
	{
	public:
		IDxcBlob* GetByteCode() const { return m_pByteCode; }

	private:
		Shader(const ShaderDesc& desc)
		{
			Microsoft::WRL::ComPtr<IDxcCompiler3> pCompiler{ nullptr };
			ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));

			Microsoft::WRL::ComPtr<IDxcUtils> pUtils{ nullptr };
			ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));

			Microsoft::WRL::ComPtr<IDxcIncludeHandler> pIncludeHandler{ nullptr };
			pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

			dU32 codePage{ CP_UTF8 };

			Microsoft::WRL::ComPtr<IDxcBlobEncoding> pSourceBlob{ nullptr };
			Microsoft::WRL::ComPtr<IDxcCompilerArgs> pArgs{ nullptr };
			ThrowIfFailed(pUtils->BuildArguments(desc.filePath, desc.entryFunc, GetTargetProfile(desc.stage), desc.args, desc.argsCount, NULL, 0, &pArgs));
			ThrowIfFailed(pUtils->LoadFile(desc.filePath, &codePage, &pSourceBlob));
			Microsoft::WRL::ComPtr<IDxcResult> pResult{ nullptr };

			const DxcBuffer sourceBuffer
			{
				.Ptr = pSourceBlob->GetBufferPointer(),
				.Size = pSourceBlob->GetBufferSize(),
				.Encoding = codePage,
			};

			ThrowIfFailed(pCompiler->Compile(
				&sourceBuffer,
				pArgs->GetArguments(), pArgs->GetCount(),
				pIncludeHandler.Get(),	
				IID_PPV_ARGS(&pResult)
			));

			Microsoft::WRL::ComPtr<IDxcBlobEncoding> pErrorsBlob{ nullptr };
			if (SUCCEEDED(pResult->GetErrorBuffer(&pErrorsBlob)) && pErrorsBlob)
			{
				OutputDebugStringA((const char*)pErrorsBlob->GetBufferPointer());
			}

			pResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&m_pByteCode), nullptr);
		}

		~Shader()
		{
			m_pByteCode->Release();
		}

		DISABLE_COPY_AND_MOVE(Shader);

	private:
		friend Pool<Shader, Shader>;

		IDxcBlob* m_pByteCode{ nullptr };
	};

	Handle<Shader>	CreateShader(const ShaderDesc& desc)
	{
		return g_shaderPool.Create(desc);
	}

	void ReleaseShader(Handle<Shader> handle)
	{
		g_shaderPool.Remove(handle);
	}

	constexpr D3D12_SHADER_VISIBILITY ConvertShaderVisibility(EShaderVisibility stage)
	{
		switch (stage)
		{
		case EShaderVisibility::Vertex:
			return D3D12_SHADER_VISIBILITY_VERTEX;
		case EShaderVisibility::Pixel:
			return D3D12_SHADER_VISIBILITY_PIXEL;
		case EShaderVisibility::All:
		default:
			return D3D12_SHADER_VISIBILITY_ALL;
		}
	}

	ID3D12RootSignature* ComputeRootSignature(ID3D12Device* pDevice, const BindingLayout& layout, D3D12_ROOT_SIGNATURE_FLAGS flags)
	{
		// 64 descriptor tables with 3 types of resource at most
		CD3DX12_DESCRIPTOR_RANGE1 ranges[3 * 64];
		dU32 rangeIndex{ 0 };

		// 64 parameters at most
		CD3DX12_ROOT_PARAMETER1 rootParameters[64];
		dU32 rootParamCount{ 0 };
		dU32 bufferRegister[(dU32)EShaderVisibility::Count]{ 0 };
		dU32 resourceRegister[(dU32)EShaderVisibility::Count]{ 0 };
		dU32 uavRegister[(dU32)EShaderVisibility::Count]{ 0 };
		dU32 samplerRegister[(dU32)EShaderVisibility::Count]{ 0 };

		// TODO : Specify visibility and flags
		for (dU8 i = 0; i < layout.slotCount; i++)
		{
			const BindingSlot& slot{ layout.slots[i] };

			dU32 bufferRegisterOffset;
			dU32 resourceRegisterOffset;
			dU32 uavRegisterOffset;
			dU32 samplerRegisterOffset;
			if (slot.visibility == EShaderVisibility::All)
			{
				bufferRegisterOffset = resourceRegisterOffset = uavRegisterOffset = samplerRegisterOffset = 0;
				flags &= ~(D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS);
			}
			else
			{
				bufferRegisterOffset = bufferRegister[(dU32)EShaderVisibility::All];
				resourceRegisterOffset = resourceRegister[(dU32)EShaderVisibility::All];
				uavRegisterOffset = uavRegister[(dU32)EShaderVisibility::All];
				samplerRegisterOffset = samplerRegister[(dU32)EShaderVisibility::All];
			}

			switch (slot.visibility)
			{
			case EShaderVisibility::Vertex:
				flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
				break;
			case EShaderVisibility::Pixel:
				flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
				break;
			case EShaderVisibility::All:
			default:
				break;
			}

			D3D12_SHADER_VISIBILITY visibility{ ConvertShaderVisibility( slot.visibility ) };
			switch (slot.type)
			{
			case EBindingType::Constant:
			{
				rootParameters[rootParamCount++].InitAsConstants( slot.byteSize, bufferRegisterOffset + bufferRegister[(dU32)slot.visibility]++, 0u, visibility) ;
				break;
			}
			case EBindingType::Buffer:
			{
				rootParameters[rootParamCount++].InitAsConstantBufferView( bufferRegisterOffset+ bufferRegister[(dU32)slot.visibility]++, 0u, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, visibility );
				break;
			}

			case EBindingType::Resource:
			{
				rootParameters[rootParamCount++].InitAsShaderResourceView( resourceRegisterOffset + resourceRegister[(dU32)slot.visibility]++, 0u, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, visibility );
				break;
			}
			case EBindingType::UAV:
			{
				rootParameters[rootParamCount++].InitAsUnorderedAccessView( uavRegisterOffset + uavRegister[(dU32)slot.visibility]++, 0u, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, visibility );
				break;
			}
			case EBindingType::Group:
			{
				dU32 rangeCount{ 0 };

				if (slot.groupDesc.bufferCount > 0)
				{
					ranges[rangeIndex + rangeCount++].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, slot.groupDesc.bufferCount, bufferRegisterOffset + bufferRegister[(dU32)slot.visibility] );
					bufferRegister[(dU32)slot.visibility] += slot.groupDesc.bufferCount;
				}

				if (slot.groupDesc.uavCount > 0)
				{
					ranges[rangeIndex + rangeCount++].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, slot.groupDesc.uavCount, uavRegisterOffset + uavRegister[(dU32)slot.visibility] );
					uavRegister[(dU32)slot.visibility] += slot.groupDesc.uavCount;
				}

				if (slot.groupDesc.resourceCount > 0)
				{
					ranges[rangeIndex + rangeCount++].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, slot.groupDesc.resourceCount, resourceRegisterOffset + resourceRegister[(dU32)slot.visibility] );
					resourceRegister[(dU32)slot.visibility] += slot.groupDesc.resourceCount;
				}

				rootParameters[rootParamCount++].InitAsDescriptorTable(rangeCount, &ranges[rangeIndex], visibility);
				rangeIndex += rangeCount;
				break;
			}
			case EBindingType::Samplers:
			{
				ranges[rangeIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, slot.samplerCount, samplerRegisterOffset + samplerRegister[(dU32)slot.visibility] );
				samplerRegister[(dU32)slot.visibility] += slot.samplerCount;
				rootParameters[rootParamCount++].InitAsDescriptorTable(1, &ranges[rangeIndex++], visibility);
				break;
			}
			}
		}

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		CD3DX12_STATIC_SAMPLER_DESC1 staticSamplerDesc;
		staticSamplerDesc.Init(0);
		staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootSignatureDesc.Init_1_1(rootParamCount, rootParameters, 1, (D3D12_STATIC_SAMPLER_DESC*)&staticSamplerDesc, flags);

		Microsoft::WRL::ComPtr<ID3DBlob> pError{ nullptr };
		Microsoft::WRL::ComPtr<ID3DBlob> pSignature{ nullptr };
		if (FAILED(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &pSignature, &pError)))
		{
			OutputDebugStringA((const char*)pError->GetBufferPointer());
			Assert(0);
		}
		ID3D12RootSignature* pRootSignature{ nullptr };
		ThrowIfFailed(pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&pRootSignature)));
		return pRootSignature;
	}
	
	constexpr D3D12_COMPARISON_FUNC ConvertDepthFunc(ECompFunc func)
	{
		switch (func)
		{
		case ECompFunc::NONE:
			return D3D12_COMPARISON_FUNC_NONE;
		case ECompFunc::NEVER:
			return D3D12_COMPARISON_FUNC_NEVER;
		case ECompFunc::LESS:
			return D3D12_COMPARISON_FUNC_LESS;
		case ECompFunc::EQUAL:
			return D3D12_COMPARISON_FUNC_EQUAL;
		case ECompFunc::LESS_EQUAL:
			return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case ECompFunc::GREATER:
			return D3D12_COMPARISON_FUNC_GREATER;
		case ECompFunc::NOT_EQUAL:
			return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case ECompFunc::GREATER_EQUAL:
			return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case ECompFunc::ALWAYS:
			return D3D12_COMPARISON_FUNC_ALWAYS;
		default:
			break;
		}
		return D3D12_COMPARISON_FUNC_NONE;
	}

	class Pipeline
	{
	public:
		[[nodiscard]] ID3D12PipelineState* GetPipelineStateObject() { return m_pPipelineState; }
		[[nodiscard]] ID3D12RootSignature* GetRootSignature() { return m_pRootSignature; }

	private:
		Pipeline(const GraphicsPipelineDesc& desc)
		{
			Assert(desc.pDevice);
			m_pDevice = desc.pDevice->pDevice;

			D3D12_ROOT_SIGNATURE_FLAGS flags
			{
				(desc.inputLayout.empty()) ? D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
				| D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
				| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
				| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
				| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
				| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
				| D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS
				| D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS
			};

			m_pRootSignature = ComputeRootSignature(desc.pDevice->pDevice, desc.bindingLayout, flags);

			D3D12_INPUT_ELEMENT_DESC* pInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[desc.inputLayout.size()];
			for (dU32 i = 0; i < desc.inputLayout.size(); i++)
			{
				const VertexInput& input = desc.inputLayout[i];
				pInputElementDescs[i] = { input.pName, input.index, (DXGI_FORMAT)input.format, input.slot, input.byteAlignedOffset, input.bPerInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
			}

			IDxcBlob* pVSBlob{ (desc.vertexShader.IsValid()) ? g_shaderPool.Get(desc.vertexShader).GetByteCode() : nullptr};
			IDxcBlob* pPSBlob{ (desc.vertexShader.IsValid()) ? g_shaderPool.Get(desc.pixelShader).GetByteCode() : nullptr};

			// Describe and create the graphics pipeline state object (PSO).
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
			psoDesc.InputLayout = { pInputElementDescs, (dU32)desc.inputLayout.size()};
			psoDesc.pRootSignature = m_pRootSignature;
			psoDesc.VS.BytecodeLength = pVSBlob->GetBufferSize();
			psoDesc.VS.pShaderBytecode = pVSBlob->GetBufferPointer();
			psoDesc.PS.BytecodeLength = (pPSBlob) ? pPSBlob->GetBufferSize() : 0;
			psoDesc.PS.pShaderBytecode = (pPSBlob) ? pPSBlob->GetBufferPointer() : nullptr;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.FillMode = (desc.rasterizerState.bWireframe) ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
			psoDesc.RasterizerState.CullMode = (D3D12_CULL_MODE) desc.rasterizerState.cullingMode;
			psoDesc.RasterizerState.DepthBias = desc.rasterizerState.depthBias;
			psoDesc.RasterizerState.DepthBiasClamp = desc.rasterizerState.depthBiasClamp;
			psoDesc.RasterizerState.SlopeScaledDepthBias = desc.rasterizerState.slopeScaledDepthBias;
			psoDesc.RasterizerState.DepthClipEnable = desc.rasterizerState.bDepthClipEnable;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = desc.depthStencilState.bDepthEnabled;
			psoDesc.DepthStencilState.DepthWriteMask = (desc.depthStencilState.bDepthWrite) ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
			psoDesc.DepthStencilState.DepthFunc = ConvertDepthFunc(desc.depthStencilState.bDepthFunc);
			psoDesc.SampleMask = UINT_MAX;
			dU32 renderTargetCount = (dU32) desc.renderTargetsFormat.size();
			psoDesc.NumRenderTargets = renderTargetCount;
			for (dU32 i = 0; i < renderTargetCount; i++)
			{
				psoDesc.RTVFormats[i] = (DXGI_FORMAT)desc.renderTargetsFormat[i];
			}
			psoDesc.DSVFormat = (DXGI_FORMAT)desc.depthStencilFormat;
			psoDesc.SampleDesc.Count = 1;
			ThrowIfFailed(m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));
			delete[] pInputElementDescs;
		}

		~Pipeline()
		{
			m_pRootSignature->Release();
			m_pPipelineState->Release();
		}

		DISABLE_COPY_AND_MOVE(Pipeline);

	private:
		friend Pool<Pipeline, Pipeline>;

		ID3D12RootSignature* m_pRootSignature{ nullptr };
		ID3D12PipelineState* m_pPipelineState{ nullptr };
		ID3D12Device* m_pDevice{ nullptr };
	};

	Handle<Pipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
	{
		return g_pipelinePool.Create(desc);
	}

	void ReleasePipeline(Handle<Pipeline> handle)
	{
		g_pipelinePool.Remove(handle);
	}

	Command* InitializeCommand(ID3D12Device* pDevice, Command* pCommand, D3D12_COMMAND_LIST_TYPE type)
	{
		for (dU32 i = 0; i < pCommand->commandCount; i++)
		{
			ThrowIfFailed(pDevice->CreateCommandAllocator(type, IID_PPV_ARGS(&pCommand->pCommandAllocators[i])));
			NameDXObject(pCommand->pCommandAllocators[i], L"CommandAllocator");
		}

		ThrowIfFailed(pDevice->CreateCommandList(0, type, pCommand->pCommandAllocators[0], nullptr, IID_PPV_ARGS(&pCommand->pCommandList)));
		ThrowIfFailed(pCommand->pCommandList->Close());
		NameDXObject(pCommand->pCommandList, L"CommandList");

		ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pCommand->pFence)));
		NameDXObject(pCommand->pFence, L"Fence");

		return pCommand;
	}

	DirectCommand* CreateDirectCommand(const DirectCommandDesc& desc)
	{
		DirectCommand* pCommand = new DirectCommand();
		Assert(desc.pView);
		ViewInternal* pView = (ViewInternal*)desc.pView;
		ID3D12Device* pDevice{ pView->GetDevice()->pDevice };
		pCommand->commandCount = pView->GetFrameCount();
		pCommand->heaps[0] = pView->GetDevice()->srvHeap.Get();
		pCommand->heaps[1] = pView->GetDevice()->samplerHeap.Get();
		InitializeCommand(pDevice, pCommand, D3D12_COMMAND_LIST_TYPE_DIRECT);

		return pCommand;
	}

	ComputeCommand* CreateComputeCommand(const ComputeCommandDesc& desc)
	{
		ComputeCommand* pCommand = new ComputeCommand();
		Assert(desc.pDevice);
		Device* pDeviceInterface = desc.pDevice;
		ID3D12Device* pDevice{ pDeviceInterface->pDevice };
		pCommand->commandCount = (dU32) desc.buffering;
		pCommand->heaps[0] = pDeviceInterface->srvHeap.Get();
		pCommand->heaps[1] = pDeviceInterface->samplerHeap.Get();

		InitializeCommand(pDevice, pCommand, D3D12_COMMAND_LIST_TYPE_COMPUTE);

		return pCommand;
	}

	CopyCommand* CreateCopyCommand(const CopyCommandDesc& desc)
	{
		CopyCommand* pCommand = new CopyCommand();
		Assert(desc.pDevice);
		Device* pDeviceInterface = desc.pDevice;
		ID3D12Device* pDevice{ pDeviceInterface->pDevice };
		pCommand->commandCount = (dU32)desc.buffering;

		InitializeCommand(pDevice, pCommand, D3D12_COMMAND_LIST_TYPE_COPY);

		return pCommand;
	}

	void WaitCommand(Command* pCommand)
	{
		dU64 fenceValue = pCommand->fenceValues[pCommand->commandIndex];
		if (pCommand->pFence->GetCompletedValue() < fenceValue)
		{
			ThrowIfFailed(pCommand->pFence->SetEventOnCompletion(fenceValue, NULL));
		}
	}

	void ReleaseCommand(Command* pCommand)
	{
		WaitCommand(pCommand);
		pCommand->pCommandList->Release();
		for (dU32 i = 0; i < pCommand->commandCount; i++)
		{
			pCommand->pCommandAllocators[i]->Release();
		}
		pCommand->pFence->Release();
	}

	void DestroyCommand(DirectCommand* pCommand)
	{
		ReleaseCommand(pCommand);
		delete pCommand;
	}

	void DestroyCommand(ComputeCommand* pCommand)
	{
		ReleaseCommand(pCommand);
		delete pCommand;
	}

	void DestroyCommand(CopyCommand* pCommand)
	{
		ReleaseCommand(pCommand);
		delete pCommand;
	}

	void ResetCommand(DirectCommand* pCommand)
	{
		dU32 commandIndex = pCommand->commandIndex = (pCommand->commandIndex + 1) % pCommand->commandCount;
		WaitCommand(pCommand);
		ThrowIfFailed(pCommand->pCommandAllocators[commandIndex]->Reset());
		ThrowIfFailed(pCommand->pCommandList->Reset(pCommand->pCommandAllocators[commandIndex], nullptr));
		pCommand->pCommandList->SetDescriptorHeaps(2, pCommand->heaps);
	}

	void ResetCommand(DirectCommand* pCommand, Handle<Pipeline> handle)
	{
		dU32 commandIndex = pCommand->commandIndex = (pCommand->commandIndex + 1) % pCommand->commandCount;
		WaitCommand(pCommand);
		Pipeline& pipeline{ g_pipelinePool.Get(handle) };
		ThrowIfFailed(pCommand->pCommandAllocators[commandIndex]->Reset());
		ThrowIfFailed(pCommand->pCommandList->Reset(pCommand->pCommandAllocators[commandIndex], pipeline.GetPipelineStateObject()));
		pCommand->pCommandList->SetGraphicsRootSignature(pipeline.GetRootSignature());
		pCommand->pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommand->pCommandList->SetDescriptorHeaps(2, pCommand->heaps);
	}

	void SetPipeline(DirectCommand* pCommand, Handle<Pipeline> handle)
	{
		Assert(handle.IsValid());
		Pipeline& pipeline{ g_pipelinePool.Get(handle) };
		pCommand->pCommandList->SetGraphicsRootSignature(pipeline.GetRootSignature());
		pCommand->pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommand->pCommandList->SetPipelineState(pipeline.GetPipelineStateObject());
	}

	void SetRenderTarget(DirectCommand* pCommand, Handle<Texture> renderTarget)
	{
		Assert(renderTarget.IsValid());
		Texture& texture{ g_texturePool.Get(renderTarget) };
		const dU32* pDimensions{ texture.GetDimensions() };
		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)pDimensions[0], (float)pDimensions[1], 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, (LONG)pDimensions[0], (LONG)pDimensions[1] };

		if (texture.GetState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(texture.GetResource(), texture.GetState(), D3D12_RESOURCE_STATE_RENDER_TARGET) };
			pCommand->pCommandList->ResourceBarrier(1, &barrier);
			texture.SetState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		pCommand->pCommandList->RSSetViewports(1, &viewport);
		pCommand->pCommandList->RSSetScissorRects(1, &scissor);
		pCommand->pCommandList->OMSetRenderTargets(1, &texture.GetRTV().cpuAdress, false, nullptr);
	}

	void SetRenderTarget(DirectCommand* pCommand, Handle<Texture> renderTarget, Handle<Texture> depthBuffer)
	{
		Assert(renderTarget.IsValid());
		Texture& texture{ g_texturePool.Get(renderTarget) };
		const dU32* pDimensions{ texture.GetDimensions() };
		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)pDimensions[0], (float)pDimensions[1], 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, (LONG)pDimensions[0], (LONG)pDimensions[1] };

		if (texture.GetState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(texture.GetResource(), texture.GetState(), D3D12_RESOURCE_STATE_RENDER_TARGET) };
			pCommand->pCommandList->ResourceBarrier(1, &barrier);
			texture.SetState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		Texture& depthTexture{ g_texturePool.Get(depthBuffer) };

		if (depthTexture.GetState() != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(depthTexture.GetResource(), depthTexture.GetState(), D3D12_RESOURCE_STATE_DEPTH_WRITE) };
			pCommand->pCommandList->ResourceBarrier(1, &barrier);
			depthTexture.SetState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}

		pCommand->pCommandList->RSSetViewports(1, &viewport);
		pCommand->pCommandList->RSSetScissorRects(1, &scissor);
		pCommand->pCommandList->OMSetRenderTargets(1, &texture.GetRTV().cpuAdress, false, &depthTexture.GetDSV().cpuAdress);
	}

	void SetRenderTarget(DirectCommand* pCommand, View* pViewInterface, Handle<Texture> depthBuffer)
	{
		ViewInternal* pView{ ((ViewInternal*)pViewInterface) };
		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)pView->GetWidth(), (float)pView->GetHeight(), 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, (LONG)pView->GetWidth(), (LONG)pView->GetHeight() };

		if (pView->GetCurrentBackBufferState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(pView->GetCurrentBackBuffer(), pView->GetCurrentBackBufferState(), D3D12_RESOURCE_STATE_RENDER_TARGET)};
			pCommand->pCommandList->ResourceBarrier(1, &barrier);
			pView->SetCurrentBackBufferState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		Texture& depthTexture{ g_texturePool.Get(depthBuffer) };

		if (depthTexture.GetState() != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(depthTexture.GetResource(), depthTexture.GetState(), D3D12_RESOURCE_STATE_DEPTH_WRITE) };
			pCommand->pCommandList->ResourceBarrier(1, &barrier);
			depthTexture.SetState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}

		pCommand->pCommandList->RSSetViewports(1, &viewport);
		pCommand->pCommandList->RSSetScissorRects(1, &scissor);
		pCommand->pCommandList->OMSetRenderTargets(1, &pView->GetCurrentBackBufferView().cpuAdress, false, &depthTexture.GetDSV().cpuAdress);
	}

	void SetRenderTarget(DirectCommand* pCommand, View* pViewInterface)
	{
		ViewInternal* pView{ ((ViewInternal*)pViewInterface) };
		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)pView->GetWidth(), (float)pView->GetHeight(), 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, (LONG)pView->GetWidth(), (LONG)pView->GetHeight() };

		if (pView->GetCurrentBackBufferState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(pView->GetCurrentBackBuffer(), pView->GetCurrentBackBufferState(), D3D12_RESOURCE_STATE_RENDER_TARGET) };
			pCommand->pCommandList->ResourceBarrier(1, &barrier);
			pView->SetCurrentBackBufferState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		pCommand->pCommandList->RSSetViewports(1, &viewport);
		pCommand->pCommandList->RSSetScissorRects(1, &scissor);
		pCommand->pCommandList->OMSetRenderTargets(1, &pView->GetCurrentBackBufferView().cpuAdress, false, nullptr);
	}

	void ClearRenderTarget(DirectCommand* pCommand, Handle<Texture> handle)
	{
		Texture& texture{ g_texturePool.Get(handle) };
		if (texture.GetState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(texture.GetResource(), texture.GetState(), D3D12_RESOURCE_STATE_RENDER_TARGET) };
			pCommand->pCommandList->ResourceBarrier(1, &barrier);
			texture.SetState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		pCommand->pCommandList->ClearRenderTargetView(texture.GetRTV().cpuAdress, texture.GetClearValue(), 0, nullptr);
	}

	void ClearRenderTarget(DirectCommand* pCommand, View* pViewInterface)
	{
		ViewInternal* pView{ ((ViewInternal*)pViewInterface) };
		if (pView->GetCurrentBackBufferState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(pView->GetCurrentBackBuffer(), pView->GetCurrentBackBufferState(), D3D12_RESOURCE_STATE_RENDER_TARGET) };
			pCommand->pCommandList->ResourceBarrier(1, &barrier);
			pView->SetCurrentBackBufferState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		pCommand->pCommandList->ClearRenderTargetView(pView->GetCurrentBackBufferView().cpuAdress, pView->GetBackBufferClearValue(), 0, nullptr);
	}

	void ClearDepthBuffer(DirectCommand* pCommand, Handle<Texture> handle)
	{
		Texture& texture{ g_texturePool.Get(handle) };

		if (texture.GetState() != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(texture.GetResource(), texture.GetState(), D3D12_RESOURCE_STATE_DEPTH_WRITE) };
			pCommand->pCommandList->ResourceBarrier(1, &barrier);
			texture.SetState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}

		pCommand->pCommandList->ClearDepthStencilView(texture.GetDSV().cpuAdress, D3D12_CLEAR_FLAG_DEPTH, texture.GetClearValue()[0], 0, 0, nullptr);
	}

	void PushGraphicsConstants(DirectCommand* pCommand, dU32 slot, void* pData, dU32 byteSize)
	{
		pCommand->pCommandList->SetGraphicsRoot32BitConstants(slot, byteSize / 4, pData, 0);
	}

	void PushGraphicsBuffer(DirectCommand* pCommand, dU32 slot, Handle<Buffer> handle)
	{
		Assert(handle.IsValid());
		pCommand->pCommandList->SetGraphicsRootConstantBufferView(slot, g_bufferPool.Get(handle).GetGPUAdress());
	}

	void PushGraphicsResource(DirectCommand* pCommand, dU32 slot, Handle<Buffer> handle)
	{
		Assert(handle.IsValid());
		pCommand->pCommandList->SetGraphicsRootShaderResourceView(slot, g_bufferPool.Get(handle).GetGPUAdress());
	}

	void BindGraphicsTexture(DirectCommand* pCommand, dU32 slot, Handle<Texture> handle)
	{
		Assert(handle.IsValid());
		Texture& texture{ g_texturePool.Get(handle) };
		if (texture.GetState() != D3D12_RESOURCE_STATE_GENERIC_READ)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(texture.GetResource(), texture.GetState(), D3D12_RESOURCE_STATE_GENERIC_READ) };
			pCommand->pCommandList->ResourceBarrier(1, &barrier);
			texture.SetState(D3D12_RESOURCE_STATE_GENERIC_READ);
		}
		pCommand->pCommandList->SetGraphicsRootDescriptorTable(slot, texture.GetSRV().gpuAdress);
	}

	void BindIndexBuffer(DirectCommand* pCommand, Handle<Buffer> handle)
	{
		Assert(handle.IsValid());
		pCommand->pCommandList->IASetIndexBuffer(&g_bufferPool.Get(handle).GetIndexBufferView());
	}

	void BindVertexBuffer(DirectCommand* pCommand, Handle<Buffer> handle)
	{
		Assert(handle.IsValid());
		pCommand->pCommandList->IASetVertexBuffers(0, 1, &g_bufferPool.Get(handle).GetVertexBufferView());
	}

	void DrawIndexedInstanced(DirectCommand* pCommand, dU32 indexCount, dU32 instanceCount)
	{
		pCommand->pCommandList->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
	}
}