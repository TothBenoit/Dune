#pragma once

#include "Dune/Graphics/BufferDesc.h"

namespace Dune
{
	class Buffer
	{
	public:
		inline dU32 GetSize() const { return m_size; }
		inline EBufferUsage GetUsage() const { return m_usage; }
		ID3D12Resource* GetResource() { return m_buffer; }

		void UploadData(const void* pData, dU32 size);
	private:
		Buffer(const BufferDesc& desc, const void* pData, dU32 size);
		~Buffer();
		DISABLE_COPY_AND_MOVE(Buffer);
	
	private:
		dU8*				m_cpuAdress;
		ID3D12Resource*		m_uploadBuffer;
		ID3D12Resource*		m_buffer;
		const EBufferUsage	m_usage;
		dU32				m_size;

		template <typename T, typename H> friend class Pool;
	};
}
