#include "pch.h"
#include "GraphicsDX12.h"
#include "Dune/Core/Graphics/Window.h"
#include "Dune/Core/Graphics.h"
#include "Dune/Core/Graphics/Mesh.h"
#include "Dune/Core/Graphics/RHI/Buffer.h"
#include "Dune/Core/Graphics/RHI/Texture.h"
#include "Dune/Core/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Core/Graphics/RHI/CommandList.h"
#include "Dune/Core/Graphics/RHI/Fence.h"
#include "Dune/Common/Pool.h"
#include "Dune/Utilities/Utils.h"

namespace Dune::Graphics
{
	class DescriptorHeap;
	class ViewInternal;

	inline ID3D12Resource* ToResource(void* pResource) 
	{
		return (ID3D12Resource*)pResource;
	}

	inline const ID3D12Resource* ToResource(const void* pResource)
	{
		return (const ID3D12Resource*)pResource;
	}

	inline ID3D12DescriptorHeap* ToDescriptorHeap(void* pDescriptorHeap)
	{
		return (ID3D12DescriptorHeap*)pDescriptorHeap;
	}

	inline const ID3D12DescriptorHeap* ToDescriptorHeap(const void* pDescriptorHeap)
	{
		return (const ID3D12DescriptorHeap*)pDescriptorHeap;
	}

	constexpr D3D12_COMMAND_LIST_TYPE ToCommandType(CommandType type)
	{
		switch (type)
		{
		case CommandType::Direct:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		case CommandType::Compute:
			return D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case CommandType::Copy:
			return D3D12_COMMAND_LIST_TYPE_COPY;
		default:
			Assert(0);
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		}
	}

	inline ID3D12CommandAllocator* ToCommandAllocator(void* pCommandAllocator)
	{
		return (ID3D12CommandAllocator*)pCommandAllocator;
	}

	inline const ID3D12CommandAllocator* ToCommandAllocator(const void* pCommandAllocator)
	{
		return (const ID3D12CommandAllocator*)pCommandAllocator;
	}

	inline ID3D12GraphicsCommandList* ToCommandList(void* pCommandList)
	{
		return (ID3D12GraphicsCommandList*)pCommandList;
	}

	inline const ID3D12GraphicsCommandList* ToCommandList(const void* pCommandList)
	{
		return (const ID3D12GraphicsCommandList*)pCommandList;
	}

	inline ID3D12CommandQueue* ToCommandQueue(void* pCommandQueue)
	{
		return (ID3D12CommandQueue*)pCommandQueue;
	}

	inline const ID3D12CommandQueue* ToCommandQueue(const void* pCommandQueue)
	{
		return (const ID3D12CommandQueue*)pCommandQueue;
	}

	inline ID3D12Fence* ToFence(void* pFence)
	{
		return (ID3D12Fence*)pFence;
	}

	inline const ID3D12Fence* ToFence(const void* pFence)
	{
		return (const ID3D12Fence*)pFence;
	}

	struct RingBufferAllocation
	{
		ID3D12Resource* pResource{ nullptr };
		dU8* pCpuAddress{ nullptr };
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

				if (workingAllocation.pFence->GetValue() < workingAllocation.fenceValue)
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
			allocation.pCpuAddress = m_pCpuMemory + offset;
			allocation.offset = offset;
			allocation.byteSize = byteSize;

			return true;
		}

		void Free(RingBufferAllocation& allocation, Fence* pFence, dU64 fenceValue)
		{
			std::lock_guard lock(m_lock);

			WorkingAllocation workingAllocation;
			workingAllocation.offset = allocation.offset;
			workingAllocation.byteSize = allocation.byteSize;			
			m_pLastFence = workingAllocation.pFence = pFence;
			m_lastFenceValue = workingAllocation.fenceValue = fenceValue;
			m_workingAllocation.push(workingAllocation);

			allocation.pResource = nullptr;
			allocation.pCpuAddress = nullptr;
		}

		void Wait()
		{
			if (m_pLastFence)
				m_pLastFence->Wait(m_lastFenceValue);
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
			Fence* pFence{ nullptr };
			dU64 fenceValue{ 0 };
		};

		std::queue<WorkingAllocation> m_workingAllocation;
		Fence* m_pLastFence{ nullptr };
		dU64 m_lastFenceValue{ 0 };
	};

	struct Command
	{
		dU32 commandIndex{ 0 };
		dU32 commandCount{ 0 };
		CommandList* pCommandList{ nullptr };
		CommandAllocator* pCommandAllocators[3]{ nullptr, nullptr, nullptr };
		dU64 fenceValues[3]{ 0, 0, 0 };
		Fence* pFence{ nullptr };
		Device* pDeviceInterface{ nullptr };
	};

	struct DirectCommand : public Command
	{
		DescriptorHeap* heaps[2]{ nullptr, nullptr };
	};

	struct ComputeCommand : public Command
	{
		DescriptorHeap* heaps[2]{ nullptr, nullptr };
	};

	struct CopyCommand : public Command
	{
	};

	struct CommandQueue1
	{
		CommandQueue* pCommandQueue{ nullptr };
		Fence* pFence{ nullptr };
		dU64 fenceValue{ 0 };
		std::mutex mutex{};
	
		dU64 Signal()
		{
			std::lock_guard lock { mutex };
			pCommandQueue->Signal(*pFence, ++fenceValue);
			return fenceValue;
		}
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
		ID3D12InfoQueue* pInfoQueue{ nullptr };
	#endif

		CommandQueue1 directQueue;
		CommandQueue1 copyQueue;
		CommandQueue1 computeQueue;

		CommandAllocator* pCopyCommandAllocator{ nullptr };
		CommandList* pCopyCommandList{ nullptr };

		std::mutex commandLock;
		std::mutex releaseLock;

		dQueue<DyingResource> dyingResources;

		RingBufferAllocator ringBufferAllocator;

		DescriptorHeap* pSrvHeap{ nullptr };
		DescriptorHeap* pSamplerHeap{ nullptr };
		DescriptorHeap* pRtvHeap{ nullptr };
		DescriptorHeap* pDsvHeap{ nullptr };

		Pool<Buffer>	bufferPool;
		Pool<Mesh>		meshPool;
		Pool<Texture>	texturePool;
		Pool<Shader>	shaderPool;
		Pool<Pipeline>	pipelinePool;

		void WaitForCopy()
		{
			Assert(copyQueue.pFence);

			if ( copyQueue.pFence->GetValue() < copyQueue.fenceValue)
				copyQueue.pFence->Wait(copyQueue.fenceValue);
		}

		void CopyBufferRegion(ID3D12Resource* pDestBuffer, dU64 dstOffset, ID3D12Resource* pSrcBuffer, dU64 srcOffset, dU64 size)
		{
			pCopyCommandAllocator->Reset();
			pCopyCommandList->Reset(*pCopyCommandAllocator);

			ToCommandList(pCopyCommandList->Get())->CopyBufferRegion(pDestBuffer, dstOffset, pSrcBuffer, srcOffset, size);
			pCopyCommandList->Close();

			copyQueue.pCommandQueue->ExecuteCommandLists(pCopyCommandList, 1);
			copyQueue.Signal();
			WaitForCopy();
		}

		void UploadTexture(ID3D12Resource* pDest, ID3D12Resource* pUploadBuffer, dU64 uploadByteOffset, dU32 firstSubresource, dU32 subresourceCount, D3D12_SUBRESOURCE_DATA* pSrcData)
		{
			pCopyCommandAllocator->Reset();
			pCopyCommandList->Reset(*pCopyCommandAllocator);

			UpdateSubresources(ToCommandList(pCopyCommandList->Get()), pDest, pUploadBuffer, uploadByteOffset, firstSubresource, subresourceCount, pSrcData);
			pCopyCommandList->Close();

			copyQueue.pCommandQueue->ExecuteCommandLists(pCopyCommandList, 1);
			copyQueue.Signal();
			WaitForCopy();
		}

		void ReleaseResource(ID3D12Object* pResource)
		{
			std::lock_guard lock{ releaseLock };
			dyingResources.push({ pResource, directQueue.fenceValue });
		}

		void ReleaseDyingResources()
		{
			std::lock_guard lock{ releaseLock };
			while (!dyingResources.empty())
			{
				const auto& [pRes, fenceValue] = dyingResources.front();
				if (directQueue.pFence->GetValue() < fenceValue)
					break;

				pRes->Release();
				dyingResources.pop();
			}
		}
	};

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
		pDebugController->SetEnableGPUBasedValidation(true);
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
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDeviceInterface->pDebug));
		ThrowIfFailed(pDevice->QueryInterface(&pDeviceInterface->pInfoQueue));
		pDeviceInterface->pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif // _DEBUG

		DescriptorHeapDesc heapDesc = { .type = (DescriptorHeapType)D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .capacity = 4096 };
		pDeviceInterface->pSrvHeap = new DescriptorHeap(pDeviceInterface, heapDesc);

		heapDesc.capacity = 64;
		heapDesc.type = (DescriptorHeapType)D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		pDeviceInterface->pSamplerHeap = new DescriptorHeap(pDeviceInterface, heapDesc);

		heapDesc.type = (DescriptorHeapType)D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		pDeviceInterface->pRtvHeap = new DescriptorHeap(pDeviceInterface, heapDesc);

		heapDesc.type = (DescriptorHeapType)D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		pDeviceInterface->pDsvHeap = new DescriptorHeap(pDeviceInterface, heapDesc);

		pDeviceInterface->directQueue.pCommandQueue = new CommandQueue(pDeviceInterface, CommandType::Direct);
		pDeviceInterface->directQueue.pFence = new Fence(pDeviceInterface, 0);

		pDeviceInterface->computeQueue.pCommandQueue = new CommandQueue(pDeviceInterface, CommandType::Compute);
		pDeviceInterface->computeQueue.pFence = new Fence(pDeviceInterface, 0);

		pDeviceInterface->copyQueue.pCommandQueue = new CommandQueue(pDeviceInterface, CommandType::Copy);
		pDeviceInterface->copyQueue.pFence = new Fence(pDeviceInterface, 0);

		pDeviceInterface->pCopyCommandAllocator = new CommandAllocator(pDeviceInterface, CommandType::Copy);
		pDeviceInterface->pCopyCommandList = new CommandList(pDeviceInterface, CommandType::Copy, *pDeviceInterface->pCopyCommandAllocator);
		pDeviceInterface->pCopyCommandList->Close();

		pDeviceInterface->ringBufferAllocator.Initialize(pDevice, 128 * 1024 * 1024);

		pDeviceInterface->bufferPool.Initialize(4096);
		pDeviceInterface->meshPool.Initialize(32);
		pDeviceInterface->texturePool.Initialize(64);
		pDeviceInterface->shaderPool.Initialize(16);
		pDeviceInterface->pipelinePool.Initialize(16);

		return pDeviceInterface;
	}

	void DestroyDevice(Device* pDeviceInterface)
	{
		const dU64 frameFenceValue{ pDeviceInterface->directQueue.fenceValue };
		if (pDeviceInterface->directQueue.pFence->GetValue() < frameFenceValue)
			pDeviceInterface->directQueue.pFence->Wait(frameFenceValue);

		pDeviceInterface->WaitForCopy();
		pDeviceInterface->ringBufferAllocator.Shutdown();
		pDeviceInterface->ReleaseDyingResources();
		delete pDeviceInterface->directQueue.pCommandQueue;
		delete pDeviceInterface->directQueue.pFence;

		pDeviceInterface->bufferPool.Destroy();
		pDeviceInterface->meshPool.Destroy();
		pDeviceInterface->texturePool.Destroy();
		pDeviceInterface->shaderPool.Destroy();
		pDeviceInterface->pipelinePool.Destroy();

		delete pDeviceInterface->computeQueue.pCommandQueue;
		delete pDeviceInterface->computeQueue.pFence;

		delete pDeviceInterface->copyQueue.pCommandQueue;
		delete pDeviceInterface->copyQueue.pFence;
		delete pDeviceInterface->pCopyCommandList;
		delete pDeviceInterface->pCopyCommandAllocator;

		pDeviceInterface->pDevice->Release();
		pDeviceInterface->pFactory->Release();
		delete pDeviceInterface->pSrvHeap;
		delete pDeviceInterface->pSamplerHeap;
		delete pDeviceInterface->pRtvHeap;
		delete pDeviceInterface->pDsvHeap;
#ifdef _DEBUG
		pDeviceInterface->pInfoQueue->Release();
		pDeviceInterface->pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		pDeviceInterface->pDebug->Release();
#endif // _DEBUG
		delete(pDeviceInterface);
	}

	CommandAllocator::CommandAllocator(Device* pDeviceInterface, CommandType type)
	{
		ID3D12Device* pDevice{ pDeviceInterface->pDevice };
		ID3D12CommandAllocator* pCommandAllocator{ nullptr };
		ThrowIfFailed(pDevice->CreateCommandAllocator(ToCommandType(type), IID_PPV_ARGS(&pCommandAllocator)));
		NameDXObject(pCommandAllocator, L"CommandAllocator");
		m_pResource = pCommandAllocator;
	}

	CommandAllocator::~CommandAllocator()
	{
		ToCommandAllocator(Get())->Release();
	}

	void CommandAllocator::Reset()
	{
		ToCommandAllocator(Get())->Reset();
	}

	CommandList::CommandList(Device* pDeviceInterface, CommandType type, CommandAllocator& commandAllocator)
	{
		ID3D12Device* pDevice{ pDeviceInterface->pDevice };
		ID3D12GraphicsCommandList* pCommandList{ nullptr };
		ThrowIfFailed(pDevice->CreateCommandList(0, ToCommandType(type), ToCommandAllocator(commandAllocator.Get()), nullptr, IID_PPV_ARGS(&pCommandList)));
		NameDXObject(pCommandList, L"CommandList");
		m_pResource = pCommandList;
	}

	CommandList::~CommandList()
	{
		ToCommandList(Get())->Release();
	}

	void CommandList::Reset(CommandAllocator& commandAllocator/*, Pipeline& pipeline*/)
	{
		ToCommandList(Get())->Reset(ToCommandAllocator(commandAllocator.Get()), nullptr);
	}

	void CommandList::Close()
	{
		ToCommandList(Get())->Close();
	}

	void CommandList::SetDescriptorHeaps(DescriptorHeap& pSrvHeap)
	{
		ID3D12DescriptorHeap* pDescriptorHeaps[]{ ToDescriptorHeap(pSrvHeap.Get()) };
		ToCommandList(Get())->SetDescriptorHeaps(1, pDescriptorHeaps);
	}

	void CommandList::SetDescriptorHeaps(DescriptorHeap& pSrvHeap, DescriptorHeap& pSamplerHeap)
	{
		ID3D12DescriptorHeap* pDescriptorHeaps[]{ ToDescriptorHeap(pSrvHeap.Get()), ToDescriptorHeap(pSamplerHeap.Get()) };
		ToCommandList(Get())->SetDescriptorHeaps(2, pDescriptorHeaps);
	}

	// void CommandList::SetPipeline(const Pipeline& pipeline)

	void CommandList::SetRenderTarget(const dU64* rtvs, dU32 rtvCount, const dU64* dsv)
	{
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		pCommandList->OMSetRenderTargets(rtvCount, (const D3D12_CPU_DESCRIPTOR_HANDLE *)rtvs, false, (const D3D12_CPU_DESCRIPTOR_HANDLE*)dsv);
	}

	void CommandList::ClearRenderTargetView(const Descriptor& rtv, const float clearColor[4])
	{
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		pCommandList->ClearRenderTargetView({ rtv.cpuAddress }, clearColor, 0, nullptr);
	}

	void CommandList::ClearDepthBuffer(const Descriptor& dsv, float depth, float stencil)
	{
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		pCommandList->ClearDepthStencilView({ dsv.cpuAddress }, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, (dU8)stencil, 0, nullptr);
	}

	void CommandList::PushGraphicsConstants(dU32 slot, void* pData, dU32 byteSize)
	{
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		pCommandList->SetGraphicsRoot32BitConstants(slot, byteSize / 4, pData, 0);
	}

	void CommandList::PushGraphicsBuffer(dU32 slot, const Descriptor& cbv)
	{
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		pCommandList->SetGraphicsRootConstantBufferView(slot, { cbv.gpuAddress });
	}

	void CommandList::PushGraphicsResource(dU32 slot, const Descriptor& srv)
	{
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		pCommandList->SetGraphicsRootShaderResourceView(slot, { srv.gpuAddress });
	}

	void CommandList::BindGraphicsResource(dU32 slot, const Descriptor& srv)
	{
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		pCommandList->SetGraphicsRootDescriptorTable(slot, { srv.gpuAddress });
	}

	void CommandList::BindIndexBuffer(Buffer& indexBuffer)
	{
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		D3D12_INDEX_BUFFER_VIEW ibv
		{
			.BufferLocation = indexBuffer.GetGPUAddress(),
			.SizeInBytes = indexBuffer.GetByteSize(),
			.Format = (indexBuffer.GetByteStride() == sizeof(dU32)) ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT
		};
		pCommandList->IASetIndexBuffer(&ibv);
	}

	void CommandList::BindVertexBuffer(Buffer& vertexBuffer)
	{
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		D3D12_VERTEX_BUFFER_VIEW vbv
		{
			.BufferLocation = vertexBuffer.GetGPUAddress(),
			.SizeInBytes = vertexBuffer.GetByteSize(),
			.StrideInBytes = vertexBuffer.GetByteStride()
		};
		pCommandList->IASetVertexBuffers(0, 1, &vbv);
	}

	void CommandList::DrawIndexedInstanced(dU32 indexCount, dU32 instanceCount, dU32 indexStart, dU32 stride, dU32 instanceStart)
	{
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		pCommandList->DrawIndexedInstanced(indexCount, instanceCount, indexStart, stride, instanceStart);
	}

	CommandQueue::CommandQueue(Device* pDeviceInterface, CommandType type)
	{
		ID3D12Device* pDevice{ pDeviceInterface->pDevice };

		D3D12_COMMAND_QUEUE_DESC queueDesc
		{
			.Type = ToCommandType(type),
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
		};

		ID3D12CommandQueue* pCommandQueue{ nullptr };
		ThrowIfFailed(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue)));
		NameDXObject(pCommandQueue, L"CommandQueue");
		m_pResource = pCommandQueue;
	}

	CommandQueue::~CommandQueue()
	{
		ToCommandQueue(Get())->Release();
	}

	void CommandQueue::ExecuteCommandLists(CommandList* pCommandLists, dU32 commandListCount)
	{
		ID3D12CommandList** ppCommandLists{ (ID3D12CommandList**)(pCommandLists) };
		ToCommandQueue(Get())->ExecuteCommandLists(commandListCount, ppCommandLists);
	}

	void CommandQueue::Signal(Fence& fence, dU64 value)
	{
		ToCommandQueue(Get())->Signal(ToFence(fence.Get()), value);
	}

	void CommandQueue::Wait(Fence& fence, dU64 value)
	{
		ToCommandQueue(Get())->Wait(ToFence(fence.Get()), value);
	}

	Fence::Fence(Device* pDeviceInterface, dU64 initialValue)
	{
		ID3D12Device* pDevice{ pDeviceInterface->pDevice };
		ID3D12Fence* pFence{ nullptr };
		ThrowIfFailed(pDevice->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));
		NameDXObject(pFence, L"Fence");
		m_pResource = pFence;
	}

	Fence::~Fence()
	{
		ToFence(Get())->Release();
	}

	dU64 Fence::GetValue()
	{
		return ToFence(Get())->GetCompletedValue();
	}

	void Fence::Wait(dU64 value)
	{
		ThrowIfFailed(ToFence(Get())->SetEventOnCompletion(value, NULL));
	}

	DescriptorHeap::DescriptorHeap(Device* pDeviceInterface, const DescriptorHeapDesc& desc)
	{
		Assert(!Get());
		Assert( desc.capacity != 0);
		m_capacity = desc.capacity;

		ID3D12Device* pDevice = pDeviceInterface->pDevice;

		D3D12_DESCRIPTOR_HEAP_FLAGS flags{};
		flags = (desc.type == DescriptorHeapType::RTV || desc.type == DescriptorHeapType::DSV) ? D3D12_DESCRIPTOR_HEAP_FLAG_NONE : D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		D3D12_DESCRIPTOR_HEAP_TYPE heapType = (D3D12_DESCRIPTOR_HEAP_TYPE) desc.type;

		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
		descriptorHeapDesc.NumDescriptors = m_capacity;
		descriptorHeapDesc.Type = heapType;
		descriptorHeapDesc.Flags = flags;
		descriptorHeapDesc.NodeMask = 0;
		ID3D12DescriptorHeap* pHeap{ nullptr };
		ThrowIfFailed(pDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&pHeap)));
		NameDXObject(pHeap, L"DescriptorHeap");
		m_pResource = pHeap;

		m_cpuAddress = pHeap->GetCPUDescriptorHandleForHeapStart().ptr;
		m_gpuAddress = (flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE ) ? pHeap->GetGPUDescriptorHandleForHeapStart().ptr : 0;
		m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(heapType);

		m_freeSlots.reserve(m_capacity);
		for (dU32 i{ 0 }; i < m_capacity; i++)
		{
			m_freeSlots.push_back(i);
		}
	}

	DescriptorHeap::~DescriptorHeap()
	{
		Assert(m_freeSlots.size() == m_capacity);
		ToDescriptorHeap(Get())->Release();
		m_capacity = 0;
		m_freeSlots.clear();
	}

	[[nodiscard]] Descriptor DescriptorHeap::Allocate()
	{
		Descriptor handle{};
		dU32 slot;

		{
			std::lock_guard lock{ m_lock };
			Assert(!m_freeSlots.empty());
			slot = m_freeSlots.back();
			m_freeSlots.pop_back();
		}

		handle.cpuAddress = m_cpuAddress + m_descriptorSize * (dU64)slot;
		handle.gpuAddress = (m_gpuAddress > 0 ) ? m_gpuAddress + m_descriptorSize * (dU64)slot : 0;

		return handle;
	}

	void DescriptorHeap::Free(Descriptor handle)
	{
		Assert(handle.IsValid());

		dU32 slot{ (dU32)((handle.cpuAddress - m_cpuAddress) / m_descriptorSize) };

		std::lock_guard lock { m_lock };
		m_freeSlots.push_back(slot);
	}

	void OnResize(void* pData);
	class ViewInternal : public View
	{
	public:
		void Initialize(const ViewDesc& desc)
		{
			Window* pWindow = new Window();
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
				ToCommandQueue(m_pDeviceInterface->directQueue.pCommandQueue->Get()),
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
			m_pBackBufferViews = new Descriptor[m_backBufferCount];
			m_pBackBufferStates = new D3D12_RESOURCE_STATES[m_backBufferCount];
			for (UINT i = 0; i < m_backBufferCount; i++)
			{
				m_pBackBufferViews[i] = m_pDeviceInterface->pRtvHeap->Allocate();
				ThrowIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pBackBuffers[i])));
				pDevice->CreateRenderTargetView(m_pBackBuffers[i], nullptr, { m_pBackBufferViews[i].cpuAddress });
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
				m_pDeviceInterface->pRtvHeap->Free(m_pBackBufferViews[i]);
				ReleaseDyingResources();
			}
			DestroyCommand(m_pCommand);

			delete[] m_pBackBufferStates;
			delete[] m_pBackBufferViews;
			delete[] m_pBackBuffers;

			m_pSwapChain->Release();
			
			delete[] m_directFenceValues;

			m_pWindow->Destroy();
			delete(m_pWindow);
		}

		bool ProcessEvents()
		{
			return m_pWindow->Update();
		}

		[[nodiscard]] ID3D12Resource* GetCurrentBackBuffer()
		{
			return m_pBackBuffers[m_frameIndex];
		}

		[[nodiscard]] const Descriptor& GetCurrentBackBufferView() const
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
				ToCommandList(m_pCommand->pCommandList->Get())->ResourceBarrier(1, &barrier);
				m_pBackBufferStates[m_frameIndex] = D3D12_RESOURCE_STATE_PRESENT;
				SubmitCommand(m_pDeviceInterface, m_pCommand);
			}

			ThrowIfFailed(m_pSwapChain->Present(1, 0));

			m_directFenceValues[m_frameIndex] = m_pDeviceInterface->directQueue.Signal();
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
			if (m_pDeviceInterface->directQueue.pFence->GetValue() < frameFenceValue)
				m_pDeviceInterface->directQueue.pFence->Wait(frameFenceValue);

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
				m_pDeviceInterface->pDevice->CreateRenderTargetView(m_pBackBuffers[i], nullptr, { m_pBackBufferViews[i].cpuAddress });
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
		Descriptor* m_pBackBufferViews{ nullptr };
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
		pCommand->pCommandList->Close();
		pDevice->directQueue.pCommandQueue->ExecuteCommandLists(pCommand->pCommandList, 1);
		pDevice->directQueue.Signal();
		pDevice->directQueue.pCommandQueue->Signal(*pCommand->pFence, ++pCommand->fenceValues[pCommand->commandIndex]);
	}

	void SubmitCommand(Device* pDevice, ComputeCommand* pCommand)
	{
		pCommand->pCommandList->Close();
		pDevice->computeQueue.pCommandQueue->ExecuteCommandLists(pCommand->pCommandList, 1);
		pDevice->computeQueue.Signal();
		pDevice->computeQueue.pCommandQueue->Signal(*pCommand->pFence, ++pCommand->fenceValues[pCommand->commandIndex]);
	}

	void SubmitCommand(Device* pDevice, CopyCommand* pCommand)
	{
		pCommand->pCommandList->Close();
		pDevice->copyQueue.pCommandQueue->ExecuteCommandLists(pCommand->pCommandList, 1);
		pDevice->copyQueue.Signal();
		pDevice->copyQueue.pCommandQueue->Signal(*pCommand->pFence, ++pCommand->fenceValues[pCommand->commandIndex]);
	}

	void EndFrame(View* pView)
	{
		((ViewInternal*)pView)->EndFrame();
	}

	const Input* GetInput(View* pView)
	{
		return ((ViewInternal*)pView)->GetInput();
	}	

	void Buffer::Map(dU32 byteOffset, dU32 byteSize, void** pCpuAddress)
	{
		Assert(byteSize <= m_byteSize);
		D3D12_RANGE readRange{ byteOffset, byteOffset + byteSize };
		ThrowIfFailed(ToResource(Get())->Map(0, &readRange, pCpuAddress));
	}

	void Buffer::Unmap(dU32 byteOffset, dU32 byteSize)
	{
		Assert(byteSize <= m_byteSize);
		D3D12_RANGE readRange{ byteOffset, byteOffset + byteSize };
		ToResource(Get())->Unmap(0, &readRange);
	}

	void Buffer::UploadData(const void* pData, dU32  byteOffset, dU32 byteSize)
	{
		Assert(byteSize <= m_byteSize);

		dU64 bufferOffset{ CycleBuffer() };

		RingBufferAllocation allocation;
		m_pDeviceInterface->ringBufferAllocator.Allocate(allocation, m_byteSize);
			
		memcpy(allocation.pCpuAddress, pData, byteSize);
		m_pDeviceInterface->WaitForCopy();
		m_pDeviceInterface->CopyBufferRegion(ToResource(Get()), bufferOffset + byteOffset, allocation.pResource, allocation.offset, byteSize);
			
		m_pDeviceInterface->ringBufferAllocator.Free(allocation, m_pDeviceInterface->copyQueue.pFence, m_pDeviceInterface->copyQueue.fenceValue);
	}

	Buffer::Buffer(Device* pDeviceInterface, const BufferDesc& desc)
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

		ID3D12Resource* pResource;
		ThrowIfFailed(m_pDeviceInterface->pDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			resourceState,
			nullptr,
			IID_PPV_ARGS(&pResource)));
		pResource->SetName(desc.debugName);
		m_pResource = pResource;

		if (m_memory == EBufferMemory::CPU)
		{
			if (desc.pData) 
			{
				void* pCpuAddress{ nullptr };
				Map(0, 0, &pCpuAddress);
				memcpy(pCpuAddress, desc.pData, m_byteSize);
			}				
		}
		else if ( desc.pData )
		{				
			RingBufferAllocation allocation;
			m_pDeviceInterface->ringBufferAllocator.Allocate(allocation, m_byteSize);

			memcpy(allocation.pCpuAddress, desc.pData, m_byteSize);
			m_pDeviceInterface->WaitForCopy();
			m_pDeviceInterface->CopyBufferRegion(pResource, 0, allocation.pResource, allocation.offset, m_byteSize);

			m_pDeviceInterface->ringBufferAllocator.Free(allocation, m_pDeviceInterface->copyQueue.pFence, m_pDeviceInterface->copyQueue.fenceValue);
		}
	}

	Buffer::~Buffer()
	{
		Assert(m_descriptorAllocated == 0);
		m_pDeviceInterface->ReleaseResource(ToResource(Get()));
	}

	dU64 Buffer::GetGPUAddress()
	{
		return ToResource(Get())->GetGPUVirtualAddress() + GetOffset();
	}

	const Descriptor Buffer::CreateSRV()
	{ 
		Assert(m_usage == EBufferUsage::Structured);
#if _DEBUG
		m_descriptorAllocated++;
#endif
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
		Descriptor srv{ m_pDeviceInterface->pSrvHeap->Allocate() };
		ID3D12Device* pDevice{ m_pDeviceInterface->pDevice };
		pDevice->CreateShaderResourceView(ToResource(Get()), &desc, { srv.cpuAddress });
		return srv; 
	}

	const Descriptor Buffer::CreateCBV()
	{ 
		Assert(m_usage == EBufferUsage::Constant); 
#if _DEBUG
		m_descriptorAllocated++;
#endif
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc
		{
			.BufferLocation = ToResource(Get())->GetGPUVirtualAddress(),
			.SizeInBytes = (dU32)m_byteSize
		};
		Descriptor cbv{ m_pDeviceInterface->pSrvHeap->Allocate() };
		ID3D12Device* pDevice{ m_pDeviceInterface->pDevice };
		pDevice->CreateConstantBufferView(&desc, { cbv.cpuAddress } );
		return cbv;
	}

	void Buffer::ReleaseDescriptor(const Descriptor& descriptor) 
	{
		Assert(m_descriptorAllocated == 0);
#if _DEBUG
		m_descriptorAllocated--;
#endif
		m_pDeviceInterface->pSrvHeap->Free(descriptor);
	}

	dU32 Buffer::CycleBuffer()
	{
		Assert(m_memory != EBufferMemory::GPUStatic);

		m_currentBuffer = (m_currentBuffer + 1) % 3;
		return m_currentBuffer * m_byteSize;
	}

	Handle<Buffer> CreateBuffer(Device* pDevice,const BufferDesc& desc)
	{
		return pDevice->bufferPool.Create(pDevice, desc);
	}

	void ReleaseBuffer(Device* pDevice, Handle<Buffer> handle)
	{
		pDevice->bufferPool.Remove(handle);
	}

	void UploadBuffer(Device* pDevice, Handle<Buffer> handle, const void* pData, dU32 byteOffset, dU32 byteSize)
	{
		pDevice->bufferPool.Get(handle).UploadData(pData, byteOffset, byteSize);
	}

	void MapBuffer(Device* pDevice, Handle<Buffer> handle, const void* pData, dU32 byteOffset, dU32 byteSize)
	{
		dU8* pCpuAddress{ nullptr };
		Buffer& buffer{ pDevice->bufferPool.Get(handle) };
		buffer.Map(0, 0, reinterpret_cast<void**>(&pCpuAddress));
		memcpy(pCpuAddress + byteOffset, pData, byteSize);
		buffer.Unmap(0, 0);
	}

	Handle<Mesh> CreateMesh(Device* pDevice, const dU16* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		return pDevice->meshPool.Create(pDevice, pIndices, indexCount, pVertices, vertexCount, vertexByteStride);
	}

	Handle<Mesh> CreateMesh(Device* pDevice, const dU32* pIndices, dU32 indexCount, const void* pVertices, dU32 vertexCount, dU32 vertexByteStride)
	{
		return pDevice->meshPool.Create(pDevice, pIndices, indexCount, pVertices, vertexCount, vertexByteStride);
	}

	void ReleaseMesh(Device* pDevice, Handle<Mesh> handle)
	{
		pDevice->meshPool.Remove(handle);
	}

	const Mesh& GetMesh(Device* pDevice, Handle<Mesh> handle)
	{
		return pDevice->meshPool.Get(handle);
	}

	void Texture::Transition(CommandList* pCommand, dU32 targetState)
	{
		std::lock_guard lock{ m_transitionMutex };
		if (targetState == m_state)
			return;
		D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(ToResource(Get()), (D3D12_RESOURCE_STATES)m_state, (D3D12_RESOURCE_STATES)targetState) };
		ToCommandList(pCommand->Get())->ResourceBarrier(1, &barrier);
		m_state = targetState;
	}

	Texture::Texture(Device* pDeviceInterface, const TextureDesc& desc)
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

		ID3D12Resource* pResource{ nullptr };
		ThrowIfFailed(pDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureResourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			pClearValue,
			IID_PPV_ARGS(&pResource)));
		NameDXObject(pResource, desc.debugName);
		m_pResource = pResource;

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
			m_pDeviceInterface->ringBufferAllocator.Allocate(allocation, (dU32)byteSize);

			D3D12_SUBRESOURCE_DATA srcDatas[16];
			LONG_PTR prevSlice = 0;
			for (dU32 i = 0; i < numSubresource; i++)
			{
				LONG_PTR slicePitch = rowsSizeInBytes[i] * rowsCount[i];
				srcDatas[i] = { .pData = (dU8*) desc.pData + prevSlice, .RowPitch = (LONG_PTR)rowsSizeInBytes[i], .SlicePitch = slicePitch };
				prevSlice += slicePitch;
			}
						
			m_pDeviceInterface->UploadTexture(ToResource(Get()), allocation.pResource, allocation.offset, 0, numSubresource, srcDatas);

			m_pDeviceInterface->ringBufferAllocator.Free(allocation, m_pDeviceInterface->copyQueue.pFence, m_pDeviceInterface->copyQueue.fenceValue);
		}

		switch (m_usage)
		{
		case ETextureUsage::RTV:
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc
			{
				.Format = format
			};

			m_RTV = new Descriptor[m_dimensions[2]];

			if (m_dimensions[2] > 1)
			{
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtvDesc.Texture2DArray.ArraySize = 1;
				rtvDesc.Texture2DArray.MipSlice = 0;
				rtvDesc.Texture2DArray.PlaneSlice = 0;

				for (dU32 i{ 0 }; i < m_dimensions[2]; i++)
				{
					rtvDesc.Texture2DArray.FirstArraySlice = D3D12CalcSubresource(0, i, 0, 1, 1);
					m_RTV[i] = m_pDeviceInterface->pRtvHeap->Allocate();
					pDevice->CreateRenderTargetView(ToResource(Get()), &rtvDesc, { m_RTV[i].cpuAddress });
				}
			}
			else
			{
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				rtvDesc.Texture2D.PlaneSlice = 0;
				rtvDesc.Texture2D.MipSlice = 0;
				m_RTV[0] = m_pDeviceInterface->pRtvHeap->Allocate();
				pDevice->CreateRenderTargetView(ToResource(Get()), &rtvDesc, { m_RTV[0].cpuAddress });
			}

			break;
		}
		case ETextureUsage::DSV:
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
			dsvDesc.Format = format;
			m_DSV = new Descriptor[m_dimensions[2]];

			if (m_dimensions[2] > 1)
			{
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsvDesc.Texture2DArray.ArraySize = 1;
				dsvDesc.Texture2DArray.MipSlice = 0;

				for (dU32 i{ 0 }; i < m_dimensions[2]; i++)
				{
					dsvDesc.Texture2DArray.FirstArraySlice = D3D12CalcSubresource(0, i, 0, 1, 1);
					m_DSV[i] = m_pDeviceInterface->pDsvHeap->Allocate();
					pDevice->CreateDepthStencilView(ToResource(Get()), &dsvDesc, { m_DSV[i].cpuAddress });
				}
			}
			else
			{
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsvDesc.Texture2D.MipSlice = 0;
				m_DSV[0] = m_pDeviceInterface->pDsvHeap->Allocate();
				pDevice->CreateDepthStencilView(ToResource(Get()), &dsvDesc, { m_DSV[0].cpuAddress });
			}

			break;
		}
		case ETextureUsage::SRV:
			break;
		default:
			Assert(false);
			break;
		}

		m_SRV = m_pDeviceInterface->pSrvHeap->Allocate();
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

		pDevice->CreateShaderResourceView(ToResource(Get()), &srvDesc, { m_SRV.cpuAddress });
	}

	Texture::~Texture()
	{
		m_pDeviceInterface->ReleaseResource(ToResource(Get()));
		m_pDeviceInterface->pSrvHeap->Free(m_SRV);
		if (m_usage == ETextureUsage::RTV)
		{
			for (dU32 i = 0; i < m_dimensions[2]; i++)
				m_pDeviceInterface->pRtvHeap->Free(m_RTV[i]);
			delete[] m_RTV;
		}
		else if (m_usage == ETextureUsage::DSV)
		{
			for (dU32 i = 0; i < m_dimensions[2]; i++)
				m_pDeviceInterface->pDsvHeap->Free(m_DSV[i]);
			delete[] m_DSV;
		}
	}

	Handle<Texture> CreateTexture(Device* pDevice, const TextureDesc& desc)
	{
		return pDevice->texturePool.Create(pDevice, desc);
	}

	void ReleaseTexture(Device* pDevice, Handle<Texture> handle)
	{
		pDevice->texturePool.Remove(handle);
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
		friend Pool<Shader, Shader, true>;

		IDxcBlob* m_pByteCode{ nullptr };
	};

	Handle<Shader> CreateShader(Device* pDevice, const ShaderDesc& desc)
	{
		return pDevice->shaderPool.Create(desc);
	}

	void ReleaseShader(Device* pDevice, Handle<Shader> handle)
	{
		pDevice->shaderPool.Remove(handle);
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

	D3D12_STATIC_SAMPLER_DESC AddStaticSampler( dU32 slot, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE wrap, D3D12_COMPARISON_FUNC comp = D3D12_COMPARISON_FUNC_ALWAYS)
	{
		D3D12_STATIC_SAMPLER_DESC desc;
		desc.AddressU = wrap;
		desc.AddressV = wrap;
		desc.AddressW = wrap;
		desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		desc.ComparisonFunc = comp;
		desc.Filter = filter;
		desc.MaxAnisotropy = 8;
		desc.MaxLOD = FLT_MAX;
		desc.MinLOD = 0.0f;
		desc.RegisterSpace = 1;
		desc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		desc.ShaderRegister = slot;
		desc.MipLODBias = 0.0f;

		return desc;
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
		dU32 staticSamplerCount{ 0 };
		D3D12_STATIC_SAMPLER_DESC staticSamplerDesc[]
		{
			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER),

			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER),

			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP),
			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_BORDER),

			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_COMPARISON_FUNC_GREATER),
			AddStaticSampler(staticSamplerCount++, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_COMPARISON_FUNC_GREATER)
		};

		rootSignatureDesc.Init_1_1(rootParamCount, rootParameters, staticSamplerCount, staticSamplerDesc, flags);

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
		Pipeline(Device* pDeviceInterface, const GraphicsPipelineDesc& desc)
			: m_pDeviceInterface { pDeviceInterface }
		{
			Assert(pDeviceInterface);
			ID3D12Device* pDevice = m_pDeviceInterface->pDevice;

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

			m_pRootSignature = ComputeRootSignature(pDevice, desc.bindingLayout, flags);

			D3D12_INPUT_ELEMENT_DESC* pInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[desc.inputLayout.size()];
			for (dU32 i = 0; i < desc.inputLayout.size(); i++)
			{
				const VertexInput& input = desc.inputLayout[i];
				pInputElementDescs[i] = { input.pName, input.index, (DXGI_FORMAT)input.format, input.slot, input.byteAlignedOffset, input.bPerInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
			}

			IDxcBlob* pVSBlob{ (desc.vertexShader.IsValid()) ? m_pDeviceInterface->shaderPool.Get(desc.vertexShader).GetByteCode() : nullptr};
			IDxcBlob* pPSBlob{ (desc.vertexShader.IsValid()) ? m_pDeviceInterface->shaderPool.Get(desc.pixelShader).GetByteCode() : nullptr};

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
			ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));
			delete[] pInputElementDescs;
		}

		~Pipeline()
		{
			m_pDeviceInterface->ReleaseResource(m_pRootSignature);
			m_pDeviceInterface->ReleaseResource(m_pPipelineState);
		}

		DISABLE_COPY_AND_MOVE(Pipeline);

	private:
		friend Pool<Pipeline, Pipeline, true>;

		ID3D12RootSignature* m_pRootSignature{ nullptr };
		ID3D12PipelineState* m_pPipelineState{ nullptr };
		Device* m_pDeviceInterface{ nullptr };
	};

	Handle<Pipeline> CreateGraphicsPipeline(Device* pDevice, const GraphicsPipelineDesc& desc)
	{
		return pDevice->pipelinePool.Create(pDevice, desc);
	}

	void ReleasePipeline(Device* pDevice, Handle<Pipeline> handle)
	{
		pDevice->pipelinePool.Remove(handle);
	}

	Command* InitializeCommand(Device* pDeviceInterface, Command* pCommand, CommandType type)
	{
		ID3D12Device* pDevice{ pDeviceInterface->pDevice };

		for (dU32 i = 0; i < pCommand->commandCount; i++)
			pCommand->pCommandAllocators[i] = new CommandAllocator(pDeviceInterface, type);
		pCommand->pCommandList = new CommandList(pDeviceInterface, type, *pCommand->pCommandAllocators[0]);
		pCommand->pCommandList->Close();

		pCommand->pFence = new Fence(pDeviceInterface, 0);
		pCommand->pDeviceInterface = pDeviceInterface;

		return pCommand;
	}

	DirectCommand* CreateDirectCommand(const DirectCommandDesc& desc)
	{
		DirectCommand* pCommand = new DirectCommand();
		Assert(desc.pView);
		ViewInternal* pView = (ViewInternal*)desc.pView;
		Device* pDevice{ pView->GetDevice() };
		pCommand->commandCount = pView->GetFrameCount();
		pCommand->heaps[0] = pView->GetDevice()->pSrvHeap;
		pCommand->heaps[1] = pView->GetDevice()->pSamplerHeap;
		InitializeCommand(pDevice, pCommand, CommandType::Direct);

		return pCommand;
	}

	ComputeCommand* CreateComputeCommand(const ComputeCommandDesc& desc)
	{
		ComputeCommand* pCommand = new ComputeCommand();
		Assert(desc.pDevice);
		Device* pDeviceInterface = desc.pDevice;
		pCommand->commandCount = (dU32) desc.buffering;
		pCommand->heaps[0] = pDeviceInterface->pSrvHeap;
		pCommand->heaps[1] = pDeviceInterface->pSamplerHeap;		

		InitializeCommand(pDeviceInterface, pCommand, CommandType::Compute);

		return pCommand;
	}

	CopyCommand* CreateCopyCommand(const CopyCommandDesc& desc)
	{
		CopyCommand* pCommand = new CopyCommand();
		Assert(desc.pDevice);
		Device* pDeviceInterface = desc.pDevice;
		pCommand->commandCount = (dU32)desc.buffering;

		InitializeCommand(pDeviceInterface, pCommand, CommandType::Copy);

		return pCommand;
	}

	void WaitCommand(Command* pCommand)
	{
		dU64 fenceValue = pCommand->fenceValues[pCommand->commandIndex];
		if (pCommand->pFence->GetValue() < fenceValue)
			pCommand->pFence->Wait(fenceValue);		
	}

	void ReleaseCommand(Command* pCommand)
	{
		delete pCommand->pCommandList;
		for (dU32 i = 0; i < pCommand->commandCount; i++)
			delete pCommand->pCommandAllocators[i];
		delete pCommand->pFence;	
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
		pCommand->pCommandAllocators[commandIndex]->Reset();
		pCommand->pCommandList->Reset(*pCommand->pCommandAllocators[commandIndex]);
		pCommand->pCommandList->SetDescriptorHeaps(*pCommand->heaps[0], *pCommand->heaps[1]);
	}

	void ResetCommand(DirectCommand* pCommand, Handle<Pipeline> handle)
	{
		dU32 commandIndex = pCommand->commandIndex = (pCommand->commandIndex + 1) % pCommand->commandCount;
		WaitCommand(pCommand);
		Pipeline& pipeline{ pCommand->pDeviceInterface->pipelinePool.Get(handle) };
		pCommand->pCommandAllocators[commandIndex]->Reset();
		pCommand->pCommandList->Reset(*pCommand->pCommandAllocators[commandIndex]);
		ToCommandList(pCommand->pCommandList->Get())->SetPipelineState(pipeline.GetPipelineStateObject());
		ToCommandList(pCommand->pCommandList->Get())->SetGraphicsRootSignature(pipeline.GetRootSignature());
		ToCommandList(pCommand->pCommandList->Get())->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommand->pCommandList->SetDescriptorHeaps(*pCommand->heaps[0], *pCommand->heaps[1]);
	}

	void SetPipeline(DirectCommand* pCommand, Handle<Pipeline> handle)
	{
		Assert(handle.IsValid());
		Pipeline& pipeline{ pCommand->pDeviceInterface->pipelinePool.Get(handle) };
		ToCommandList(pCommand->pCommandList->Get())->SetGraphicsRootSignature(pipeline.GetRootSignature());
		ToCommandList(pCommand->pCommandList->Get())->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ToCommandList(pCommand->pCommandList->Get())->SetPipelineState(pipeline.GetPipelineStateObject());
	}

	void SetRenderTarget(DirectCommand* pCommand, Handle<Texture> renderTarget)
	{
		Assert(renderTarget.IsValid());
		Texture& texture{ pCommand->pDeviceInterface->texturePool.Get(renderTarget) };
		const dU32* pDimensions{ texture.GetDimensions() };
		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)pDimensions[0], (float)pDimensions[1], 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, (LONG)pDimensions[0], (LONG)pDimensions[1] };	

		texture.Transition(pCommand->pCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

		ToCommandList(pCommand->pCommandList->Get())->RSSetViewports(1, &viewport);
		ToCommandList(pCommand->pCommandList->Get())->RSSetScissorRects(1, &scissor);

		pCommand->pCommandList->SetRenderTarget(&texture.GetRTV().cpuAddress, 1, nullptr);
	}

	void SetRenderTarget(DirectCommand* pCommand, Handle<Texture> renderTarget, Handle<Texture> depthBuffer)
	{
		Assert(renderTarget.IsValid());
		Texture& texture{ pCommand->pDeviceInterface->texturePool.Get(renderTarget) };
		const dU32* pDimensions{ texture.GetDimensions() };
		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)pDimensions[0], (float)pDimensions[1], 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, (LONG)pDimensions[0], (LONG)pDimensions[1] };

		texture.Transition(pCommand->pCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

		Texture& depthTexture{ pCommand->pDeviceInterface->texturePool.Get(depthBuffer) };

		depthTexture.Transition(pCommand->pCommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		ToCommandList(pCommand->pCommandList->Get())->RSSetViewports(1, &viewport);
		ToCommandList(pCommand->pCommandList->Get())->RSSetScissorRects(1, &scissor);
		pCommand->pCommandList->SetRenderTarget(&texture.GetRTV().cpuAddress, 1, &depthTexture.GetDSV().cpuAddress);
	}

	void SetRenderTarget(DirectCommand* pCommand, View* pViewInterface, Handle<Texture> depthBuffer)
	{
		ViewInternal* pView{ ((ViewInternal*)pViewInterface) };
		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)pView->GetWidth(), (float)pView->GetHeight(), 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, (LONG)pView->GetWidth(), (LONG)pView->GetHeight() };

		if (pView->GetCurrentBackBufferState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(pView->GetCurrentBackBuffer(), pView->GetCurrentBackBufferState(), D3D12_RESOURCE_STATE_RENDER_TARGET)};
			ToCommandList(pCommand->pCommandList->Get())->ResourceBarrier(1, &barrier);
			pView->SetCurrentBackBufferState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		Texture& depthTexture{ pCommand->pDeviceInterface->texturePool.Get(depthBuffer) };

		depthTexture.Transition(pCommand->pCommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		ToCommandList(pCommand->pCommandList->Get())->RSSetViewports(1, &viewport);
		ToCommandList(pCommand->pCommandList->Get())->RSSetScissorRects(1, &scissor);
		pCommand->pCommandList->SetRenderTarget(&pView->GetCurrentBackBufferView().cpuAddress, 1, &depthTexture.GetDSV().cpuAddress);
	}

	void SetRenderTarget(DirectCommand* pCommand, View* pViewInterface)
	{
		ViewInternal* pView{ ((ViewInternal*)pViewInterface) };
		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, (float)pView->GetWidth(), (float)pView->GetHeight(), 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, (LONG)pView->GetWidth(), (LONG)pView->GetHeight() };

		if (pView->GetCurrentBackBufferState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(pView->GetCurrentBackBuffer(), pView->GetCurrentBackBufferState(), D3D12_RESOURCE_STATE_RENDER_TARGET) };
			ToCommandList(pCommand->pCommandList->Get())->ResourceBarrier(1, &barrier);
			pView->SetCurrentBackBufferState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		ToCommandList(pCommand->pCommandList->Get())->RSSetViewports(1, &viewport);
		ToCommandList(pCommand->pCommandList->Get())->RSSetScissorRects(1, &scissor);

		pCommand->pCommandList->SetRenderTarget(&pView->GetCurrentBackBufferView().cpuAddress, 1, nullptr);
	}

	void ClearRenderTarget(DirectCommand* pCommand, Handle<Texture> handle)
	{
		Texture& texture{ pCommand->pDeviceInterface->texturePool.Get(handle) };
		texture.Transition(pCommand->pCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

		pCommand->pCommandList->ClearRenderTargetView(texture.GetRTV(), texture.GetClearValue());
	}

	void ClearRenderTarget(DirectCommand* pCommand, View* pViewInterface)
	{
		ViewInternal* pView{ ((ViewInternal*)pViewInterface) };
		if (pView->GetCurrentBackBufferState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(pView->GetCurrentBackBuffer(), pView->GetCurrentBackBufferState(), D3D12_RESOURCE_STATE_RENDER_TARGET) };
			ToCommandList(pCommand->pCommandList->Get())->ResourceBarrier(1, &barrier);
			pView->SetCurrentBackBufferState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		pCommand->pCommandList->ClearRenderTargetView(pView->GetCurrentBackBufferView(), pView->GetBackBufferClearValue());
	}

	void ClearDepthBuffer(DirectCommand* pCommand, Handle<Texture> handle)
	{
		Texture& texture{ pCommand->pDeviceInterface->texturePool.Get(handle) };
		texture.Transition(pCommand->pCommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		pCommand->pCommandList->ClearDepthBuffer(texture.GetDSV(), texture.GetClearValue()[0], 0);
	}

	void PushGraphicsConstants(DirectCommand* pCommand, dU32 slot, void* pData, dU32 byteSize)
	{
		pCommand->pCommandList->PushGraphicsConstants(slot, pData, byteSize);
	}

	void BindGraphicsTexture(DirectCommand* pCommand, dU32 slot, Handle<Texture> handle)
	{
		Assert(handle.IsValid());
		Texture& texture{ pCommand->pDeviceInterface->texturePool.Get(handle) };
		texture.Transition(pCommand->pCommandList, D3D12_RESOURCE_STATE_GENERIC_READ);
		pCommand->pCommandList->BindGraphicsResource(slot, texture.GetSRV());
	}

	void BindIndexBuffer(DirectCommand* pCommand, Handle<Buffer> handle)
	{
		Assert(handle.IsValid());
		Buffer& buffer{ pCommand->pDeviceInterface->bufferPool.Get(handle) };
		pCommand->pCommandList->BindIndexBuffer(buffer);
	}

	void BindVertexBuffer(DirectCommand* pCommand, Handle<Buffer> handle)
	{
		Assert(handle.IsValid());
		Buffer& buffer{ pCommand->pDeviceInterface->bufferPool.Get(handle) };
		pCommand->pCommandList->BindVertexBuffer(buffer);
	}

	void DrawIndexedInstanced(DirectCommand* pCommand, dU32 indexCount, dU32 instanceCount)
	{
		pCommand->pCommandList->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
	}
}