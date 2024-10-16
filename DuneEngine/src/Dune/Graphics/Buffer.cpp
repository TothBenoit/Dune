#include "pch.h"
#include "Buffer.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Utilities/Utils.h"

namespace Dune
{
	struct DynamicBuffer : public InternalBuffer
	{
		dU64 m_cycleFrame{ (dU64)-1 };
		dU8* m_cpuAdress{ nullptr };

		dU32 CycleBuffer()
		{
			Assert(m_cycleFrame != Renderer::GetInstance().GetElaspedFrame());
			m_cycleFrame = Renderer::GetInstance().GetElaspedFrame();

			m_currentBuffer = (m_currentBuffer + 1) % Renderer::GetFrameCount();
			return m_currentBuffer * m_size;
		}
	};

	struct GPUBuffer : public DynamicBuffer
	{
		void UploadData(const void* pData, dU32 size)
		{
			Assert(size <= m_size);

			dU32 offset{ CycleBuffer() };

			Renderer& renderer{ Renderer::GetInstance() };
			memcpy(m_cpuAdress, pData, size);
			// TODO : Upload system in the renderer.
			renderer.WaitForCopy();
			ThrowIfFailed(renderer.m_copyCommandAllocator->Reset());
			ThrowIfFailed(renderer.m_copyCommandList->Reset(renderer.m_copyCommandAllocator.Get(), nullptr));

			renderer.m_copyCommandList->CopyBufferRegion(m_buffer, offset, m_uploadBuffer, 0, m_size);
			renderer.m_copyCommandList->Close();
			dVector<ID3D12CommandList*> ppCommandLists{ renderer.m_copyCommandList.Get() };
			renderer.m_copyCommandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());
			renderer.WaitForCopy();
		}

		ID3D12Resource* m_uploadBuffer{ nullptr };
	};

	struct CPUBuffer : public DynamicBuffer
	{
		void MapData(const void* pData, dU32 size)
		{
			Assert(size <= m_size);

			dU32 offset{ CycleBuffer() };
			memcpy(m_cpuAdress + offset, pData, m_size);
		}
	};

	Buffer::Buffer(const BufferDesc& desc)
		: m_usage{ desc.usage }
		, m_memory{ desc.memory }
		, m_byteStride { desc.byteStride }
	{
		AllocateInternalBuffer(desc.memory);
		m_internalBuffer->m_size = desc.byteSize;

		D3D12_HEAP_PROPERTIES heapProps{};
		D3D12_RESOURCE_STATES resourceState{};

		Renderer& renderer{ Renderer::GetInstance() };
		Assert(renderer.IsInitialized());

		if (m_usage == EBufferUsage::Constant)
		{
			m_internalBuffer->m_size = Utils::AlignTo(desc.byteSize, 256);
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

		dU32 bufferCount = (m_memory == EBufferMemory::GPUStatic) ? 1 :  Renderer::GetFrameCount();

		D3D12_RESOURCE_DESC resourceDesc{ CD3DX12_RESOURCE_DESC::Buffer(bufferCount * m_internalBuffer->m_size )};
		
		ThrowIfFailed(renderer.GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			resourceState,
			nullptr,
			IID_PPV_ARGS(&m_internalBuffer->m_buffer)));
		m_internalBuffer->m_buffer->SetName(desc.debugName);

		CreateView();

		if (m_memory == EBufferMemory::CPU)
		{
			D3D12_RANGE readRange{};
			ThrowIfFailed(m_internalBuffer->m_buffer->Map(0, &readRange, reinterpret_cast<void**>( &((CPUBuffer*)m_internalBuffer)->m_cpuAdress) ));
			if (desc.pData)
				MapData(desc.pData, desc.byteSize);
		}
		else
		{
			heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;

			ID3D12Resource* pUploadBuffer{ nullptr };
			dU8* pCpuAdress{ nullptr };
			D3D12_RESOURCE_DESC uploadResourceDesc{ CD3DX12_RESOURCE_DESC::Buffer( m_internalBuffer->m_size ) };
			ThrowIfFailed(renderer.GetDevice()->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&uploadResourceDesc,
				resourceState,
				nullptr,
				IID_PPV_ARGS(&pUploadBuffer)));
			pUploadBuffer->SetName(L"UploadBuffer");

			D3D12_RANGE readRange{};
			ThrowIfFailed(pUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pCpuAdress)));

			if (m_memory != EBufferMemory::GPUStatic)
			{
				GPUBuffer* pGPUInternalBuffer{ (GPUBuffer*)m_internalBuffer };
				pGPUInternalBuffer->m_cpuAdress = pCpuAdress;
				pGPUInternalBuffer->m_uploadBuffer = pUploadBuffer;
				if (desc.pData)
					UploadData(desc.pData, m_internalBuffer->m_size);
			}
			else
			{
				Assert(desc.pData);
				memcpy(pCpuAdress, desc.pData, m_internalBuffer->m_size);
				Renderer& renderer{ Renderer::GetInstance() };
				// TODO : Upload system in the renderer.
				renderer.WaitForCopy();
				ThrowIfFailed(renderer.m_copyCommandAllocator->Reset());
				ThrowIfFailed(renderer.m_copyCommandList->Reset(renderer.m_copyCommandAllocator.Get(), nullptr));

				renderer.m_copyCommandList->CopyBufferRegion(m_internalBuffer->m_buffer, 0, pUploadBuffer, 0, m_internalBuffer->m_size);
				renderer.m_copyCommandList->Close();
				dVector<ID3D12CommandList*> ppCommandLists{ renderer.m_copyCommandList.Get() };
				renderer.m_copyCommandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());
				renderer.WaitForCopy();
				pUploadBuffer->Release();

			}
		}
	}

	Buffer::~Buffer()
	{
		Renderer& renderer{ Renderer::GetInstance() };
		renderer.ReleaseResource(m_internalBuffer->m_buffer);

		if (m_usage == EBufferUsage::Structured || m_usage == EBufferUsage::Constant)
		{
			dU32 viewCount{ (m_memory == EBufferMemory::GPUStatic) ? 1 : Renderer::GetFrameCount() };
			for (dU32 i = 0; i < viewCount; i++)
				renderer.m_srvHeap.Free(m_pViews[i]);
			delete[] m_pViews;
		}

		if (m_memory == EBufferMemory::GPU)
		{
			((GPUBuffer*)m_internalBuffer)->m_uploadBuffer->Release();
		}

		delete m_internalBuffer;
	}

	void Buffer::AllocateInternalBuffer(EBufferMemory memory)
	{
		switch (memory)
		{
		case EBufferMemory::CPU:
			m_internalBuffer = new CPUBuffer();
			break;
		case EBufferMemory::GPU:
			m_internalBuffer = new GPUBuffer();
			break;
		case EBufferMemory::GPUStatic:
			m_internalBuffer = new InternalBuffer();
			break;
		default:
			Assert(false);
			break;
		}
	}

	void Buffer::CreateView()
	{
		dU32 bufferCount{ (m_memory == EBufferMemory::GPUStatic) ? 1 : Renderer::GetFrameCount() };
		InternalBuffer* pInternalBuffer{ (InternalBuffer*)m_internalBuffer };

		switch (m_usage)
		{
		case EBufferUsage::Vertex:
		{
			m_vertexBufferView.BufferLocation = pInternalBuffer->m_buffer->GetGPUVirtualAddress();
			m_vertexBufferView.StrideInBytes = m_byteStride;
			m_vertexBufferView.SizeInBytes = pInternalBuffer->m_size;
			break;
		}
		case EBufferUsage::Index:
		{
			Assert(m_byteStride == sizeof(dU32) || m_byteStride == sizeof(dU16) )
			m_indexBufferView.BufferLocation = pInternalBuffer->m_buffer->GetGPUVirtualAddress();
			m_indexBufferView.Format = (m_byteStride == sizeof(dU32))? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
			m_indexBufferView.SizeInBytes = pInternalBuffer->m_size;
			break;
		}
		case EBufferUsage::Constant:
		{
			m_pViews = new DescriptorHandle[bufferCount];
			for (dU32 i = 0; i < bufferCount; i++)
				m_pViews[i] = Renderer::GetInstance().m_srvHeap.Allocate();

			D3D12_CONSTANT_BUFFER_VIEW_DESC desc
			{
				.BufferLocation = pInternalBuffer->m_buffer->GetGPUVirtualAddress(),
				.SizeInBytes = pInternalBuffer->m_size
			};

			Renderer::GetInstance().GetDevice()->CreateConstantBufferView(&desc, m_pViews[pInternalBuffer->m_currentBuffer].cpuAdress);
			break;
		}

		case EBufferUsage::Structured:
		{
			m_pViews = new DescriptorHandle[bufferCount];
			for (dU32 i = 0; i < bufferCount; i++)
				m_pViews[i] = Renderer::GetInstance().m_srvHeap.Allocate();

			dU32 elementCount = pInternalBuffer->m_size / m_byteStride;
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
				Renderer::GetInstance().GetDevice()->CreateShaderResourceView(pInternalBuffer->m_buffer, &desc, m_pViews[i].cpuAdress);
			}
			break;
		}
		default:
			break;
		}
	}

	void Buffer::MapData(const void* pData, dU32 size)
	{
		Assert(m_memory == EBufferMemory::CPU);
		CPUBuffer* pInternalBuffer{ (CPUBuffer*)m_internalBuffer };
		pInternalBuffer->MapData(pData, size);
	}

	void Buffer::UploadData(const void* pData, dU32 size)
	{
		Assert(m_memory == EBufferMemory::GPU);
		GPUBuffer* pInternalBuffer{ (GPUBuffer*)m_internalBuffer };
		pInternalBuffer->UploadData(pData, size);
	}
}

