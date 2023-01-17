#include "pch.h"
#include "Buffer.h"
#include "Renderer.h"

namespace Dune
{
	Buffer::Buffer(const BufferDesc& desc, const void* pData, dU32 size)
		: m_usage{ desc.usage }
		, m_size{ size }
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		D3D12_RESOURCE_STATES resourceState{};

		Renderer& renderer{ Renderer::GetInstance() };
		Assert(renderer.IsInitialized());

		if (m_usage == EBufferUsage::Upload)
		{
			heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
		}
		else
		{
			heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			resourceState = D3D12_RESOURCE_STATE_COMMON;
		}

		D3D12_RESOURCE_DESC resourceDesc{ CD3DX12_RESOURCE_DESC::Buffer(m_size) };
		
		ThrowIfFailed(renderer.GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			resourceState,
			nullptr,
			IID_PPV_ARGS(&m_buffer)));
		m_buffer->SetName(L"Buffer");

		if (m_usage == EBufferUsage::Upload)
		{
			D3D12_RANGE readRange;
			readRange.Begin = 0;
			readRange.End = 0;
			ThrowIfFailed(m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cpuAdress)));
		}
		else
		{
			heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;

			ThrowIfFailed(renderer.GetDevice()->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				resourceState,
				nullptr,
				IID_PPV_ARGS(&m_uploadBuffer)));
			m_buffer->SetName(L"UploadBuffer");

			D3D12_RANGE readRange;
			readRange.Begin = 0;
			readRange.End = 0;
			ThrowIfFailed(m_uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cpuAdress)));
		}

		if (pData)
		{
			UploadData(pData, m_size);
		}
	}

	Buffer::~Buffer()
	{
		Renderer& renderer{ Renderer::GetInstance() };
		renderer.m_dyingBuffer[renderer.m_frameIndex].emplace_back(m_buffer);
		if (m_usage == EBufferUsage::Default)
		{
			m_uploadBuffer->Release();
		}
	}

	void Buffer::UploadData(const void* pData, dU32 size)
	{
		// I don't support resize yet
		Assert(size <= m_size);

		memcpy(m_cpuAdress, pData, m_size);

		if (m_usage == EBufferUsage::Default)
		{
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
		}
	}
}

