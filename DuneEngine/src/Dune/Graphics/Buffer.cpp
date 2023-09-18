#include "pch.h"
#include "Buffer.h"
#include "Dune/Graphics/Renderer.h"
#include "Dune/Utilities/Utils.h"

namespace Dune
{
	Buffer::Buffer(const BufferDesc& desc)
		: m_usage{ desc.usage }
		, m_memory{ desc.memory }
		, m_size{ desc.byteSize }
		, m_currentBuffer{ 0 }
		, m_cycleFrame{ (dU64) -1}
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		D3D12_RESOURCE_STATES resourceState{};

		Renderer& renderer{ Renderer::GetInstance() };
		Assert(renderer.IsInitialized());

		if (m_usage == EBufferUsage::Constant)
		{
			m_size = Utils::AlignTo(m_size, 256);
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

		D3D12_RESOURCE_DESC resourceDesc{ CD3DX12_RESOURCE_DESC::Buffer( (m_memory == EBufferMemory::GPUStatic) ? m_size : m_size * Renderer::GetFrameCount() )};
		
		ThrowIfFailed(renderer.GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			resourceState,
			nullptr,
			IID_PPV_ARGS(&m_buffer)));
		m_buffer->SetName(desc.debugName);

		if (m_memory == EBufferMemory::CPU)
		{
			D3D12_RANGE readRange{};
			ThrowIfFailed(m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cpuAdress)));
			if (desc.pData)
				MapData(desc.pData, desc.byteSize);
		}
		else
		{
			heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;

			D3D12_RESOURCE_DESC uploadResourceDesc{ CD3DX12_RESOURCE_DESC::Buffer( m_size ) };
			ThrowIfFailed(renderer.GetDevice()->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&uploadResourceDesc,
				resourceState,
				nullptr,
				IID_PPV_ARGS(&m_uploadBuffer)));
			m_buffer->SetName(L"UploadBuffer");

			D3D12_RANGE readRange{};
			ThrowIfFailed(m_uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cpuAdress)));

			if (m_memory != EBufferMemory::GPUStatic)
			{
				if (desc.pData)
					UploadData(desc.pData, m_size);
			}
			else
			{
				Assert(desc.pData);
				memcpy(m_cpuAdress, desc.pData, m_size);
				Renderer& renderer{ Renderer::GetInstance() };
				// TODO : Upload system in the renderer.
				renderer.WaitForCopy();
				ThrowIfFailed(renderer.m_copyCommandAllocator->Reset());
				ThrowIfFailed(renderer.m_copyCommandList->Reset(renderer.m_copyCommandAllocator.Get(), nullptr));

				renderer.m_copyCommandList->CopyBufferRegion(m_buffer, 0, m_uploadBuffer, 0, m_size);
				renderer.m_copyCommandList->Close();
				dVector<ID3D12CommandList*> ppCommandLists{ renderer.m_copyCommandList.Get() };
				renderer.m_copyCommandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());
				renderer.WaitForCopy();
				m_uploadBuffer->Release();
			}
		}
	}

	Buffer::~Buffer()
	{
		Renderer& renderer{ Renderer::GetInstance() };
		renderer.ReleaseResource(m_buffer);
		if (m_memory == EBufferMemory::GPU)
		{
			m_uploadBuffer->Release();
		}
	}

	dU32 Buffer::CycleBuffer()
	{
		Assert(m_memory != EBufferMemory::GPUStatic);

		Assert(m_cycleFrame != Renderer::GetInstance().GetElaspedFrame());
		m_cycleFrame = Renderer::GetInstance().GetElaspedFrame();

		m_currentBuffer = (m_currentBuffer + 1) % Renderer::GetFrameCount();
		return m_currentBuffer * m_size;
	}

	void Buffer::MapData(const void* pData, dU32 size)
	{
		Assert(m_memory == EBufferMemory::CPU);
		Assert(size <= m_size);

		dU32 offset{ CycleBuffer() };
		memcpy(m_cpuAdress + offset, pData, m_size);
	}

	void Buffer::UploadData(const void* pData, dU32 size)
	{
		Assert(m_memory == EBufferMemory::GPU);
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
}

