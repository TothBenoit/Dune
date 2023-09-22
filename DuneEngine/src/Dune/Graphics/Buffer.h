#pragma once

#include "Dune/Graphics/BufferDesc.h"
#include "Dune/Graphics/DescriptorHeap.h"

namespace Dune
{
	template <typename T, typename H>
	class Pool;

	class Buffer
	{
	public:
		[[nodiscard]] inline dU32 GetSize() const { return m_size; }
		[[nodiscard]] inline EBufferUsage GetUsage() const { return m_usage; }
		[[nodiscard]] const ID3D12Resource* GetResource() const { return m_buffer; }
		[[nodiscard]] ID3D12Resource* GetResource() { return m_buffer; }
		[[nodiscard]] dU32 GetOffset() const { return m_currentBuffer * m_size; }
		[[nodiscard]] dU32 GetCurrentBufferIndex() const { return m_currentBuffer; }
		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetGPUAdress() const { return m_buffer->GetGPUVirtualAddress() + GetOffset(); }
		[[nodiscard]] const DescriptorHandle& GetSRV() const { Assert(m_usage == EBufferUsage::Structured); return m_pViews[m_currentBuffer]; }
		[[nodiscard]] const DescriptorHandle& GetCBV() const { Assert(m_usage == EBufferUsage::Constant); return m_pViews[m_currentBuffer]; }
		[[nodiscard]] const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { Assert(m_usage == EBufferUsage::Index); return m_indexBufferView; }
		[[nodiscard]] const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { Assert(m_usage == EBufferUsage::Vertex); return m_vertexBufferView; }

		void MapData(const void* pData, dU32 size);
		void UploadData(const void* pData, dU32 size);

	private:
		Buffer(const BufferDesc& desc);
		~Buffer();
		DISABLE_COPY_AND_MOVE(Buffer);

		dU32 CycleBuffer();
		void CreateView();
	private:
		friend Pool<Buffer, Buffer>;

		dU8*				m_cpuAdress;
		ID3D12Resource*		m_uploadBuffer;
		ID3D12Resource*		m_buffer;
		const EBufferUsage	m_usage;
		const EBufferMemory m_memory;
		dU32				m_size;
		dU32				m_byteStride;
		dU32				m_currentBuffer;
		dU64				m_cycleFrame;

		union
		{
			DescriptorHandle*			m_pViews;
			D3D12_INDEX_BUFFER_VIEW		m_indexBufferView;
			D3D12_VERTEX_BUFFER_VIEW	m_vertexBufferView;
		};
	};
}
