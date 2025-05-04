#include "pch.h"
#include "GraphicsDX12.h"
#include "Dune/Graphics/Window.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Graphics/RHI/Device.h"
#include "Dune/Graphics/RHI/Buffer.h"
#include "Dune/Graphics/RHI/Texture.h"
#include "Dune/Graphics/RHI/DescriptorHeap.h"
#include "Dune/Graphics/RHI/CommandList.h"
#include "Dune/Graphics/RHI/Fence.h"
#include "Dune/Graphics/RHI/GraphicsPipeline.h"
#include "Dune/Graphics/RHI/Swapchain.h"
#include "Dune/Graphics/RHI/Barrier.h"
#include "Dune/Graphics/RHI/Shader.h"
#include "Dune/Graphics/RHI/ImGuiWrapper.h"
#include "Dune/Utilities/Utils.h"

namespace Dune::Graphics
{
	class DescriptorHeap;
	class ViewInternal;

	struct GraphicsPipelineDX12
	{
		ID3D12RootSignature* pRootSignature{ nullptr };
		ID3D12PipelineState* pPipelineState{ nullptr };
	};

	inline ID3D12Device* ToDevice(void* pDevice)
	{
		return (ID3D12Device*)pDevice;
	}

	inline ID3D12Resource* ToResource(void* pResource) 
	{
		return (ID3D12Resource*)pResource;
	}

	inline const ID3D12Resource* ToResource(const void* pResource)
	{
		return (const ID3D12Resource*)pResource;
	}

	constexpr D3D12_RESOURCE_STATES ToResourceState(EResourceState state)
	{
		switch (state)
		{
		case EResourceState::Undefined:
			return D3D12_RESOURCE_STATE_COMMON;
		case EResourceState::ShaderResource:
			return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
		case EResourceState::ShaderResourceNonPixel:
			return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		case EResourceState::UAV:
			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		case EResourceState::CopySource:
			return D3D12_RESOURCE_STATE_COPY_SOURCE;
		case EResourceState::CopyDest:
			return D3D12_RESOURCE_STATE_COPY_DEST;
		case EResourceState::RenderTarget:
			return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case EResourceState::DepthStencil:
			return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case EResourceState::DepthStencilRead:
			return D3D12_RESOURCE_STATE_DEPTH_READ;
		case EResourceState::VertexBuffer:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		case EResourceState::IndexBuffer:
			return D3D12_RESOURCE_STATE_INDEX_BUFFER;
		case EResourceState::ConstantBuffer:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		case EResourceState::IndirectArgument:
			return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		default:
			Assert(0);
			return D3D12_RESOURCE_STATE_COMMON;
		}
	}

	inline ID3D12DescriptorHeap* ToDescriptorHeap(void* pDescriptorHeap)
	{
		return (ID3D12DescriptorHeap*)pDescriptorHeap;
	}

	inline const ID3D12DescriptorHeap* ToDescriptorHeap(const void* pDescriptorHeap)
	{
		return (const ID3D12DescriptorHeap*)pDescriptorHeap;
	}

	constexpr D3D12_COMMAND_LIST_TYPE ToCommandType(ECommandType type)
	{
		switch (type)
		{
		case ECommandType::Direct:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		case ECommandType::Compute:
			return D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case ECommandType::Copy:
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

	inline GraphicsPipelineDX12* ToPipeline(void* pPipeline)
	{
		return (GraphicsPipelineDX12*)pPipeline;
	}

	inline const GraphicsPipelineDX12* ToPipeline(const void* pPipeline)
	{
		return (const GraphicsPipelineDX12*)pPipeline;
	}

	inline IDXGISwapChain4* ToSwapchain(void* pSwapchain)
	{
		return (IDXGISwapChain4*)pSwapchain;
	}

	inline const IDXGISwapChain4* ToSwapchain(const void* pSwapchain)
	{
		return (const IDXGISwapChain4*)pSwapchain;
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

	struct DeviceInternal
	{
		IDXGIFactory7* pFactory{ nullptr };
	#ifdef _DEBUG
		IDXGIDebug1* pDebug{ nullptr };
		ID3D12InfoQueue* pInfoQueue{ nullptr };
	#endif
	};

	void Device::Initialize()
	{
		UINT dxgiFactoryFlags{ 0 };
	#ifdef _DEBUG
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	#endif // _DEBUG

		DeviceInternal* pInternal = new DeviceInternal();

		IDXGIFactory7* pFactory{ nullptr };
		ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&pFactory)));
		pInternal->pFactory = pFactory;

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
		m_pResource = pDevice;

	#ifdef _DEBUG
		IDXGIDebug1* pDebug{ nullptr };
		ID3D12InfoQueue* pInfoQueue{ nullptr };
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));
		ThrowIfFailed(pDevice->QueryInterface(&pInfoQueue));
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		pInternal->pDebug = pDebug;
		pInternal->pInfoQueue = pInfoQueue;
	#endif // _DEBUG
		m_pInternal = pInternal;
	}

	void Device::Destroy()
	{
		ID3D12Device* pDevice{ (ID3D12Device*)m_pResource };
		pDevice->Release();
		DeviceInternal* pInternal = (DeviceInternal*)m_pInternal;
		pInternal->pFactory->Release();
#ifdef _DEBUG
		pInternal->pInfoQueue->Release();
		pInternal->pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		pInternal->pDebug->Release();
#endif // _DEBUG
		delete pInternal;
	}

	void Device::CreateSRV(Descriptor& descriptor, Texture& texture, const SRVDesc& desc)
	{
		ID3D12Device* pDevice{ ToDevice(Get()) };

		const dU32* pDimensions = texture.GetDimensions();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = (DXGI_FORMAT)texture.GetFormat();

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		if (pDimensions[2] > 1)
		{
			srvDesc.Texture2DArray.ArraySize = pDimensions[2];
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.MipLevels = desc.mipLevels;
			srvDesc.Texture2DArray.MostDetailedMip = desc.mipStart;
			srvDesc.Texture2DArray.PlaneSlice = 0;
			srvDesc.Texture2DArray.ResourceMinLODClamp = desc.mipBias;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		}
		else
		{
			srvDesc.Texture2D.MipLevels = desc.mipLevels;
			srvDesc.Texture2D.MostDetailedMip = desc.mipStart;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = desc.mipBias;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		}

		pDevice->CreateShaderResourceView(ToResource(texture.Get()), &srvDesc, { descriptor.cpuAddress });
	}

	void Device::CreateSRV(Descriptor& descriptor, Buffer& buffer)
	{
		ID3D12Device* pDevice{ ToDevice(Get()) };
		
		dU32 byteStride = buffer.GetByteStride();
		dU32 elementCount = (dU32)(buffer.GetByteSize() / byteStride);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc
		{
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Buffer
			{
				.NumElements = elementCount,
				.StructureByteStride = byteStride,
				.Flags = D3D12_BUFFER_SRV_FLAG_NONE,
			}
		};

		pDevice->CreateShaderResourceView(ToResource(buffer.Get()), &srvDesc, { descriptor.cpuAddress });
	}

	void Device::CreateRTV(Descriptor& descriptor, Texture& texture, const RTVDesc& desc)
	{
		ID3D12Device* pDevice{ ToDevice(Get()) };

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = (DXGI_FORMAT)texture.GetFormat();
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		pDevice->CreateRenderTargetView(ToResource(texture.Get()), &rtvDesc, { descriptor.cpuAddress });
	}

	void Device::CreateDSV(Descriptor& descriptor, Texture& texture, const DSVDesc& desc)
	{
		ID3D12Device* pDevice{ ToDevice(Get()) };

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = (DXGI_FORMAT)texture.GetFormat();
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		pDevice->CreateDepthStencilView(ToResource(texture.Get()), &dsvDesc, { descriptor.cpuAddress });
	}

	void Barrier::Initialize(dU32 barrierCapacity)
	{
		m_barrierCapacity = barrierCapacity;
		D3D12_RESOURCE_BARRIER* barriers = new D3D12_RESOURCE_BARRIER[m_barrierCapacity];
		m_pResource = barriers;
	}

	void Barrier::Destroy()
	{
		D3D12_RESOURCE_BARRIER* barriers = (D3D12_RESOURCE_BARRIER*)Get();
		delete[] barriers;
	}

	void Barrier::PushTransition(void* pResource, EResourceState stateBefore, EResourceState stateAfter)
	{
		Assert(m_barrierCount < m_barrierCapacity);
		D3D12_RESOURCE_BARRIER* barriers = (D3D12_RESOURCE_BARRIER*)Get();
		D3D12_RESOURCE_BARRIER& barrier = barriers[m_barrierCount++];
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(ToResource(pResource), ToResourceState(stateBefore), ToResourceState(stateAfter));
	}

	void CommandAllocator::Initialize(Device* pDeviceInterface, ECommandType type)
	{
		ID3D12Device* pDevice{ ToDevice(pDeviceInterface->Get()) };
		ID3D12CommandAllocator* pCommandAllocator{ nullptr };
		ThrowIfFailed(pDevice->CreateCommandAllocator(ToCommandType(type), IID_PPV_ARGS(&pCommandAllocator)));
		NameDXObject(pCommandAllocator, L"CommandAllocator");
		m_pResource = pCommandAllocator;
	}

	void CommandAllocator::Destroy()
	{
		ToCommandAllocator(Get())->Release();
	}

	void CommandAllocator::Reset()
	{
		ToCommandAllocator(Get())->Reset();
	}

	void CommandList::Initialize(Device* pDeviceInterface, ECommandType type, CommandAllocator& commandAllocator)
	{
		ID3D12Device* pDevice{ ToDevice(pDeviceInterface->Get()) };
		ID3D12GraphicsCommandList* pCommandList{ nullptr };
		ThrowIfFailed(pDevice->CreateCommandList(0, ToCommandType(type), ToCommandAllocator(commandAllocator.Get()), nullptr, IID_PPV_ARGS(&pCommandList)));
		NameDXObject(pCommandList, L"CommandList");
		m_pResource = pCommandList;
	}

	void CommandList::Destroy()
	{
		ToCommandList(Get())->Release();
	}

	void CommandList::Reset(CommandAllocator& commandAllocator)
	{
		ToCommandList(Get())->Reset(ToCommandAllocator(commandAllocator.Get()), nullptr);
	}

	void CommandList::Reset(CommandAllocator& commandAllocator, const GraphicsPipeline& pipeline)
	{
		const GraphicsPipelineDX12* pPipeline{ ToPipeline(pipeline.Get()) };
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		pCommandList->Reset(ToCommandAllocator(commandAllocator.Get()), pPipeline->pPipelineState);
		pCommandList->SetGraphicsRootSignature(pPipeline->pRootSignature);
	}

	void CommandList::Close()
	{
		ToCommandList(Get())->Close();
	}

	void CommandList::SetPrimitiveTopology(EPrimitiveTopology primitiveTopology)
	{
		ToCommandList(Get())->IASetPrimitiveTopology((D3D12_PRIMITIVE_TOPOLOGY)primitiveTopology);
	}

	void CommandList::SetViewports(dU32 numViewport, Viewport* pViewports )
	{
		ToCommandList(Get())->RSSetViewports(numViewport, (const D3D12_VIEWPORT*)pViewports);
	}

	void CommandList::SetScissors(dU32 numScissors, Scissor* scissors)
	{
		ToCommandList(Get())->RSSetScissorRects(numScissors, (const D3D12_RECT*)scissors);
	}

	void CommandList::CopyBufferRegion(Buffer& destBuffer, dU64 dstOffset, Buffer& srcBuffer, dU64 srcOffset, dU64 size)
	{
		ToCommandList(Get())->CopyBufferRegion(ToResource(destBuffer.Get()), dstOffset, ToResource(srcBuffer.Get()), srcOffset, size);
	}

	void CommandList::UploadTexture(Texture& destTexture, Buffer& uploadBuffer, dU64 uploadByteOffset, dU32 firstSubresource, dU32 numSubresource, void* pSrcData)
	{
		ID3D12Resource* pResource = ToResource(destTexture.Get());
		D3D12_RESOURCE_DESC desc = pResource->GetDesc();

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[16];
		dU32 rowsCount[16];
		dU64 rowsSizeInBytes[16];
		dU64 byteSize = 0;
		ID3D12Device* pDevice{ nullptr };
		pResource->GetDevice(IID_PPV_ARGS(&pDevice));
		pDevice->GetCopyableFootprints(&desc, 0, numSubresource, 0, layouts, rowsCount, rowsSizeInBytes, &byteSize);

		D3D12_SUBRESOURCE_DATA srcDatas[16];
		LONG_PTR prevSlice = 0;
		for (dU32 i = 0; i < numSubresource; i++)
		{
			LONG_PTR slicePitch = rowsSizeInBytes[i] * rowsCount[i];
			srcDatas[i] = { .pData = (dU8*)pSrcData + prevSlice, .RowPitch = (LONG_PTR)rowsSizeInBytes[i], .SlicePitch = slicePitch };
			prevSlice += slicePitch;
		}

		UpdateSubresources(ToCommandList(Get()), pResource, ToResource(uploadBuffer.Get()), uploadByteOffset, firstSubresource, numSubresource, srcDatas);
		pDevice->Release();
	}

	void CommandList::Transition(const Barrier& barrier)
	{
		ToCommandList(Get())->ResourceBarrier(barrier.GetBarrierCount(), (const D3D12_RESOURCE_BARRIER*)barrier.Get());
	}

	void CommandList::SetDescriptorHeaps(DescriptorHeap& srvHeap)
	{
		ID3D12DescriptorHeap* pDescriptorHeaps[]{ ToDescriptorHeap(srvHeap.Get()) };
		ToCommandList(Get())->SetDescriptorHeaps(1, pDescriptorHeaps);
	}

	void CommandList::SetDescriptorHeaps(DescriptorHeap& srvHeap, DescriptorHeap& samplerHeap)
	{
		ID3D12DescriptorHeap* pDescriptorHeaps[]{ ToDescriptorHeap(srvHeap.Get()), ToDescriptorHeap(samplerHeap.Get()) };
		ToCommandList(Get())->SetDescriptorHeaps(2, pDescriptorHeaps);
	}

	void CommandList::SetGraphicsPipeline(const GraphicsPipeline& pipeline)
	{
		const GraphicsPipelineDX12* pPipline{ ToPipeline(pipeline.Get()) };
		ID3D12GraphicsCommandList* pCommandList{ ToCommandList(Get()) };
		pCommandList->SetGraphicsRootSignature(pPipline->pRootSignature);
		pCommandList->SetPipelineState(pPipline->pPipelineState);
	}

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
			.BufferLocation = ToResource(indexBuffer.Get())->GetGPUVirtualAddress(),
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
			.BufferLocation = ToResource(vertexBuffer.Get())->GetGPUVirtualAddress(),
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

	void CommandQueue::Initialize(Device* pDeviceInterface, ECommandType type)
	{
		ID3D12Device* pDevice{ ToDevice(pDeviceInterface->Get()) };

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

	void CommandQueue::Destroy()
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

	void Fence::Initialize(Device* pDeviceInterface, dU64 initialValue)
	{
		ID3D12Device* pDevice{ ToDevice(pDeviceInterface->Get()) };
		ID3D12Fence* pFence{ nullptr };
		ThrowIfFailed(pDevice->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));
		NameDXObject(pFence, L"Fence");
		m_pResource = pFence;
	}

	void Fence::Destroy()
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

	void DescriptorHeap::Initialize(Device* pDeviceInterface, const DescriptorHeapDesc& desc)
	{
		Assert(!Get());
		Assert(desc.capacity != 0);
		m_capacity = desc.capacity;

		ID3D12Device* pDevice{ ToDevice(pDeviceInterface->Get()) };

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
		for (dU32 i = 0; i < m_capacity; i++)
			m_freeSlots.push_back(i);
	}

	void DescriptorHeap::Destroy()
	{
		Assert(m_freeSlots.size() == m_capacity);
		ToDescriptorHeap(Get())->Release();
		m_capacity = 0;
		m_freeSlots.clear();
	}

	Descriptor DescriptorHeap::Allocate()
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

	void Swapchain::Initialize(Device* pDeviceInterface, Window* pWindow, CommandQueue* pCommandQueue, const SwapchainDesc& desc)
	{
		IDXGIFactory7* pFactory = ((DeviceInternal*)pDeviceInterface->GetInternal())->pFactory;
		ID3D12Device* pDevice{ ToDevice(pDeviceInterface->Get()) };

		m_latency = desc.latency;

		RECT clientRect;
		HWND handle{ (HWND)pWindow->GetHandle() };
		GetClientRect(handle, &clientRect);

		D3D12_COMMAND_QUEUE_DESC queueDesc
		{
			.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
		};

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc
		{
			.Width = 0,
			.Height = 0,
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.Stereo = false,
			.SampleDesc = {.Count = 1, .Quality = 0},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = m_latency,
			.Scaling = DXGI_SCALING_STRETCH,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
			.Flags = 0
		};

		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain{ nullptr };
		ThrowIfFailed(pFactory->CreateSwapChainForHwnd(
			ToCommandQueue(pCommandQueue->Get()),
			handle,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain
		));

		ThrowIfFailed(pFactory->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER));
		ThrowIfFailed(swapChain.As(&pSwapChain));

		m_pBackBuffers = new Texture[m_latency];
		for (UINT i = 0; i < m_latency; i++)
		{
			Texture& backBuffer = m_pBackBuffers[i];
			ID3D12Resource* pResource{ nullptr };
			ThrowIfFailed(pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pResource)));
			NameDXObjectIndexed(pResource, i, L"BackBuffers");
			backBuffer.m_pResource = pResource;
			backBuffer.m_desc.format = EFormat::R8G8B8A8_UNORM;
			backBuffer.m_desc.dimensions[2] = 1;
		}

		m_pResource = pSwapChain.Detach();
	}

	void Swapchain::Destroy()
	{
		for (dU32 i = 0; i < m_latency; i++)
			m_pBackBuffers[i].Destroy();

		delete[] m_pBackBuffers;
		ToSwapchain(Get())->Release();
	}

	void Swapchain::Resize(dU32 width, dU32 height)
	{
		for (dU32 i = 0; i < m_latency; i++)
			m_pBackBuffers[i].Destroy();

		IDXGISwapChain4* pSwapChain{ ToSwapchain(Get()) };
		ThrowIfFailed(pSwapChain->ResizeBuffers(
			m_latency,
			width, height,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0
		));

		for (dU32 i = 0; i < m_latency; i++)
		{
			Texture& backBuffer = m_pBackBuffers[i];
			ID3D12Resource* pResource{ nullptr };
			ThrowIfFailed(pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pResource)));
			NameDXObjectIndexed(pResource, i, L"BackBuffers");
			backBuffer.m_pResource = pResource;
			backBuffer.m_desc.dimensions[0] = width;
			backBuffer.m_desc.dimensions[1] = height;
		}
	}

	void Swapchain::Present()
	{
		ToSwapchain(Get())->Present(1, 0);
	}

	Texture& Swapchain::GetBackBuffer(dU32 index)
	{
		Assert(m_latency > index); 
		return m_pBackBuffers[index];
	}

	dU32 Swapchain::GetCurrentBackBufferIndex()
	{
		return ToSwapchain(Get())->GetCurrentBackBufferIndex();
	}

	void Buffer::Initialize(Device* pDeviceInterface, const BufferDesc& desc)
	{
		m_usage = desc.usage;
		m_memory = desc.memory;
		m_byteSize = desc.byteSize;
		m_byteStride = desc.byteStride;

		D3D12_HEAP_PROPERTIES heapProps{};
		D3D12_RESOURCE_STATES resourceState{ ToResourceState(desc.initialState) };

		if (m_usage == EBufferUsage::Constant)
		{
			m_byteSize = Utils::AlignTo(m_byteSize, 256);
		}

		if (m_memory == EBufferMemory::CPU)
			heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		else
			heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		D3D12_RESOURCE_DESC resourceDesc{ CD3DX12_RESOURCE_DESC::Buffer(m_byteSize) };

		ID3D12Resource* pResource;
		ThrowIfFailed(ToDevice(pDeviceInterface->Get())->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			resourceState,
			nullptr,
			IID_PPV_ARGS(&pResource)));
		NameDXObject(pResource, desc.debugName);
		m_pResource = pResource;
	}

	void Buffer::Destroy()
	{
		ToResource(Get())->Release();
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

	void Texture::Initialize(Device* pDeviceInterface, const TextureDesc& desc)
	{
		m_desc = desc;
		
		ID3D12Device* pDevice{ ToDevice(pDeviceInterface->Get()) };
		D3D12_CLEAR_VALUE clearValue{};
		D3D12_CLEAR_VALUE* pClearValue{ nullptr };

		DXGI_FORMAT format{ (DXGI_FORMAT)desc.format };
		D3D12_RESOURCE_FLAGS resourceFlag{ D3D12_RESOURCE_FLAG_NONE };
 		
		if ((m_desc.usage & ETextureUsage::RenderTarget) != ETextureUsage::Invalid)
			resourceFlag |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		if ((m_desc.usage & ETextureUsage::DepthStencil) != ETextureUsage::Invalid)
			resourceFlag |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if ((m_desc.usage & ETextureUsage::UnorderedAccess) != ETextureUsage::Invalid)
			resourceFlag |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		if ((m_desc.usage & (ETextureUsage::RenderTarget | ETextureUsage::DepthStencil)) != ETextureUsage::Invalid)
		{
			clearValue = CD3DX12_CLEAR_VALUE{ format, m_desc.clearValue };
			pClearValue = &clearValue;
		}

		CD3DX12_RESOURCE_DESC textureResourceDesc(
			D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			0,
			m_desc.dimensions[0],
			m_desc.dimensions[1],
			m_desc.dimensions[2],
			m_desc.mipLevels,
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
			ToResourceState(desc.initialState),
			pClearValue,
			IID_PPV_ARGS(&pResource)));
		NameDXObject(pResource, desc.debugName);
		m_pResource = pResource;
	}

	void Texture::Destroy()
	{
		ToResource(Get())->Release();
	}

	dU64 Texture::GetRequiredIntermediateSize(dU32 firstSubresource, dU32 numSubresource)
	{
		ID3D12Resource* pResource{ ToResource(Get()) };
		D3D12_RESOURCE_DESC Desc = pResource->GetDesc();
		dU64 requiredSize = 0;

		ID3D12Device* pDevice;
		pResource->GetDevice(IID_PPV_ARGS(&pDevice));
		pDevice->GetCopyableFootprints(&Desc, firstSubresource, numSubresource, 0, nullptr, nullptr, nullptr, &requiredSize);
		pDevice->Release();

		return requiredSize;
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
	
	void Shader::Initialize(const ShaderDesc& desc)
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

		IDxcBlob* pByteCode{ nullptr };
		pResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pByteCode), nullptr);
		m_pResource = pByteCode;
	}

	void Shader::Destroy()
	{
		((IDxcBlob*)m_pResource)->Release();
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

	ID3D12RootSignature* ComputeRootSignature(ID3D12Device* pDevice, const dSpan<BindingSlot>& layout, D3D12_ROOT_SIGNATURE_FLAGS flags)
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
		for (const BindingSlot& slot : layout)
		{
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
				rootParameters[rootParamCount++].InitAsConstants( slot.byteSize >> 2, bufferRegisterOffset + bufferRegister[(dU32)slot.visibility]++, 0u, visibility) ;
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

	void GraphicsPipeline::Initialize(Device* pDeviceInterface, const GraphicsPipelineDesc& desc)
	{
		Assert(pDeviceInterface);
		ID3D12Device* pDevice{ ToDevice(pDeviceInterface->Get()) };

		D3D12_ROOT_SIGNATURE_FLAGS flags
		{
			(desc.inputLayout.IsEmpty()) ? D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS
		};

		ID3D12RootSignature* pRootSignature{ ComputeRootSignature(pDevice, desc.bindingLayout, flags) };

		D3D12_INPUT_ELEMENT_DESC* pInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[desc.inputLayout.GetSize()];
		for (dU32 i = 0; i < desc.inputLayout.GetSize(); i++)
		{
			const VertexInput& input = desc.inputLayout[i];
			pInputElementDescs[i] = { input.pName, input.index, (DXGI_FORMAT)input.format, input.slot, input.byteAlignedOffset, input.bPerInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		}

		IDxcBlob* pVSBlob{ (desc.pVertexShader) ? (IDxcBlob*)desc.pVertexShader->Get() : nullptr };
		IDxcBlob* pPSBlob{ (desc.pPixelShader) ? (IDxcBlob*)desc.pPixelShader->Get() : nullptr };

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.InputLayout = { pInputElementDescs, (dU32)desc.inputLayout.GetSize()};
		psoDesc.pRootSignature = pRootSignature;
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
		dU8 renderTargetCount = desc.renderTargetCount;
		psoDesc.NumRenderTargets = renderTargetCount;
		for (dU8 i = 0; i < renderTargetCount; i++)
			psoDesc.RTVFormats[i] = (DXGI_FORMAT)desc.renderTargetsFormat[i];
		psoDesc.DSVFormat = (DXGI_FORMAT)desc.depthStencilFormat;
		psoDesc.SampleDesc.Count = 1;
		ID3D12PipelineState* pPipelineState{ nullptr };
		ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pPipelineState)));

		GraphicsPipelineDX12* pPipeline = new GraphicsPipelineDX12();
		pPipeline->pPipelineState = pPipelineState;
		pPipeline->pRootSignature = pRootSignature;
		m_pResource = pPipeline;

		delete[] pInputElementDescs;
	}

	void GraphicsPipeline::Destroy()
	{
		GraphicsPipelineDX12* pPipelineDX12 = ToPipeline(Get());
		
		pPipelineDX12->pRootSignature->Release();
		pPipelineDX12->pPipelineState->Release();

		delete pPipelineDX12;
	}

	void ImGuiWrapper::Initialize(Window& window, Renderer& renderer) 
	{
		Assert(!window.m_pImGui && !renderer.m_pImGui);
		Assert(!m_pContext);
		window.m_pImGui = renderer.m_pImGui = this;
		m_pRenderer = &renderer;
		m_pWindow = &window;
		Lock();
		IMGUI_CHECKVERSION();
		m_pContext = ImGui::CreateContext();
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(window.GetHandle());

		ImGui_ImplDX12_InitInfo init_info = {};
		init_info.Device = ToDevice(renderer.m_pDevice->Get());
		init_info.CommandQueue = ToCommandQueue(renderer.m_pCommandQueue->Get());;
		init_info.NumFramesInFlight = _countof(renderer.m_frames);
		init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
		init_info.SrvDescriptorHeap = ToDescriptorHeap(renderer.m_pSrvHeap->Get());
		init_info.UserData = renderer.m_pSrvHeap;
		init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) 
			{ 
				Descriptor descriptor{ ((DescriptorHeap*)info->UserData)->Allocate() };
				*out_cpu_handle = { descriptor.cpuAddress };
				*out_gpu_handle = { descriptor.gpuAddress };
			};
		init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
			{ 
				Descriptor descriptor{ cpu_handle.ptr, gpu_handle.ptr };
				((DescriptorHeap*)info->UserData)->Free(descriptor);
			};
		ImGui_ImplDX12_Init(&init_info);
		Unlock();
	}

	void ImGuiWrapper::NewFrame()
	{
		Assert(m_pContext);
		Lock();
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		Unlock();
	}

	void ImGuiWrapper::Render(CommandList& commandList)
	{
		Assert(m_pContext);
		Lock();
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), ToCommandList(commandList.Get()));
		Unlock();
	}

	void ImGuiWrapper::Destroy()
	{
		Assert(m_pContext);
		Lock();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		Unlock();
		m_pWindow->m_pImGui = m_pRenderer->m_pImGui = nullptr;
		m_pContext = nullptr;
		m_pRenderer = nullptr;
		m_pWindow = nullptr;
	}
}